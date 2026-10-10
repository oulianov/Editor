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


#pragma once

#include <QCoreApplication>
#include <QFileInfo>
#include <QImage>
#include <QLocale>
#include <QStringList>
#include "ui/common/import_image_dialog.h"

/** What a resource file is, line by line, as the Resource Manager and the
 *  pickers show it: where it comes from, its size, an image's dimensions and
 *  colors. image: the file read (an image), or null. */
inline QStringList resourceInfo(const QString& path, bool rtp, const QImage& image = QImage()) {
	auto tr = [](const char* text) { return QCoreApplication::translate("ResourceManagerDialog", text); };
	const QFileInfo info(path);
	QStringList lines;
	lines << (rtp ? tr("RTP file: %1") : tr("Game file: %1")).arg(info.fileName());
	lines << tr("Size: %1").arg(QLocale().formattedDataSize(info.size()));
	if (!image.isNull()) {
		lines << tr("%1 x %2 pixels").arg(image.width()).arg(image.height());
		const int colors = ImportImageDialog::paletteSize(image);
		lines << (colors > 0 ? tr("Palette image, %1 colors").arg(colors)
			: tr("True color image: the game needs the EasyRPG extensions (RPG Maker only uses 256 color images)."));
	}
	return lines;
}
