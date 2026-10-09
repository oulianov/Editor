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

#include <QDialog>
#include "core.h"

class MapPreview;

/**
 * Picks tiles of a chipset (RPG Maker's TFORMSELREPLCHIP "Replace Chip" with
 * its TFORMCHIPSELECTOR): the lower or upper tiles in the order of the
 * palette, 6 per row. A click picks a tile, a drag a block up to the
 * maximum size, a double-click accepts.
 */
class ChipSelectorDialog : public QDialog
{
	Q_OBJECT

public:
	ChipSelectorDialog(int chipset_id, Core::Layer layer, const QSize& max_size, QWidget* parent = nullptr);

	/** The tiles selected, in palette cells: x from 0 to 5, y the row */
	QRect selection() const;
	void setSelection(const QRect& cells);

private:
	MapPreview* m_tiles;
};
