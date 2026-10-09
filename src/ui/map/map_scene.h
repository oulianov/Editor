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

#include <optional>

#include <QGraphicsScene>
#include <QGraphicsLineItem>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsPixmapItem>
#include <QGraphicsView>
#include <QMap>
#include <QMenu>
#include <QPointer>
#include <QTimer>
#include <QUndoStack>
#include <QTreeWidgetItem>
#include <memory>
#include <lcf/rpg/map.h>
#include <lcf/rpg/mapinfo.h>
#include <lcf/rpg/start.h>
#include "core.h"

class ProjectData;
class EventDialog;

class MapScene : public QGraphicsScene
{
	Q_OBJECT
public:
	explicit MapScene(ProjectData& project, int id, QGraphicsView *view, QObject *parent = nullptr);
	~MapScene();

	void Init();
	float scale() const;
	/** Presentation state of this map, independent of its game data. */
	QVariantMap viewState() const;
	void restoreViewState(const QVariantMap& state);
	void selectTile(int x, int y);
	void centerOnTile(int x, int y);
	/** Shown at this scale, rounded by snapScale */
	void setScale(float scale);
	/** The nearest scale showing each pixel of the game on a power of two of screen pixels */
	float snapScale(float scale) const;
	QString mapName() const;
	bool isModified() const;
	bool canUndo() const;
	bool canRedo() const;
	int id() const;
	int chipsetId() const;
	// The data that undo and redo exchange
	const std::vector<short>& layerData(Core::Layer layer) const;
	void setLayerData(Core::Layer layer, std::vector<short> data);
	std::optional<lcf::rpg::Event> eventData(int id) const;
	/** Adds, replaces or (none) removes the event with this id */
	void setEventData(int id, const std::optional<lcf::rpg::Event> &data);
	const lcf::rpg::Map& mapData() const;
	const lcf::rpg::MapInfo& mapInfo() const;
	/** Restore the whole map and its info (undo of Map Properties) */
	void setMapData(const lcf::rpg::Map &map, const lcf::rpg::MapInfo &info);
	/** Restore the start positions (undo of Place Starting Position) */
	void setStartData(const lcf::rpg::Start &start);
	const lcf::rpg::Start& startData() const;
	/** The events by id; owned by the scene, valid until its events change */
	QMap<int, lcf::rpg::Event *> *mapEvents();
	/** The tile of a layer (LOWER or UPPER) at a cell, -1 outside the map */
	int tileAt(Core::Layer layer, int x, int y) const;
	/** The id of the event at a cell, 0 when there is none */
	int eventIdAt(int x, int y) const;
	/** Draw the boundaries of an area (cells) as a dashed rectangle; empty rect: hidden */
	void showArea(const QRect &cells);
	void editMapProperties(QTreeWidgetItem *item);
	/** Take the map info (name...) from the map tree again, unless edited */
	void refreshMapInfo();
	/** The map file was changed by another program: read it again (asking
	 *  first when the map has unsaved changes and ask is set) */
	void reloadFromDisk(bool ask = true);
	/** Open the event dialog of a new or existing event (modal) */
	void editEvent(const lcf::rpg::Event& event);
	/** Put an edited event on the map (replacing the one with its id), with undo */
	void commitEvent(const lcf::rpg::Event& event);
	lcf::rpg::Event* findEvent(int id);
	/** The texts of the menus, in the current language */
	void retranslate();
	/** Shift Map: everything moves, wrapping round, with undo */
	void shiftMap(int dx, int dy);

signals:

	void mapChanged();

	void mapSaved();

	void mapReverted();

	/** The map was read again from disk (its events are new objects) */
	void mapReloaded();

	/** The zoom changed (zoom tool, Ctrl+wheel, menus) */
	void scaleChanged();

	/** The cell under the mouse, and its event on the event layer, for the status bar */
	void cellHovered(const QString& text);

public slots:
	void redrawMap();

	void onLayerChanged();

	void onToolChanged();

	void Save(bool properties_changed = false);

	void Load(bool revert = false);

	void undo();

	void redo();

private slots:
	void on_actionRunHere();

	void on_actionSetStartPosition();

	void on_actionSetVehiclePosition();

	void on_actionPlaceTeleport();

	void on_actionNewEvent();

	void on_actionEditEvent();

	void on_actionCopyEvent();

	void on_actionCutEvent();

	void on_actionPasteEvent();

	void on_actionDeleteEvent();

	void on_actionCutArea();

	void on_actionCopyArea();

	void on_actionPasteArea();

	void on_actionClearArea();

	void on_actionSelectAll();

	void on_user_interaction();

	void on_view_V_Scroll();

	void on_view_H_Scroll();

	/** Draw the next frame of the water and animated tiles shown */
	void animateTiles();

protected:
	void mousePressEvent(QGraphicsSceneMouseEvent *event);
	void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
	void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);
	void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event);
	void keyPressEvent(QKeyEvent *event);
	void wheelEvent(QGraphicsSceneWheelEvent *event);
	bool event(QEvent *event);
	bool eventFilter(QObject *watched, QEvent *event);

private:
	/** The cursor of the tool, or of the event layer */
	void applyCursor();
	/** Frames the cells the tool would draw on at the mouse */
	void updateBrush();
	int _x(int index);
	int _y(int index);
	int _index(int x, int y);
	void redrawTile(const Core::Layer &layer,
					const int &x,
					const int &y,
					const QRect &dest_rec);
	void stopDrawing();
	void stopSelecting();
	void updateArea(int x1, int y1, int x2, int y2);
	void redrawLayer(Core::Layer layer);
	void drawPen();
	void drawRect();
	void drawEllipse();
	void drawFill();
	void bindArea(int x1, int y1, int x2, int y2);
	short bind(int x, int y);
	/** Set cur_x, cur_y from a scene position; false outside the map */
	bool updateCursor(const QPointF &pos);
	/** Select the cell of the event layer the actions apply to */
	void selectCell(int x, int y);
	/** Run (or only test, for ShortcutOverride) a key of the map view */
	bool handleKey(QKeyEvent *event, bool run);
	void pickTile();
	void moveEvent(int id, int x, int y);
	void pushStart(const lcf::rpg::Start &old_start);
	void fillArea(const QRect &area);
	/** Push the layers changed since the last commit (one undo step) */
	void pushLayers(const QString &text);
	void updateSelectionArea();
	void clearSelectionArea();
	void showFloating();
	void commitFloating();
	void cancelFloating();
	lcf::rpg::Event* getEventAt(int x, int y);
	int getFirstFreeId();
	void redrawPanorama();
	void redrawGrid();
	/** Shows map (read from its file); keep_view keeps the grid and the
	 *  scale unless the size changed */
	void setMap(std::unique_ptr<lcf::rpg::Map> map, bool keep_view);
	/** Merges what another program changed in the map file since it was
	 *  read; false when both sides changed the same thing (conflicts) */
	bool mergeFromDisk(QStringList* conflicts);
	/** The map as last read or written: the base of a merge */
	std::optional<lcf::rpg::Map> m_savedMap;


	QMenu *m_eventMenu;
	QMenu *m_areaMenu;
	QMenu *m_vehicleMenu = nullptr;
	QAction *m_actionNew;
	QAction *m_actionEdit;
	QAction *m_actionCopy;
	QAction *m_actionCut;
	QAction *m_actionPaste;
	QAction *m_actionDelete;
	QAction *m_actionTeleport;
	QAction *m_actionCutArea;
	QAction *m_actionCopyArea;
	QAction *m_actionPasteArea;
	QAction *m_actionClearArea;
	QGraphicsPixmapItem *m_lowerpix;
	QGraphicsPixmapItem *m_upperpix;
	QGraphicsItemGroup* m_lines;
	QGraphicsRectItem* m_selectionTile;
	QUndoStack *m_undoStack;
	std::unique_ptr<lcf::rpg::Map> m_map;
	lcf::rpg::MapInfo n_mapInfo; //To store unsaved changes
	std::vector<short> m_lower;
	std::vector<short> m_upper;
	float m_scale = 0.0f; // 0 until the first setScale
	bool m_init = false;
	int s_tileSize;
	int cur_x;
	int cur_y;
	int fst_x;
	int fst_y;
	int lst_x = 0;
	int lst_y = 0;
	QGraphicsView* m_view;
	bool m_drawing;
	bool m_cancelled;
	bool m_selecting;
	bool m_userInteraction = false;
	ProjectData& m_project;
	QMap<int, lcf::rpg::Event*> m_events;
	QPixmap m_panorama;
	// the animated cells of the lower pixmap (water, animated tiles), the
	// first cell of the pixmap and the frames it shows
	std::vector<QPoint> m_waterCells;
	std::vector<QPoint> m_animatedCells;
	QPoint m_lowerOrigin;
	int m_drawnWaterFrame = 0;
	int m_drawnTileFrame = 0;
	// redraw when shown: drawing a hidden tab would load its chipset
	bool m_needsRedraw = false;
	// event layer: the selected cell and the event dragged from it
	int m_dragEventId = 0;
	// selection tool: the selected cells, the dashed rectangle around them
	QRect m_selArea;
	bool m_selDragging = false;
	bool m_selMoving = false;
	QPoint m_selGrab;
	Core::TileClipboard m_selContents;
	// Both layers beneath a movable selection, restored as it crosses them.
	Core::TileClipboard m_selBackground;
	QGraphicsRectItem *m_selAreaItem;
	QGraphicsRectItem *m_selAntsItem;
	QTimer m_antsTimer;
	int m_antsOffset = 0;
	// a pasted region floating over the map until a click outside it
	Core::TileClipboard m_float;
	QPoint m_floatPos;
	QPoint m_floatGrab;
	bool m_floating = false;
	bool m_floatDragging = false;
	QGraphicsPixmapItem *m_floatItem;
	QGraphicsRectItem *m_areaItem;
	/** The cells the tool draws on, under the mouse */
	QGraphicsRectItem *m_brushItem;
	QGraphicsRectItem *m_brushInner;
	QList<QGraphicsItem*> grid_lines;
	QPointer<EventDialog> m_eventDialog;
	int m_eventDialogId = 0;
};
