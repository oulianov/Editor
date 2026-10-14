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
#include <QMap>
#include <QPixmap>
#include <QString>
#include <memory>
#include <lcf/rpg/chipset.h>

class Project;

/**
 * The tiles of one chipset, cut from its image: every tile id the maps can
 * hold, with its borders, and the frames of the water and of the animated
 * tiles. It never changes once made, so views drawing different chipsets
 * can share them (std::shared_ptr<const ChipsetTiles>).
 */
class ChipsetTiles
{
public:
	/** No tiles: an unknown chipset (the ID 0) */
	explicit ChipsetTiles(const lcf::rpg::Chipset& chipset = {});
	/** The tiles cut from the image of the chipset, tile_size pixels each;
	 *  a null image gives the tiles of a dummy picture */
	ChipsetTiles(const lcf::rpg::Chipset& chipset, QPixmap image, int tile_size);

	/** The tiles of a chipset of the project: its image in the project,
	 *  else in the RTP (rtp_dir as Core::rtpPath has it); none for the ID 0 */
	static std::shared_ptr<const ChipsetTiles> load(const Project& project, const lcf::rpg::Chipset& chipset,
		int tile_size, const QString& rtp_dir);

	const lcf::rpg::Chipset& chipset() const { return m_chipset; }
	int id() const { return m_chipset.ID; }
	/** Without tiles: an unknown chipset */
	bool isNull() const { return m_tiles.value(0).isNull(); }
	/** The size the tiles were cut at, in pixels (0 without tiles) */
	int tileSize() const { return m_tileSize; }
	/** The color of the image drawn transparent (invalid without an image) */
	QColor keyColor() const { return m_keyColor; }

	/** A tile in a frame: water has 3 (0-2), the animated tiles 4 (0-3);
	 *  a null pixmap for an id the chipset does not have */
	QPixmap tile(short tile_id, int frame = 0) const;

private:
	lcf::rpg::Chipset m_chipset;
	int m_tileSize = 0;
	QColor m_keyColor;
	QMap<int, QPixmap> m_tiles;
	// frames 1 to 3 of the animated tiles; m_tiles has the frame 0
	QMap<int, QPixmap> m_frames[3];
};
