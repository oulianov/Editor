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
#include <QStringList>
#include <functional>
#include <memory>
#include <optional>
#include <vector>
#include <lcf/rpg/event.h>
#include <lcf/rpg/map.h>
#include <lcf/rpg/mapinfo.h>
#include <lcf/rpg/start.h>
#include "editor_state.h"
#include "stale_choice.h"

class ProjectData;
class QUndoStack;

/**
 * A map being edited, apart from how it is shown: its data, its undo stack
 * and every change made to it (the drawing tools, the selection tool, the
 * events, the start positions, Shift Map, Map Properties), reading it and
 * saving it. A view (MapEditor, MapCanvas.qml) turns the mouse into these
 * calls and draws again what the signals say changed.
 *
 * The drawing tools draw on the working layers, which are the map's plus
 * the stroke in progress; commitStroke() makes the stroke one undo step.
 * Nothing here asks the user: what to do with a file changed on disk is
 * asked through the handlers the interface sets.
 */
class MapDocument : public QObject
{
	Q_OBJECT

public:
	using Layer = EditorState::Layer;
	using TileClipboard = EditorState::TileClipboard;

	/** The tiles a drawing tool puts down, picked in the palette: repeated
	 *  past its size, counted from the cell the stroke started on */
	struct Brush
	{
		std::vector<short> tiles{0};
		int width = 1;
		int height = 1;
		short at(int off_x, int off_y) const;
	};

	/** The map id of the map tree of project; the map is read by load() */
	MapDocument(ProjectData& project, int id, QObject* parent = nullptr);
	~MapDocument() override;

	int id() const;
	/** A map was read or set: map() and the layers can be used */
	bool hasMap() const;
	QString name() const;
	const lcf::rpg::Map& map() const;
	const lcf::rpg::MapInfo& mapInfo() const;
	/** The cells of the map: QRect(0, 0, width, height) */
	QRect bounds() const;
	QUndoStack* undoStack() const;
	/** Undoing back to the saved map makes it unmodified again */
	bool isModified() const;
	bool canUndo() const;
	bool canRedo() const;
	void undo();
	void redo();

	/** Shows map, read from its file or made by the caller: no edit is left */
	void setMap(std::unique_ptr<lcf::rpg::Map> map);
	/** Take the map info (name...) from the map tree again, unless Map
	 *  Properties edited it: the other edits keep it as the tree has it */
	void refreshMapInfo();
	/** The map is in the map tree: another program can delete it (its
	 *  entry and its file) while it is open */
	bool hasTreeEntry() const;
	/** Where the view was scrolled to, kept in the map info (saved in the tree) */
	void setScrollPosition(int x, int y);

	// The data that undo and redo exchange (map_undo.h)
	const std::vector<short>& layerData(Layer layer) const;
	void setLayerData(Layer layer, std::vector<short> data);
	std::optional<lcf::rpg::Event> eventData(int id) const;
	/** Adds, replaces or (none) removes the event with this id */
	void setEventData(int id, const std::optional<lcf::rpg::Event>& data);
	/** The whole map and its info (undo of Map Properties and Shift Map) */
	void setMapData(const lcf::rpg::Map& map, const lcf::rpg::MapInfo& info);
	/** The start positions, in the map tree: shared by all maps */
	const lcf::rpg::Start& startData() const;
	void setStartData(const lcf::rpg::Start& start);

	/*
	 * Tiles
	 */

	/** The tile of a layer (LOWER or UPPER) at a cell, as the map has it; -1
	 *  outside the map */
	int tileAt(Layer layer, int x, int y) const;
	/** A layer as drawn: the map's with the stroke or the drag in progress */
	const std::vector<short>& workingLayer(Layer layer) const;
	short workingTile(Layer layer, int x, int y) const;

	/** The pen: the brush on the cells from cell, origin the first cell of
	 *  the stroke; autotiles get their borders */
	void drawPen(Layer layer, const QPoint& cell, const QPoint& origin, const Brush& brush);
	/** The rectangle from origin to cell, in place of the stroke's last one */
	void drawRect(Layer layer, const QPoint& origin, const QPoint& cell, const Brush& brush);
	/** The filled ellipse inscribed in the rectangle from origin to cell */
	void drawEllipse(Layer layer, const QPoint& origin, const QPoint& cell, const Brush& brush);
	/** The cells joined to origin by the same terrain (lower layer) or the
	 *  same tile (upper layer) */
	void drawFill(Layer layer, const QPoint& origin, const Brush& brush);
	/** The working layers become the map's: one undo step, when they differ */
	void commitStroke(const QString& text = QString());
	/** The stroke is dropped: the working layers are the map's again */
	void cancelStroke();

	/** Both layers of cells, for the clipboard */
	TileClipboard copyArea(const QRect& cells) const;
	/** RPG Maker's Clear (and Cut): the brush over cells of a layer, one undo step */
	void fillArea(Layer layer, const QRect& cells, const Brush& brush);

	/** Dragging the tiles of cells with the selection tool: the cells they
	 *  leave get background below and nothing above */
	void beginMoveArea(const QRect& cells, short background);
	/** The dragged tiles with their top left on a cell, inside the map; the
	 *  cells they are on */
	QRect moveAreaTo(const QPoint& top_left);
	/** The drag ends: one undo step (commit), else the tiles go back */
	void endMoveArea(bool commit);

	/** Pasted tiles floating over the map until put down or dropped */
	void startFloating(const TileClipboard& tiles, const QPoint& pos);
	bool isFloating() const;
	const TileClipboard& floatingTiles() const;
	QPoint floatingPos() const;
	void setFloatingPos(const QPoint& pos);
	/** The floating tiles go on the map as they are (one undo step) */
	void commitFloating();
	void cancelFloating();

	/*
	 * Events and start positions
	 */

	const lcf::rpg::Event* findEvent(int id) const;
	const lcf::rpg::Event* eventAt(int x, int y) const;
	/** The id of the event at a cell, 0 when there is none */
	int eventIdAt(int x, int y) const;
	int firstFreeEventId() const;
	/** No event and no start position on the cell: what can be placed there */
	bool isFreeCell(int x, int y) const;
	/** A new event as RPG Maker makes it (EV0001..., the first free id),
	 *  not on the map yet */
	lcf::rpg::Event newEvent(int x, int y) const;
	/** Puts an event on the map, replacing the one with its id (one undo
	 *  step); false when nothing changed */
	bool commitEvent(const lcf::rpg::Event& event);
	/** A copy of an event on a free cell, with the first free id */
	bool pasteEvent(const lcf::rpg::Event& event, int x, int y);
	bool deleteEventAt(int x, int y);
	/** Moves an event to a cell without one */
	bool moveEvent(int id, int x, int y);
	/** RPG Maker's Place Teleport Event: an event touched by the hero going
	 *  to map_id, to_x, to_y; direction: the facing after it (RPG Maker
	 *  2003, 0 keeps it) */
	void placeTeleport(int x, int y, int map_id, int to_x, int to_y, std::optional<int> direction);

	/** The start position (a Core::StartMarker) on a cell, -1 when there is none */
	int startAt(int x, int y) const;
	/** The cell of a start position, when it is on this map */
	std::optional<QPoint> startCell(int kind) const;
	/** Puts a start position on a cell (one undo step) */
	void setStartPosition(int kind, int x, int y);

	// Several events and start positions: the selection tool of the event layer
	QList<int> eventsIn(const QRect& cells) const;
	QList<int> startsIn(const QRect& cells) const;
	/** Every item can go offset cells away: in the map, on a free cell or a
	 *  cell one of them leaves */
	bool canMoveItems(const QList<int>& events, const QList<int>& starts, const QPoint& offset) const;
	/** One undo step; false when they cannot move */
	bool moveItems(const QList<int>& events, const QList<int>& starts, const QPoint& offset);
	/** Copies of events, their cells from origin (start positions are not
	 *  copied: there is one of each) */
	std::vector<lcf::rpg::Event> copyEvents(const QList<int>& events, const QPoint& origin) const;
	/** One undo step */
	void deleteEvents(const QList<int>& events);
	/** Every copy goes on a free cell of the map, from cell */
	bool canPasteEvents(const std::vector<lcf::rpg::Event>& events, const QPoint& cell) const;
	/** Copies with the first free ids (one undo step); their ids, none when
	 *  they do not fit */
	QList<int> pasteEvents(const std::vector<lcf::rpg::Event>& events, const QPoint& cell);

	/*
	 * The whole map
	 */

	/** Map Properties: the map and its info (one undo step); false when
	 *  nothing changed */
	bool setMapProperties(const lcf::rpg::Map& map, const lcf::rpg::MapInfo& info);
	/** Shift Map: everything moves, wrapping round (one undo step) */
	void shiftMap(int dx, int dy);

	/*
	 * The file (map_document_file.cpp)
	 */

	/** Reads the map from its file; revert undoes every edit first (the
	 *  start positions are in the map tree, not in the file). An unreadable
	 *  file keeps the map, else gives an empty one. */
	void load(bool revert = false);
	/** The file was changed by another program: read it again (asking the
	 *  reload handler first when there are unsaved changes and ask is set) */
	void reloadFromDisk(bool ask = true);
	/** Writes the map and, when it changed, the map tree. A file another
	 *  program changed is merged, or the stale handler decides. false: not
	 *  saved (cancelled, or a file could not be written: see lastError) and
	 *  still modified. */
	bool save(bool properties_changed = false);
	/** Why the last save failed, empty when it worked */
	QString lastError() const;

	/** what: "The map ..."; conflicts: what both sides changed differently.
	 *  Without a handler, nothing is overwritten (Cancel). */
	using StaleHandler = std::function<StaleChoice(const QString& what, bool can_reload, const QStringList& conflicts)>;
	void setStaleHandler(StaleHandler handler);
	/** Discard the unsaved changes and read the file again? Without a
	 *  handler, they are kept. */
	using ReloadHandler = std::function<bool()>;
	void setReloadHandler(ReloadHandler handler);
	/** Another program deleted the map while it was edited (its entry left
	 *  the map tree): a file without its entry is never written. Overwrite:
	 *  the handler put it back in the tree, it is saved; Reload: its edits
	 *  are dropped, nothing is written (the interface closes it); Cancel:
	 *  they are kept. Without a handler, nothing is written (Cancel). */
	using DeletedHandler = std::function<StaleChoice()>;
	void setDeletedHandler(DeletedHandler handler);

signals:
	/** Tiles of a layer changed: in the map, or only in the working layers */
	void cellsChanged(const QRect& cells, int layer);
	/** Events were added, changed or removed: pointers to them are stale */
	void eventsChanged();
	/** A start position on this map moved, came or left */
	void startChanged();
	/** The whole map was replaced (its file read, Map Properties, Shift Map,
	 *  a merge); its events are new objects */
	void mapReplaced(bool size_changed);
	/** Pasted tiles started or stopped floating */
	void floatingChanged();
	/** An edit was made, undone or redone */
	void edited();
	void modifiedChanged(bool modified);
	/** Read from the file: no edit is left */
	void loaded();
	/** Read again after another program changed the file */
	void reloaded();
	/** A save starts: what floats over the map is put down first */
	void aboutToSave();
	void saved();
	void saveFailed(const QString& message);

private:
	int index(int x, int y) const;
	std::vector<short>& working(Layer layer);
	/** Autotile borders on the working lower layer, in x1, y1 - x2, y2 */
	void bindArea(int x1, int y1, int x2, int y2);
	/** The borders of the lower layer near a stroke, and the redraw */
	void updateArea(Layer layer, int x1, int y1, int x2, int y2);
	void pushStart(const lcf::rpg::Start& old_start);
	/** Merges what another program changed in the file since it was read;
	 *  false when both sides changed the same thing (conflicts) */
	bool mergeFromDisk(QStringList* conflicts);
	StaleChoice askStale(const QString& what, bool can_reload, const QStringList& conflicts) const;
	void fail(const QString& message);

	ProjectData& m_project;
	std::unique_ptr<lcf::rpg::Map> m_map;
	lcf::rpg::MapInfo m_info; // with its unsaved changes
	/** The map info as last read or written: Map Properties edited m_info
	 *  when they differ (infoEdited) */
	lcf::rpg::MapInfo m_savedInfo;
	bool infoEdited() const;
	std::vector<short> m_lower;
	std::vector<short> m_upper;
	/** The map as last read or written: the base of a merge */
	std::optional<lcf::rpg::Map> m_savedMap;
	QUndoStack* m_undoStack;
	// a selection dragged: its tiles, and both layers beneath them, restored
	// as it crosses them
	TileClipboard m_moveContents;
	TileClipboard m_moveBackground;
	// a pasted region floating over the map
	TileClipboard m_float;
	QPoint m_floatPos;
	bool m_floating = false;
	StaleHandler m_staleHandler;
	ReloadHandler m_reloadHandler;
	DeletedHandler m_deletedHandler;
	QString m_lastError;
};
