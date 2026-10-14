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

#include "tile_atlas.h"
#include <QPainter>
#include <QPixmap>
#include <algorithm>
#include <bit>
#include <map>
#include "model/autotile.h"

namespace {
// tiles in a row of a sheet: 512 pixels for tiles of 16
constexpr int Columns = 32;
}

TileAtlas::TileAtlas(std::shared_ptr<const ChipsetTiles> tiles) :
	m_tiles(tiles ? std::move(tiles) : std::make_shared<const ChipsetTiles>()),
	m_tileSize(m_tiles->tileSize()),
	m_rects(IdCount),
	m_empty(IdCount, true)
{
	// The tiles of each sheet, once each: the 50 ids of an animated tile
	// share their pixmaps. The key: the pixmaps of every frame.
	using Key = std::array<qint64, 4>;
	std::array<std::map<Key, int>, SheetCount> places;
	std::array<std::vector<short>, SheetCount> first_ids;
	std::vector<int> place_of(IdCount, -1);
	for (int id = 0; id < IdCount; id++) {
		const short tile_id = static_cast<short>(id);
		if (m_tiles->tile(tile_id).isNull())
			continue;
		const Sheet s = sheet(tile_id);
		Key key{};
		for (int frame = 0; frame < frameCount(s); frame++)
			key[static_cast<size_t>(frame)] = m_tiles->tile(tile_id, frame).cacheKey();
		const auto [it, added] = places[s].try_emplace(key, static_cast<int>(first_ids[s].size()));
		if (added)
			first_ids[s].push_back(tile_id);
		place_of[static_cast<size_t>(id)] = it->second;
	}

	const int size = std::max(m_tileSize, 1);
	for (int s = 0; s < SheetCount; s++) {
		const Sheet the_sheet = static_cast<Sheet>(s);
		const int count = static_cast<int>(first_ids[s].size());
		const int rows = std::max(1, (count + Columns - 1) / Columns);
		// powers of two: x / width is exact in floating point
		const QSize image_size(static_cast<int>(std::bit_ceil(static_cast<unsigned>(Columns * size))),
			static_cast<int>(std::bit_ceil(static_cast<unsigned>(rows * size))));
		std::vector<bool> empty(static_cast<size_t>(count), true);
		for (int frame = 0; frame < frameCount(the_sheet); frame++) {
			QImage image(image_size, QImage::Format_ARGB32_Premultiplied);
			image.fill(Qt::transparent);
			QPainter painter(&image);
			// the pixels as they are, transparent ones included
			painter.setCompositionMode(QPainter::CompositionMode_Source);
			for (int place = 0; place < count; place++) {
				// a frame the chipset does not have stays empty, as TilePainter draws it
				const QPoint pos((place % Columns) * size, (place / Columns) * size);
				painter.drawPixmap(pos, m_tiles->tile(first_ids[s][static_cast<size_t>(place)], frame));
			}
			painter.end();
			for (int place = 0; place < count; place++) {
				if (!empty[static_cast<size_t>(place)])
					continue;
				const int x0 = (place % Columns) * size;
				const int y0 = (place / Columns) * size;
				for (int y = y0; y < y0 + size && empty[static_cast<size_t>(place)]; y++) {
					const QRgb* line = reinterpret_cast<const QRgb*>(image.constScanLine(y));
					for (int x = x0; x < x0 + size; x++)
						if (qAlpha(line[x]) != 0) {
							empty[static_cast<size_t>(place)] = false;
							break;
						}
				}
			}
			m_images[s].push_back(std::move(image));
		}
		for (int id = 0; id < IdCount; id++) {
			const int place = place_of[static_cast<size_t>(id)];
			if (place < 0 || sheet(static_cast<short>(id)) != the_sheet)
				continue;
			m_rects[static_cast<size_t>(id)] = QRect((place % Columns) * size, (place / Columns) * size, size, size);
			m_empty[static_cast<size_t>(id)] = empty[static_cast<size_t>(place)];
		}
	}
}

std::shared_ptr<const TileAtlas> TileAtlas::forTiles(const std::shared_ptr<const ChipsetTiles>& tiles)
{
	// on the GUI thread, as the pixmaps of the tiles
	static std::vector<std::weak_ptr<const TileAtlas>> atlases;
	std::shared_ptr<const TileAtlas> found;
	std::erase_if(atlases, [&](const std::weak_ptr<const TileAtlas>& weak) {
		const auto atlas = weak.lock();
		if (!atlas)
			return true;
		if (tiles && atlas->tiles() == tiles)
			found = atlas;
		return false;
	});
	if (!found) {
		found = std::make_shared<const TileAtlas>(tiles);
		atlases.push_back(found);
	}
	return found;
}

const std::shared_ptr<const ChipsetTiles>& TileAtlas::tiles() const
{
	return m_tiles;
}

int TileAtlas::tileSize() const
{
	return m_tileSize;
}

TileAtlas::Sheet TileAtlas::sheet(short tile_id)
{
	if (!Autotile::isAnimatedTile(tile_id))
		return Still;
	return tile_id < 3000 ? Water : Animated;
}

int TileAtlas::frameCount(Sheet sheet)
{
	switch (sheet) {
	case Water:
		return 3;
	case Animated:
		return 4;
	default:
		return 1;
	}
}

int TileAtlas::frame(Sheet sheet, int water_frame, int animated_tile_frame)
{
	switch (sheet) {
	case Water:
		return std::clamp(water_frame, 0, 2);
	case Animated:
		return std::clamp(animated_tile_frame, 0, 3);
	default:
		return 0;
	}
}

const QImage& TileAtlas::image(Sheet sheet, int frame) const
{
	const auto& frames = m_images[static_cast<size_t>(sheet)];
	return frames[static_cast<size_t>(std::clamp(frame, 0, static_cast<int>(frames.size()) - 1))];
}

QRect TileAtlas::rect(short tile_id) const
{
	if (tile_id < 0 || tile_id >= IdCount)
		return QRect();
	return m_rects[static_cast<size_t>(tile_id)];
}

QRectF TileAtlas::textureRect(short tile_id) const
{
	const QRect r = rect(tile_id);
	if (r.isNull())
		return QRectF();
	const QSize size = image(sheet(tile_id)).size();
	return QRectF(r.x() / qreal(size.width()), r.y() / qreal(size.height()),
		r.width() / qreal(size.width()), r.height() / qreal(size.height()));
}

bool TileAtlas::isEmpty(short tile_id) const
{
	if (tile_id < 0 || tile_id >= IdCount)
		return true;
	return m_empty[static_cast<size_t>(tile_id)];
}
