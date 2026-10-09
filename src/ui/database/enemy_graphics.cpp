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

#include "enemy_graphics.h"
#include "common/dbstring.h"
#include "common/image_loader.h"
#include "model/project.h"
#include "model/project_data.h"
#include "defines.h"
#include <QColor>
#include <QImage>
#include <QPainter>
#include <QPixmapCache>

namespace {
	QRgb rotateHue(QRgb rgb, int hue) {
		QColor color = QColor::fromRgba(rgb);
		int h, s, l, a;
		color.getHsl(&h, &s, &l, &a);
		// Grays have no hue (-1) and stay as they are
		if (h < 0) {
			return rgb;
		}
		color.setHsl((h + hue) % 360, s, l, a);
		return color.rgba();
	}
}

QPixmap EnemyGraphics::load(const QString& path, int hue) {
	if (path.isEmpty()) {
		return QPixmap();
	}

	hue = ((hue % 360) + 360) % 360;
	// the lists draw them on every repaint
	const QString key = QStringLiteral("monster:%1:%2").arg(path).arg(hue);
	QPixmap cached;
	if (QPixmapCache::find(key, &cached)) {
		return cached;
	}
	QImage image(path);
	if (image.isNull()) {
		// XYZ pictures: the loader keeps no palette, so no transparent color
		image = ImageLoader::Load(path).toImage();
	}

	if (image.colorCount() > 0) {
		// Palette pictures (as RPG Maker wants them): the first color is transparent
		auto table = image.colorTable();
		for (auto& rgb : table) {
			rgb = rotateHue(rgb, hue);
		}
		table[0] &= 0x00FFFFFF;
		image.setColorTable(table);
	} else if (hue != 0) {
		image = image.convertToFormat(QImage::Format_ARGB32);
		for (int y = 0; y < image.height(); ++y) {
			auto* line = reinterpret_cast<QRgb*>(image.scanLine(y));
			for (int x = 0; x < image.width(); ++x) {
				line[x] = rotateHue(line[x], hue);
			}
		}
	}

	cached = QPixmap::fromImage(image);
	QPixmapCache::insert(key, cached);
	return cached;
}

QPixmap EnemyGraphics::thumbnail(const ProjectData& project, const lcf::rpg::Enemy& enemy, int size) {
	// Lists draw it on every repaint: no file search each time
	const QString key = QStringLiteral("monster-thumbnail:%1:%2:%3:%4").arg(project.project().projectDir().absolutePath(),
		ToQString(enemy.battler_name)).arg(enemy.battler_hue).arg(size);
	QPixmap thumbnail;
	if (QPixmapCache::find(key, &thumbnail)) {
		return thumbnail;
	}
	// the same size for all: the names line up
	thumbnail = QPixmap(size, size);
	thumbnail.fill(Qt::transparent);
	const QPixmap monster = load(project, enemy);
	if (!monster.isNull()) {
		const QPixmap scaled = monster.width() > size || monster.height() > size
			? monster.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation) : monster;
		QPainter painter(&thumbnail);
		painter.drawPixmap((size - scaled.width()) / 2, (size - scaled.height()) / 2, scaled);
	}
	QPixmapCache::insert(key, thumbnail);
	return thumbnail;
}

QPixmap EnemyGraphics::load(const ProjectData& project, const lcf::rpg::Enemy& enemy) {
	if (enemy.battler_name.empty()) {
		return QPixmap();
	}
	QString path = project.project().findFile(MONSTER, ToQString(enemy.battler_name), FileFinder::FileType::Image);
	return load(path, enemy.battler_hue);
}
