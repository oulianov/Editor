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

#include "exfont.h"
#include "common/image_loader.h"
#include "common/rpg_rt_exe.h"
#include "model/project.h"

#include <QFileInfo>
#include <QHash>

QByteArray ExFont::fromExe(const QString& path) {
	return RpgRtExe::exfont(path);
}

ExFont::Sheet ExFont::load(const Project& project) {
	// an image of the game (Font/ExFont first), as the Player
	for (const QString& path : {project.findFile(QStringLiteral("Font"), QStringLiteral("ExFont"), FileFinder::FileType::Image),
			project.findFile(QStringLiteral("ExFont"), FileFinder::FileType::Image)}) {
		if (path.isEmpty()) {
			continue;
		}
		// BMP and PNG as they are (their palette tells the strokes), XYZ converted
		QImage image(path);
		if (image.isNull()) {
			image = ImageLoader::Load(path).toImage();
		}
		if (!image.isNull()) {
			return {image, QFileInfo(path).fileName()};
		}
	}
	const QString exe = project.findFile(QStringLiteral("RPG_RT.exe"));
	if (!exe.isEmpty()) {
		const QImage image = QImage::fromData(fromExe(exe), "BMP");
		if (!image.isNull()) {
			return {image, QStringLiteral("RPG_RT.exe")};
		}
	}
	// EasyRPG's, by filosofiamanga (Player/resources/exfont.png)
	return {QImage(QStringLiteral(":/app/exfont.png")), QStringLiteral("EasyRPG")};
}

QString ExFont::code(int index) {
	const QChar letter = index < 26 ? QChar(u'A' + index) : QChar(u'a' + index - 26);
	return QStringLiteral("$") + letter;
}

QImage ExFont::glyph(const QImage& sheet, int index, const QColor& color) {
	const QRect cell((index % Columns) * GlyphSize, (index / Columns) * GlyphSize, GlyphSize, GlyphSize);
	if (index < 0 || index >= Count || !sheet.rect().contains(cell)) {
		return {};
	}
	// the background: the color most of the sheet has (black, or transparent)
	const QImage all = sheet.convertToFormat(QImage::Format_ARGB32);
	QHash<QRgb, int> counts;
	for (int y = 0; y < all.height(); ++y) {
		for (int x = 0; x < all.width(); ++x) {
			++counts[all.pixel(x, y)];
		}
	}
	QRgb background = 0;
	int most = -1;
	for (auto it = counts.cbegin(); it != counts.cend(); ++it) {
		if (it.value() > most) {
			most = it.value();
			background = it.key();
		}
	}

	// as the Player: gray strokes take the color of the text, colored ones
	// (an EasyRPG extension) keep theirs
	QImage result(GlyphSize, GlyphSize, QImage::Format_ARGB32);
	result.fill(Qt::transparent);
	for (int y = 0; y < GlyphSize; ++y) {
		for (int x = 0; x < GlyphSize; ++x) {
			const QRgb pixel = all.pixel(cell.x() + x, cell.y() + y);
			if (pixel == background || qAlpha(pixel) == 0) {
				continue;
			}
			const bool gray = qRed(pixel) == qGreen(pixel) && qGreen(pixel) == qBlue(pixel);
			result.setPixelColor(x, y, gray ? color : QColor::fromRgba(pixel | 0xFF000000));
		}
	}
	return result;
}
