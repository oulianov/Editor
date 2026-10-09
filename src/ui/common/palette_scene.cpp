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

#include "palette_scene.h"
#include <QGraphicsDropShadowEffect>
#include <QPainter>

PaletteScene::PaletteScene(QObject *parent) :
	QGraphicsScene(parent),
	m_cancel(false),
	m_pressed(false)
{
	this->setSceneRect(QRect(0,0,192,896));
	m_selectionItem = new QGraphicsRectItem(QRectF(QRect(0,32,32,32)));
	last_selection = QRectF(QRect(0,32,32,32));
	this->addItem(m_selectionItem);
	m_tiles = new QGraphicsPixmapItem();
	this->addItem(m_tiles);
	m_tiles->setVisible(false);
	QPen selPen(Qt::yellow);
	selPen.setWidth(3);
	m_selectionItem->setPen(selPen);
	m_selectionItem->setVisible(false);
	QGraphicsDropShadowEffect *effect = new QGraphicsDropShadowEffect(this);
	effect->setOffset(4);
	m_tiles->setGraphicsEffect(effect);
	m_tiles->stackBefore(m_selectionItem);
	setBackgroundBrush(QBrush(QPixmap(":/embedded/share/old_grid.png")));
	// the eyedropper of the map picks tiles too
	connect(&core(), &Core::selectionChanged, this, &PaletteScene::onSelectionChanged);
}

void PaletteScene::onLayerChange()
{
	if (core().layer() == Core::LOWER)
	{
		m_tiles->setPixmap(m_lowerTiles);
		this->setSceneRect(QRect(0,0,192,896));
	}
	else
	{
		m_tiles->setPixmap(m_upperTiles);
		this->setSceneRect(QRect(0,0,192,800));
	}
	m_tiles->graphicsEffect()->setEnabled(core().layer() != Core::LOWER);
	// each layer keeps its own selection
	onSelectionChanged();
}

void PaletteScene::onSelectionChanged()
{
	if (m_settingSelection || core().layer() == Core::EVENT)
		return;
	// the palette index of the first tile: the terrain for lower tiles, from
	// 162 for upper tiles; the row 0 is the eraser
	int index = core().translate(core().selection(0, 0));
	if (core().layer() == Core::UPPER)
		index -= 162;
	if (index < 0)
		return;
	const QRectF rect((index % 6) * 32, (index / 6 + 1) * 32, core().selWidth() * 32, core().selHeight() * 32);
	m_selectionItem->setRect(rect);
	last_selection = rect;
}

void PaletteScene::onChipsetChange()
{
	if (core().chipsetIsNull())
	{
		m_tiles->setVisible(false);
		m_selectionItem->setVisible(false);
		return;
	}
	m_tiles->setVisible(true);
	m_selectionItem->setVisible(true);
	m_lowerTiles = QPixmap(192, 896);
	m_upperTiles = QPixmap(192, 800);
	m_lowerTiles.fill(QColor(0,0,0,0));
	m_upperTiles.fill(QColor(0,0,0,0));
	// the eraser, then the tiles
	QPainter p(&m_lowerTiles);
	p.drawPixmap(0,0,192,32,QPixmap(":/embedded/share/eraser.png"));
	p.drawPixmap(0,32,core().paletteTiles(Core::LOWER));
	p.end();
	p.begin(&m_upperTiles);
	p.drawPixmap(0,0,192,32,QPixmap(":/embedded/share/eraser.png"));
	p.drawPixmap(0,32,core().paletteTiles(Core::UPPER));
	p.end();
	// deep water is shown surrounded by water
	core().beginPainting(m_lowerTiles);
	core().renderTile(core().translate(2,0,240), QRect(64,32,32,32));
	core().endPainting();
	onLayerChange();
}

void PaletteScene::updateSelectionRect()
{
	QRectF selRect;
	int small_x = (m_initial.x() <= m_current.x()) ? static_cast<int>(m_initial.x())/32 : static_cast<int>(m_current.x())/32;
	int big_x = (m_initial.x() >= m_current.x()) ? static_cast<int>(m_initial.x())/32 : static_cast<int>(m_current.x())/32;
	int small_y = (m_initial.y() <= m_current.y()) ? static_cast<int>(m_initial.y())/32 : static_cast<int>(m_current.y())/32;
	int big_y = (m_initial.y() >= m_current.y()) ? static_cast<int>(m_initial.y())/32 : static_cast<int>(m_current.y())/32;
	//keep inside the scene
	if (small_x < 0)
		small_x = 0;
	if (small_x > 5)
		small_x = 5;
	if (big_x > 5)
		big_x = 5;
	if (small_y < 0)
		small_y = 0;
	if (big_y > 27)
		big_y = 27;
	if (big_y - small_y > 5)
	{
		if (static_cast<int>(m_initial.y())/32 == small_y)
			big_y = small_y + 5;
		else
			small_y = big_y - 5;
	}
	if (m_initial.y() < 32.0)
		selRect = QRectF(QRect(0,0,192,32));
	else
		selRect = QRectF(QRect(small_x*32,small_y*32,(big_x-small_x+1)*32,(big_y-small_y+1)*32));

	m_selectionItem->setRect(selRect);
}

void PaletteScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
	if (event->button() == Qt::RightButton)
	{
		cancelSelection();
		return;
	}
	else if (event->button() != Qt::LeftButton)
		return;
	m_initial = event->scenePos();
	m_current = event->scenePos();
	updateSelectionRect();
	QGraphicsScene::mousePressEvent(event);
}

void PaletteScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
	if (m_cancel)
		return;
	if (m_initial.y() < 32) //eraser selected
		return;
	if (m_initial.y() >= 32 && event->scenePos().y() < 32) //avoid mix with eraser
		m_current = QPointF(event->scenePos().x(), m_current.y());
	else
		m_current = event->scenePos();

	updateSelectionRect();
	QGraphicsScene::mouseMoveEvent(event);
}

void PaletteScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
	if (event->button() == Qt::LeftButton && m_cancel)
	{
		m_cancel = false;
		return;
	}
	if (event->button() != Qt::LeftButton)
		return;

	//TODO: set selection
	std::vector<short> sel;
	int x = static_cast<int>(m_selectionItem->rect().left())/32;
	int y = static_cast<int>(m_selectionItem->rect().top())/32;
	int w = static_cast<int>(m_selectionItem->rect().width())/32;
	int h = static_cast<int>(m_selectionItem->rect().height())/32;
	if (y == 0)
	{
		// RPG Maker's empty tiles: water (the first lower tile, as in a new
		// map) and the first upper tile, transparent. The Player cannot draw
		// other ids (NTILE corrupted the maps)
		if (core().layer() == Core::LOWER)
			sel.push_back(core().translate(0, SAMPLE));
		else
			sel.push_back(10000);
		m_settingSelection = true;
		core().setSelection(sel, 1, 1);
		m_settingSelection = false;
	}
	else
	{
		y--;
		for (int _y = y; _y < y+h; _y++)
			for (int _x = x; _x < x+w; _x++)
				if (core().layer() == Core::LOWER)
					sel.push_back(core().translate(_x+_y*6, SAMPLE));
				else
					sel.push_back(core().translate(_x+_y*6+162, SAMPLE));
		m_settingSelection = true;
		core().setSelection(sel, w, h);
		m_settingSelection = false;
	}
	last_selection = m_selectionItem->boundingRect();
	QGraphicsScene::mouseReleaseEvent(event);
}

void PaletteScene::cancelSelection()
{
	m_cancel = true;
	m_selectionItem->setRect(last_selection);
}


