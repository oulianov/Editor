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

#include <QPoint>
#include <QPointF>
#include <cstdint>
#include <lcf/rpg/chipset.h>

/**
 * The editing of the tiles of a chipset, as RPG Maker's Chipset page: in a
 * layer (lower or upper), a click gives a tile the terrain chosen, cycles
 * its passability (passable, blocked, above the hero), opens or closes the
 * side of the tile under the pointer (Directions) or makes it a counter;
 * dragging gives the other tiles what the click gave.
 *
 * The tiles are shown 6 per row. Points are in tiles (1.0 is the width of
 * a tile), whatever the size the tiles are drawn at.
 */
class ChipsetEditor
{
public:
	enum class Mode { Terrain, Passability, Directions, Counter };

	/** The flags of passable_data_lower and passable_data_upper */
	enum Flag : uint8_t {
		Down = 0x01, Left = 0x02, Right = 0x04, Up = 0x08,
		Above = 0x10, Wall = 0x20, Counter = 0x40,
		AllDirections = Down | Left | Right | Up
	};

	static constexpr int Columns = 6;
	/** RPG Maker 2003's: the lower layer has the water and the autotiles too */
	static constexpr int LowerTiles = 162;
	static constexpr int UpperTiles = 144;

	/** The side under the pointer in the Directions mode, shown as RPG Maker does */
	struct Hover {
		/** -1: none */
		int tile = -1;
		int side = 0;
	};

	lcf::rpg::Chipset* chipset() const;
	/** Edits chipset (nullptr: none). Its lists are grown to RPG Maker
	 *  2003's sizes: older files can have shorter ones */
	void setChipset(lcf::rpg::Chipset* chipset);

	bool lowerLayer() const;
	void setLowerLayer(bool lower);
	Mode mode() const;
	void setMode(Mode mode);
	/** The terrain a click gives in the Terrain mode (its ID) */
	int terrainId() const;
	void setTerrainId(int terrain_id);

	/** The tiles of the current layer */
	int tileCount() const;
	static int tileCount(bool lower);
	/** Where a tile of a layer is in the chipset image (30 x 16 tiles), in
	 *  tiles, as EasyRPG Player reads them */
	static QPoint tilePosition(bool lower, int index);
	/** The flags of a tile of the current layer */
	int flags(int index) const;
	/** The terrain of a tile of the lower layer, 0 when it has none */
	int terrain(int index) const;

	/** The tile at a point, -1 when none */
	int tileAt(const QPointF& pos) const;
	/** The side of a tile (Up, Down, Left, Right) a point is closest to */
	static int sideAt(const QPointF& pos, int index);

	/** A click at a point (the right button cycles the passability
	 *  backwards); true when it was on a tile, to be drawn again */
	bool press(const QPointF& pos, bool right_button = false);
	/** The click dragged to a point: the tile there gets what the click
	 *  gave; true when it reached another tile */
	bool drag(const QPointF& pos);
	/** The button released: the drag ends */
	void release();
	/** Whether a click is being dragged */
	bool dragging() const;

	const Hover& hover() const;
	/** The pointer moved to a point; true when the side under it changed */
	bool hoverAt(const QPointF& pos);
	/** The pointer left; true when there was a side under it */
	bool clearHover();

	/** RPG Maker's "Set Uniform Terrain": one terrain on all lower tiles */
	void setUniformTerrain(int terrain_id);

private:
	/** Gives a tile what the mode does: the stroke's flags when dragging */
	void apply(int index, bool right_button, bool dragged);

	lcf::rpg::Chipset* m_chipset = nullptr;
	bool m_lower = true;
	Mode m_mode = Mode::Passability;
	int m_terrainId = 1;
	/** The click being dragged: its last tile (-1: none), the flags it gave
	 *  and the side in the Directions mode */
	struct Stroke {
		int tile = -1;
		uint8_t flags = 0;
		int side = 0;
	} m_stroke;
	Hover m_hover;
};
