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

#include <QUrlQuery>

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
