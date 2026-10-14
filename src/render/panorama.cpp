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

#include "panorama.h"
#include <algorithm>

QTransform Panorama::transform(const QPoint& first_cell, double tile_size, int map_tile_size)
{
	// as zoomed as the tiles
	const double zoom = map_tile_size > 0 ? tile_size / map_tile_size : 1.0;
	QTransform transform;
	transform.translate(-first_cell.x() * tile_size, -first_cell.y() * tile_size);
	transform.scale(zoom, zoom);
	return transform;
}

std::vector<Panorama::Copy> Panorama::copies(const QSize& picture, const QSize& map_pixels, const QRect& area)
{
	std::vector<Copy> copies;
	const QRect map(QPoint(0, 0), map_pixels);
	const QRect wanted = area & map;
	if (picture.isEmpty() || wanted.isEmpty())
		return copies;
	const int w = picture.width();
	const int h = picture.height();
	for (int y = wanted.top() / h * h; y <= wanted.bottom(); y += h)
		for (int x = wanted.left() / w * w; x <= wanted.right(); x += w) {
			const QRect target = QRect(x, y, w, h) & map;
			copies.push_back({target, QRect(0, 0, target.width(), target.height())});
		}
	return copies;
}
