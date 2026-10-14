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
#include <QRect>
#include <QRectF>
#include <array>
#include <memory>
#include <vector>
#include "render/chipset_tiles.h"

/**
 * The tiles of a chipset packed into images, to draw a map as textured
 * quads (MapLayerView). Three sheets: the still tiles, the water and the
 * animated tiles. The sheets that move have an image per frame, a tile at
 * the same place in each, so a frame is shown by changing the image alone.
 * The pixels are those of ChipsetTiles, which TilePainter draws.
 *
 * Made once per ChipsetTiles (forTiles), on the GUI thread; the images are
 * then only read, from any thread.
 */
class TileAtlas
{
public:
	enum Sheet
	{
		Still,
		Water,
		Animated,
		SheetCount
	};

	explicit TileAtlas(std::shared_ptr<const ChipsetTiles> tiles);

	/** The atlas of these tiles, made once and shared while it is used */
	static std::shared_ptr<const TileAtlas> forTiles(const std::shared_ptr<const ChipsetTiles>& tiles);

	const std::shared_ptr<const ChipsetTiles>& tiles() const;
	/** The size of a tile in the images, in pixels (16 for the Editor's) */
	int tileSize() const;

	/** The sheet of a tile id: the water and the animated tiles move */
	static Sheet sheet(short tile_id);
	/** The frames of a sheet: 1, 3 for the water, 4 for the animated tiles */
	static int frameCount(Sheet sheet);
	/** The frame of a sheet shown with the frames of TilePainter::setFrames */
	static int frame(Sheet sheet, int water_frame, int animated_tile_frame);

	/** The image of a sheet in a frame; its sides are powers of two, so the
	 *  texture coordinates of the tiles are exact */
	const QImage& image(Sheet sheet, int frame = 0) const;
	/** Where a tile is in the images of its sheet; null for an id the
	 *  chipset does not have */
	QRect rect(short tile_id) const;
	/** rect() in texture coordinates (0-1) of the images of its sheet */
	QRectF textureRect(short tile_id) const;
	/** A tile with no pixel in any frame, which a view can skip; also an id
	 *  the chipset does not have */
	bool isEmpty(short tile_id) const;

private:
	// the ids a map can hold: lower tiles below 5144, upper ones from 10000
	static constexpr int IdCount = 10144;

	std::shared_ptr<const ChipsetTiles> m_tiles;
	int m_tileSize = 0;
	std::array<std::vector<QImage>, SheetCount> m_images;
	std::vector<QRect> m_rects;
	std::vector<bool> m_empty;
};
