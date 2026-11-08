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

#include <QList>
#include <QPoint>
#include <QPointer>
#include <QQuickPaintedItem>
#include <QRect>
#include <QtQmlIntegration>
#include "model/editor_state.h"

/**
 * The tiles of the palette of the layer edited and their picking
 * (Palette.qml draws them): 6 a row, the eraser row first, then the lower
 * tiles (the water and the animated tiles, deep water among water, then the
 * terrains) or the upper ones; the event layer shows the upper ones. With
 * a layer of its own (the palette wide enough for both side by side), it
 * shows that layer's tiles whatever the layer edited, and a pick makes its
 * layer the one edited.
 *
 * Its cells are those of the palette, the eraser row 0. A drag of the left
 * button picks a rectangle of tiles, at most 6 rows high, which the right
 * button cancels; the eraser row is picked alone. The tiles picked go to
 * the EditorState, whose selection the palette frames: the tiles picked on
 * the map too (the right button), when they are a rectangle of the palette.
 */
class TilePalette : public QObject
{
	Q_OBJECT
	QML_ELEMENT
	Q_PROPERTY(EditorState* state READ state WRITE setState NOTIFY stateChanged)
	Q_PROPERTY(int layer READ layer WRITE setLayer NOTIFY layerChanged)
	Q_PROPERTY(bool active READ isActive NOTIFY activeChanged)
	Q_PROPERTY(QList<int> tiles READ tiles NOTIFY tilesChanged)
	Q_PROPERTY(int columns READ columns CONSTANT)
	Q_PROPERTY(int rows READ rows NOTIFY tilesChanged)
	Q_PROPERTY(QRect selection READ selection NOTIFY selectionChanged)
	Q_PROPERTY(bool picking READ isPicking NOTIFY pickingChanged)

public:
	static constexpr int Columns = 6;
	/** The lower terrains, and the upper tiles after them */
	static constexpr int LowerTiles = 162;
	static constexpr int UpperTiles = 144;

	explicit TilePalette(QObject* parent = nullptr);

	EditorState* state() const;
	void setState(EditorState* state);

	/** The layer whose tiles are shown: -1 (the default) the layer edited,
	 *  EditorState::LOWER or UPPER that one, edited once a tile is picked */
	int layer() const;
	void setLayer(int layer);
	/** Its tiles are those of the layer edited: always without a layer of its own */
	bool isActive() const;

	/** The tile ids of the cells, row by row; -1: the eraser row's, drawn apart */
	QList<int> tiles() const;
	int columns() const;
	/** The eraser row and the tiles' */
	int rows() const;
	/** The cells framed: the tiles picked, or dragged over */
	QRect selection() const;
	/** The left button is down: a drag picks the tiles on release */
	bool isPicking() const;

	/** The pointer on a cell of the palette (outside it: any cell out of it) */
	Q_INVOKABLE void press(int x, int y, Qt::MouseButton button);
	Q_INVOKABLE void move(int x, int y);
	Q_INVOKABLE void release(int x, int y, Qt::MouseButton button);

signals:
	void stateChanged();
	void layerChanged();
	void activeChanged();
	void tilesChanged();
	void selectionChanged();
	void pickingChanged();

private:
	bool upper() const;
	/** The layer of the tiles shown: its own, else the state's */
	EditorState::Layer shownLayer() const;
	/** Its own layer becomes the one edited, as the layer actions make it */
	void edit();
	/** The tiles, the frame and the activity, after a change of layer */
	void followLayer();
	/** The rectangle dragged from m_initial to m_current: within the tiles, 6 rows at most */
	QRect dragged() const;
	/** The tiles of a rectangle go to the state (a copy: the frame can move meanwhile) */
	void pick(QRect cells);
	/** The rectangle of the state's selection, unless on the event layer */
	void showState();
	void setSelection(const QRect& cells);
	void setPicking(bool picking);

	QPointer<EditorState> m_state;
	QList<QMetaObject::Connection> m_stateConnections;
	int m_layer = -1;
	bool m_upper = false;
	bool m_active = true;
	QRect m_selection;
	// the rectangle picked last, framed again when a drag is cancelled
	QRect m_last;
	bool m_picking = false;
	bool m_cancelled = false;
	bool m_settingState = false;
	QPoint m_initial;
	QPoint m_current;
};

/**
 * Grey squares where nothing is drawn (TilePainter::transparencyBrush),
 * under the palette and the tile pickers, with the eraser row of the
 * palette at the top (TilePainter::eraserRow) when it has cells.
 */
class TransparencyPattern : public QQuickPaintedItem
{
	Q_OBJECT
	QML_ELEMENT
	Q_PROPERTY(int eraserCells READ eraserCells WRITE setEraserCells NOTIFY eraserChanged)
	Q_PROPERTY(qreal cellSize READ cellSize WRITE setCellSize NOTIFY eraserChanged)

public:
	explicit TransparencyPattern(QQuickItem* parent = nullptr);

	/** The cells of the eraser row, 0: none */
	int eraserCells() const;
	void setEraserCells(int cells);
	qreal cellSize() const;
	void setCellSize(qreal size);

	void paint(QPainter* painter) override;

signals:
	void eraserChanged();

private:
	int m_eraserCells = 0;
	qreal m_cellSize = 32;
};
