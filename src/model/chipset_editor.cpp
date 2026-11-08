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

#include "chipset_editor.h"
#include <algorithm>
#include <cmath>

lcf::rpg::Chipset* ChipsetEditor::chipset() const {
	return m_chipset;
}

void ChipsetEditor::setChipset(lcf::rpg::Chipset* chipset) {
	m_chipset = chipset;
	m_stroke = {};
	if (chipset) {
		chipset->terrain_data.resize(LowerTiles, 1);
		chipset->passable_data_lower.resize(LowerTiles, AllDirections);
		chipset->passable_data_upper.resize(UpperTiles, AllDirections);
	}
	updateHover();
}

bool ChipsetEditor::lowerLayer() const {
	return m_lower;
}

void ChipsetEditor::setLowerLayer(bool lower) {
	m_lower = lower;
	updateHover();
}

ChipsetEditor::Mode ChipsetEditor::mode() const {
	return m_mode;
}

void ChipsetEditor::setMode(Mode mode) {
	m_mode = mode;
	updateHover();
}

int ChipsetEditor::terrainId() const {
	return m_terrainId;
}

void ChipsetEditor::setTerrainId(int terrain_id) {
	m_terrainId = terrain_id;
}

int ChipsetEditor::tileCount() const {
	return tileCount(m_lower);
}

int ChipsetEditor::tileCount(bool lower) {
	return lower ? LowerTiles : UpperTiles;
}

QPoint ChipsetEditor::tilePosition(bool lower, int index) {
	// Positions in the 30 x 16 tiles of a chipset, as EasyRPG Player reads them
	if (!lower) {
		return index < 48 ? QPoint(18 + index % 6, 8 + index / 6) : QPoint(24 + (index - 48) % 6, (index - 48) / 6);
	}
	switch (index) {
		case 0: return {0, 0};  // water A
		case 1: return {3, 0};  // water B
		case 2: return {0, 4};  // deep water
		case 3: case 4: case 5: return {index, 4};  // animated tiles
		default: break;
	}
	if (index < 18) {
		const int block = index - 6;
		return block < 4 ? QPoint((block % 2) * 3, 8 + (block / 2) * 4)
			: QPoint(6 + (block % 2) * 3, ((block - 4) / 2) * 4);
	}
	const int id = index - 18;
	return id < 96 ? QPoint(12 + id % 6, id / 6) : QPoint(18 + (id - 96) % 6, (id - 96) / 6);
}

int ChipsetEditor::flags(int index) const {
	if (!m_chipset) {
		return AllDirections;
	}
	const auto& passable = m_lower ? m_chipset->passable_data_lower : m_chipset->passable_data_upper;
	return index >= 0 && index < static_cast<int>(passable.size()) ? passable[static_cast<size_t>(index)] : AllDirections;
}

int ChipsetEditor::terrain(int index) const {
	if (!m_chipset || index < 0 || index >= static_cast<int>(m_chipset->terrain_data.size())) {
		return 0;
	}
	return m_chipset->terrain_data[static_cast<size_t>(index)];
}

int ChipsetEditor::tileAt(const QPointF& pos) const {
	const int col = static_cast<int>(std::floor(pos.x()));
	const int row = static_cast<int>(std::floor(pos.y()));
	const int index = row * Columns + col;
	if (col < 0 || col >= Columns || row < 0 || index >= tileCount()) {
		return -1;
	}
	return index;
}

int ChipsetEditor::sideAt(const QPointF& pos, int index) {
	// the tile is cut along its diagonals, as in RPG Maker
	const QPointF offset = pos - QPointF(index % Columns + 0.5, index / Columns + 0.5);
	if (std::abs(offset.x()) > std::abs(offset.y())) {
		return offset.x() < 0 ? Left : Right;
	}
	return offset.y() < 0 ? Up : Down;
}

int ChipsetEditor::passability(uint8_t flags) {
	if (flags & Wall) {
		return 3;
	}
	if (flags & Above) {
		return 2;
	}
	return (flags & AllDirections) == 0 ? 1 : 0;
}

uint8_t ChipsetEditor::cyclePassability(uint8_t v, bool right_button, bool lower, int tile) {
	// RPG2003.EXE (0x538C5C): the autotiles of the lower layer have the square
	const bool autotile = lower && tile >= 6 && tile < 18;
	switch (passability(v)) {
		case 3: // □: left ○, right ☆
			return right_button ? (v | 0x1F) & ~Wall : (v | AllDirections) & ~(Wall | Above);
		case 2: // ☆: left □ (an autotile) or the directions as they are, right ×
			if (right_button) {
				return v & ~(AllDirections | Above);
			}
			return autotile ? (v | Wall | Above) & ~AllDirections : v & ~Above;
		case 1: // ×: left ☆, right ○
			return right_button ? v | AllDirections : v | AllDirections | Above;
		default: // ○: left ×, right □ (an autotile) or ☆ with the directions as they are
			if (!right_button) {
				return v & ~AllDirections;
			}
			return autotile ? (v | Wall | Above) & ~AllDirections : v | Above;
	}
}

bool ChipsetEditor::editable(int index) const {
	if (index < 0) {
		return false;
	}
	switch (m_mode) {
		case Mode::Terrain: return m_lower;
		case Mode::Counter: return !m_lower && index > 0;
		default: return m_lower || index > 0;
	}
}

namespace {
	/** The flags of a mark as RPG Maker writes it, the other flags kept */
	uint8_t withPassability(uint8_t flags, int mark) {
		using F = ChipsetEditor::Flag;
		flags &= ~(F::AllDirections | F::Above | F::Wall);
		switch (mark) {
			case 0: return flags | F::AllDirections;
			case 2: return flags | F::AllDirections | F::Above;
			case 3: return flags | F::Wall | F::Above;
			default: return flags;
		}
	}
}

bool ChipsetEditor::press(const QPointF& pos, bool right_button) {
	m_stroke.tile = -1;
	const int index = tileAt(pos);
	auto* passable = !m_chipset ? nullptr : m_lower ? &m_chipset->passable_data_lower : &m_chipset->passable_data_upper;
	if (!editable(index) || !passable || index >= static_cast<int>(passable->size())) {
		return false;
	}
	uint8_t& flags = (*passable)[static_cast<size_t>(index)];

	switch (m_mode) {
		case Mode::Terrain:
			if (m_lower) {
				m_chipset->terrain_data[static_cast<size_t>(index)] = m_terrainId;
			}
			break;
		case Mode::Passability:
			flags = cyclePassability(flags, right_button, m_lower, index);
			break;
		case Mode::Directions:
			m_stroke.side = sideAt(pos, index);
			flags ^= m_stroke.side;
			break;
		case Mode::Counter:
			flags ^= Counter;
			break;
	}
	// what the drag that may follow gives to the other tiles
	m_stroke.tile = index;
	m_stroke.flags = flags;
	return true;
}

bool ChipsetEditor::drag(const QPointF& pos) {
	const int index = tileAt(pos);
	auto* passable = !m_chipset ? nullptr : m_lower ? &m_chipset->passable_data_lower : &m_chipset->passable_data_upper;
	if (m_stroke.tile < 0 || !editable(index) || index == m_stroke.tile || !passable
			|| index >= static_cast<int>(passable->size())) {
		return false;
	}
	m_stroke.tile = index;
	uint8_t& flags = (*passable)[static_cast<size_t>(index)];
	switch (m_mode) {
		case Mode::Terrain:
			if (m_lower) {
				m_chipset->terrain_data[static_cast<size_t>(index)] = m_terrainId;
			}
			break;
		case Mode::Passability: {
			// the mark of the tile clicked, on the tiles that can have it (the
			// square: the autotiles of the lower layer)
			const int mark = passability(m_stroke.flags);
			if (mark != 3 || (m_lower && index >= 6 && index < 18)) {
				flags = withPassability(flags, mark);
			}
			break;
		}
		case Mode::Directions:
			flags = (flags & ~m_stroke.side) | (m_stroke.flags & m_stroke.side);
			break;
		case Mode::Counter:
			flags = (flags & ~Counter) | (m_stroke.flags & Counter);
			break;
	}
	return true;
}

void ChipsetEditor::release() {
	m_stroke.tile = -1;
}

bool ChipsetEditor::dragging() const {
	return m_stroke.tile >= 0;
}

const ChipsetEditor::Hover& ChipsetEditor::hover() const {
	return m_hover;
}

bool ChipsetEditor::hoverAt(const QPointF& pos) {
	m_pointer = pos;
	m_pointed = true;
	return updateHover();
}

bool ChipsetEditor::clearHover() {
	m_pointed = false;
	return updateHover();
}

bool ChipsetEditor::updateHover() {
	Hover hover;
	// the tiles a click changes (not the first of the upper layer)
	if (m_pointed && m_chipset && editable(tileAt(m_pointer))) {
		hover.tile = tileAt(m_pointer);
		if (m_mode == Mode::Directions) {
			hover.side = sideAt(m_pointer, hover.tile);
		}
	}
	const bool changed = hover.tile != m_hover.tile || hover.side != m_hover.side;
	m_hover = hover;
	return changed;
}

void ChipsetEditor::setUniformTerrain(int terrain_id) {
	if (m_chipset) {
		std::fill(m_chipset->terrain_data.begin(), m_chipset->terrain_data.end(), terrain_id);
	}
}
