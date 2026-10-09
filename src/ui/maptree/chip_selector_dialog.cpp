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

#include "chip_selector_dialog.h"
#include "ui/common/map_preview.h"
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QScrollBar>
#include <QVBoxLayout>

ChipSelectorDialog::ChipSelectorDialog(int chipset_id, Core::Layer layer, const QSize& max_size, QWidget* parent) :
	QDialog(parent)
{
	setWindowTitle(tr("Select Tile"));
	const bool lower = layer != Core::UPPER;
	const int size = 32;
	QPixmap tiles;
	{
		// the chipset of the map edited in this dialog, not of the open map
		PreviewChipset chipset(chipset_id);
		core().setBackground(Core::transparencyBrush().texture());
		tiles = core().paletteTiles(lower ? Core::LOWER : Core::UPPER, size);
		if (lower) {
			// deep water is shown surrounded by water, as in the palette
			core().beginPainting(tiles);
			core().renderTile(core().translate(2, 0, 240), QRect(2 * size, 0, size, size));
			core().endPainting();
		}
	}

	m_tiles = new MapPreview(this);
	m_tiles->setObjectName(QStringLiteral("chipSelector"));
	m_tiles->setCellSize(size);
	m_tiles->setRectangleMode(true, max_size);
	m_tiles->showImage(tiles, 6, tiles.height() / size);
	// exactly the 6 columns
	m_tiles->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_tiles->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	m_tiles->setMinimumSize(6 * size + m_tiles->verticalScrollBar()->sizeHint().width() + 2 * m_tiles->frameWidth(), 8 * size);
	connect(m_tiles, &MapPreview::cellActivated, this, &ChipSelectorDialog::accept);

	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(buttons, &QDialogButtonBox::accepted, this, &ChipSelectorDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &ChipSelectorDialog::reject);
	connect(m_tiles, &MapPreview::selectionChanged, this, [buttons](const QRect& cells) {
		buttons->button(QDialogButtonBox::Ok)->setEnabled(!cells.isEmpty());
	});

	auto* layout = new QVBoxLayout(this);
	if (max_size.width() * max_size.height() > 1) {
		auto* hint = new QLabel(tr("Drag to select up to %1 x %2 tiles.").arg(max_size.width()).arg(max_size.height()), this);
		hint->setWordWrap(true);
		layout->addWidget(hint);
	}
	layout->addWidget(m_tiles, 1);
	layout->addWidget(buttons);
	resize(sizeHint().width(), 14 * size);
}

QRect ChipSelectorDialog::selection() const {
	return m_tiles->selection();
}

void ChipSelectorDialog::setSelection(const QRect& cells) {
	m_tiles->setSelection(cells);
	if (!cells.isEmpty()) {
		m_tiles->centerOn(cells.x(), cells.y());
	}
}
