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

// The editing of a map without a view or a project: drawing with the
// autotiles, undo and redo, events, start positions and the selection
// tool, on a small map in memory. Reading and saving the file need the
// project; the script tests (tests/scripts/map_tools.txt) draw through the
// map view.

#include <QSignalSpy>
#include <QTest>
#include <QUndoStack>
#include "defines.h"
#include "common/dbstring.h"
#include "model/autotile.h"
#include "model/map_document.h"
#include "model/project_data.h"

namespace {
// terrains of the palette: water, two ground autotiles (D block), a wall (E block)
constexpr int Water = 0;
constexpr int Grass = 6;
constexpr int Dirt = 7;
constexpr int Wall = 18;
constexpr int Width = 8;
constexpr int Height = 6;
constexpr int MapId = 1;
// TilePainter::StartMarker
constexpr int Party = 0;
constexpr int Boat = 1;

MapDocument::Brush brush(int terrain)
{
	// as the palette picks it
	return {{Autotile::translate(terrain, SAMPLE)}, 1, 1};
}

int terrainAt(const MapDocument& doc, int x, int y)
{
	return Autotile::translate(static_cast<short>(doc.tileAt(EditorState::LOWER, x, y)));
}
}

class MapDocumentTest : public QObject
{
	Q_OBJECT

private slots:
	void init();
	void penBindsAutotiles();
	void rectangleUndoRedo();
	void ellipse();
	void fillStopsAtOtherTerrains();
	void cancelledStroke();
	void eventsCreateMoveDelete();
	void selectedItemsMove();
	void areaCopyPaste();
	void areaMove();
	void startPositions();
	void shiftMap();
	void mapProperties();
	void treeChangedByAnotherProgram();

private:
	ProjectData m_project;
	std::unique_ptr<MapDocument> m_doc;
};

void MapDocumentTest::init()
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
	m_doc = std::make_unique<MapDocument>(m_project, MapId);
	m_doc->setMap(std::move(map));
}

void MapDocumentTest::penBindsAutotiles()
{
	QSignalSpy cells(m_doc.get(), &MapDocument::cellsChanged);
	QSignalSpy edited(m_doc.get(), &MapDocument::edited);
	m_doc->drawPen(EditorState::LOWER, QPoint(3, 2), QPoint(3, 2), brush(Dirt));
	QCOMPARE(cells.count(), 1);
	QCOMPARE(cells.first().at(1).toInt(), static_cast<int>(EditorState::LOWER));

	// drawn in the working layer only, with the borders against the grass
	const short dirt = m_doc->workingTile(EditorState::LOWER, 3, 2);
	QCOMPARE(Autotile::translate(dirt), Dirt);
	QVERIFY(dirt != Autotile::translate(Dirt, SAMPLE));
	QVERIFY(dirt != Autotile::translate(Dirt));
	QCOMPARE(dirt, Autotile::bind(m_doc->workingLayer(EditorState::LOWER), Width, Height, 3, 2));
	// the grass around takes its border too
	const short grass = m_doc->workingTile(EditorState::LOWER, 2, 2);
	QCOMPARE(Autotile::translate(grass), Grass);
	QVERIFY(grass != Autotile::translate(Grass));
	QCOMPARE(m_doc->tileAt(EditorState::LOWER, 3, 2), static_cast<int>(Autotile::translate(Grass)));
	QVERIFY(!m_doc->isModified());

	// a second cell of the stroke, then the stroke is one undo step
	m_doc->drawPen(EditorState::LOWER, QPoint(4, 2), QPoint(3, 2), brush(Dirt));
	m_doc->commitStroke();
	QCOMPARE(terrainAt(*m_doc, 3, 2), Dirt);
	QCOMPARE(terrainAt(*m_doc, 4, 2), Dirt);
	QCOMPARE(m_doc->layerData(EditorState::LOWER), m_doc->workingLayer(EditorState::LOWER));
	QCOMPARE(m_doc->undoStack()->count(), 1);
	QCOMPARE(edited.count(), 1);
	QVERIFY(m_doc->isModified());

	// a brush of 2 x 2 water tiles stops at the edges of the map
	MapDocument::Brush water = {{Autotile::translate(Water, SAMPLE), Autotile::translate(Water, SAMPLE),
		Autotile::translate(Water, SAMPLE), Autotile::translate(Water, SAMPLE)}, 2, 2};
	m_doc->drawPen(EditorState::LOWER, QPoint(Width - 1, Height - 1), QPoint(Width - 1, Height - 1), water);
	m_doc->commitStroke();
	QCOMPARE(terrainAt(*m_doc, Width - 1, Height - 1), Water);
	QCOMPARE(terrainAt(*m_doc, Width - 2, Height - 1), Grass);
}

void MapDocumentTest::rectangleUndoRedo()
{
	const std::vector<short> before = m_doc->layerData(EditorState::LOWER);
	// the rectangle follows the mouse: the last one stays
	m_doc->drawRect(EditorState::LOWER, QPoint(1, 1), QPoint(5, 4), brush(Dirt));
	m_doc->drawRect(EditorState::LOWER, QPoint(1, 1), QPoint(3, 3), brush(Dirt));
	QCOMPARE(Autotile::translate(m_doc->workingTile(EditorState::LOWER, 5, 4)), Grass);
	m_doc->commitStroke();
	// inside, no border; at the edges, borders
	QCOMPARE(m_doc->tileAt(EditorState::LOWER, 2, 2), static_cast<int>(Autotile::translate(Dirt)));
	QCOMPARE(terrainAt(*m_doc, 1, 1), Dirt);
	QVERIFY(m_doc->tileAt(EditorState::LOWER, 1, 1) != Autotile::translate(Dirt));
	QCOMPARE(terrainAt(*m_doc, 4, 4), Grass);
	const std::vector<short> drawn = m_doc->layerData(EditorState::LOWER);

	QSignalSpy cells(m_doc.get(), &MapDocument::cellsChanged);
	QSignalSpy modified(m_doc.get(), &MapDocument::modifiedChanged);
	QVERIFY(m_doc->canUndo());
	m_doc->undo();
	QCOMPARE(m_doc->layerData(EditorState::LOWER), before);
	QCOMPARE(m_doc->workingLayer(EditorState::LOWER), before);
	QCOMPARE(cells.count(), 1);
	QVERIFY(!m_doc->isModified());
	QCOMPARE(modified.count(), 1);
	QCOMPARE(modified.first().at(0).toBool(), false);
	QVERIFY(m_doc->canRedo());
	m_doc->redo();
	QCOMPARE(m_doc->layerData(EditorState::LOWER), drawn);
	QCOMPARE(m_doc->workingLayer(EditorState::LOWER), drawn);
	QVERIFY(m_doc->isModified());
}

void MapDocumentTest::ellipse()
{
	// in a 5 x 5 square: the corners stay out
	m_doc->drawEllipse(EditorState::LOWER, QPoint(1, 0), QPoint(5, 4), brush(Water));
	m_doc->commitStroke();
	QCOMPARE(terrainAt(*m_doc, 3, 2), Water);
	QCOMPARE(terrainAt(*m_doc, 1, 2), Water);
	QCOMPARE(terrainAt(*m_doc, 3, 0), Water);
	QCOMPARE(terrainAt(*m_doc, 1, 0), Grass);
	QCOMPARE(terrainAt(*m_doc, 5, 4), Grass);
}

void MapDocumentTest::fillStopsAtOtherTerrains()
{
	// a wall from top to bottom: the fill stays on its side
	for (int y = 0; y < Height; y++)
		m_doc->drawPen(EditorState::LOWER, QPoint(4, y), QPoint(4, 0), brush(Wall));
	m_doc->commitStroke();
	QCOMPARE(m_doc->tileAt(EditorState::LOWER, 4, 3), static_cast<int>(Autotile::translate(Wall, SAMPLE)));

	m_doc->drawFill(EditorState::LOWER, QPoint(1, 1), brush(Water));
	m_doc->commitStroke();
	for (int y = 0; y < Height; y++) {
		QCOMPARE(terrainAt(*m_doc, 0, y), Water);
		QCOMPARE(terrainAt(*m_doc, 3, y), Water);
		QCOMPARE(terrainAt(*m_doc, 4, y), Wall);
		QCOMPARE(terrainAt(*m_doc, 5, y), Grass);
	}
	// the water inside has no shore
	QCOMPARE(m_doc->tileAt(EditorState::LOWER, 1, 2), static_cast<int>(Autotile::translate(Water)));

	// with the tile already there: nothing
	m_doc->drawFill(EditorState::LOWER, QPoint(6, 1), brush(Grass));
	m_doc->commitStroke();
	QCOMPARE(m_doc->undoStack()->count(), 2);

	// the upper layer: the same tile
	m_doc->drawFill(EditorState::UPPER, QPoint(0, 0), {{10005}, 1, 1});
	m_doc->commitStroke();
	QCOMPARE(m_doc->tileAt(EditorState::UPPER, 7, 5), 10005);
	QCOMPARE(m_doc->undoStack()->count(), 3);
}

void MapDocumentTest::cancelledStroke()
{
	QSignalSpy cells(m_doc.get(), &MapDocument::cellsChanged);
	m_doc->drawPen(EditorState::LOWER, QPoint(2, 2), QPoint(2, 2), brush(Dirt));
	m_doc->cancelStroke();
	QCOMPARE(m_doc->workingLayer(EditorState::LOWER), m_doc->layerData(EditorState::LOWER));
	// the pen, then both layers drawn again
	QCOMPARE(cells.count(), 3);
	m_doc->commitStroke();
	QVERIFY(!m_doc->canUndo());
	QVERIFY(!m_doc->isModified());
}

void MapDocumentTest::eventsCreateMoveDelete()
{
	QSignalSpy events(m_doc.get(), &MapDocument::eventsChanged);
	lcf::rpg::Event event = m_doc->newEvent(2, 2);
	QCOMPARE(event.ID, 1);
	QCOMPARE(ToQString(event.name), QStringLiteral("EV0001"));
	QCOMPARE(static_cast<int>(event.pages.size()), 1);
	QCOMPARE(m_doc->eventIdAt(2, 2), 0);
	QVERIFY(m_doc->commitEvent(event));
	QCOMPARE(m_doc->eventIdAt(2, 2), 1);
	QVERIFY(!m_doc->isFreeCell(2, 2));
	QCOMPARE(m_doc->firstFreeEventId(), 2);
	// the same again: nothing to undo
	QVERIFY(!m_doc->commitEvent(event));
	QCOMPARE(m_doc->undoStack()->count(), 1);

	QVERIFY(m_doc->moveEvent(1, 5, 3));
	QCOMPARE(m_doc->eventIdAt(2, 2), 0);
	QCOMPARE(m_doc->eventIdAt(5, 3), 1);
	// a copy goes on a free cell only, with the next id
	QVERIFY(!m_doc->pasteEvent(*m_doc->findEvent(1), 5, 3));
	QVERIFY(m_doc->pasteEvent(*m_doc->findEvent(1), 6, 4));
	QCOMPARE(m_doc->eventIdAt(6, 4), 2);
	QCOMPARE(m_doc->findEvent(2)->name, m_doc->findEvent(1)->name);
	QVERIFY(m_doc->deleteEventAt(5, 3));
	QVERIFY(!m_doc->deleteEventAt(5, 3));
	QVERIFY(!m_doc->findEvent(1));
	QCOMPARE(events.count(), 4);
	QCOMPARE(m_doc->undoStack()->count(), 4);

	// back step by step
	m_doc->undo();
	QCOMPARE(m_doc->eventIdAt(5, 3), 1);
	m_doc->undo();
	QVERIFY(!m_doc->findEvent(2));
	m_doc->undo();
	QCOMPARE(m_doc->eventIdAt(2, 2), 1);
	m_doc->undo();
	QVERIFY(m_doc->map().events.empty());
	for (int i = 0; i < 4; i++)
		m_doc->redo();
	QCOMPARE(m_doc->eventIdAt(6, 4), 2);
	QVERIFY(!m_doc->findEvent(1));

	// a teleport: touched by the hero, Hide Screen, Teleport, Show Screen
	m_doc->placeTeleport(0, 0, 3, 4, 5, 6);
	const lcf::rpg::Event* teleport = m_doc->findEvent(m_doc->eventIdAt(0, 0));
	QVERIFY(teleport);
	QCOMPARE(teleport->pages.front().trigger, static_cast<int32_t>(lcf::rpg::EventPage::Trigger_touched));
	QCOMPARE(static_cast<int>(teleport->pages.front().event_commands.size()), 3);
	const auto& params = teleport->pages.front().event_commands[1].parameters;
	QCOMPARE(std::vector<int32_t>(params.begin(), params.end()), (std::vector<int32_t>{3, 4, 5, 6}));
}

void MapDocumentTest::selectedItemsMove()
{
	QVERIFY(m_doc->commitEvent(m_doc->newEvent(1, 1)));
	QVERIFY(m_doc->commitEvent(m_doc->newEvent(2, 1)));
	m_doc->setStartPosition(Party, 1, 2);
	const QRect cells(1, 1, 2, 2);
	const QList<int> events = m_doc->eventsIn(cells);
	const QList<int> starts = m_doc->startsIn(cells);
	QCOMPARE(events, (QList<int>{1, 2}));
	QCOMPARE(starts, (QList<int>{Party}));

	// to a cell one of them leaves: allowed; out of the map: not
	QVERIFY(m_doc->canMoveItems(events, starts, QPoint(1, 0)));
	QVERIFY(!m_doc->canMoveItems(events, starts, QPoint(0, -2)));
	QVERIFY(!m_doc->moveItems(events, starts, QPoint(0, -2)));
	const int steps = m_doc->undoStack()->count();
	QVERIFY(m_doc->moveItems(events, starts, QPoint(1, 0)));
	QCOMPARE(m_doc->undoStack()->count(), steps + 1);
	QCOMPARE(m_doc->eventIdAt(2, 1), 1);
	QCOMPARE(m_doc->eventIdAt(3, 1), 2);
	QCOMPARE(m_doc->startAt(2, 2), Party);
	m_doc->undo();
	QCOMPARE(m_doc->eventIdAt(1, 1), 1);
	QCOMPARE(m_doc->eventIdAt(2, 1), 2);
	QCOMPARE(m_doc->startAt(1, 2), Party);

	// copied from the top left of the selection, pasted on free cells only
	const std::vector<lcf::rpg::Event> copies = m_doc->copyEvents(events, cells.topLeft());
	QCOMPARE(static_cast<int>(copies.size()), 2);
	QCOMPARE(QPoint(copies[1].x, copies[1].y), QPoint(1, 0));
	QVERIFY(!m_doc->canPasteEvents(copies, QPoint(2, 1)));
	QVERIFY(!m_doc->canPasteEvents(copies, QPoint(Width - 1, 0)));
	QVERIFY(m_doc->pasteEvents(copies, QPoint(2, 1)).isEmpty());
	const QList<int> pasted = m_doc->pasteEvents(copies, QPoint(4, 4));
	QCOMPARE(pasted, (QList<int>{3, 4}));
	QCOMPARE(m_doc->eventIdAt(5, 4), 4);
	m_doc->deleteEvents(pasted);
	QCOMPARE(m_doc->eventIdAt(4, 4), 0);
	QCOMPARE(m_doc->eventIdAt(5, 4), 0);
	m_doc->undo();
	QCOMPARE(m_doc->eventIdAt(4, 4), 3);
	QCOMPARE(m_doc->eventIdAt(5, 4), 4);
}

void MapDocumentTest::areaCopyPaste()
{
	m_doc->drawRect(EditorState::LOWER, QPoint(0, 0), QPoint(1, 1), brush(Dirt));
	m_doc->commitStroke();
	m_doc->drawPen(EditorState::UPPER, QPoint(1, 1), QPoint(1, 1), {{10003}, 1, 1});
	m_doc->commitStroke();

	// both layers, the drawing in progress aside
	m_doc->drawPen(EditorState::LOWER, QPoint(0, 0), QPoint(0, 0), brush(Water));
	const MapDocument::TileClipboard clipboard = m_doc->copyArea(QRect(0, 0, 2, 2));
	m_doc->cancelStroke();
	QCOMPARE(clipboard.width, 2);
	QCOMPARE(clipboard.height, 2);
	QCOMPARE(clipboard.lower[0], static_cast<short>(m_doc->tileAt(EditorState::LOWER, 0, 0)));
	QCOMPARE(clipboard.lower[3], static_cast<short>(m_doc->tileAt(EditorState::LOWER, 1, 1)));
	QCOMPARE(clipboard.upper[3], static_cast<short>(10003));

	// floating, then put down: the tiles keep their borders, the grass
	// around keeps its own
	QSignalSpy floating(m_doc.get(), &MapDocument::floatingChanged);
	const int steps = m_doc->undoStack()->count();
	m_doc->startFloating(clipboard, QPoint(3, 2));
	QVERIFY(m_doc->isFloating());
	m_doc->setFloatingPos(QPoint(5, 3));
	QCOMPARE(m_doc->tileAt(EditorState::LOWER, 5, 3), static_cast<int>(Autotile::translate(Grass)));
	m_doc->commitFloating();
	QVERIFY(!m_doc->isFloating());
	QCOMPARE(floating.count(), 2);
	QCOMPARE(m_doc->undoStack()->count(), steps + 1);
	for (int i = 0; i < 4; i++) {
		QCOMPARE(m_doc->tileAt(EditorState::LOWER, 5 + i % 2, 3 + i / 2), static_cast<int>(clipboard.lower[i]));
		QCOMPARE(m_doc->tileAt(EditorState::UPPER, 5 + i % 2, 3 + i / 2), static_cast<int>(clipboard.upper[i]));
	}
	QCOMPARE(m_doc->tileAt(EditorState::LOWER, 4, 3), static_cast<int>(Autotile::translate(Grass)));
	m_doc->undo();
	QCOMPARE(m_doc->tileAt(EditorState::LOWER, 5, 3), static_cast<int>(Autotile::translate(Grass)));
	QCOMPARE(m_doc->tileAt(EditorState::UPPER, 6, 4), 10000);

	// past the edge: what is outside is left out
	m_doc->startFloating(clipboard, QPoint(Width - 1, Height - 1));
	m_doc->commitFloating();
	QCOMPARE(m_doc->tileAt(EditorState::LOWER, Width - 1, Height - 1), static_cast<int>(clipboard.lower[0]));
	// dropped: nothing changes
	const std::vector<short> lower = m_doc->layerData(EditorState::LOWER);
	m_doc->startFloating(clipboard, QPoint(3, 3));
	m_doc->cancelFloating();
	m_doc->commitFloating();
	QCOMPARE(m_doc->layerData(EditorState::LOWER), lower);

	// Clear: the brush over the area, one step
	m_doc->fillArea(EditorState::LOWER, QRect(0, 0, 2, 2), brush(Water));
	QCOMPARE(terrainAt(*m_doc, 0, 0), Water);
	QCOMPARE(terrainAt(*m_doc, 1, 1), Water);
	QCOMPARE(m_doc->undoStack()->undoText(), QStringLiteral("Clear"));
}

void MapDocumentTest::areaMove()
{
	m_doc->drawRect(EditorState::LOWER, QPoint(1, 1), QPoint(2, 2), brush(Dirt));
	m_doc->commitStroke();
	const short corner = static_cast<short>(m_doc->tileAt(EditorState::LOWER, 1, 1));
	const short grass = Autotile::translate(Grass);

	m_doc->beginMoveArea(QRect(1, 1, 2, 2), grass);
	QCOMPARE(m_doc->moveAreaTo(QPoint(4, 2)), QRect(4, 2, 2, 2));
	QCOMPARE(m_doc->workingTile(EditorState::LOWER, 4, 2), corner);
	QCOMPARE(m_doc->workingTile(EditorState::LOWER, 1, 1), grass);
	QCOMPARE(m_doc->tileAt(EditorState::LOWER, 1, 1), static_cast<int>(corner));
	// kept inside the map
	QCOMPARE(m_doc->moveAreaTo(QPoint(7, 5)), QRect(6, 4, 2, 2));
	m_doc->endMoveArea(true);
	QCOMPARE(m_doc->tileAt(EditorState::LOWER, 6, 4), static_cast<int>(corner));
	QCOMPARE(m_doc->tileAt(EditorState::LOWER, 1, 1), static_cast<int>(grass));
	QCOMPARE(m_doc->undoStack()->undoText(), QStringLiteral("Move"));

	// a drag given up: back where they were
	const std::vector<short> lower = m_doc->layerData(EditorState::LOWER);
	m_doc->beginMoveArea(QRect(6, 4, 2, 2), grass);
	m_doc->moveAreaTo(QPoint(0, 0));
	m_doc->endMoveArea(false);
	QCOMPARE(m_doc->workingLayer(EditorState::LOWER), lower);
	QCOMPARE(m_doc->layerData(EditorState::LOWER), lower);
}

void MapDocumentTest::startPositions()
{
	QSignalSpy start(m_doc.get(), &MapDocument::startChanged);
	QCOMPARE(m_doc->startAt(3, 3), -1);
	QVERIFY(!m_doc->startCell(Party));
	m_doc->setStartPosition(Boat, 3, 3);
	QCOMPARE(m_doc->startAt(3, 3), Boat);
	QCOMPARE(*m_doc->startCell(Boat), QPoint(3, 3));
	QCOMPARE(m_project.treeMap().start.boat_map_id, MapId);
	QVERIFY(!m_doc->isFreeCell(3, 3));
	// in the same place again: nothing to undo
	m_doc->setStartPosition(Boat, 3, 3);
	QCOMPARE(m_doc->undoStack()->count(), 1);
	QCOMPARE(start.count(), 1);
	m_doc->undo();
	QCOMPARE(m_doc->startAt(3, 3), -1);
	QCOMPARE(m_project.treeMap().start.boat_map_id, 0);
}

void MapDocumentTest::shiftMap()
{
	m_doc->drawPen(EditorState::LOWER, QPoint(0, 0), QPoint(0, 0), brush(Water));
	m_doc->commitStroke();
	QVERIFY(m_doc->commitEvent(m_doc->newEvent(Width - 1, 0)));
	m_doc->setStartPosition(Party, 0, Height - 1);
	const lcf::rpg::Map before = m_doc->map();

	// what leaves one side comes back on the other
	QSignalSpy replaced(m_doc.get(), &MapDocument::mapReplaced);
	const int steps = m_doc->undoStack()->count();
	m_doc->shiftMap(1, 1);
	QCOMPARE(replaced.count(), 1);
	QCOMPARE(replaced.first().at(0).toBool(), false);
	QCOMPARE(m_doc->undoStack()->count(), steps + 1);
	QCOMPARE(terrainAt(*m_doc, 1, 1), Water);
	QCOMPARE(m_doc->eventIdAt(0, 1), 1);
	QCOMPARE(m_doc->startAt(1, 0), Party);
	m_doc->undo();
	QVERIFY(m_doc->map() == before);
	QCOMPARE(m_doc->startAt(0, Height - 1), Party);
}

void MapDocumentTest::mapProperties()
{
	lcf::rpg::Map map = m_doc->map();
	lcf::rpg::MapInfo info = m_doc->mapInfo();
	QVERIFY(!m_doc->setMapProperties(map, info));
	map.chipset_id = 2;
	info.name = ToDBString(QStringLiteral("Meadow"));
	QVERIFY(m_doc->setMapProperties(map, info));
	QCOMPARE(m_doc->name(), QStringLiteral("Meadow"));
	QCOMPARE(m_doc->map().chipset_id, 2);
	m_doc->undo();
	QCOMPARE(m_doc->name(), QStringLiteral("Field"));
	QCOMPARE(m_doc->map().chipset_id, 1);
	// the map tree keeps its name until the map is saved
	QCOMPARE(ToQString(m_project.treeMap().maps[1].name), QStringLiteral("Field"));
}

void MapDocumentTest::treeChangedByAnotherProgram()
{
	// another program renames the map while it has unsaved tiles: the map
	// takes the name (saving it would write the old one back otherwise)
	m_doc->drawPen(EditorState::LOWER, QPoint(1, 1), QPoint(1, 1), brush(Dirt));
	m_doc->commitStroke();
	QVERIFY(m_doc->isModified());
	m_project.treeMap().maps[1].name = ToDBString(QStringLiteral("Meadow"));
	m_doc->refreshMapInfo();
	QCOMPARE(m_doc->name(), QStringLiteral("Meadow"));
	// where the view is scrolled to is not an edit of the info
	m_doc->setScrollPosition(3, 2);
	m_project.treeMap().maps[1].name = ToDBString(QStringLiteral("Glade"));
	m_doc->refreshMapInfo();
	QCOMPARE(m_doc->name(), QStringLiteral("Glade"));

	// Map Properties' name is kept until saved
	lcf::rpg::MapInfo info = m_doc->mapInfo();
	info.name = ToDBString(QStringLiteral("Forest"));
	QVERIFY(m_doc->setMapProperties(m_doc->map(), info));
	m_project.treeMap().maps[1].name = ToDBString(QStringLiteral("Wood"));
	m_doc->refreshMapInfo();
	QCOMPARE(m_doc->name(), QStringLiteral("Forest"));

	// deleted: its entry leaves the tree, the map keeps its own
	QVERIFY(m_doc->hasTreeEntry());
	m_project.treeMap().maps.pop_back();
	QVERIFY(!m_doc->hasTreeEntry());
	m_doc->refreshMapInfo();
	QCOMPARE(m_doc->id(), MapId);
	QCOMPARE(m_doc->name(), QStringLiteral("Forest"));
}

QTEST_GUILESS_MAIN(MapDocumentTest)
#include "map_document_test.moc"
