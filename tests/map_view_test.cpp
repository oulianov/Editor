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

// The rest of the map of the QML interface: the panorama (MapPanoramaView),
// the events and the start positions (MapEventsView), the textures the
// views of a window share, the zoom snapped, and MapPreview.qml stacking
// them over the layers, on maps of TestGame-2003 (EDITOR_TEST_PROJECT,
// skipped without it). What is drawn where and when, not pixels: the
// pictures are looked at; but for the rules of the layers not edited (the
// lower one at half its brightness, the upper one at 0.7 of its opacity),
// checked on the pixels drawn. The scene graph backend is chosen once per
// process: ctest runs the test with the software one (map_view) and the
// system's (map_view_rhi), as map_layer.
// With EDITOR_TEST_OUTPUT set, pictures of the maps are written there: the
// QML items at several zooms and layers, next to the same map painted by
// TilePainter.

#include <QDir>
#include <QPainter>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSignalSpy>
#include <QTest>
#include <algorithm>
#include <lcf/rpg/mapinfo.h>
#include "core.h"
#include "defines.h"
#include "common/dbstring.h"
#include "common/filefinder.h"
#include "common/image_loader.h"
#include "model/autotile.h"
#include "model/map_document.h"
#include "model/project.h"
#include "render/animation_clock.h"
#include "render/panorama.h"
#include "render/tile_painter.h"
#include "ui/map/map_events_item.h"
#include "ui/map/map_layer_item.h"
#include "ui/map/map_panorama_item.h"
#include "ui/map/map_texture_cache.h"

namespace {
const QColor Background(40, 40, 40);
constexpr int TileSize = 16;

QString layerName(int layer)
{
	return layer == EditorState::LOWER ? QStringLiteral("lower") : layer == EditorState::UPPER ? QStringLiteral("upper") : QStringLiteral("event");
}
}

class MapViewTest : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase();
	void init();
	void cleanup();
	void snapScale();
	void panoramaMath();
	void panorama();
	void events();
	void markers();
	void documentSignals();
	void sharedTextures();
	void preview();
	void pictures();

private:
	std::unique_ptr<MapDocument> openMap(int id);
	void show(QQuickItem* item, const QSize& size);
	QImage grab();
	/** The map as TilePainter paints it (no blur) */
	QImage reference(MapDocument& doc, qreal zoom, int layer) const;
	QQuickItem* createPreview(MapDocument* doc, qreal zoom, int layer, bool snap = true);

	std::shared_ptr<Project> m_project;
	// the map with the most events, one with a panorama, one without
	int m_eventsMap = 0;
	int m_panoramaMap = 0;
	int m_plainMap = 0;
	std::unique_ptr<QQuickWindow> m_window;
	std::unique_ptr<QQmlEngine> m_engine;
	AnimationClock m_clock;
	QString m_output;
	bool m_software = false;
};

void MapViewTest::initTestCase()
{
	const QString game = qEnvironmentVariable("EDITOR_TEST_PROJECT");
	if (game.isEmpty())
		QSKIP("EDITOR_TEST_PROJECT (TestGame-2003) is not set");
	m_output = qEnvironmentVariable("EDITOR_TEST_OUTPUT");
	{
		// Metal, Direct3D or OpenGL: a frame drawn alone, offscreen, in a
		// window never shown
		QQuickWindow probe;
		probe.resize(16, 16);
		m_software = probe.rendererInterface()->graphicsApi() == QSGRendererInterface::Software;
		if (qEnvironmentVariableIsSet("EDITOR_TEST_RHI") && m_software)
			QSKIP("no scene graph on Metal, Direct3D or OpenGL here (offscreen platform?)");
		if (probe.grabWindow().isNull())
			QSKIP("the scene graph cannot draw offscreen here");
	}
	m_project = Project::load(QDir(game));
	QVERIFY(m_project);
	QVERIFY(m_project->loadDatabaseAndMapTree());
	// the views read the pictures of Core's project
	core().project() = m_project;

	int most_events = -1;
	for (const lcf::rpg::MapInfo& info : m_project->treeMap().maps) {
		if (info.type != lcf::rpg::TreeMap::MapType_map)
			continue;
		const auto map = m_project->loadMap(info.ID);
		if (!map)
			continue;
		if (static_cast<int>(map->events.size()) > most_events) {
			most_events = static_cast<int>(map->events.size());
			m_eventsMap = info.ID;
		}
		const QString panorama = ToQString(map->parallax_name);
		if (map->parallax_flag && !m_panoramaMap
				&& !m_project->findFile(PANORAMA, panorama, FileFinder::FileType::Image).isEmpty())
			m_panoramaMap = info.ID;
		if (!map->parallax_flag && !m_plainMap)
			m_plainMap = info.ID;
	}
	QVERIFY(m_eventsMap > 0);
	QVERIFY(m_plainMap > 0);
	qInfo() << "events on map" << m_eventsMap << "(" << most_events << "), panorama on map" << m_panoramaMap
		<< ", none on map" << m_plainMap << (m_software ? "software" : "rhi");

	m_engine = std::make_unique<QQmlEngine>();
	// the module of the Editor, without the Editor: its map types
	const char* uri = "org.easyrpg.editor";
	qmlRegisterType<MapLayerView>(uri, 1, 0, "MapLayerView");
	qmlRegisterType<MapPanoramaView>(uri, 1, 0, "MapPanoramaView");
	qmlRegisterType<MapEventsView>(uri, 1, 0, "MapEventsView");
	qmlRegisterUncreatableType<MapDocument>(uri, 1, 0, "MapDocument", QStringLiteral("opened by the Editor"));
	qmlRegisterUncreatableType<EditorState>(uri, 1, 0, "EditorState", QStringLiteral("shared"));
}

void MapViewTest::init()
{
	if (!m_project)
		QSKIP("no test game");
	m_clock.setTick(0);
	m_window = std::make_unique<QQuickWindow>();
	m_window->setColor(Background);
}

void MapViewTest::cleanup()
{
	m_window.reset();
}

std::unique_ptr<MapDocument> MapViewTest::openMap(int id)
{
	auto doc = std::make_unique<MapDocument>(m_project->projectData(), id);
	doc->setMap(m_project->loadMap(id));
	return doc;
}

void MapViewTest::show(QQuickItem* item, const QSize& size)
{
	item->setParentItem(m_window->contentItem());
	item->setSize(QSizeF(item->implicitWidth(), item->implicitHeight()));
	// never shown: the content item takes the size from no resize event
	m_window->resize(size);
	m_window->contentItem()->setSize(size);
}

QImage MapViewTest::grab()
{
	return m_window->grabWindow().convertToFormat(QImage::Format_ARGB32);
}

void MapViewTest::snapScale()
{
	// a power of two of pixels of the screen a pixel
	QCOMPARE(MapLayerView::snapScale(1, 1), 1.0);
	QCOMPARE(MapLayerView::snapScale(1.5, 1), 2.0);
	QCOMPARE(MapLayerView::snapScale(1.4, 1), 1.0);
	QCOMPARE(MapLayerView::snapScale(3, 1), 4.0);
	QCOMPARE(MapLayerView::snapScale(0.75, 1), 1.0);
	QCOMPARE(MapLayerView::snapScale(0.5, 1), 0.5);
	// on a screen of twice the resolution
	QCOMPARE(MapLayerView::snapScale(0.75, 2), 1.0);
	QCOMPARE(MapLayerView::snapScale(3, 2), 4.0);
	QCOMPARE(MapLayerView::snapScale(0.25, 2), 0.25);
	// Windows at 150%: 1.5 is 2 pixels of the screen
	QCOMPARE(MapLayerView::snapScale(1, 1.5), 2 / 1.5);
	// 1/8 to 16 pixels of the screen
	QCOMPARE(MapLayerView::snapScale(0.001, 1), 1.0 / 8);
	QCOMPARE(MapLayerView::snapScale(100, 2), 8.0);
	QCOMPARE(MapLayerView::snapScale(1.5, 0), 2.0);

	MapLayerView view;
	QCOMPARE(view.snapZoom(1.5), 2.0);
	QCOMPARE(view.snapZoom(0.75, 2), 1.0);
	show(&view, QSize(64, 64));
	const qreal dpr = m_window->effectiveDevicePixelRatio();
	QCOMPARE(view.snapZoom(3), MapLayerView::snapScale(3, dpr));
	view.setParentItem(nullptr);
}

void MapViewTest::panoramaMath()
{
	// from the top-left corner of the map, as zoomed as the tiles
	const QTransform transform = Panorama::transform(QPoint(2, 3), 32, 16);
	QCOMPARE(transform.map(QPointF(0, 0)), QPointF(-64, -96));
	QCOMPARE(transform.map(QPointF(10, 10)), QPointF(-44, -76));
	QCOMPARE(Panorama::transform(QPoint(), 8, 16).m11(), 0.5);

	// repeated over the map, the last ones cut at its edges
	auto copies = Panorama::copies(QSize(100, 50), QSize(250, 120), QRect(0, 0, 250, 120));
	QCOMPARE(copies.size(), size_t(9));
	QCOMPARE(copies[2].target, QRect(200, 0, 50, 50));
	QCOMPARE(copies[2].source, QRect(0, 0, 50, 50));
	QCOMPARE(copies[8].target, QRect(200, 100, 50, 20));
	// only those that meet the area
	copies = Panorama::copies(QSize(100, 50), QSize(250, 120), QRect(120, 60, 10, 10));
	QCOMPARE(copies.size(), size_t(1));
	QCOMPARE(copies[0].target, QRect(100, 50, 100, 50));
	QVERIFY(Panorama::copies(QSize(100, 50), QSize(250, 120), QRect(300, 0, 10, 10)).empty());
	QVERIFY(Panorama::copies(QSize(), QSize(250, 120), QRect(0, 0, 10, 10)).empty());
}

void MapViewTest::panorama()
{
	if (!m_panoramaMap)
		QSKIP("no map of the test game has a panorama");
	auto doc = openMap(m_panoramaMap);
	MapPanoramaView view;
	view.setDocument(doc.get());
	view.setZoom(2);
	QCOMPARE(view.name(), ToQString(doc->map().parallax_name));
	QVERIFY(view.found());
	const QImage picture = view.picture();
	QVERIFY(!picture.isNull());
	const QSize map_pixels(doc->map().width * TileSize, doc->map().height * TileSize);
	QCOMPARE(QSizeF(view.implicitWidth(), view.implicitHeight()), QSizeF(map_pixels * 2));

	// the copies of the picture in sight
	const QSize size(400, 300);
	show(&view, size);
	QVERIFY(!grab().isNull());
	const QRect in_sight(0, 0, size.width() / 2, size.height() / 2);
	QCOMPARE(view.builtCopyCount(), static_cast<int>(Panorama::copies(picture.size(), map_pixels, in_sight).size()));

	// read once for every view, as long as the file is the same
	MapPanoramaView other;
	other.setDocument(doc.get());
	QCOMPARE(other.picture().cacheKey(), picture.cacheKey());

	// Map Properties takes it away: magenta, no copies
	QSignalSpy changed(&view, &MapPanoramaView::pictureChanged);
	lcf::rpg::Map map = doc->map();
	map.parallax_flag = false;
	QVERIFY(doc->setMapProperties(map, doc->mapInfo()));
	QCOMPARE(changed.count(), 1);
	QVERIFY(view.name().isEmpty());
	QVERIFY(view.found());
	QVERIFY(view.picture().isNull());
	QVERIFY(!grab().isNull());
	QCOMPARE(view.builtCopyCount(), 0);
	doc->undo();
	QCOMPARE(changed.count(), 2);
	// read again (no view kept it), once for both
	QCOMPARE(view.picture().size(), picture.size());
	QCOMPARE(other.picture().cacheKey(), view.picture().cacheKey());

	// a file that is not there: nothing drawn
	map = doc->map();
	map.parallax_name = "NoSuchPanorama";
	QVERIFY(doc->setMapProperties(map, doc->mapInfo()));
	QVERIFY(!view.found());
	QVERIFY(!grab().isNull());
	QCOMPARE(view.builtCopyCount(), 0);
	view.setParentItem(nullptr);
}

void MapViewTest::events()
{
	auto doc = openMap(m_eventsMap);
	MapEventsView view;
	view.setDocument(doc.get());
	QCOMPARE(QSizeF(view.implicitWidth(), view.implicitHeight()),
		QSizeF(doc->map().width * TileSize, doc->map().height * TileSize));

	// every event with a page on its cell, but those under a start position
	QMap<int, QPoint> expected;
	QList<QPoint> markers;
	for (int kind = TilePainter::PARTY; kind <= TilePainter::AIRSHIP; kind++)
		if (const auto cell = doc->startCell(kind))
			markers << *cell;
	for (const auto& event : doc->map().events)
		if (!event.pages.empty() && !markers.contains(QPoint(event.x, event.y)))
			expected.insert(event.ID, QPoint(event.x, event.y));
	QVERIFY(!expected.isEmpty());
	QCOMPARE(view.eventCells(), expected);

	// drawn, at every zoom
	const QSize size(doc->map().width * TileSize * 3, doc->map().height * TileSize * 3);
	show(&view, size);
	for (qreal zoom : {0.5, 1.0, 2.0, 3.0}) {
		view.setZoom(zoom);
		view.setSize(QSizeF(view.implicitWidth(), view.implicitHeight()));
		QVERIFY(!grab().isNull());
		QCOMPARE(view.builtEventCount(), static_cast<int>(expected.size()));
	}

	// the events dragged by the selection tool, where they are dragged
	const int moved = expected.firstKey();
	view.setMovingEvents({moved});
	view.setMoveOffset(QPoint(1, 2));
	QCOMPARE(view.eventCells().value(moved), expected.value(moved) + QPoint(1, 2));
	QCOMPARE(view.eventCells().value(expected.lastKey()), expected.value(expected.lastKey()));
	view.setMoveOffset(QPoint());
	QCOMPARE(view.eventCells(), expected);

	// only those near the part in sight have nodes
	if (doc->map().width > 40) {
		auto* viewport = new QQuickItem(m_window->contentItem());
		viewport->setClip(true);
		viewport->setSize(QSizeF(8 * TileSize, 8 * TileSize));
		view.setZoom(1);
		view.setParentItem(viewport);
		QVERIFY(!grab().isNull());
		QVERIFY(view.builtEventCount() <= static_cast<int>(expected.size()));
	}
	view.setParentItem(nullptr);
}

void MapViewTest::markers()
{
	auto doc = openMap(m_eventsMap);
	// the party on a cell with an event, the boat on a free one
	const lcf::rpg::Event* under = nullptr;
	for (const auto& event : doc->map().events)
		if (!event.pages.empty())
			under = &event;
	QVERIFY(under);
	const QPoint party(under->x, under->y);
	const int under_id = under->ID;
	QPoint boat(-1, -1);
	for (int x = 0; x < doc->map().width && boat.x() < 0; x++)
		if (doc->isFreeCell(x, 0))
			boat = QPoint(x, 0);
	QVERIFY(boat.x() >= 0);
	doc->setStartPosition(TilePainter::PARTY, party.x(), party.y());
	doc->setStartPosition(TilePainter::BOAT, boat.x(), boat.y());

	MapEventsView view;
	view.setDocument(doc.get());
	QCOMPARE(view.markerCells().value(TilePainter::PARTY), party);
	QCOMPARE(view.markerCells().value(TilePainter::BOAT), boat);
	// the marker in place of the event
	QVERIFY(!view.eventCells().contains(under_id));

	// RPG Maker's opacity: whole on the event layer, half on the tile layers
	QSignalSpy layer(&view, &MapEventsView::editedLayerChanged);
	QCOMPARE(view.editedLayer(), EditorState::EVENT);
	QCOMPARE(view.markerOpacity(), 1.0);
	view.setEditedLayer(EditorState::LOWER);
	QCOMPARE(view.markerOpacity(), 0.5);
	view.setEditedLayer(EditorState::UPPER);
	QCOMPARE(view.markerOpacity(), 0.5);
	view.setEditedLayer(EditorState::EVENT);
	QCOMPARE(view.markerOpacity(), 1.0);
	QCOMPARE(layer.count(), 3);

	// dragged away, the event shows again
	view.setMovingStarts({TilePainter::PARTY});
	view.setMoveOffset(QPoint(0, 1));
	QCOMPARE(view.markerCells().value(TilePainter::PARTY), party + QPoint(0, 1));
	QCOMPARE(view.eventCells().value(under_id), party);
	view.setMovingStarts({});

	// drawn: the frames of the events and the pictures of the markers
	show(&view, QSize(doc->map().width * TileSize, doc->map().height * TileSize));
	QVERIFY(!grab().isNull());
	QCOMPARE(view.builtEventCount(), static_cast<int>(view.eventCells().size()));
	view.setParentItem(nullptr);
	// the start positions are the project's, in memory: as they were
	doc->undo();
	doc->undo();
}

void MapViewTest::documentSignals()
{
	auto doc = openMap(m_eventsMap);
	MapEventsView view;
	view.setDocument(doc.get());
	show(&view, QSize(doc->map().width * TileSize, doc->map().height * TileSize));
	QVERIFY(!grab().isNull());
	const QMap<int, QPoint> before = view.eventCells();
	QCOMPARE(view.builtEventCount(), static_cast<int>(before.size()));

	// an event added
	QPoint free(-1, -1);
	for (int y = 0; y < doc->map().height && free.x() < 0; y++)
		for (int x = 0; x < doc->map().width && free.x() < 0; x++)
			if (doc->isFreeCell(x, y))
				free = QPoint(x, y);
	const lcf::rpg::Event added = doc->newEvent(free.x(), free.y());
	QVERIFY(doc->commitEvent(added));
	QCOMPARE(view.eventCells().value(added.ID, QPoint(-1, -1)), free);
	QVERIFY(!grab().isNull());
	QCOMPARE(view.builtEventCount(), static_cast<int>(before.size()) + 1);

	// moved, then deleted
	QPoint other(-1, -1);
	for (int y = doc->map().height - 1; y >= 0 && other.x() < 0; y--)
		for (int x = doc->map().width - 1; x >= 0 && other.x() < 0; x--)
			if (doc->isFreeCell(x, y))
				other = QPoint(x, y);
	QVERIFY(doc->moveEvent(added.ID, other.x(), other.y()));
	QCOMPARE(view.eventCells().value(added.ID), other);
	QVERIFY(doc->deleteEventAt(other.x(), other.y()));
	QVERIFY(!view.eventCells().contains(added.ID));
	QVERIFY(!grab().isNull());
	QCOMPARE(view.builtEventCount(), static_cast<int>(before.size()));

	// undone: there again
	doc->undo();
	QCOMPARE(view.eventCells().value(added.ID), other);
	doc->undo();
	doc->undo();
	QCOMPARE(view.eventCells(), before);

	// a start position put on the map, and undone
	doc->setStartPosition(TilePainter::AIRSHIP, free.x(), free.y());
	QCOMPARE(view.markerCells().value(TilePainter::AIRSHIP), free);
	doc->undo();
	QVERIFY(!view.markerCells().contains(TilePainter::AIRSHIP) || doc->startCell(TilePainter::AIRSHIP));

	// another map in its place (read again, Shift Map...)
	doc->setMap(m_project->loadMap(m_plainMap));
	QMap<int, QPoint> plain;
	for (const auto& event : doc->map().events)
		if (!event.pages.empty())
			plain.insert(event.ID, QPoint(event.x, event.y));
	for (int kind = TilePainter::PARTY; kind <= TilePainter::AIRSHIP; kind++)
		if (const auto cell = doc->startCell(kind))
			for (auto it = plain.begin(); it != plain.end();)
				it = it.value() == *cell ? plain.erase(it) : std::next(it);
	QCOMPARE(view.eventCells(), plain);
	QCOMPARE(view.implicitWidth(), doc->map().width * TileSize * 1.0);

	// gone with its document
	doc.reset();
	QVERIFY(view.eventCells().isEmpty());
	QVERIFY(!grab().isNull());
	QCOMPARE(view.builtEventCount(), 0);
	view.setParentItem(nullptr);
}

void MapViewTest::sharedTextures()
{
	// the map, the palette, a preview: the same chipset, charsets and
	// panorama made into textures once in a window
	const int id = m_panoramaMap ? m_panoramaMap : m_eventsMap;
	auto doc = openMap(id);
	const auto tiles = MapLayerView::sharedChipsetTiles(doc->map().chipset_id);
	QVERIFY(tiles && !tiles->isNull());
	QCOMPARE(MapLayerView::sharedChipsetTiles(doc->map().chipset_id).get(), tiles.get());
	const QSize size(doc->map().width * TileSize, doc->map().height * TileSize);
	m_window->resize(size);
	m_window->contentItem()->setSize(size);
	std::vector<std::unique_ptr<QQuickItem>> views;
	auto addViews = [&]() {
		auto layers = std::make_unique<MapLayerView>();
		layers->setClock(&m_clock);
		auto panorama = std::make_unique<MapPanoramaView>();
		auto events = std::make_unique<MapEventsView>();
		layers->setDocument(doc.get());
		panorama->setDocument(doc.get());
		events->setDocument(doc.get());
		for (QQuickItem* item : {static_cast<QQuickItem*>(layers.get()), static_cast<QQuickItem*>(panorama.get()), static_cast<QQuickItem*>(events.get())}) {
			item->setParentItem(m_window->contentItem());
			item->setSize(QSizeF(item->implicitWidth(), item->implicitHeight()));
		}
		views.push_back(std::move(layers));
		views.push_back(std::move(panorama));
		views.push_back(std::move(events));
	};
	addViews();
	int uploads = MapTextureCache::uploadCount(m_window.get());
	QVERIFY(!grab().isNull());
	const int first = MapTextureCache::uploadCount(m_window.get()) - uploads;
	QVERIFY(first > 0);
	addViews();
	uploads = MapTextureCache::uploadCount(m_window.get());
	QVERIFY(!grab().isNull());
	const int second = MapTextureCache::uploadCount(m_window.get()) - uploads;
	// the software scene graph keeps its nodes from a grab to the next, and
	// their textures: none is made again. The others make every node again
	// for a grab: the textures once for both views.
	qInfo() << "textures made:" << first << "for a view of each kind," << second << "next with twice as many";
	QCOMPARE(second, m_software ? 0 : first);
	views.clear();
}

QQuickItem* MapViewTest::createPreview(MapDocument* doc, qreal zoom, int layer, bool snap)
{
	QQmlComponent component(m_engine.get(), QUrl::fromLocalFile(QStringLiteral(EDITOR_MAP_QML_DIR "/MapPreview.qml")));
	QObject* object = component.createWithInitialProperties({
		{QStringLiteral("document"), QVariant::fromValue(doc)},
		{QStringLiteral("zoom"), zoom},
		{QStringLiteral("snap"), snap},
		{QStringLiteral("editedLayer"), layer},
	});
	if (!object)
		qWarning() << component.errorString();
	auto* item = qobject_cast<QQuickItem*>(object);
	if (item)
		item->setParentItem(m_window->contentItem());
	return item;
}

void MapViewTest::preview()
{
	auto doc = openMap(m_panoramaMap ? m_panoramaMap : m_eventsMap);
	std::unique_ptr<QQuickItem> preview(createPreview(doc.get(), 1.5, EditorState::UPPER));
	QVERIFY(preview);
	// the zoom snapped, the size of the map at it
	const qreal dpr = m_window->effectiveDevicePixelRatio();
	const qreal zoom = MapLayerView::snapScale(1.5, dpr);
	QCOMPARE(preview->property("shownZoom").toReal(), zoom);
	QCOMPARE(preview->implicitWidth(), doc->map().width * TileSize * zoom);
	QCOMPARE(preview->implicitHeight(), doc->map().height * TileSize * zoom);
	auto* panorama = preview->property("panoramaView").value<MapPanoramaView*>();
	auto* lower = preview->property("lowerView").value<MapLayerView*>();
	auto* upper = preview->property("upperView").value<MapLayerView*>();
	auto* events = preview->property("eventsView").value<MapEventsView*>();
	QVERIFY(panorama && lower && upper && events);
	for (QQuickItem* item : {static_cast<QQuickItem*>(panorama), static_cast<QQuickItem*>(lower),
			static_cast<QQuickItem*>(upper), static_cast<QQuickItem*>(events)}) {
		QCOMPARE(item->property("document").value<MapDocument*>(), doc.get());
		QCOMPARE(item->property("zoom").toReal(), zoom);
		QCOMPARE(item->width(), preview->implicitWidth());
	}
	QCOMPARE(lower->layers(), MapLayerView::Lower);
	QCOMPARE(upper->layers(), MapLayerView::Upper);

	// the upper layer edited: the lower one darkened, with the panorama
	preview->setSize(QSizeF(preview->implicitWidth(), preview->implicitHeight()));
	m_window->resize(preview->size().toSize());
	m_window->contentItem()->setSize(preview->size());
	QVERIFY(!grab().isNull());
	QVERIFY(lower->darkened() && panorama->darkened());
	QVERIFY(!lower->dimmed());
	QVERIFY(!upper->dimmed() && !upper->darkened());
	QCOMPARE(events->markerOpacity(), 0.5);
	// the lower one edited: the upper one and the events faded
	preview->setProperty("editedLayer", int(EditorState::LOWER));
	QVERIFY(!lower->darkened() && !panorama->darkened());
	QVERIFY(!lower->dimmed());
	QVERIFY(upper->dimmed());
	QVERIFY(events->dimmed());
	QCOMPARE(events->markerOpacity(), 0.5);
	// the events: nothing faded, the markers whole
	preview->setProperty("editedLayer", int(EditorState::EVENT));
	QVERIFY(!lower->darkened() && !panorama->darkened());
	QVERIFY(!lower->dimmed() && !upper->dimmed() && !events->dimmed());
	QCOMPARE(events->markerOpacity(), 1.0);
	QVERIFY(!grab().isNull());

	// The pixels. The lower layer alone (with the panorama): at half its
	// brightness while the upper one is edited, but for the selection tool
	upper->setVisible(false);
	events->setVisible(false);
	const QImage normal = grab();
	auto halved = [](const QImage& image) {
		QImage half = image;
		for (int y = 0; y < half.height(); y++) {
			auto* line = reinterpret_cast<QRgb*>(half.scanLine(y));
			for (int x = 0; x < half.width(); x++)
				line[x] = qRgb(qRed(line[x]) / 2, qGreen(line[x]) / 2, qBlue(line[x]) / 2);
		}
		return half;
	};
	// the largest difference of a channel between two pictures
	auto difference = [](const QImage& a, const QImage& b) {
		if (a.size() != b.size())
			return 256;
		int most = 0;
		for (int y = 0; y < a.height(); y++)
			for (int x = 0; x < a.width(); x++) {
				const QRgb p = a.pixel(x, y);
				const QRgb q = b.pixel(x, y);
				most = std::max({most, std::abs(qRed(p) - qRed(q)), std::abs(qGreen(p) - qGreen(q)), std::abs(qBlue(p) - qBlue(q))});
			}
		return most;
	};
	// a picture worth checking: not all one colour
	QVERIFY(normal.pixel(0, 0) != normal.pixel(normal.width() / 2, normal.height() / 2)
		|| normal.pixel(0, 0) != normal.pixel(normal.width() - 1, normal.height() - 1));
	preview->setProperty("editedLayer", int(EditorState::UPPER));
	QCOMPARE(difference(grab(), halved(normal)), 0);
	preview->setProperty("tool", int(EditorState::SELECT));
	QVERIFY(!lower->darkened() && !panorama->darkened());
	QCOMPARE(difference(grab(), normal), 0);
	preview->setProperty("tool", int(EditorState::PENCIL));
	QCOMPARE(difference(grab(), halved(normal)), 0);
	preview->setProperty("editedLayer", int(EditorState::LOWER));
	QCOMPARE(difference(grab(), normal), 0);
	preview->setProperty("editedLayer", int(EditorState::EVENT));
	QCOMPARE(difference(grab(), normal), 0);

	// The upper layer alone: at 0.7 of its opacity over the background
	// while the lower one is edited, whole for the selection tool
	upper->setVisible(true);
	lower->setVisible(false);
	panorama->setVisible(false);
	const QImage whole = grab();
	QImage faded = whole;
	for (int y = 0; y < faded.height(); y++)
		for (int x = 0; x < faded.width(); x++) {
			const QRgb p = whole.pixel(x, y);
			auto mix = [](int c, int background) {
				return qRound(MapLayerView::DimmedOpacity * c + (1 - MapLayerView::DimmedOpacity) * background);
			};
			faded.setPixel(x, y, qRgb(mix(qRed(p), Background.red()), mix(qGreen(p), Background.green()), mix(qBlue(p), Background.blue())));
		}
	QImage empty(whole.size(), whole.format());
	empty.fill(Background);
	QVERIFY2(whole != empty, "the upper layer draws nothing");
	preview->setProperty("editedLayer", int(EditorState::LOWER));
	QVERIFY(upper->dimmed() && events->dimmed());
	// the blending of the backends rounds by a step or two
	QVERIFY(difference(grab(), faded) <= 2);
	preview->setProperty("tool", int(EditorState::SELECT));
	QVERIFY(!upper->dimmed() && !events->dimmed());
	QCOMPARE(difference(grab(), whole), 0);
	preview->setProperty("tool", int(EditorState::PENCIL));
	QVERIFY(difference(grab(), faded) <= 2);
	lower->setVisible(true);
	panorama->setVisible(true);
	events->setVisible(true);

	// unsnapped on demand
	preview->setProperty("snap", false);
	QCOMPARE(lower->zoom(), 1.5);
}

QImage MapViewTest::reference(MapDocument& doc, qreal zoom, int layer) const
{
	// the lower layer over the panorama, the upper one with the events (faded
	// while the lower layer is edited), the markers at the screen's
	// resolution (half opaque off the event layer); no blur
	const lcf::rpg::Map& map = doc.map();
	const qreal dpr = m_window->effectiveDevicePixelRatio();
	const int cell = qRound(TileSize * zoom);
	const QSize size(map.width * cell, map.height * cell);
	const auto tiles = MapLayerView::sharedChipsetTiles(map.chipset_id);
	QPixmap panorama;
	if (map.parallax_flag)
		panorama = ImageLoader::Load(m_project->findFile(PANORAMA, ToQString(map.parallax_name), FileFinder::FileType::Image));
	else {
		panorama = QPixmap(640, 480);
		panorama.fill(Panorama::NoneColor);
	}
	QImage result(size * dpr, QImage::Format_ARGB32_Premultiplied);
	result.setDevicePixelRatio(dpr);
	result.fill(Background);
	QPainter out(&result);
	QList<QPoint> marker_cells;
	for (int kind = TilePainter::PARTY; kind <= TilePainter::AIRSHIP; kind++)
		if (const auto at = doc.startCell(kind))
			marker_cells << *at;
	for (const auto which : {EditorState::LOWER, EditorState::UPPER}) {
		QPixmap pixmap(size);
		pixmap.fill(Qt::transparent);
		TilePainter painter(tiles);
		painter.setBackground(panorama, Panorama::transform(QPoint(), cell, TileSize));
		painter.begin(pixmap);
		for (int y = 0; y < map.height; y++)
			for (int x = 0; x < map.width; x++)
				painter.renderTile(doc.workingTile(which, x, y), QRect(x * cell, y * cell, cell, cell));
		if (which == EditorState::UPPER)
			for (const auto& event : map.events) {
				if (marker_cells.contains(QPoint(event.x, event.y)) || event.pages.empty())
					continue;
				const auto& page = event.pages[0];
				QPixmap character;
				if (!page.character_name.empty()) {
					const QPixmap charset = ImageLoader::Load(m_project->findFile(CHARSET, ToQString(page.character_name), FileFinder::FileType::Image));
					character = charset.copy((page.character_index % 4) * 72 + page.character_pattern * 24,
						(page.character_index / 4) * 128 + page.character_direction * 32, 24, 32);
				}
				painter.renderEvent(event, QRect(event.x * cell, event.y * cell, cell, cell), character.isNull() ? nullptr : &character);
			}
		painter.end();
		out.setOpacity((which == EditorState::UPPER && layer == EditorState::LOWER) ? 0.7 : 1.0);
		out.drawPixmap(QRect(QPoint(), size), pixmap);
	}
	QPixmap markers(size * dpr);
	markers.setDevicePixelRatio(dpr);
	markers.fill(Qt::transparent);
	TilePainter painter;
	painter.begin(markers);
	const QString letters[] = {QStringLiteral("P"), QStringLiteral("B"), QStringLiteral("S"), QStringLiteral("A")};
	for (int kind = TilePainter::PARTY; kind <= TilePainter::AIRSHIP; kind++)
		if (const auto at = doc.startCell(kind))
			painter.renderMarker(static_cast<TilePainter::StartMarker>(kind), letters[kind], QRect(at->x() * cell, at->y() * cell, cell, cell));
	painter.end();
	out.setOpacity(layer == EditorState::EVENT ? 1.0 : 0.5);
	out.drawPixmap(QRect(QPoint(), size), markers);
	out.end();
	return result.convertToFormat(QImage::Format_ARGB32);
}

void MapViewTest::pictures()
{
	if (m_output.isEmpty())
		QSKIP("EDITOR_TEST_OUTPUT is not set: no pictures to look at");
	QDir().mkpath(m_output);
	const QString backend = m_software ? QStringLiteral("software") : QStringLiteral("rhi");
	QList<int> maps{m_eventsMap};
	if (m_panoramaMap && m_panoramaMap != m_eventsMap)
		maps << m_panoramaMap;
	for (int id : maps) {
		auto doc = openMap(id);
		// the start positions on this map, for their markers
		doc->setStartPosition(TilePainter::PARTY, 1, 1);
		doc->setStartPosition(TilePainter::SHIP, 3, 1);
		for (qreal zoom : {0.5, 1.0, 2.0, 3.0})
			for (int layer : {int(EditorState::LOWER), int(EditorState::UPPER), int(EditorState::EVENT)}) {
				// 3 as it is (uneven pixels) and snapped
				for (bool snap : {false, true}) {
					if (snap && zoom != 3.0)
						continue;
					std::unique_ptr<QQuickItem> preview(createPreview(doc.get(), zoom, layer, snap));
					QVERIFY(preview);
					preview->setSize(QSizeF(preview->implicitWidth(), preview->implicitHeight()));
					m_window->resize(preview->size().toSize());
					m_window->contentItem()->setSize(preview->size());
					const QImage image = grab();
					QVERIFY(!image.isNull());
					const QString name = QStringLiteral("%1-map%2-z%3%4-%5").arg(backend).arg(id).arg(zoom)
						.arg(snap ? QStringLiteral("snapped") : QString()).arg(layerName(layer));
					image.save(QDir(m_output).filePath(name + QStringLiteral(".png")));
					if (!snap)
						reference(*doc, zoom, layer).save(QDir(m_output).filePath(name + QStringLiteral("-widgets.png")));
				}
			}
	}
}

QTEST_MAIN(MapViewTest)

#include "map_view_test.moc"
