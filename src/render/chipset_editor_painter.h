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

#include <QPainter>
#include <QPixmap>
#include <QPoint>
#include <QSize>

class ChipsetEditor;

/**
 * Draws the tiles of a chipset as its database page shows them: the tiles
 * of the editor's layer, 6 per row at twice their size, with the marks of
 * its mode (the terrain; ○ × ☆ for the passability; the RPG Maker 2003
 * Directions arrows and dots; □ for a counter). QPainter only: the Qt
 * Quick item (ChipsetCanvas) and the tests draw the same.
 */
namespace ChipsetEditorPainter {

/** The size a tile is drawn at, in pixels */
inline constexpr int Cell = 32;

/** The size of all the tiles of the editor's layer */
QSize size(const ChipsetEditor& editor);

/**
 * Draws the tiles from the top-left corner of the painter, on the
 * background already there. graphic: the chipset image, null for the
 * marks alone.
 */
void paint(QPainter& painter, const ChipsetEditor& editor, const QPixmap& graphic);

/**
 * A mark of the Directions mode, as RPG Maker 2003 draws it in a square of
 * 16 pixels at a point: an arrow where the hero can walk out of the tile,
 * a dot where not; white, the one under the mouse yellow. side: a flag of
 * ChipsetEditor (Up, Down, Left, Right).
 */
void drawDirection(QPainter& painter, QPoint at, int side, bool open, bool hovered);

}
