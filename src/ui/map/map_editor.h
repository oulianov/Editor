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
#include <QObject>
#include <QPoint>
#include <QRect>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QTimer>
#include <QVariantMap>
#include "model/editor_state.h"
#include "model/map_document.h"

/**
 * The editing of a map with the pointer and the keys, apart from how it is
 * drawn: the drawing tools, the selection tool and the pasted tiles
 * floating, the events and the start positions selected and dragged, the
 * keys, the steps of the zoom and what the context menus can do.
 *
 * It works in cells: the view gives the cell under the pointer (any cell
 * outside the map when it is not over it) and the part of the map it shows
 * (viewCells). The edits go to the MapDocument. The view draws its state
 * again on its signals, and opens the menus and the dialogs it asks for
 * (contextMenuRequested, editEventRequested...): the Qt Quick map canvas
 * (MapCanvas.qml) and the main window (QmlShell).
 */
class MapEditor : public QObject
{
	Q_OBJECT
	Q_PROPERTY(QPoint pointerCell READ pointerCell NOTIFY pointerCellChanged)
	Q_PROPERTY(QRect brushRect READ brushRect NOTIFY brushRectChanged)
	Q_PROPERTY(QPoint selectedCell READ selectedCell NOTIFY selectionChanged)
	Q_PROPERTY(QPoint markedCell READ markedCell NOTIFY selectionChanged)
	Q_PROPERTY(QRect selectionArea READ selectionArea NOTIFY selectionChanged)
	Q_PROPERTY(QRect selectionRect READ selectionRect NOTIFY selectionChanged)
	Q_PROPERTY(QList<int> selectedEvents READ selectedEvents NOTIFY selectionChanged)
	Q_PROPERTY(QList<int> selectedStarts READ selectedStarts NOTIFY selectionChanged)
	Q_PROPERTY(QList<QPoint> itemCells READ itemCells NOTIFY selectionChanged)
	Q_PROPERTY(QPoint itemsOffset READ itemsOffset NOTIFY itemsOffsetChanged)
	Q_PROPERTY(int antsOffset READ antsOffset NOTIFY antsOffsetChanged)
	Q_PROPERTY(Cursor cursor READ cursor NOTIFY cursorChanged)
	Q_PROPERTY(bool dragging READ isDragging NOTIFY draggingChanged)
	Q_PROPERTY(QString hoverText READ hoverText NOTIFY cellHovered)
	Q_PROPERTY(qreal scale READ scale WRITE setScale NOTIFY scaleChanged)
	Q_PROPERTY(qreal devicePixelRatio READ devicePixelRatio WRITE setDevicePixelRatio NOTIFY devicePixelRatioChanged)
	Q_PROPERTY(QRectF viewCells READ viewCells WRITE setViewCells NOTIFY viewCellsChanged)
	// what a view needs of the map and of the shared state, for QML
	Q_PROPERTY(MapDocument* document READ document CONSTANT)
	Q_PROPERTY(EditorState* state READ state CONSTANT)
	Q_PROPERTY(QString mapName READ mapName NOTIFY mapChanged)
	Q_PROPERTY(QSize mapSize READ mapSize NOTIFY mapChanged)
	Q_PROPERTY(int chipsetId READ chipsetId NOTIFY mapChanged)
	Q_PROPERTY(QPoint scrollPosition READ scrollPosition WRITE setScrollPosition NOTIFY mapChanged)
	Q_PROPERTY(bool floating READ isFloating NOTIFY floatingChanged)
	Q_PROPERTY(QRect floatingRect READ floatingRect NOTIFY floatingRectChanged)
	Q_PROPERTY(QList<int> floatingLower READ floatingLower NOTIFY floatingChanged)
	Q_PROPERTY(QList<int> floatingUpper READ floatingUpper NOTIFY floatingChanged)
	Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
	Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)

public:
	/** The pointer over the map. The tools' first, in the order of
	 *  EditorState::Tool (drawn by MapCursors::forTool) */
	enum Cursor
	{
		ZoomCursor,
		PenCursor,
		RectangleCursor,
		CircleCursor,
		FillCursor,
		SelectCursor,
		/** The event layer's, without a drag */
		ArrowCursor,
		/** A selection, an event or a start position dragged (MapCursors::move) */
		MoveCursor,
		/** Dropped there, nothing would move */
		ForbiddenCursor
	};
	Q_ENUM(Cursor)

	/** The context menus: the event layer's, the selection tool's */
	enum Menu
	{
		EventMenu,
		AreaMenu
	};
	Q_ENUM(Menu)

	/** The zoom tool, Ctrl + wheel and a pinch stop at these scales */
	static constexpr qreal MinScale = 0.125;
	static constexpr qreal MaxScale = 8.0;

	/** The editing of document (not owned) with the layer, the tool, the
	 *  tiles picked and the clipboards of state */
	MapEditor(MapDocument* document, EditorState& state, QObject* parent = nullptr);

	MapDocument* document() const;
	/** The layer, the tool and the tiles picked, shared by all maps */
	EditorState* state() const;
	QString mapName() const;
	/** In cells, empty before the map is read */
	QSize mapSize() const;
	/** The chipset of the map, -1 before it is read */
	int chipsetId() const;
	/** Where the view was scrolled to, in pixels of the map (kept in its map info) */
	QPoint scrollPosition() const;
	void setScrollPosition(const QPoint& pixels);
	/** Pasted tiles float over the map (MapDocument::startFloating), on these
	 *  cells (empty: none), these tiles row by row */
	bool isFloating() const;
	QRect floatingRect() const;
	QList<int> floatingLower() const;
	QList<int> floatingUpper() const;
	bool canUndo() const;
	bool canRedo() const;

	/** The cell under the pointer, (-1, -1) when it is not over the map */
	QPoint pointerCell() const;
	/** The cells the tool would draw on under the pointer, framed; empty: none */
	QRect brushRect() const;
	/** The cell of the event layer the event actions apply to, (-1, -1): none */
	QPoint selectedCell() const;
	/** The cell framed as selected: the selected cell, or the cell an event
	 *  or a start position is dragged to. (-1, -1): none, or the selected
	 *  items have their own frames (itemCells) */
	QPoint markedCell() const;
	/** The cells of the selection tool: the tiles selected or floating, or
	 *  the area of the events and start positions selected */
	QRect selectionArea() const;
	/** The selection shown, where the selected items are dragged */
	QRect selectionRect() const;
	/** The events (ids) and start positions (Core::StartMarker) selected on
	 *  the event layer */
	QList<int> selectedEvents() const;
	QList<int> selectedStarts() const;
	/** The cells of the selected items, where they are dragged */
	QList<QPoint> itemCells() const;
	/** How far the selected items are dragged, in cells */
	QPoint itemsOffset() const;
	/** How far the dashes of the selection marched, a pixel of the screen
	 *  every step (RPG Maker's marching ants) */
	int antsOffset() const;
	Cursor cursor() const;
	/** A button is held on the map: a stroke, a selection or a drag (the
	 *  view scrolls when the pointer goes past its edges) */
	bool isDragging() const;
	/** The cell under the pointer, and its events on the event layer */
	QString hoverText() const;

	/** The zoom, 0 until set */
	qreal scale() const;
	/** Rounded by snapScale */
	void setScale(qreal scale);
	/** The nearest scale showing each pixel of the game on a power of two of
	 *  screen pixels, on the view's screen */
	Q_INVOKABLE qreal snapScale(qreal scale) const;
	static qreal snappedScale(qreal scale, qreal device_pixel_ratio);
	qreal devicePixelRatio() const;
	void setDevicePixelRatio(qreal ratio);
	/** The part of the map shown, in cells: where a paste goes and where
	 *  the arrow keys start */
	QRectF viewCells() const;
	void setViewCells(const QRectF& cells);

	/*
	 * Input, the pointer on a cell (outside the map: any cell out of it)
	 */

	Q_INVOKABLE void press(int x, int y, Qt::MouseButton button);
	Q_INVOKABLE void move(int x, int y, Qt::MouseButtons buttons);
	/** buttons: those still held */
	Q_INVOKABLE void release(int x, int y, Qt::MouseButton button, Qt::MouseButtons buttons);
	Q_INVOKABLE void doubleClick(int x, int y);
	/**
	 * The pointer pressed went past the platform's drag distance: what the
	 * press grabbed (an event, a start position, the selected items or
	 * tiles) follows it from now on. Before, the press is a click, which
	 * moves nothing wherever it is released.
	 */
	Q_INVOKABLE void startDrag();
	/** The press ended without its release (another window took the pointer) */
	Q_INVOKABLE void cancelPress();
	/** The pointer left the view: the brush is no longer framed */
	Q_INVOKABLE void leave();
	/** Run (or only test, for a shortcut override) a key of the map; true
	 *  when the map takes it, before the shortcuts of the window */
	Q_INVOKABLE bool handleKey(int key, Qt::KeyboardModifiers modifiers, bool run = true);
	/** Ctrl + wheel zooms, by the steps of the zoom tool; true when taken */
	Q_INVOKABLE bool wheel(int delta, Qt::KeyboardModifiers modifiers);
	/** A step of a pinch: the fingers spread by factor since the last one */
	Q_INVOKABLE void pinch(qreal factor);
	/** A pinch starts or ends */
	Q_INVOKABLE void resetPinch();
	Q_INVOKABLE bool canMoveItems(const QPoint& offset) const;
	/** Whether each action of a menu can be used now, by name
	 *  (actionEventNew...): {"enabled": bool, "visible": bool} */
	Q_INVOKABLE QVariantMap actionStates(Menu menu) const;

public slots:
	void zoomIn();
	void zoomOut();
	/** The cell under the pointer, and the selected one on the event layer */
	void selectTile(int x, int y);
	/** RPG Maker's eyedropper: the tile under the pointer becomes the tile picked */
	void pickTile();

	// The event layer's menu, on the selected cell
	void newEvent();
	void editEvent();
	void copyEvent();
	void cutEvent();
	void pasteEvent();
	void deleteEvent();
	void placeTeleport();
	/** The party's or a vehicle's start position (Core::StartMarker) */
	void placeStart(int kind);
	void runHere();

	// The selection tool's menu: the tiles selected or floating, or the
	// events selected on the event layer
	void cutArea();
	void copyArea();
	void pasteArea();
	void clearArea();
	void selectAll();
	void clearSelection();
	/** The pasted tiles go on the map (a click outside them) */
	void commitFloating();
	void cancelFloating();

	void undo();
	void redo();
	/** Shift Map: everything moves, wrapping round, with undo */
	void shiftMap(int dx, int dy);

signals:
	void pointerCellChanged();
	void brushRectChanged();
	/** The selected cell, the selection or the selected items changed */
	void selectionChanged();
	/** The selected items are dragged elsewhere: drawn there */
	void itemsOffsetChanged();
	void antsOffsetChanged();
	void cursorChanged();
	void draggingChanged();
	/** The pointer is on another cell, for the status bar */
	void cellHovered(const QString& text);
	void scaleChanged();
	void devicePixelRatioChanged();
	void viewCellsChanged();
	/** Scroll as little as needed to show a cell */
	void revealRequested(const QPoint& cell);
	/** A right click: the menu, with the actions of actionStates */
	void contextMenuRequested(MapEditor::Menu menu, const QPoint& cell);
	/** The event dialog, of a new event on a cell or of an event (id) */
	void newEventRequested(const QPoint& cell);
	void editEventRequested(int id);
	/** Where a teleport event on a free cell goes (MapDocument::placeTeleport) */
	void placeTeleportRequested(const QPoint& cell);
	/** Test play from a cell */
	void runHereRequested(const QPoint& cell);
	/** The map was read or replaced: its name, size, chipset... */
	void mapChanged();
	/** Pasted tiles started or stopped floating */
	void floatingChanged();
	/** They moved, or started or stopped floating */
	void floatingRectChanged();
	/** An edit was made, undone or redone, or the map read again */
	void historyChanged();

private:
	using Layer = EditorState::Layer;

	void onPress(int x, int y, Qt::MouseButton button);
	void onMove(int x, int y, Qt::MouseButtons buttons);
	void onRelease(int x, int y, Qt::MouseButton button, Qt::MouseButtons buttons);
	void onDoubleClick(int x, int y);
	/** What the press grabbed follows the pointer to m_cur */
	void dragTo();
	void onLayerChanged();
	void onToolChanged();
	/** The selection follows the pasted tiles */
	void onFloatingChanged();
	/** The pointer on a cell; false outside the map (none) */
	bool setPointer(int x, int y);
	void updateBrush();
	void setBrushRect(const QRect& cells);
	/** The tiles picked in the palette for the current layer */
	MapDocument::Brush brush() const;
	void stopDrawing();
	void stopSelecting();
	/** Select the cell of the event layer the actions apply to */
	void selectCell(int x, int y);
	/** The arrow keys of the event layer: the cell next to the selected one */
	void moveSelectedCell(int dx, int dy);
	/** The selection tool on the event layer: it selects events and start positions */
	bool selectingItems() const;
	/** Select the events and the start positions in cells; none: the selection is cleared */
	void selectItems(const QRect& cells);
	/** Move the selected items (one undo step) */
	void moveItems(const QPoint& offset);
	void copyItems();
	void deleteItems();
	/** Paste the copied events, the top left of their cells on a cell (one undo step) */
	void pasteItems(int x, int y);
	void setItemsOffset(const QPoint& offset);
	/** The selection changed: shown again, its ants marching */
	void updateSelection();
	/** The cursor and the drag state, from the rest */
	void refresh();
	Cursor currentCursor() const;
	bool currentlyDragging() const;

	MapDocument* m_doc;
	EditorState& m_state;
	// the cell under the pointer: none (-1) until it is over the map, so
	// the frame of the tool is not drawn on a map just opened
	QPoint m_cur{-1, -1};
	QRect m_brush;
	// the first cell of a stroke, or of the cells being selected
	QPoint m_first;
	// event layer: the selected cell, and the cell framed (where an event
	// or a start position is dragged)
	QPoint m_last;
	QPoint m_mark;
	bool m_selecting = false;
	bool m_drawing = false;
	bool m_cancelled = false;
	// event layer: the event or the start position (a Core::StartMarker)
	// dragged from the selected cell
	int m_dragEventId = 0;
	int m_dragStart = -1;
	// the pointer was dragged past the drag distance since the press
	bool m_dragStarted = false;
	// selection tool: the selected cells; their tiles dragged
	// (MapDocument::beginMoveArea)
	QRect m_selArea;
	bool m_selDragging = false;
	bool m_selMoving = false;
	QPoint m_selGrab;
	// event layer: the events (ids) and the start positions (Core::StartMarker)
	// selected in m_selArea, dragged by an offset of cells
	QList<int> m_selEvents;
	QList<int> m_selStarts;
	bool m_itemsSelecting = false;
	bool m_itemsMoving = false;
	QPoint m_itemsGrab;
	QPoint m_itemsOffset;
	QTimer m_antsTimer;
	int m_antsOffset = 0;
	// a pasted region floating over the map until a click outside it
	// (MapDocument::startFloating), dragged by a cell of it
	QPoint m_floatGrab;
	bool m_floatDragging = false;
	QString m_hoverText;
	Cursor m_cursor = ArrowCursor;
	bool m_dragging = false;
	qreal m_scale = 0;
	qreal m_devicePixelRatio = 1;
	// how much the fingers of a pinch spread since the last zoom step
	qreal m_pinch = 1;
	QRectF m_viewCells;
};
