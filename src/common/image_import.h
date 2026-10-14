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

#include <QImage>
#include <QString>

/**
 * The images of the games, as RPG Maker reads and writes them: palette
 * images (256 colors at most) where palette index 0 is transparent, in PNG,
 * BMP or RPG Maker's XYZ format. The import windows and the Resource Manager
 * use them.
 */
namespace ImageImport {
	/** A PNG, BMP or XYZ file; palette images (and XYZ) as Format_Indexed8 */
	QImage loadImage(const QString& path);

	/** Writes a palette image in RPG Maker's XYZ format (zlib compressed) */
	bool saveXyz(const QImage& image, const QString& path);

	/** The same pixels as a palette image, or a null image above 256 colors.
	 *  Fully transparent pixels become index 0, otherwise colors are numbered
	 *  in the order they appear, so the top left pixel's color is index 0. */
	QImage toIndexed(const QImage& image);

	/** Reduced to 256 colors (dithered); transparent pixels become index 0 */
	QImage reduceColors(const QImage& image);

	/** As the game shows it: the pixels of palette index 0 transparent */
	QImage withTransparentIndex0(const QImage& image);

	/** Palette images: the number of colors, else 0 */
	int paletteSize(const QImage& image);

	/** Palette entries opaque: transparency is index 0, not the alpha of the palette */
	QImage opaquePalette(QImage image);

	/** Swaps palette indexes a and b: entries and pixels */
	void swapIndexes(QImage& image, int a, int b);
}
