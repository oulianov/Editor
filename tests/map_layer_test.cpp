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

// MapLayerView, the map of the QML interface drawn by the scene graph: a
// map of TestGame-2003 (EDITOR_TEST_PROJECT, skipped without it) rendered
// in an offscreen window and compared with the same cells painted by
// TilePainter. The scene graph backend is
// chosen once per process: ctest runs the test with the software one
// (map_layer, offscreen) and with the default one, Metal, Direct3D or
// OpenGL, on a screen of twice the resolution (map_layer_rhi, skipped
// without one).
// With EDITOR_TEST_OUTPUT set, the images compared are written there.

#include <QDir>
#include <QElapsedTimer>
#include <QPainter>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QTest>
#include <algorithm>
#include <lcf/rpg/mapinfo.h>
#include "defines.h"
#include "model/autotile.h"
#include "model/map_document.h"
#include "model/project.h"
#include "render/animation_clock.h"
#include "render/chipset_tiles.h"
#include "render/tile_atlas.h"
#include "render/tile_painter.h"
#include "ui/map/map_layer_item.h"

namespace {
// under the tiles, where the panorama goes (magenta without one)
const QColor Background(255, 0, 255);
constexpr int TileSize = 16;
constexpr int Copies = 4;

struct Difference
{
	int pixels = 0;
	int maximum = 0;
};

Difference compare(const QImage& a, const QImage& b)
{
	Difference difference;
	if (a.size() != b.size())
		return {a.width() * a.height(), 255};
	for (int y = 0; y < a.height(); y++) {
		const QRgb* line_a = reinterpret_cast<const QRgb*>(a.constScanLine(y));
		const QRgb* line_b = reinterpret_cast<const QRgb*>(b.constScanLine(y));
		for (int x = 0; x < a.width(); x++) {
			const int d = std::max({std::abs(qRed(line_a[x]) - qRed(line_b[x])),
				std::abs(qGreen(line_a[x]) - qGreen(line_b[x])),
				std::abs(qBlue(line_a[x]) - qBlue(line_b[x])),
				std::abs(qAlpha(line_a[x]) - qAlpha(line_b[x]))});
			if (d > 0) {
				difference.pixels++;
				difference.maximum = std::max(difference.maximum, d);
			}
		}
	}
	return difference;
}

MapDocument::Brush brush(short tile)
{
	return {{tile}, 1, 1};
}
}

class MapLayerTest : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase();
	void init();
	void cleanup();
	void atlas();
	void wholeMap_data();
	void wholeMap();
	void dimmed();
	void animation();
	void cellChange();
	void visibleChunks();
	void grid();
	void largeMap();

private:
	bool software() const;
	void show(const QSize& size, const QPoint& offset = QPoint());
	QImage grab();
	/** The cells of the layers as TilePainter paints them: each layer a
	 *  pixmap, over the background */
	QImage reference(MapLayerView::LayerSet layers, qreal zoom, const QPoint& offset, const QSize& size,
		unsigned tick = 0, bool dimmed = false) const;
	void check(const QImage& actual, const QImage& expected, const QString& name, int tolerance = 0);

	std::shared_ptr<Project> m_project;
	int m_mapId = 0;
	lcf::rpg::Map m_map;
	QSize m_mapSize;
	std::shared_ptr<const ChipsetTiles> m_tiles;
	std::unique_ptr<MapDocument> m_doc;
	std::unique_ptr<QQuickWindow> m_window;
	MapLayerView* m_view = nullptr;
	AnimationClock m_clock;
	QString m_output;
	bool m_software = false;
	QSGRendererInterface::GraphicsApi m_api = QSGRendererInterface::Unknown;
	qreal m_dpr = 1;
};

void MapLayerTest::initTestCase()
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
		m_api = probe.rendererInterface()->graphicsApi();
		m_dpr = probe.effectiveDevicePixelRatio();
		m_software = m_api == QSGRendererInterface::Software;
		if (qEnvironmentVariableIsSet("EDITOR_TEST_RHI") && m_software)
			QSKIP("no scene graph on Metal, Direct3D or OpenGL here (offscreen platform?)");
		if (probe.grabWindow().isNull())
			QSKIP("the scene graph cannot draw offscreen here");
	}
	m_project = Project::load(QDir(game));
	QVERIFY(m_project);
	QVERIFY(m_project->loadDatabaseAndMapTree());

	// the map with the most water and animated tiles
	int best = -1;
	for (const lcf::rpg::MapInfo& info : m_project->treeMap().maps) {
		if (info.type != lcf::rpg::TreeMap::MapType_map)
			continue;
		const auto map = m_project->loadMap(info.ID);
		if (!map)
			continue;
		const int score = static_cast<int>(std::count_if(map->lower_layer.begin(), map->lower_layer.end(), Autotile::isAnimatedTile));
		if (score > best) {
			best = score;
			m_mapId = info.ID;
		}
	}
	QVERIFY(m_mapId > 0);
	const auto map = m_project->loadMap(m_mapId);
	QVERIFY(map);
	// The maps of the test game are smaller than a chunk: this one repeated
	// Copies x Copies times, its events left out
	m_map = *map;
	m_map.events.clear();
	m_map.width = map->width * Copies;
	m_map.height = map->height * Copies;
	m_map.lower_layer.clear();
	m_map.upper_layer.clear();
	for (int y = 0; y < m_map.height; y++)
		for (int x = 0; x < m_map.width; x++) {
			const size_t i = static_cast<size_t>(x % map->width + (y % map->height) * map->width);
			m_map.lower_layer.push_back(map->lower_layer[i]);
			m_map.upper_layer.push_back(map->upper_layer[i]);
		}
	m_mapSize = QSize(m_map.width, m_map.height);
	lcf::rpg::Chipset chipset;
	for (const auto& c : m_project->database().chipsets)
		if (c.ID == map->chipset_id)
			chipset = c;
	QVERIFY(chipset.ID > 0);
	m_tiles = ChipsetTiles::load(*m_project, chipset, TileSize, QString());
	QVERIFY(!m_tiles->isNull());
	qInfo() << "map" << m_mapId << m_mapSize << "chipset" << chipset.ID << "backend"
		<< m_api << "pixel ratio" << m_dpr;
}

void MapLayerTest::init()
{
	if (!m_project)
		QSKIP("no test game");
	m_doc = std::make_unique<MapDocument>(m_project->projectData(), m_mapId);
	m_doc->setMap(std::make_unique<lcf::rpg::Map>(m_map));
	m_clock.setTick(0);
	m_window = std::make_unique<QQuickWindow>();
	m_window->setColor(Background);
	m_view = new MapLayerView();
	// its own clock, before the window would give it the shared one
	m_view->setClock(&m_clock);
	m_view->setChipsetTiles(m_tiles);
	m_view->setDocument(m_doc.get());
	m_view->setParentItem(m_window->contentItem());
}

void MapLayerTest::cleanup()
{
	delete m_view;
	m_view = nullptr;
	m_window.reset();
	m_doc.reset();
}

bool MapLayerTest::software() const
{
	return m_software;
}

void MapLayerTest::show(const QSize& size, const QPoint& offset)
{
	// never shown: the content item takes the size from no resize event
	m_window->resize(size);
	m_window->contentItem()->setSize(size);
	m_view->setPosition(-offset);
}

QImage MapLayerTest::grab()
{
	return m_window->grabWindow().convertToFormat(QImage::Format_ARGB32);
}

QImage MapLayerTest::reference(MapLayerView::LayerSet layers, qreal zoom, const QPoint& offset, const QSize& size,
	unsigned tick, bool dimmed) const
{
	// in pixels of the screen
	const qreal dpr = m_window->effectiveDevicePixelRatio();
	const int cell = qRound(TileSize * zoom * dpr);
	const QSize pixels = size * dpr;
	const QPoint shift = offset * dpr;
	QImage result(pixels, QImage::Format_ARGB32_Premultiplied);
	result.fill(Background);
	QPainter out(&result);
	for (const auto layer : {EditorState::LOWER, EditorState::UPPER}) {
		if (!(layers & (layer == EditorState::LOWER ? MapLayerView::Lower : MapLayerView::Upper)))
			continue;
		QPixmap pixmap(pixels);
		pixmap.fill(Qt::transparent);
		TilePainter painter(m_tiles);
		// the panorama is another item's
		QPixmap none(TileSize, TileSize);
		none.fill(Qt::transparent);
		painter.setBackground(none);
		painter.setFrames(Autotile::waterFrame(m_tiles->chipset(), tick), Autotile::animatedTileFrame(tick));
		painter.begin(pixmap);
		const auto& tiles = m_doc->workingLayer(layer);
		for (int y = 0; y < m_mapSize.height(); y++)
			for (int x = 0; x < m_mapSize.width(); x++) {
				const QRect dest(x * cell - shift.x(), y * cell - shift.y(), cell, cell);
				if (dest.intersects(QRect(QPoint(), pixels)))
					painter.renderTile(tiles[static_cast<size_t>(x + y * m_mapSize.width())], dest);
			}
		painter.end();
		out.setOpacity(dimmed ? MapLayerView::DimmedOpacity : 1.0);
		out.drawPixmap(0, 0, pixmap);
	}
	out.end();
	return result.convertToFormat(QImage::Format_ARGB32);
}

void MapLayerTest::check(const QImage& actual, const QImage& expected, const QString& name, int tolerance)
{
	const QString backend = software() ? QStringLiteral("software") : QStringLiteral("rhi");
	if (!m_output.isEmpty()) {
		QDir().mkpath(m_output);
		actual.save(QDir(m_output).filePath(backend + '-' + name + QStringLiteral("-actual.png")));
		expected.save(QDir(m_output).filePath(backend + '-' + name + QStringLiteral("-expected.png")));
	}
	QVERIFY2(!actual.isNull(), qPrintable(name + QStringLiteral(": nothing grabbed")));
	const Difference difference = compare(actual, expected);
	if (difference.pixels > 0)
		qInfo() << name << difference.pixels << "pixels differ, by" << difference.maximum << "at most";
	QVERIFY2(difference.maximum <= tolerance, qPrintable(QStringLiteral("%1: %2 pixels differ, by %3 at most")
		.arg(name).arg(difference.pixels).arg(difference.maximum)));
}

void MapLayerTest::atlas()
{
	// every tile id of the chipset has a place, with the pixels of its frames
	const auto atlas = TileAtlas::forTiles(m_tiles);
	QCOMPARE(TileAtlas::forTiles(m_tiles), atlas);
	QCOMPARE(atlas->tileSize(), TileSize);
	int tiles = 0;
	for (int id = 0; id < 10144; id++) {
		const short tile_id = static_cast<short>(id);
		const QPixmap tile = m_tiles->tile(tile_id);
		QCOMPARE(atlas->rect(tile_id).isNull(), tile.isNull());
		if (tile.isNull())
			continue;
		tiles++;
		const TileAtlas::Sheet sheet = TileAtlas::sheet(tile_id);
		for (int frame = 0; frame < TileAtlas::frameCount(sheet); frame++) {
			const QImage expected = m_tiles->tile(tile_id, frame).toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
			const QImage actual = atlas->image(sheet, frame).copy(atlas->rect(tile_id));
			QVERIFY2(actual == expected, qPrintable(QStringLiteral("tile %1 frame %2").arg(id).arg(frame)));
		}
	}
	QVERIFY(tiles > 700);
	QVERIFY(atlas->isEmpty(-1));
	QCOMPARE(TileAtlas::sheet(1000), TileAtlas::Water);
	QCOMPARE(TileAtlas::sheet(3050), TileAtlas::Animated);
	QCOMPARE(TileAtlas::sheet(4000), TileAtlas::Still);
	QCOMPARE(TileAtlas::sheet(10000), TileAtlas::Still);
}

void MapLayerTest::wholeMap_data()
{
	QTest::addColumn<int>("layers");
	QTest::addColumn<qreal>("zoom");
	QTest::addColumn<QPoint>("offset");
	QTest::addColumn<QSize>("size");
	QTest::newRow("both-1") << int(MapLayerView::Both) << 1.0 << QPoint() << QSize(1024, 768);
	// across the edges of chunks (1024 pixels at 2)
	QTest::newRow("lower-2") << int(MapLayerView::Lower) << 2.0 << QPoint(700, 300) << QSize(640, 480);
	QTest::newRow("upper-2") << int(MapLayerView::Upper) << 2.0 << QPoint(700, 300) << QSize(640, 480);
	QTest::newRow("both-3") << int(MapLayerView::Both) << 3.0 << QPoint(1400, 0) << QSize(640, 480);
	QTest::newRow("both-0.5") << int(MapLayerView::Both) << 0.5 << QPoint() << QSize(640, 480);
}

void MapLayerTest::wholeMap()
{
	QFETCH(int, layers);
	QFETCH(qreal, zoom);
	QFETCH(QPoint, offset);
	QFETCH(QSize, size);
	m_view->setLayers(static_cast<MapLayerView::LayerSet>(layers));
	m_view->setZoom(zoom);
	QCOMPARE(m_view->implicitWidth(), m_mapSize.width() * TileSize * zoom);
	m_view->setSize(QSizeF(m_view->implicitWidth(), m_view->implicitHeight()));
	show(size, offset);
	const QImage image = grab();
	check(image, reference(static_cast<MapLayerView::LayerSet>(layers), zoom, offset, size), QString::fromLatin1(QTest::currentDataTag()));
}

void MapLayerTest::dimmed()
{
	// the layer not edited, faded (the upper layer while the lower one is edited)
	m_view->setLayers(MapLayerView::Upper);
	m_view->setDimmed(true);
	m_view->setSize(QSizeF(m_view->implicitWidth(), m_view->implicitHeight()));
	show(QSize(640, 480));
	// the blending of the GPU rounds otherwise
	const QImage image = grab();
	check(image, reference(MapLayerView::Upper, 1, QPoint(), QSize(640, 480), 0, true), QStringLiteral("dimmed"), 2);
}

void MapLayerTest::animation()
{
	m_view->setSize(QSizeF(m_view->implicitWidth(), m_view->implicitHeight()));
	show(QSize(1024, 768));
	grab();
	// the frames of the water (3, at the chipset's speed) and of the
	// animated tiles (4)
	for (unsigned tick : {1u, 2u, 3u, 4u, 6u, 9u}) {
		m_clock.setTick(tick);
		const QImage image = grab();
		check(image, reference(MapLayerView::Both, 1, QPoint(), QSize(1024, 768), tick), QStringLiteral("tick%1").arg(tick));
	}
}

void MapLayerTest::cellChange()
{
	m_view->setSize(QSizeF(m_view->implicitWidth(), m_view->implicitHeight()));
	const QSize size(1024, 768);
	show(size);
	grab();
	QVERIFY(!m_view->takeRebuiltChunks().isEmpty());
	// the software scene graph keeps its nodes between grabs; the others
	// make them again when they are not shown
	grab();
	const bool kept = m_view->takeRebuiltChunks().isEmpty();
	if (!kept)
		qInfo() << "the nodes are made again at each grab: the chunks made again are not checked";

	// a cell of the upper layer, in the second chunk when there is one
	const QPoint cell(std::min(m_mapSize.width() - 1, MapLayerView::ChunkSize + 16), std::min(m_mapSize.height() - 1, 16));
	const QPoint chunk(cell.x() / MapLayerView::ChunkSize, cell.y() / MapLayerView::ChunkSize);
	const short before = m_doc->workingTile(EditorState::UPPER, cell.x(), cell.y());
	const short tile = before == 10001 ? 10002 : 10001;
	m_doc->drawPen(EditorState::UPPER, cell, cell, brush(tile));
	QImage image = grab();
	if (kept)
		QCOMPARE(m_view->takeRebuiltChunks(), QList<QPoint>{chunk});
	check(image, reference(MapLayerView::Both, 1, QPoint(), size), QStringLiteral("pen"));

	// committed: the whole map is signalled, with the tiles shown
	m_doc->commitStroke();
	grab();
	if (kept)
		QCOMPARE(m_view->takeRebuiltChunks(), QList<QPoint>());
	m_doc->undo();
	QCOMPARE(m_doc->workingTile(EditorState::UPPER, cell.x(), cell.y()), before);
	image = grab();
	if (kept)
		QCOMPARE(m_view->takeRebuiltChunks(), QList<QPoint>{chunk});
	check(image, reference(MapLayerView::Both, 1, QPoint(), size), QStringLiteral("undo"));

	// an autotile of the lower layer: the borders around change too
	m_doc->drawPen(EditorState::LOWER, cell, cell, brush(Autotile::translate(6, SAMPLE)));
	image = grab();
	if (kept)
		QCOMPARE(m_view->takeRebuiltChunks(), QList<QPoint>{chunk});
	check(image, reference(MapLayerView::Both, 1, QPoint(), size), QStringLiteral("autotile"));
}

void MapLayerTest::visibleChunks()
{
	if (m_mapSize.width() <= MapLayerView::ChunkSize)
		QSKIP("the map has a single column of chunks");
	// a clipping parent: the viewport, as a Flickable is
	auto* viewport = new QQuickItem(m_window->contentItem());
	viewport->setClip(true);
	viewport->setSize(QSizeF(320, 240));
	m_view->setParentItem(viewport);
	m_view->setSize(QSizeF(m_view->implicitWidth(), m_view->implicitHeight()));
	show(QSize(640, 480));
	QImage image = grab();
	// the first chunk of each layer
	QCOMPARE(m_view->builtChunkCount(), 2);
	m_view->takeRebuiltChunks();
	grab();
	const bool kept = m_view->takeRebuiltChunks().isEmpty();
	const QRect in_viewport(QPoint(), QSize(320, 240) * m_window->effectiveDevicePixelRatio());
	check(image.copy(in_viewport), reference(MapLayerView::Both, 1, QPoint(), QSize(320, 240)), QStringLiteral("viewport"));

	// scrolled to the second column: the first is kept, next to it (when
	// the nodes last from a grab to the next)
	m_view->setX(-600);
	image = grab();
	QCOMPARE(m_view->builtChunkCount(), kept ? 4 : 2);
	check(image.copy(in_viewport), reference(MapLayerView::Both, 1, QPoint(600, 0), QSize(320, 240)), QStringLiteral("scrolled"));
}

void MapLayerTest::grid()
{
	// the palette: the lower tiles 6 a row, as TilePainter::paletteTiles
	QList<int> ids;
	for (int i = 0; i < 162; i++)
		ids << Autotile::translate(i, 15);
	m_view->setGridTiles(ids);
	m_view->setGridColumns(6);
	m_view->setZoom(2);
	QCOMPARE(m_view->cells(), QSize(6, 27));
	m_view->setSize(QSizeF(m_view->implicitWidth(), m_view->implicitHeight()));
	const QSize size(6 * 32, 27 * 32);
	show(size);

	TilePainter painter(m_tiles);
	QPixmap none(TileSize, TileSize);
	none.fill(Qt::transparent);
	painter.setBackground(none);
	const qreal dpr = m_window->effectiveDevicePixelRatio();
	QImage expected(size * dpr, QImage::Format_ARGB32_Premultiplied);
	expected.fill(Background);
	QPainter out(&expected);
	out.drawPixmap(0, 0, painter.paletteTiles(EditorState::LOWER, qRound(32 * dpr)));
	out.end();
	const QImage image = grab();
	check(image, expected.convertToFormat(QImage::Format_ARGB32), QStringLiteral("palette"));
}

void MapLayerTest::largeMap()
{
	// RPG Maker's largest map seen whole, its water moving: every chunk has
	// nodes, and a frame of the water takes no new ones
	auto map = std::make_unique<lcf::rpg::Map>(m_map);
	map->width = 500;
	map->height = 500;
	map->lower_layer.clear();
	map->upper_layer.clear();
	for (int y = 0; y < 500; y++)
		for (int x = 0; x < 500; x++) {
			const size_t i = static_cast<size_t>(x % m_mapSize.width() + (y % m_mapSize.height()) * m_mapSize.width());
			map->lower_layer.push_back(m_map.lower_layer[i]);
			map->upper_layer.push_back(m_map.upper_layer[i]);
		}
	m_doc->setMap(std::move(map));
	m_view->setZoom(1.0 / 16);
	m_view->setSize(QSizeF(m_view->implicitWidth(), m_view->implicitHeight()));
	show(QSize(500, 500));
	QElapsedTimer timer;
	timer.start();
	QVERIFY(!grab().isNull());
	const qint64 first = timer.restart();
	QCOMPARE(m_view->builtChunkCount(), 2 * 16 * 16);
	m_view->takeRebuiltChunks();
	// another frame of the water, slow or fast
	m_clock.setTick(4);
	QVERIFY(!grab().isNull());
	const qint64 frame = timer.elapsed();
	if (m_software)
		QCOMPARE(m_view->takeRebuiltChunks(), QList<QPoint>());
	qInfo() << "500 x 500 cells:" << first << "ms to the first grab," << frame << "ms to the next frame of the water";
}

QTEST_MAIN(MapLayerTest)

#include "map_layer_test.moc"
