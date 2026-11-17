/*
 * This file is part of EasyRPG Editor.
 *
 * EasyRPG Editor is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * EasyRPG Editor is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with EasyRPG Editor. If not, see <http://www.gnu.org/licenses/>.
 */

#include "project_image_provider.h"
#include "common/image_loader.h"

#include <QTransform>
#include <QUrlQuery>

#include <algorithm>

namespace {

/** A percentage of a command (100: unchanged) as a tone of the Player (128: unchanged) */
int toneOf(int percent) {
	return std::clamp(percent * 128 / 100, 0, 255);
}

/** The hard light of a tone on a level, the Player's tint of a picture */
int hardLight(int tone, int level) {
	return std::clamp(tone <= 128 ? 2 * tone * level / 255 : 255 - 2 * (255 - tone) * (255 - level) / 255, 0, 255);
}

/** A level of a pixel of luma luma, its saturation scaled (1024: unchanged) */
int saturated(int level, int luma, int factor) {
	return std::clamp((luma * 1024 + (level - luma) * factor) >> 10, 0, 255);
}

/** The numbers of a query item: "100,50" */
QList<int> numbersOf(const QUrlQuery& query, const QString& key) {
	QList<int> numbers;
	for (const QString& one : query.queryItemValue(key).split(u',', Qt::SkipEmptyParts)) {
		numbers << one.toInt();
	}
	return numbers;
}

/**
 * What Show Picture does to its picture, as the Player draws it (Bitmap::
 * ToneBlit, Sprite_Picture): the saturation, then the tint; each half's
 * opacity (the bottom one: its lower half, rounded down); the flips
 */
void applyAppearance(QImage& image, const QUrlQuery& query) {
	const QList<int> tone = numbersOf(query, QStringLiteral("tone"));
	const QList<int> opacity = numbersOf(query, QStringLiteral("opacity"));
	const QString flip = query.queryItemValue(QStringLiteral("flip"));
	if (tone.size() != 4 && opacity.size() != 2 && flip.isEmpty()) {
		return;
	}
	image = image.convertToFormat(QImage::Format_ARGB32);
	const int red = tone.size() == 4 ? toneOf(tone[0]) : 128;
	const int green = tone.size() == 4 ? toneOf(tone[1]) : 128;
	const int blue = tone.size() == 4 ? toneOf(tone[2]) : 128;
	const int gray = tone.size() == 4 ? toneOf(tone[3]) : 128;
	const int factor = gray > 128 ? 1024 + (gray - 128) * 16 : gray * 8;
	const bool tinted = red != 128 || green != 128 || blue != 128;
	const int top = opacity.size() == 2 ? std::clamp(opacity[0], 0, 100) : 100;
	const int bottom = opacity.size() == 2 ? std::clamp(opacity[1], 0, 100) : 100;
	const int bottomFrom = image.height() - image.height() / 2;
	for (int y = 0; y < image.height(); ++y) {
		auto* line = reinterpret_cast<QRgb*>(image.scanLine(y));
		const int percent = y < bottomFrom ? top : bottom;
		for (int x = 0; x < image.width(); ++x) {
			int r = qRed(line[x]);
			int g = qGreen(line[x]);
			int b = qBlue(line[x]);
			if (gray != 128) {
				const int luma = (7471 * b + 38470 * g + 19595 * r) >> 16;
				r = saturated(r, luma, factor);
				g = saturated(g, luma, factor);
				b = saturated(b, luma, factor);
			}
			if (tinted) {
				r = hardLight(red, r);
				g = hardLight(green, g);
				b = hardLight(blue, b);
			}
			line[x] = qRgba(r, g, b, qAlpha(line[x]) * percent / 100);
		}
	}
	if (!flip.isEmpty()) {
		image = image.transformed(QTransform::fromScale(flip.contains(u'x') ? -1 : 1, flip.contains(u'y') ? -1 : 1));
	}
}

} // namespace

ProjectImageProvider::ProjectImageProvider(Finder finder) :
	QQuickImageProvider(QQuickImageProvider::Pixmap), m_finder(std::move(finder))
{
}

bool ProjectImageProvider::isTransparentFolder(const QString& folder) {
	// the backgrounds are drawn opaque, everything over them not
	static const QStringList opaque = {
		QStringLiteral("Backdrop"), QStringLiteral("Panorama"), QStringLiteral("Title"),
		QStringLiteral("GameOver"), QStringLiteral("Frame"), QStringLiteral("System"),
	};
	return !opaque.contains(folder, Qt::CaseInsensitive);
}

QImage ProjectImageProvider::load(const QString& path, bool transparent) {
	if (path.isEmpty()) {
		return {};
	}
	QImage image(path);
	if (image.isNull()) {
		// XYZ pictures: the loader keeps no palette
		return ImageLoader::Load(path).toImage();
	}
	if (transparent && image.colorCount() > 0) {
		auto table = image.colorTable();
		table[0] &= 0x00FFFFFF;
		image.setColorTable(table);
	}
	return image;
}

QPixmap ProjectImageProvider::requestPixmap(const QString& id, QSize* size, const QSize& requestedSize) {
	// "CharSet/Hero?transparent=0"
	const qsizetype question = id.indexOf(u'?');
	const QString path = question < 0 ? id : id.left(question);
	const QUrlQuery query(question < 0 ? QString() : id.mid(question + 1));
	const qsizetype slash = path.indexOf(u'/');
	if (slash <= 0 || slash == path.size() - 1 || !m_finder) {
		return {};
	}
	const QString folder = path.left(slash);
	const QString name = path.mid(slash + 1);

	bool transparent = isTransparentFolder(folder);
	if (query.hasQueryItem(QStringLiteral("transparent"))) {
		transparent = query.queryItemValue(QStringLiteral("transparent")) != QLatin1String("0");
	}
	QImage image = load(m_finder(folder, name), transparent);
	if (!image.isNull()) {
		applyAppearance(image, query);
	}
	if (image.isNull()) {
		// a picture the game does not have (or that cannot be read): nothing
		// shown, as in RPG Maker's previews, without a warning of the QML engine
		image = QImage(1, 1, QImage::Format_ARGB32);
		image.fill(Qt::transparent);
	}
	if (size) {
		*size = image.size();
	}
	if (!image.isNull() && (requestedSize.width() > 0 || requestedSize.height() > 0)) {
		// one side asked: the other one keeps the proportions
		QSize target = requestedSize;
		if (target.width() <= 0) {
			target.setWidth(image.width() * target.height() / image.height());
		} else if (target.height() <= 0) {
			target.setHeight(image.height() * target.width() / image.width());
		}
		if (!target.isEmpty() && target != image.size()) {
			// pixel art: zoomed without blur
			image = image.scaled(target, Qt::KeepAspectRatio, Qt::FastTransformation);
		}
	}
	return QPixmap::fromImage(std::move(image));
}
