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
#include <QImage>
#include <QPixmap>
#include <QPointer>
#include <QQuickPaintedItem>
#include <QtQmlIntegration>

#include "model/chipset_editor.h"
#include "qmlbinding/json_view.h"

/**
 * The tiles of a chipset on its database page (ChipsetEditor, drawn by
 * ChipsetEditorPainter), in a database window's copy: those of a layer, 6
 * per row at twice their size, with the marks of the editing mode (the
 * terrain; ○ × ☆; RPG Maker 2003's Directions arrows and dots, the side
 * under the pointer yellow; □). A click or a drag edits them; the item is
 * as large as the tiles (in a scrolled view), 32 points a tile.
 */
class ChipsetCanvas : public QQuickPaintedItem
{
	Q_OBJECT
	QML_ELEMENT
	/** The chipset shown (its view, "/chipsets/2"), null for none */
	Q_PROPERTY(JsonView* entry READ entry WRITE setEntry NOTIFY entryChanged)
	/** The lower layer, else the upper one */
	Q_PROPERTY(bool lowerLayer READ lowerLayer WRITE setLowerLayer NOTIFY lowerLayerChanged)
	/** ChipsetEditor::Mode: 0 Terrain, 1 Passability, 2 Directions, 3 Counter */
	Q_PROPERTY(int mode READ mode WRITE setMode NOTIFY modeChanged)
	/** The terrain a click gives in the Terrain mode (its ID) */
	Q_PROPERTY(int terrainId READ terrainId WRITE setTerrainId NOTIFY terrainIdChanged)
	/** Behind the tiles (the color of the lists) */
	Q_PROPERTY(QColor background READ background WRITE setBackground NOTIFY backgroundChanged)

public:
	explicit ChipsetCanvas(QQuickItem* parent = nullptr);

	JsonView* entry() const;
	void setEntry(JsonView* entry);
	bool lowerLayer() const;
	void setLowerLayer(bool lower);
	int mode() const;
	void setMode(int mode);
	int terrainId() const;
	void setTerrainId(int terrain_id);
	QColor background() const;
	void setBackground(const QColor& color);

	/** RPG Maker's "Set Uniform Terrain": one terrain on all lower tiles */
	Q_INVOKABLE void setUniformTerrain(int terrain_id);
	/** The flags of a tile of the layer shown (the scripts) */
	Q_INVOKABLE int flags(int index) const;
	/** The terrain of a lower tile */
	Q_INVOKABLE int terrain(int index) const;

	void paint(QPainter* painter) override;

signals:
	void entryChanged();
	void lowerLayerChanged();
	void modeChanged();
	void terrainIdChanged();
	void backgroundChanged();
	/** A click or a drag changed tiles */
	void edited();

protected:
	void mousePressEvent(QMouseEvent* event) override;
	void mouseDoubleClickEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void hoverMoveEvent(QHoverEvent* event) override;
	void hoverLeaveEvent(QHoverEvent* event) override;

private:
	/** The chipset of the entry, again (the list may have moved) */
	void findChipset();
	void loadGraphic();
	/** Draws the tiles again (on this thread: paint() only shows them) */
	void render();
	void changed();
	void onDataChanged(const QString& jsonPtr);
	QPointF tilePoint(const QPointF& pos) const;

	QPointer<JsonView> m_entry;
	ChipsetEditor m_editor;
	QPixmap m_graphic;
	QString m_graphicName;
	QImage m_rendered;
	QColor m_background = Qt::white;
	bool m_editing = false;
};
