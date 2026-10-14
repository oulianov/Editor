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

#include <QColor>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QTransform>
#include <vector>

/**
 * Where the panorama of a map is drawn behind its lower layer, as the map
 * view shows it: from the top-left corner of the map, a pixel of the
 * picture a pixel of the map (as zoomed as the tiles), repeated over the
 * whole map. A map without one shows NoneColor.
 */
namespace Panorama
{
	/** Under the tiles of a map without a panorama */
	inline const QColor NoneColor = QColor(255, 0, 255);

	/** The brush transform of the panorama in a picture of the map whose
	 *  top-left corner is first_cell, cells tile_size pixels wide, for tiles
	 *  of map_tile_size pixels (Core::tileSize) */
	QTransform transform(const QPoint& first_cell, double tile_size, int map_tile_size);

	/** A copy of the panorama: where it goes and the part of the picture it
	 *  shows (the copies at the right and bottom edges are cut) */
	struct Copy
	{
		QRect target;
		QRect source;
	};
	/** The copies of a picture over a map of map_pixels (in pixels of the
	 *  map, unzoomed) that meet area, row by row */
	std::vector<Copy> copies(const QSize& picture, const QSize& map_pixels, const QRect& area);
}
