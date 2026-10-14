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

// The workspace of a project in settings files of their own: written and
// read back (through the file), and the values older versions wrote.

#include <QCryptographicHash>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include "controller/workspace_store.h"

namespace {
const QString project = QStringLiteral("/games/My Game");

Workspace sample()
{
	Workspace workspace;
	Workspace::MapView first;
	first.id = 3;
	first.zoom = 4.0;
	first.cell = QPoint(5, 7);
	first.scroll = QPoint(120, 48);
	Workspace::MapView second;
	second.id = 12;
	second.zoom = 0.5;
	workspace.maps = QList<Workspace::MapView>{first, second};
	workspace.activeMap = 12;
	workspace.selectedMap = 9;
	workspace.expanded = QList<int>{0, 3, 4};
	workspace.treeScroll = 33;
	workspace.editing = {{"layer", 1}, {"tool", 2}};
	workspace.paletteScroll = QPoint(0, 64);
	workspace.layout = {{"docks", QByteArray("docks")}, {"splitters", QVariantMap()}};
	workspace.dockLayout = QByteArray("{\"serializationVersion\":3}");
	return workspace;
}
}

class WorkspaceStoreTest : public QObject
{
	Q_OBJECT

private slots:
	void init();
	void key();
	void roundTrip();
	void roundTripThroughFile();
	void notSaved();
	void readsWhatTheEditorWrote();
	void readsOlderWorkspaces();
	void window();
	void dialogs();

private:
	QSettings* settings() { return m_settings.get(); }
	QTemporaryDir m_dir;
	std::unique_ptr<QSettings> m_settings;
};

void WorkspaceStoreTest::init()
{
	QVERIFY(m_dir.isValid());
	QFile::remove(m_dir.filePath("settings.ini"));
	m_settings = std::make_unique<QSettings>(m_dir.filePath("settings.ini"), QSettings::IniFormat);
}

void WorkspaceStoreTest::key()
{
	// the key of every version: the SHA-256 of the folder
	const QString hash = QString::fromLatin1(QCryptographicHash::hash(project.toUtf8(), QCryptographicHash::Sha256).toHex());
	QCOMPARE(WorkspaceStore::key(project), "project_workspace/" + hash);
	QVERIFY(WorkspaceStore::key(project) != WorkspaceStore::key(project + "2"));
}

void WorkspaceStoreTest::roundTrip()
{
	WorkspaceStore store(settings());
	QVERIFY(!store.contains(project));
	const Workspace workspace = sample();
	store.save(project, workspace);
	QVERIFY(store.contains(project));
	QCOMPARE(store.load(project), workspace);
	QVERIFY(!store.contains("/games/Other"));
}

void WorkspaceStoreTest::roundTripThroughFile()
{
	{
		WorkspaceStore store(settings());
		store.save(project, sample());
		store.sync();
	}
	// read again from the file, by other settings
	QSettings again(m_dir.filePath("settings.ini"), QSettings::IniFormat);
	const Workspace workspace = WorkspaceStore(&again).load(project);
	QCOMPARE(workspace.maps->size(), 2);
	QCOMPARE(workspace.maps->at(0), sample().maps->at(0));
	QVERIFY(!workspace.maps->at(1).scroll);
	QCOMPARE(workspace.expanded, sample().expanded);
	QCOMPARE(workspace.activeMap, 12);
	QCOMPARE(workspace.selectedMap, 9);
	QCOMPARE(workspace.paletteScroll, QPoint(0, 64));
	QCOMPARE(workspace.editing.value("tool").toInt(), 2);
	QCOMPARE(workspace.dockLayout, sample().dockLayout);
}

void WorkspaceStoreTest::notSaved()
{
	// a project never opened: the map tree's active map and expanded nodes,
	// the window's layout
	WorkspaceStore store(settings());
	const QVariantMap window_layout{{"docks", QByteArray("window")}};
	store.setWindowLayout(window_layout);
	const Workspace workspace = store.load(project);
	QVERIFY(!workspace.maps);
	QVERIFY(!workspace.expanded);
	QCOMPARE(workspace.activeMap, -1);
	QCOMPARE(workspace.selectedMap, -1);
	QVERIFY(workspace.editing.isEmpty());
	QCOMPARE(workspace.layout, window_layout);
	QVERIFY(workspace.dockLayout.isEmpty());
}

void WorkspaceStoreTest::readsWhatTheEditorWrote()
{
	// as older versions wrote it, before WorkspaceStore: the maps with a
	// float zoom
	const QVariantMap written{
		{"maps", QVariantList{
			QVariantMap{{"id", 3}, {"zoom", 4.0f}, {"cell", QPoint(5, 7)}, {"scroll", QVariantMap{{"x", 120}, {"y", 48}}}},
			QVariantMap{{"id", 12}, {"zoom", 0.5f}, {"cell", QPoint(-1, -1)}, {"scroll", QVariantMap{{"x", 0}, {"y", 0}}}}}},
		{"active_map", 12}, {"selected_map", 9}, {"expanded", QVariantList{0, 3, 4}},
		{"tree_scroll", 33}, {"editing", QVariantMap{{"layer", 1}, {"tool", 2}}},
		{"palette_x", 0}, {"palette_y", 64},
		{"layout", QVariantMap{{"docks", QByteArray("docks")}}}};
	settings()->setValue(WorkspaceStore::key(project), written);
	const Workspace workspace = WorkspaceStore(settings()).load(project);
	QCOMPARE(workspace.maps->size(), 2);
	QCOMPARE(workspace.maps->at(0).id, 3);
	QCOMPARE(workspace.maps->at(0).zoom, 4.0);
	QCOMPARE(workspace.maps->at(0).cell, QPoint(5, 7));
	QCOMPARE(workspace.maps->at(0).scroll, QPoint(120, 48));
	QCOMPARE(workspace.maps->at(1).zoom, 0.5);
	QCOMPARE(workspace.activeMap, 12);
	QCOMPARE(workspace.selectedMap, 9);
	QCOMPARE(workspace.expanded, (QList<int>{0, 3, 4}));
	QCOMPARE(workspace.treeScroll, 33);
	QCOMPARE(workspace.editing, (QVariantMap{{"layer", 1}, {"tool", 2}}));
	QCOMPARE(workspace.paletteScroll, QPoint(0, 64));
	QCOMPARE(workspace.layout.value("docks").toByteArray(), QByteArray("docks"));
	// read back as they were written
	const QVariantMap view = workspace.maps->at(0).toVariant();
	QCOMPARE(view.value("zoom").toFloat(), 4.0f);
	QCOMPARE(view.value("cell").toPoint(), QPoint(5, 7));
	QCOMPARE(view.value("scroll").toMap().value("y").toInt(), 48);
	// written again, it is what was read
	QCOMPARE(WorkspaceStore::fromVariant(WorkspaceStore::toVariant(workspace)), workspace);
}

void WorkspaceStoreTest::readsOlderWorkspaces()
{
	// the docks as "panels", the editing state at the top level, no maps
	const QVariantMap older{{"panels", QByteArray("panels")}, {"layer", 2}, {"tool", 1},
		{"tiles", QVariantList{1, 2}}, {"tile_width", 2}, {"tile_height", 1}};
	const Workspace workspace = WorkspaceStore::fromVariant(older, {{"docks", QByteArray("window")}});
	QCOMPARE(workspace.layout, (QVariantMap{{"docks", QByteArray("panels")}}));
	// EditorState::restoreEditingState reads the old keys from it
	QCOMPARE(workspace.editing.value("layer").toInt(), 2);
	QCOMPARE(workspace.editing.value("tile_width").toInt(), 2);
	QVERIFY(!workspace.maps);
	QVERIFY(!workspace.expanded);
	// without "panels" nor "layout": the window's
	QCOMPARE(WorkspaceStore::fromVariant({{"active_map", 1}}, {{"docks", QByteArray("window")}}).layout,
		(QVariantMap{{"docks", QByteArray("window")}}));
}

void WorkspaceStoreTest::window()
{
	WorkspaceStore store(settings());
	QVERIFY(store.windowGeometry().isEmpty());
	store.setWindowGeometry(QByteArray("geometry"));
	store.setWindowLayout({{"docks", QByteArray("state")}});
	store.setWindowDockLayout(QByteArray("{}"));
	store.sync();
	QSettings again(m_dir.filePath("settings.ini"), QSettings::IniFormat);
	// the keys the Editor always used
	QCOMPARE(again.value("main_window_geometry").toByteArray(), QByteArray("geometry"));
	QCOMPARE(again.value("main_window_layout").toMap().value("docks").toByteArray(), QByteArray("state"));
	WorkspaceStore read(&again);
	QCOMPARE(read.windowGeometry(), QByteArray("geometry"));
	QCOMPARE(read.windowDockLayout(), QByteArray("{}"));
}

void WorkspaceStoreTest::dialogs()
{
	WorkspaceStore store(settings());
	QVERIFY(!store.dialogGeometry("DatabaseWindow").isValid());
	store.setDialogGeometry("DatabaseWindow", QRect(40, 60, 800, 640));
	store.sync();
	QSettings again(m_dir.filePath("settings.ini"), QSettings::IniFormat);
	// beside the other window states: Settings > Reset local state removes it with them
	QVERIFY(again.contains("window_states/qml/DatabaseWindow"));
	QCOMPARE(WorkspaceStore(&again).dialogGeometry("DatabaseWindow"), QRect(40, 60, 800, 640));
	QVERIFY(!WorkspaceStore(&again).dialogGeometry("EntryPicker").isValid());
}

QTEST_GUILESS_MAIN(WorkspaceStoreTest)
#include "workspace_store_test.moc"
