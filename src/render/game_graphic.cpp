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

#include "game_graphic.h"
#include "common/image_loader.h"
#include "model/project.h"
#include "model/project_data.h"
#include <QPixmap>

QImage GameGraphic::load(const ProjectData& project, const QString& dir, const QString& name)
{
	if (name.isEmpty() || !project.hasProject()) {
		return QImage();
	}
	const QString path = project.project().findFile(dir, name, FileFinder::FileType::Image);
	if (path.isEmpty()) {
		return QImage();
	}
	QImage image(path);
	if (image.isNull()) {
		// XYZ pictures: the loader keeps no palette
		return ImageLoader::Load(path).toImage();
	}
	if (image.colorCount() > 0) {
		auto table = image.colorTable();
		table[0] &= 0x00FFFFFF;
		image.setColorTable(table);
	}
	return image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}
