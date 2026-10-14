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

// The editing of a map with the mouse and the keys without a view: the
// pointer on cells, as the Qt Quick canvas gives them, on a
// small map in memory. The drawing itself is MapDocument's
// (map_document_test); the script tests drive the map view.

#include <QKeySequence>
#include <QSignalSpy>
#include <QTest>
#include <QUndoStack>
#include "defines.h"
#include "common/dbstring.h"
#include "model/autotile.h"
#include "model/editor_state.h"
#include "model/map_document.h"
#include "model/project_data.h"
#include "ui/map/map_editor.h"

namespace {
// terrains of the palette: water, two ground autotiles (D block)
constexpr int Water = 0;
constexpr int Grass = 6;
constexpr int Dirt = 7;
constexpr int Width = 8;
constexpr int Height = 6;
constexpr int MapId = 1;
// TilePainter::StartMarker
constexpr int Party = 0;
constexpr int Boat = 1;

short sample(int terrain)
{
	// as the palette picks it
	return Autotile::translate(terrain, SAMPLE);
}

int terrainAt(const MapDocument& doc, int x, int y)
{
	return Autotile::translate(static_cast<short>(doc.tileAt(EditorState::LOWER, x, y)));
}

int workingTerrainAt(const MapDocument& doc, int x, int y)
{
	return Autotile::translate(doc.workingTile(EditorState::LOWER, x, y));
}

bool enabled(const QVariantMap& states, const char* action)
{
	return states.value(QLatin1String(action)).toMap().value("enabled").toBool();
}

bool visible(const QVariantMap& states, const char* action)
{
	return states.value(QLatin1String(action)).toMap().value("visible").toBool();
}
}

class MapEditorTest : public QObject
{
	Q_OBJECT

private slots:
	void init();
	void cleanup();
	void penStrokeUndo();
	void rectangleCancelled();
	void fill();
	void brushAndEyedropper();
	void selectionDragMove();
	void floatingPaste();
	void eventSelectAndDrag();
	void startPositionDrag();
	void itemsMoveValidity();
	void keys();
	void actionStates();
	void zoomSteps();
	void hoverText();

private:
	/** A standard key (Copy...) as the platform has it */
	bool key(QKeySequence::StandardKey standard, bool run = true);

	ProjectData m_project;
	std::unique_ptr<EditorState> m_state;
	std::unique_ptr<MapDocument> m_doc;
	std::unique_ptr<MapEditor> m_editor;
};

void MapEditorTest::init()
{
	// a map tree with the project (0) and the map, the start elsewhere
	m_project = ProjectData();
	lcf::rpg::MapInfo root;
	root.ID = 0;
	lcf::rpg::MapInfo info;
	info.ID = MapId;
	info.name = ToDBString(QStringLiteral("Field"));
	m_project.treeMap().maps = {root, info};
	m_project.treeMap().start.party_map_id = 2;

	auto map = std::make_unique<lcf::rpg::Map>();
	map->width = Width;
	map->height = Height;
	map->lower_layer.assign(Width * Height, Autotile::translate(Grass));
	map->upper_layer.assign(Width * Height, 10000);
	m_state = std::make_unique<EditorState>();
	m_doc = std::make_unique<MapDocument>(m_project, MapId);
	m_doc->setMap(std::move(map));
	m_editor = std::make_unique<MapEditor>(m_doc.get(), *m_state);
}

void MapEditorTest::cleanup()
{
	m_editor.reset();
	m_doc.reset();
	m_state.reset();
}

bool MapEditorTest::key(QKeySequence::StandardKey standard, bool run)
{
	const QList<QKeySequence> bindings = QKeySequence::keyBindings(standard);
	if (bindings.isEmpty())
		return false;
	const QKeyCombination combination = bindings.first()[0];
	return m_editor->handleKey(combination.key(), combination.keyboardModifiers(), run);
}

void MapEditorTest::penStrokeUndo()
{
	m_state->setSelection({sample(Dirt)}, 1, 1);
	QCOMPARE(m_editor->cursor(), MapEditor::PenCursor);
	m_editor->press(3, 2, Qt::LeftButton);
	QVERIFY(m_editor->isDragging());
	// drawn in the working layer until the button is released
	QCOMPARE(workingTerrainAt(*m_doc, 3, 2), Dirt);
	QCOMPARE(terrainAt(*m_doc, 3, 2), Grass);
	m_editor->move(4, 2, Qt::LeftButton);
	m_editor->move(4, 3, Qt::LeftButton);
	m_editor->release(4, 3, Qt::LeftButton, Qt::NoButton);
	QVERIFY(!m_editor->isDragging());
	QCOMPARE(terrainAt(*m_doc, 3, 2), Dirt);
	QCOMPARE(terrainAt(*m_doc, 4, 2), Dirt);
	QCOMPARE(terrainAt(*m_doc, 4, 3), Dirt);
	QCOMPARE(terrainAt(*m_doc, 5, 2), Grass);
	// one undo step for the stroke
	QCOMPARE(m_doc->undoStack()->count(), 1);
	m_editor->undo();
	QCOMPARE(terrainAt(*m_doc, 3, 2), Grass);
	QCOMPARE(terrainAt(*m_doc, 4, 3), Grass);
	m_editor->redo();
	QCOMPARE(terrainAt(*m_doc, 4, 3), Dirt);

	// out of the map: nothing drawn
	m_editor->press(-1, 2, Qt::LeftButton);
	QVERIFY(!m_editor->isDragging());
	m_editor->release(-1, 2, Qt::LeftButton, Qt::NoButton);
	QCOMPARE(m_doc->undoStack()->count(), 1);
}

void MapEditorTest::rectangleCancelled()
{
	m_state->setTool(EditorState::RECTANGLE);
	QCOMPARE(m_editor->cursor(), MapEditor::RectangleCursor);
	m_state->setSelection({sample(Dirt)}, 1, 1);
	m_editor->press(1, 1, Qt::LeftButton);
	m_editor->move(4, 1, Qt::LeftButton);
	// the rectangle follows the pointer, in place of the last one
	m_editor->move(3, 2, Qt::LeftButton);
	QCOMPARE(workingTerrainAt(*m_doc, 4, 1), Grass);
	m_editor->release(3, 2, Qt::LeftButton, Qt::NoButton);
	for (int y = 1; y <= 2; y++)
		for (int x = 1; x <= 3; x++)
			QCOMPARE(terrainAt(*m_doc, x, y), Dirt);
	QCOMPARE(terrainAt(*m_doc, 4, 2), Grass);
	QCOMPARE(terrainAt(*m_doc, 3, 3), Grass);
	QCOMPARE(m_doc->undoStack()->count(), 1);

	// a right click drops the stroke; the left button released draws nothing
	m_state->setSelection({sample(Water)}, 1, 1);
	m_editor->press(5, 3, Qt::LeftButton);
	m_editor->move(6, 4, Qt::LeftButton);
	QCOMPARE(workingTerrainAt(*m_doc, 6, 4), Water);
	m_editor->press(6, 4, Qt::RightButton);
	QCOMPARE(workingTerrainAt(*m_doc, 6, 4), Grass);
	QVERIFY(!m_editor->isDragging());
	m_editor->release(6, 4, Qt::RightButton, Qt::LeftButton);
	m_editor->move(7, 5, Qt::LeftButton);
	m_editor->release(7, 5, Qt::LeftButton, Qt::NoButton);
	QCOMPARE(workingTerrainAt(*m_doc, 7, 5), Grass);
	QCOMPARE(m_doc->undoStack()->count(), 1);
	// the next stroke is drawn
	m_editor->press(6, 4, Qt::LeftButton);
	m_editor->release(6, 4, Qt::LeftButton, Qt::NoButton);
	QCOMPARE(terrainAt(*m_doc, 6, 4), Water);
	QCOMPARE(m_doc->undoStack()->count(), 2);
}

void MapEditorTest::fill()
{
	m_state->setTool(EditorState::FILL);
	m_state->setSelection({sample(Water)}, 1, 1);
	m_editor->press(0, 0, Qt::LeftButton);
	m_editor->release(0, 0, Qt::LeftButton, Qt::NoButton);
	// the whole grass, joined to the cell clicked
	QCOMPARE(terrainAt(*m_doc, 0, 0), Water);
	QCOMPARE(terrainAt(*m_doc, Width - 1, Height - 1), Water);
	QCOMPARE(m_doc->undoStack()->count(), 1);
	m_editor->undo();
	QCOMPARE(terrainAt(*m_doc, Width - 1, Height - 1), Grass);
}

void MapEditorTest::brushAndEyedropper()
{
	QSignalSpy brushes(m_editor.get(), &MapEditor::brushRectChanged);
	// nothing framed on a map just opened
	QCOMPARE(m_editor->pointerCell(), QPoint(-1, -1));
	QVERIFY(m_editor->brushRect().isEmpty());

	// the pen frames the tiles picked, cut at the edges of the map
	m_state->setSelection({sample(Water), sample(Water), sample(Water), sample(Water)}, 2, 2);
	m_editor->move(2, 2, Qt::NoButton);
	QCOMPARE(m_editor->pointerCell(), QPoint(2, 2));
	QCOMPARE(m_editor->brushRect(), QRect(2, 2, 2, 2));
	QCOMPARE(brushes.count(), 1);
	m_editor->move(Width - 1, Height - 1, Qt::NoButton);
	QCOMPARE(m_editor->brushRect(), QRect(Width - 1, Height - 1, 1, 1));
	// off the map, and out of the view
	m_editor->move(Width, 0, Qt::NoButton);
	QCOMPARE(m_editor->pointerCell(), QPoint(-1, -1));
	QVERIFY(m_editor->brushRect().isEmpty());
	m_editor->move(3, 3, Qt::NoButton);
	QVERIFY(!m_editor->brushRect().isEmpty());
	m_editor->leave();
	QVERIFY(m_editor->brushRect().isEmpty());
	QCOMPARE(m_editor->pointerCell(), QPoint(3, 3));

	// the other tools frame a cell; the selection tool and the event layer none
	m_state->setTool(EditorState::RECTANGLE);
	QCOMPARE(m_editor->brushRect(), QRect(3, 3, 1, 1));
	m_state->setTool(EditorState::SELECT);
	QVERIFY(m_editor->brushRect().isEmpty());
	QCOMPARE(m_editor->cursor(), MapEditor::SelectCursor);
	m_state->setTool(EditorState::PENCIL);
	m_state->setLayer(EditorState::EVENT);
	QVERIFY(m_editor->brushRect().isEmpty());
	QCOMPARE(m_editor->cursor(), MapEditor::ArrowCursor);
	m_state->setLayer(EditorState::LOWER);

	// the right button picks the tile of the cell, as the palette has it
	m_doc->drawPen(EditorState::LOWER, QPoint(5, 1), QPoint(5, 1), {{sample(Dirt)}, 1, 1});
	m_doc->commitStroke();
	m_editor->press(5, 1, Qt::RightButton);
	QCOMPARE(m_state->selection(0, 0), sample(Dirt));
	QCOMPARE(m_state->selWidth(), 1);
	QCOMPARE(m_state->selHeight(), 1);
	QCOMPARE(m_doc->undoStack()->count(), 1);
	// on the upper layer, a lower terrain is not picked
	m_state->setLayer(EditorState::UPPER);
	const short upper = m_state->selection(0, 0);
	m_doc->setLayerData(EditorState::UPPER, std::vector<short>(Width * Height, Autotile::translate(Dirt)));
	m_editor->press(5, 1, Qt::RightButton);
	QCOMPARE(m_state->selection(0, 0), upper);
}

void MapEditorTest::selectionDragMove()
{
	m_doc->drawRect(EditorState::LOWER, QPoint(1, 1), QPoint(2, 2), {{sample(Dirt)}, 1, 1});
	m_doc->commitStroke();
	const short corner = static_cast<short>(m_doc->tileAt(EditorState::LOWER, 1, 1));
	m_state->setSelection({sample(Water)}, 1, 1);
	m_state->setTool(EditorState::SELECT);

	// a rectangle of cells, its dashes marching
	QSignalSpy selection(m_editor.get(), &MapEditor::selectionChanged);
	m_editor->press(1, 1, Qt::LeftButton);
	QVERIFY(m_editor->isDragging());
	m_editor->move(2, 2, Qt::LeftButton);
	m_editor->release(2, 2, Qt::LeftButton, Qt::NoButton);
	QCOMPARE(m_editor->selectionArea(), QRect(1, 1, 2, 2));
	QCOMPARE(m_editor->selectionRect(), QRect(1, 1, 2, 2));
	QVERIFY(selection.count() >= 2);
	QTRY_VERIFY(m_editor->antsOffset() > 0);
	// over it, the cursor to drag it
	QCOMPARE(m_editor->cursor(), MapEditor::MoveCursor);
	m_editor->move(4, 4, Qt::NoButton);
	QCOMPARE(m_editor->cursor(), MapEditor::SelectCursor);

	// dragged by a cell of it: its tiles go along, inside the map
	const int steps = m_doc->undoStack()->count();
	m_editor->press(2, 2, Qt::LeftButton);
	m_editor->move(5, 3, Qt::LeftButton);
	QCOMPARE(m_editor->selectionArea(), QRect(4, 2, 2, 2));
	QCOMPARE(m_doc->workingTile(EditorState::LOWER, 4, 2), corner);
	m_editor->move(Width - 1, Height - 1, Qt::LeftButton);
	QCOMPARE(m_editor->selectionArea(), QRect(Width - 2, Height - 2, 2, 2));
	m_editor->move(5, 3, Qt::LeftButton);
	m_editor->release(5, 3, Qt::LeftButton, Qt::NoButton);
	QCOMPARE(m_doc->tileAt(EditorState::LOWER, 4, 2), static_cast<int>(corner));
	QCOMPARE(terrainAt(*m_doc, 5, 3), Dirt);
	// the cells left get the lower tile picked, without its variant
	QCOMPARE(terrainAt(*m_doc, 1, 1), Water);
	QCOMPARE(m_doc->undoStack()->count(), steps + 1);

	// Escape drops the selection, Select All takes the whole map
	QVERIFY(m_editor->handleKey(Qt::Key_Escape, Qt::NoModifier, false));
	QCOMPARE(m_editor->selectionArea(), QRect(4, 2, 2, 2));
	QVERIFY(m_editor->handleKey(Qt::Key_Escape, Qt::NoModifier));
	QVERIFY(m_editor->selectionArea().isEmpty());
	QVERIFY(!m_editor->handleKey(Qt::Key_Escape, Qt::NoModifier));
	QVERIFY(key(QKeySequence::SelectAll));
	QCOMPARE(m_editor->selectionArea(), m_doc->bounds());
	// another tool ends it
	m_state->setTool(EditorState::PENCIL);
	QVERIFY(m_editor->selectionArea().isEmpty());

	// a drag undone by another tool: the tiles go back
	m_state->setTool(EditorState::SELECT);
	m_editor->press(4, 2, Qt::LeftButton);
	m_editor->release(4, 2, Qt::LeftButton, Qt::NoButton);
	m_editor->press(4, 2, Qt::LeftButton);
	m_editor->move(0, 0, Qt::LeftButton);
	QCOMPARE(m_doc->workingTile(EditorState::LOWER, 0, 0), corner);
	m_state->setTool(EditorState::PENCIL);
	QCOMPARE(m_doc->workingTile(EditorState::LOWER, 4, 2), corner);
	QCOMPARE(m_doc->undoStack()->count(), steps + 1);
}

void MapEditorTest::floatingPaste()
{
	m_doc->drawRect(EditorState::LOWER, QPoint(0, 0), QPoint(1, 1), {{sample(Dirt)}, 1, 1});
	m_doc->commitStroke();
	m_state->setTool(EditorState::SELECT);
	m_editor->press(0, 0, Qt::LeftButton);
	m_editor->move(1, 1, Qt::LeftButton);
	m_editor->release(1, 1, Qt::LeftButton, Qt::NoButton);
	QVERIFY(key(QKeySequence::Copy));
	QCOMPARE(m_state->tileClipboard().width, 2);
	QCOMPARE(m_state->tileClipboard().height, 2);

	// in the middle of the view, selected
	m_editor->setViewCells(QRectF(2, 1, 4, 4));
	const int steps = m_doc->undoStack()->count();
	QVERIFY(key(QKeySequence::Paste));
	QVERIFY(m_doc->isFloating());
	QCOMPARE(m_doc->floatingPos(), QPoint(3, 2));
	QCOMPARE(m_editor->selectionArea(), QRect(3, 2, 2, 2));
	// dragged by a cell of it, not on the map yet
	m_editor->press(4, 3, Qt::LeftButton);
	QVERIFY(m_editor->isDragging());
	m_editor->move(6, 4, Qt::LeftButton);
	m_editor->release(6, 4, Qt::LeftButton, Qt::NoButton);
	QCOMPARE(m_doc->floatingPos(), QPoint(5, 3));
	QCOMPARE(m_editor->selectionArea(), QRect(5, 3, 2, 2));
	QCOMPARE(terrainAt(*m_doc, 5, 3), Grass);
	// a click outside puts it down: one undo step
	m_editor->press(0, 5, Qt::LeftButton);
	m_editor->release(0, 5, Qt::LeftButton, Qt::NoButton);
	QVERIFY(!m_doc->isFloating());
	QVERIFY(m_editor->selectionArea().isEmpty());
	QCOMPARE(terrainAt(*m_doc, 5, 3), Dirt);
	QCOMPARE(terrainAt(*m_doc, 6, 4), Dirt);
	QCOMPARE(m_doc->undoStack()->count(), steps + 1);

	// Escape drops it
	m_editor->pasteArea();
	QVERIFY(m_doc->isFloating());
	QVERIFY(m_editor->handleKey(Qt::Key_Escape, Qt::NoModifier));
	QVERIFY(!m_doc->isFloating());
	QVERIFY(m_editor->selectionArea().isEmpty());
	QCOMPARE(m_doc->undoStack()->count(), steps + 1);
	// a save (or another layer) puts it down as a click would
	m_editor->pasteArea();
	emit m_doc->aboutToSave();
	QVERIFY(!m_doc->isFloating());
	QCOMPARE(m_doc->undoStack()->count(), steps + 2);
	// elsewhere: the same tiles on the same cells would change nothing
	m_editor->setViewCells(QRectF(0, 0, 4, 4));
	m_editor->pasteArea();
	QCOMPARE(m_doc->floatingPos(), QPoint(1, 1));
	m_state->setLayer(EditorState::UPPER);
	QVERIFY(!m_doc->isFloating());
	QCOMPARE(m_doc->undoStack()->count(), steps + 3);
}

void MapEditorTest::eventSelectAndDrag()
{
	m_state->setLayer(EditorState::EVENT);
	QVERIFY(m_doc->commitEvent(m_doc->newEvent(2, 2)));
	QVERIFY(m_doc->commitEvent(m_doc->newEvent(4, 2)));
	QCOMPARE(m_editor->selectedCell(), QPoint(-1, -1));
	QCOMPARE(m_editor->markedCell(), QPoint(-1, -1));

	// a click selects the cell; its event follows the pointer
	m_editor->press(2, 2, Qt::LeftButton);
	QCOMPARE(m_editor->selectedCell(), QPoint(2, 2));
	QCOMPARE(m_editor->markedCell(), QPoint(2, 2));
	QVERIFY(m_editor->isDragging());
	QCOMPARE(m_editor->cursor(), MapEditor::MoveCursor);
	m_editor->move(3, 3, Qt::LeftButton);
	QCOMPARE(m_editor->markedCell(), QPoint(3, 3));
	QCOMPARE(m_editor->cursor(), MapEditor::MoveCursor);
	// onto another event: not allowed, it stays
	m_editor->move(4, 2, Qt::LeftButton);
	QCOMPARE(m_editor->cursor(), MapEditor::ForbiddenCursor);
	m_editor->release(4, 2, Qt::LeftButton, Qt::NoButton);
	QCOMPARE(m_doc->eventIdAt(2, 2), 1);
	QCOMPARE(m_editor->selectedCell(), QPoint(2, 2));
	QCOMPARE(m_editor->markedCell(), QPoint(2, 2));
	QCOMPARE(m_doc->undoStack()->count(), 2);

	// onto a free cell: one undo step, the cell selected there
	m_editor->press(2, 2, Qt::LeftButton);
	m_editor->move(3, 3, Qt::LeftButton);
	m_editor->release(3, 3, Qt::LeftButton, Qt::NoButton);
	QCOMPARE(m_doc->eventIdAt(3, 3), 1);
	QCOMPARE(m_doc->eventIdAt(2, 2), 0);
	QCOMPARE(m_editor->selectedCell(), QPoint(3, 3));
	QCOMPARE(m_editor->cursor(), MapEditor::ArrowCursor);
	QVERIFY(!m_editor->isDragging());
	QCOMPARE(m_doc->undoStack()->count(), 3);

	// out of the map: it stays
	m_editor->press(3, 3, Qt::LeftButton);
	m_editor->move(-1, -1, Qt::LeftButton);
	m_editor->release(-1, -1, Qt::LeftButton, Qt::NoButton);
	QCOMPARE(m_doc->eventIdAt(3, 3), 1);
	QCOMPARE(m_editor->selectedCell(), QPoint(3, 3));

	// a double click opens the event, or a new one on a free cell
	QSignalSpy edit(m_editor.get(), &MapEditor::editEventRequested);
	QSignalSpy created(m_editor.get(), &MapEditor::newEventRequested);
	m_editor->doubleClick(4, 2);
	QCOMPARE(edit.count(), 1);
	QCOMPARE(edit.first().at(0).toInt(), 2);
	m_editor->doubleClick(0, 0);
	QCOMPARE(created.count(), 1);
	QCOMPARE(created.first().at(0).toPoint(), QPoint(0, 0));
	QCOMPARE(m_editor->selectedCell(), QPoint(0, 0));
	// another layer ends the selection of the cell
	m_state->setLayer(EditorState::LOWER);
	QCOMPARE(m_editor->selectedCell(), QPoint(-1, -1));
}

void MapEditorTest::startPositionDrag()
{
	m_state->setLayer(EditorState::EVENT);
	m_doc->setStartPosition(Party, 1, 1);
	QVERIFY(m_doc->commitEvent(m_doc->newEvent(1, 3)));
	// a start position hides the event of its cell, and is dragged as one
	m_editor->press(1, 1, Qt::LeftButton);
	m_editor->move(1, 3, Qt::LeftButton);
	QCOMPARE(m_editor->cursor(), MapEditor::ForbiddenCursor);
	m_editor->release(1, 3, Qt::LeftButton, Qt::NoButton);
	QCOMPARE(m_doc->startAt(1, 1), Party);
	m_editor->press(1, 1, Qt::LeftButton);
	m_editor->move(2, 2, Qt::LeftButton);
	m_editor->release(2, 2, Qt::LeftButton, Qt::NoButton);
	QCOMPARE(m_doc->startAt(2, 2), Party);
	QCOMPARE(m_doc->startAt(1, 1), -1);
	QCOMPARE(m_editor->selectedCell(), QPoint(2, 2));
	// not an event: Enter and a double click do not open it
	QSignalSpy created(m_editor.get(), &MapEditor::newEventRequested);
	QSignalSpy edit(m_editor.get(), &MapEditor::editEventRequested);
	QVERIFY(!m_editor->handleKey(Qt::Key_Return, Qt::NoModifier));
	m_editor->doubleClick(2, 2);
	QCOMPARE(created.count() + edit.count(), 0);
}

void MapEditorTest::itemsMoveValidity()
{
	m_state->setLayer(EditorState::EVENT);
	m_state->setTool(EditorState::SELECT);
	QVERIFY(m_doc->commitEvent(m_doc->newEvent(1, 1)));
	QVERIFY(m_doc->commitEvent(m_doc->newEvent(2, 1)));
	QVERIFY(m_doc->commitEvent(m_doc->newEvent(3, 2)));
	m_doc->setStartPosition(Party, 1, 2);
	const int steps = m_doc->undoStack()->count();

	// a rectangle of cells selects the events and the start positions in it
	m_editor->press(1, 1, Qt::LeftButton);
	m_editor->move(2, 2, Qt::LeftButton);
	m_editor->release(2, 2, Qt::LeftButton, Qt::NoButton);
	QCOMPARE(m_editor->selectedEvents(), (QList<int>{1, 2}));
	QCOMPARE(m_editor->selectedStarts(), (QList<int>{Party}));
	QCOMPARE(m_editor->selectionArea(), QRect(1, 1, 2, 2));
	QCOMPARE(m_editor->itemCells().size(), 3);
	// the items have their own frames
	QCOMPARE(m_editor->markedCell(), QPoint(-1, -1));

	// dragged by one of them, onto an event left there: not allowed
	QSignalSpy offsets(m_editor.get(), &MapEditor::itemsOffsetChanged);
	m_editor->press(1, 1, Qt::LeftButton);
	QVERIFY(m_editor->isDragging());
	m_editor->move(2, 2, Qt::LeftButton);
	QCOMPARE(m_editor->itemsOffset(), QPoint(1, 1));
	QCOMPARE(m_editor->selectionRect(), QRect(2, 2, 2, 2));
	QVERIFY(m_editor->itemCells().contains(QPoint(3, 2)));
	QVERIFY(!m_editor->canMoveItems(QPoint(1, 1)));
	QCOMPARE(m_editor->cursor(), MapEditor::ForbiddenCursor);
	m_editor->release(2, 2, Qt::LeftButton, Qt::NoButton);
	QCOMPARE(m_editor->itemsOffset(), QPoint());
	QCOMPARE(offsets.count(), 2);
	QCOMPARE(m_doc->eventIdAt(1, 1), 1);
	QCOMPARE(m_editor->selectionArea(), QRect(1, 1, 2, 2));
	QCOMPARE(m_doc->undoStack()->count(), steps);

	// onto cells they leave or free ones: one undo step
	m_editor->press(1, 1, Qt::LeftButton);
	m_editor->move(2, 1, Qt::LeftButton);
	QVERIFY(m_editor->canMoveItems(QPoint(1, 0)));
	QCOMPARE(m_editor->cursor(), MapEditor::MoveCursor);
	m_editor->release(2, 1, Qt::LeftButton, Qt::NoButton);
	QCOMPARE(m_doc->eventIdAt(2, 1), 1);
	QCOMPARE(m_doc->eventIdAt(3, 1), 2);
	QCOMPARE(m_doc->startAt(2, 2), Party);
	QCOMPARE(m_editor->selectionArea(), QRect(2, 1, 2, 2));
	QCOMPARE(m_editor->selectedCell(), QPoint(2, 1));
	QCOMPARE(m_doc->undoStack()->count(), steps + 1);
	// out of the map: nothing moves
	m_editor->press(2, 1, Qt::LeftButton);
	m_editor->move(2, 0, Qt::LeftButton);
	m_editor->move(2, -1, Qt::LeftButton);
	m_editor->release(2, -1, Qt::LeftButton, Qt::NoButton);
	QCOMPARE(m_doc->eventIdAt(2, 1), 1);
	QCOMPARE(m_doc->undoStack()->count(), steps + 1);

	// Delete: the events go, the start position stays selected
	QVERIFY(m_editor->handleKey(Qt::Key_Delete, Qt::NoModifier, false));
	QCOMPARE(m_doc->eventIdAt(2, 1), 1);
	QVERIFY(m_editor->handleKey(Qt::Key_Backspace, Qt::NoModifier));
	QCOMPARE(m_doc->eventIdAt(2, 1), 0);
	QCOMPARE(m_doc->eventIdAt(3, 1), 0);
	QVERIFY(m_editor->selectedEvents().isEmpty());
	QCOMPARE(m_editor->selectedStarts(), (QList<int>{Party}));
	QVERIFY(!m_editor->selectionArea().isEmpty());
	m_editor->undo();
	QCOMPARE(m_doc->eventIdAt(3, 1), 2);
	QVERIFY(m_editor->selectionArea().isEmpty());

	// copied, then pasted on the cell under the pointer, selected
	QVERIFY(key(QKeySequence::SelectAll));
	QCOMPARE(m_editor->selectedEvents().size(), 3);
	QVERIFY(key(QKeySequence::Copy));
	m_editor->move(0, 3, Qt::NoButton);
	QVERIFY(key(QKeySequence::Paste));
	const QList<int> pasted = m_editor->selectedEvents();
	QCOMPARE(pasted.size(), 3);
	for (const QPoint& cell : {QPoint(2, 4), QPoint(3, 4), QPoint(3, 5)}) {
		QVERIFY(m_doc->eventIdAt(cell.x(), cell.y()) > 3);
		QVERIFY(pasted.contains(m_doc->eventIdAt(cell.x(), cell.y())));
	}
	QCOMPARE(m_editor->selectionArea(), QRect(2, 4, 2, 2));
	// the arrows do not move the cell with the selection tool
	QVERIFY(!m_editor->handleKey(Qt::Key_Left, Qt::NoModifier));
	// Escape drops the selection
	QVERIFY(m_editor->handleKey(Qt::Key_Escape, Qt::NoModifier));
	QVERIFY(m_editor->selectedEvents().isEmpty());
}

void MapEditorTest::keys()
{
	m_state->setLayer(EditorState::EVENT);
	QSignalSpy reveal(m_editor.get(), &MapEditor::revealRequested);
	// no cell selected and no view: nothing to start from
	QVERIFY(m_editor->handleKey(Qt::Key_Right, Qt::NoModifier));
	QCOMPARE(m_editor->selectedCell(), QPoint(-1, -1));
	// the arrows start at the top left cell shown
	m_editor->setViewCells(QRectF(1.5, 0.25, 4, 3));
	QVERIFY(m_editor->handleKey(Qt::Key_Right, Qt::NoModifier, false));
	QCOMPARE(m_editor->selectedCell(), QPoint(-1, -1));
	QVERIFY(m_editor->handleKey(Qt::Key_Right, Qt::NoModifier));
	QCOMPARE(m_editor->selectedCell(), QPoint(2, 1));
	QCOMPARE(reveal.count(), 1);
	QCOMPARE(reveal.first().at(0).toPoint(), QPoint(2, 1));
	// then move it, inside the map
	QVERIFY(m_editor->handleKey(Qt::Key_Left, Qt::NoModifier));
	QVERIFY(m_editor->handleKey(Qt::Key_Left, Qt::NoModifier));
	QVERIFY(m_editor->handleKey(Qt::Key_Left, Qt::NoModifier));
	QCOMPARE(m_editor->selectedCell(), QPoint(0, 1));
	QVERIFY(m_editor->handleKey(Qt::Key_Up, Qt::KeypadModifier));
	QCOMPARE(m_editor->selectedCell(), QPoint(0, 0));
	QVERIFY(!m_editor->handleKey(Qt::Key_Up, Qt::ShiftModifier));

	// Enter: a new event on a free cell, an event's dialog on its cell
	QSignalSpy created(m_editor.get(), &MapEditor::newEventRequested);
	QSignalSpy edit(m_editor.get(), &MapEditor::editEventRequested);
	QVERIFY(m_editor->handleKey(Qt::Key_Return, Qt::NoModifier));
	QCOMPARE(created.count(), 1);
	QCOMPARE(created.first().at(0).toPoint(), QPoint(0, 0));
	QVERIFY(m_doc->commitEvent(m_doc->newEvent(0, 0)));
	QVERIFY(m_editor->handleKey(Qt::Key_Enter, Qt::KeypadModifier));
	QCOMPARE(edit.count(), 1);
	QCOMPARE(edit.first().at(0).toInt(), 1);

	// Copy, Delete (tested only first, as for a shortcut override), Paste
	QVERIFY(!key(QKeySequence::Paste, false));
	QVERIFY(key(QKeySequence::Copy));
	QVERIFY(m_state->eventClipboard());
	QVERIFY(m_editor->handleKey(Qt::Key_Delete, Qt::NoModifier, false));
	QCOMPARE(m_doc->eventIdAt(0, 0), 1);
	QVERIFY(m_editor->handleKey(Qt::Key_Delete, Qt::NoModifier));
	QCOMPARE(m_doc->eventIdAt(0, 0), 0);
	QVERIFY(!key(QKeySequence::Copy));
	QVERIFY(key(QKeySequence::Paste));
	QVERIFY(m_doc->eventIdAt(0, 0) > 0);
	// Cut: copied and deleted
	QVERIFY(key(QKeySequence::Cut));
	QCOMPARE(m_doc->eventIdAt(0, 0), 0);
	// the other keys are the window's
	QVERIFY(!m_editor->handleKey(Qt::Key_A, Qt::NoModifier));
	QVERIFY(!m_editor->handleKey(Qt::Key_Escape, Qt::NoModifier));

	// the tile layers take the keys of the selection tool only
	m_state->setLayer(EditorState::LOWER);
	QVERIFY(!m_editor->handleKey(Qt::Key_Right, Qt::NoModifier));
	QVERIFY(!key(QKeySequence::SelectAll));
	m_state->setTool(EditorState::SELECT);
	QVERIFY(!key(QKeySequence::Copy));
	QVERIFY(key(QKeySequence::SelectAll));
	QVERIFY(key(QKeySequence::Copy, false));
}

void MapEditorTest::actionStates()
{
	m_state->setLayer(EditorState::EVENT);
	QVERIFY(m_doc->commitEvent(m_doc->newEvent(2, 2)));
	m_doc->setStartPosition(Party, 4, 4);
	QSignalSpy menus(m_editor.get(), &MapEditor::contextMenuRequested);

	// an empty cell: New, and Paste with an event copied
	m_editor->press(1, 1, Qt::RightButton);
	QCOMPARE(menus.count(), 1);
	QCOMPARE(menus.first().at(0).value<MapEditor::Menu>(), MapEditor::EventMenu);
	QCOMPARE(menus.first().at(1).toPoint(), QPoint(1, 1));
	QCOMPARE(m_editor->selectedCell(), QPoint(1, 1));
	QVariantMap states = m_editor->actionStates(MapEditor::EventMenu);
	QVERIFY(enabled(states, "actionEventNew") && visible(states, "actionEventNew"));
	QVERIFY(!visible(states, "actionEventEdit"));
	QVERIFY(!enabled(states, "actionEventCopy"));
	QVERIFY(!enabled(states, "actionEventCut"));
	QVERIFY(!enabled(states, "actionEventDelete"));
	QVERIFY(!enabled(states, "actionEventPaste"));
	QVERIFY(enabled(states, "actionPlaceTeleport"));
	QVERIFY(enabled(states, "actionPlacePartyStart"));
	QVERIFY(enabled(states, "actionPlaceAirshipStart"));
	QVERIFY(enabled(states, "actionRunHere"));

	// an event: Edit, Copy, Cut, Delete; nothing placed on it
	m_editor->press(2, 2, Qt::RightButton);
	states = m_editor->actionStates(MapEditor::EventMenu);
	QVERIFY(!visible(states, "actionEventNew"));
	QVERIFY(enabled(states, "actionEventEdit") && visible(states, "actionEventEdit"));
	QVERIFY(enabled(states, "actionEventCopy"));
	QVERIFY(enabled(states, "actionEventCut"));
	QVERIFY(enabled(states, "actionEventDelete"));
	QVERIFY(!enabled(states, "actionPlaceTeleport"));
	QVERIFY(!enabled(states, "actionPlaceBoatStart"));
	m_editor->copyEvent();
	m_editor->press(1, 1, Qt::RightButton);
	QVERIFY(enabled(m_editor->actionStates(MapEditor::EventMenu), "actionEventPaste"));
	// a start position: the event it hides is not edited
	m_editor->press(4, 4, Qt::RightButton);
	states = m_editor->actionStates(MapEditor::EventMenu);
	QVERIFY(!enabled(states, "actionEventEdit"));
	QVERIFY(!enabled(states, "actionEventNew"));
	QVERIFY(!enabled(states, "actionEventPaste"));

	// the actions without a dialog are done here, the others asked for
	m_editor->press(1, 1, Qt::RightButton);
	m_editor->placeStart(Boat);
	QCOMPARE(m_doc->startAt(1, 1), Boat);
	m_editor->press(3, 3, Qt::RightButton);
	m_editor->pasteEvent();
	QVERIFY(m_doc->eventIdAt(3, 3) > 0);
	QSignalSpy teleport(m_editor.get(), &MapEditor::placeTeleportRequested);
	QSignalSpy run(m_editor.get(), &MapEditor::runHereRequested);
	m_editor->placeTeleport();
	QCOMPARE(teleport.count(), 0);
	m_editor->press(5, 1, Qt::RightButton);
	m_editor->placeTeleport();
	m_editor->runHere();
	QCOMPARE(teleport.count(), 1);
	QCOMPARE(teleport.first().at(0).toPoint(), QPoint(5, 1));
	QCOMPARE(run.first().at(0).toPoint(), QPoint(5, 1));
	// out of the map: no menu
	menus.clear();
	m_editor->press(-1, 0, Qt::RightButton);
	QCOMPARE(menus.count(), 0);

	// the selection tool's menu on the tiles: Paste with tiles copied
	m_state->setLayer(EditorState::LOWER);
	m_state->setTool(EditorState::SELECT);
	m_editor->press(1, 1, Qt::RightButton);
	QCOMPARE(menus.count(), 1);
	QCOMPARE(menus.last().at(0).value<MapEditor::Menu>(), MapEditor::AreaMenu);
	states = m_editor->actionStates(MapEditor::AreaMenu);
	QVERIFY(!enabled(states, "actionAreaCut"));
	QVERIFY(!enabled(states, "actionAreaCopy"));
	QVERIFY(!enabled(states, "actionAreaClear"));
	QVERIFY(!enabled(states, "actionAreaPaste"));
	QVERIFY(enabled(states, "actionAreaSelectAll"));
	m_editor->selectAll();
	m_editor->copyArea();
	states = m_editor->actionStates(MapEditor::AreaMenu);
	QVERIFY(enabled(states, "actionAreaCut"));
	QVERIFY(enabled(states, "actionAreaClear"));
	QVERIFY(enabled(states, "actionAreaPaste"));
	// and on the events, the events selected
	m_state->setLayer(EditorState::EVENT);
	m_editor->press(2, 2, Qt::RightButton);
	QCOMPARE(menus.last().at(0).value<MapEditor::Menu>(), MapEditor::AreaMenu);
	QCOMPARE(m_editor->selectedCell(), QPoint(2, 2));
	QVERIFY(!enabled(m_editor->actionStates(MapEditor::AreaMenu), "actionAreaCopy"));
	m_editor->selectAll();
	QVERIFY(enabled(m_editor->actionStates(MapEditor::AreaMenu), "actionAreaCopy"));

	// the right button of the other tools: no menu
	m_state->setLayer(EditorState::LOWER);
	m_state->setTool(EditorState::PENCIL);
	menus.clear();
	m_editor->press(1, 1, Qt::RightButton);
	QCOMPARE(menus.count(), 0);
}

void MapEditorTest::zoomSteps()
{
	QSignalSpy scales(m_editor.get(), &MapEditor::scaleChanged);
	QCOMPARE(m_editor->scale(), 0.0);
	// each pixel of the game on a power of two of screen pixels
	m_editor->setScale(3);
	QCOMPARE(m_editor->scale(), 4.0);
	m_editor->setScale(0.7);
	QCOMPARE(m_editor->scale(), 0.5);
	QCOMPARE(scales.count(), 2);
	m_editor->setScale(0.6);
	QCOMPARE(scales.count(), 2);
	QCOMPARE(MapEditor::snappedScale(1, 1.5), 2 / 1.5);
	QCOMPARE(MapEditor::snappedScale(100, 1), 16.0);
	m_editor->setDevicePixelRatio(2);
	QCOMPARE(m_editor->snapScale(0.6), 0.5);
	QCOMPARE(m_editor->snapScale(1.5), 2.0);
	m_editor->setDevicePixelRatio(1);

	// the zoom tool: the left button in, the right one out, to the limits
	m_editor->setScale(4);
	m_state->setTool(EditorState::ZOOM);
	QCOMPARE(m_editor->cursor(), MapEditor::ZoomCursor);
	m_editor->press(1, 1, Qt::LeftButton);
	QCOMPARE(m_editor->scale(), 8.0);
	m_editor->press(1, 1, Qt::LeftButton);
	QCOMPARE(m_editor->scale(), 8.0);
	m_editor->press(1, 1, Qt::RightButton);
	QCOMPARE(m_editor->scale(), 4.0);
	// on the event layer, a click selects the cell
	m_state->setLayer(EditorState::EVENT);
	m_editor->press(1, 1, Qt::LeftButton);
	QCOMPARE(m_editor->scale(), 4.0);
	QCOMPARE(m_editor->selectedCell(), QPoint(1, 1));

	// Ctrl + wheel
	QVERIFY(!m_editor->wheel(120, Qt::NoModifier));
	QVERIFY(m_editor->wheel(120, Qt::ControlModifier));
	QCOMPARE(m_editor->scale(), 8.0);
	QVERIFY(m_editor->wheel(-120, Qt::ControlModifier));
	QVERIFY(m_editor->wheel(-120, Qt::ControlModifier));
	QCOMPARE(m_editor->scale(), 2.0);

	// a pinch: a step each time the fingers spread by about 1.4
	m_editor->resetPinch();
	m_editor->pinch(1.2);
	QCOMPARE(m_editor->scale(), 2.0);
	m_editor->pinch(1.2);
	QCOMPARE(m_editor->scale(), 4.0);
	m_editor->resetPinch();
	m_editor->pinch(0.8);
	QCOMPARE(m_editor->scale(), 4.0);
	m_editor->pinch(0.8);
	QCOMPARE(m_editor->scale(), 2.0);
	// at a limit, the fingers going on do not pile up
	m_editor->setScale(MapEditor::MaxScale);
	m_editor->resetPinch();
	for (int i = 0; i < 10; i++)
		m_editor->pinch(1.5);
	QCOMPARE(m_editor->scale(), MapEditor::MaxScale);
	m_editor->pinch(0.45);
	QCOMPARE(m_editor->scale(), 4.0);
}

void MapEditorTest::hoverText()
{
	m_state->setLayer(EditorState::EVENT);
	QVERIFY(m_doc->commitEvent(m_doc->newEvent(2, 2)));
	QSignalSpy hovered(m_editor.get(), &MapEditor::cellHovered);
	m_editor->move(1, 1, Qt::NoButton);
	QCOMPARE(m_editor->hoverText(), QStringLiteral("(1, 1)"));
	m_editor->move(2, 2, Qt::NoButton);
	QCOMPARE(hovered.count(), 2);
	QCOMPARE(hovered.last().at(0).toString(), QStringLiteral("(2, 2) - Event 1: EV0001"));
	// the same cell: nothing new
	m_editor->move(2, 2, Qt::NoButton);
	QCOMPARE(hovered.count(), 2);
	// out of the map: nothing
	m_editor->move(Width, 2, Qt::NoButton);
	QCOMPARE(hovered.count(), 2);
	// the events only on the event layer
	m_state->setLayer(EditorState::LOWER);
	m_editor->move(2, 2, Qt::NoButton);
	QCOMPARE(m_editor->hoverText(), QStringLiteral("(2, 2)"));
}

QTEST_MAIN(MapEditorTest)
#include "map_editor_test.moc"
