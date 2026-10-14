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

// The map tabs without a window (MapTabsController, with Qt's model checks)
// on a copy of TestGame-2003 (EDITOR_TEST_PROJECT, skipped without it),
// opened by a ProjectController: the temporary tab, keeping, moving and
// closing tabs (the questions answered by a stand-in of the dialogs), the
// documents going after their views, and the project closing them.

#include <QAbstractItemModelTester>
#include <QDir>
#include <QFile>
#include <QPointer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include "core.h"
#include "controller/dialog_service.h"
#include "controller/map_tabs_controller.h"
#include "controller/project_controller.h"
#include "model/map_document.h"

namespace {

/** Answers the questions in turn, and remembers what was asked and said */
class Answers : public DialogService
{
public:
	Answer question(const QString& title, const QString&, Buttons) override
	{
		asked << title;
		return answers.isEmpty() ? Answer::Cancel : answers.takeFirst();
	}
	void warning(const QString& title, const QString&) override { said << title; }
	void error(const QString& title, const QString&) override { said << title; }

	QList<Answer> answers;
	QStringList asked;
	QStringList said;
};

}

class MapTabsControllerTest : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase();
	void init();
	void cleanup();
	void temporaryTab();
	void keptByAnEdit();
	void closeAsks();
	void currentAfterClose();
	void move();
	void restore();
	void unreadable();
	void closedByTheProject();
	void closeAll();

private:
	/** A change to the map (one undo step) */
	void edit(int id);
	QString titles() const;

	std::unique_ptr<QTemporaryDir> m_dir;
	std::unique_ptr<ProjectController> m_project;
	std::unique_ptr<MapTabsController> m_tabs;
	std::unique_ptr<QAbstractItemModelTester> m_tester;
	Answers m_answers;
	/** The maps whose view was made (its document read), and let go */
	QList<int> m_viewed;
	QList<int> m_released;
	QHash<int, QPointer<MapDocument>> m_documents;
};

void MapTabsControllerTest::initTestCase()
{
	if (qEnvironmentVariableIsEmpty("EDITOR_TEST_PROJECT"))
		QSKIP("EDITOR_TEST_PROJECT (TestGame-2003) is not set");
	DialogService::install(&m_answers);
}

void MapTabsControllerTest::init()
{
	// the database, the map tree and the maps
	m_dir = std::make_unique<QTemporaryDir>();
	QVERIFY(m_dir->isValid());
	const QDir from(qEnvironmentVariable("EDITOR_TEST_PROJECT"));
	for (const QString& file : from.entryList({QStringLiteral("*.ini"), QStringLiteral("*.ldb"),
			QStringLiteral("*.lmt"), QStringLiteral("*.lmu")}, QDir::Files)) {
		const QString to = QDir(m_dir->path()).filePath(file);
		QVERIFY(QFile::copy(from.filePath(file), to));
		QFile::setPermissions(to, QFile::permissions(to) | QFile::WriteOwner);
	}
	m_project = std::make_unique<ProjectController>();
	QVERIFY(m_project->open(m_dir->path()));
	m_tabs = std::make_unique<MapTabsController>(m_project.get());
	m_tester = std::make_unique<QAbstractItemModelTester>(m_tabs.get(), QAbstractItemModelTester::FailureReportingMode::QtTest);
	m_viewed.clear();
	m_released.clear();
	m_documents.clear();
	// as a view: it reads the document of a tab made, lets it go first
	connect(m_tabs.get(), &QAbstractItemModel::rowsInserted, this, [this](const QModelIndex&, int first, int last) {
		for (int row = first; row <= last; row++) {
			const int id = m_tabs->mapAt(row);
			m_project->document(id)->load();
			m_viewed << id;
			m_documents[id] = m_project->document(id);
		}
	});
	connect(m_tabs.get(), &MapTabsController::mapClosing, this, [this](int id) {
		// its document is still there
		QVERIFY(m_documents.value(id));
		m_released << id;
	});
	m_answers.answers.clear();
	m_answers.asked.clear();
	m_answers.said.clear();
}

void MapTabsControllerTest::cleanup()
{
	m_tester.reset();
	m_tabs.reset();
	if (m_project)
		m_project->close();
	m_project.reset();
	m_dir.reset();
}

void MapTabsControllerTest::edit(int id)
{
	MapDocument* doc = m_project->document(id);
	MapDocument::Brush brush;
	brush.tiles = {doc->tileAt(EditorState::LOWER, 0, 0) == 1000 ? short(2000) : short(1000)};
	doc->drawPen(EditorState::LOWER, QPoint(0, 0), QPoint(0, 0), brush);
	doc->commitStroke();
	QVERIFY(doc->isModified());
}

QString MapTabsControllerTest::titles() const
{
	QStringList titles;
	for (int row = 0; row < m_tabs->rowCount(); row++) {
		const QModelIndex index = m_tabs->index(row);
		titles << (index.data(MapTabsController::TemporaryRole).toBool() ? QStringLiteral("~") : QString())
			+ index.data(MapTabsController::TitleRole).toString();
	}
	return titles.join(QStringLiteral(" | "));
}

void MapTabsControllerTest::temporaryTab()
{
	QSignalSpy current(m_tabs.get(), &MapTabsController::currentMapChanged);
	QVERIFY(m_tabs->openMap(2, false));
	QCOMPARE(titles(), QStringLiteral("~Picture events"));
	QCOMPARE(m_tabs->temporaryMap(), 2);
	QCOMPARE(m_tabs->currentMap(), 2);
	QCOMPARE(current.size(), 1);
	// the next map looked at takes its place, its document goes
	QVERIFY(m_tabs->openMap(1, false));
	QCOMPARE(titles(), QStringLiteral("~Map events"));
	QCOMPARE(m_released, QList<int>{2});
	QVERIFY(!m_project->document(2));
	QCOMPARE(m_tabs->currentMap(), 1);
	// kept: the next one opens beside it
	QVERIFY(m_tabs->openMap(1, true));
	QCOMPARE(m_tabs->temporaryMap(), 0);
	QVERIFY(m_tabs->openMap(4, false));
	QCOMPARE(titles(), QStringLiteral("Map events | ~Generate Dungeon"));
	QCOMPARE(m_tabs->mapIds(), (QList<int>{1, 4}));
	// shown again: no new tab; the view's choice
	QVERIFY(m_tabs->openMap(1, false));
	QCOMPARE(m_tabs->count(), 2);
	QCOMPARE(m_tabs->currentMap(), 1);
	m_tabs->setCurrentIndex(1);
	QCOMPARE(m_tabs->currentMap(), 4);
	QCOMPARE(m_tabs->currentDocument(), m_project->document(4));
	// an area or the project opens no tab
	QVERIFY(!m_tabs->openMap(9, false));
	QVERIFY(!m_tabs->openMap(0, true));
	QCOMPARE(m_tabs->count(), 2);
	QCOMPARE(m_viewed, (QList<int>{2, 1, 4}));
}

void MapTabsControllerTest::keptByAnEdit()
{
	QVERIFY(m_tabs->openMap(1, false));
	QSignalSpy changed(m_tabs.get(), &QAbstractItemModel::dataChanged);
	edit(1);
	QCOMPARE(m_tabs->temporaryMap(), 0);
	QCOMPARE(titles(), QStringLiteral("Map events *"));
	QVERIFY(m_tabs->index(0).data(MapTabsController::ModifiedRole).toBool());
	QVERIFY(!changed.isEmpty());
	// the next map opens in a tab of its own
	QVERIFY(m_tabs->openMap(2, false));
	QCOMPARE(titles(), QStringLiteral("Map events * | ~Picture events"));
	// undone back to the map read: no "*"
	m_project->document(1)->undo();
	QCOMPARE(titles(), QStringLiteral("Map events | ~Picture events"));
}

void MapTabsControllerTest::closeAsks()
{
	QVERIFY(m_tabs->openMap(1, true));
	QVERIFY(m_tabs->openMap(2, true));
	edit(1);
	// Cancel: it stays, with its changes
	m_answers.answers = {DialogService::Answer::Cancel};
	QVERIFY(!m_tabs->requestClose(1));
	QCOMPARE(m_answers.asked, QStringList{QStringLiteral("Save map changes")});
	QVERIFY(m_tabs->isOpen(1));
	QVERIFY(m_project->document(1)->isModified());
	// No: closed without saving, its document after its view
	m_answers.answers = {DialogService::Answer::No};
	QVERIFY(m_tabs->requestClose(1));
	QVERIFY(!m_tabs->isOpen(1));
	QVERIFY(!m_project->document(1));
	QVERIFY(!m_documents.value(1));
	QCOMPARE(m_released, QList<int>{1});
	// nothing to save: not asked
	m_answers.asked.clear();
	QVERIFY(m_tabs->requestClose(2));
	QVERIFY(m_answers.asked.isEmpty());
	QCOMPARE(m_tabs->count(), 0);
	QVERIFY(!m_tabs->requestClose(2));
}

void MapTabsControllerTest::currentAfterClose()
{
	for (int id : {1, 2, 4})
		QVERIFY(m_tabs->openMap(id, true));
	m_tabs->setCurrentMap(2);
	// the one on its right, as a tab bar
	m_tabs->closeMap(2);
	QCOMPARE(m_tabs->currentMap(), 4);
	// none on its right: the one on its left
	m_tabs->closeMap(4);
	QCOMPARE(m_tabs->currentMap(), 1);
	// another tab than the one shown: it stays shown
	QVERIFY(m_tabs->openMap(5, true));
	m_tabs->setCurrentMap(5);
	m_tabs->closeMap(1);
	QCOMPARE(m_tabs->currentMap(), 5);
	m_tabs->closeMap(5);
	QCOMPARE(m_tabs->currentMap(), 0);
	QCOMPARE(m_tabs->currentDocument(), nullptr);
}

void MapTabsControllerTest::move()
{
	for (int id : {1, 2, 4})
		QVERIFY(m_tabs->openMap(id, true));
	QSignalSpy moved(m_tabs.get(), &QAbstractItemModel::rowsMoved);
	m_tabs->moveTab(2, 0);
	QCOMPARE(m_tabs->mapIds(), (QList<int>{4, 1, 2}));
	m_tabs->moveTab(0, 2);
	QCOMPARE(m_tabs->mapIds(), (QList<int>{1, 2, 4}));
	QCOMPARE(moved.size(), 2);
	// out of range: nothing
	m_tabs->moveTab(0, 3);
	m_tabs->moveTab(1, 1);
	QCOMPARE(moved.size(), 2);
	QCOMPARE(m_tabs->currentMap(), 4);
}

void MapTabsControllerTest::restore()
{
	QVERIFY(m_tabs->openMap(2, false));
	// the maps gone, the area, a map twice and the one open are skipped;
	// those restored are kept
	const QList<int> opened = m_tabs->restoreMaps({4, 9, 999, 1, 4, 2});
	QCOMPARE(opened, (QList<int>{4, 1}));
	QCOMPARE(m_tabs->mapIds(), (QList<int>{2, 4, 1}));
	QCOMPARE(m_tabs->temporaryMap(), 2);
	// the one shown is chosen after
	m_tabs->setCurrentMap(1);
	QCOMPARE(m_tabs->currentMap(), 1);
	m_tabs->setCurrentMap(999);
	QCOMPARE(m_tabs->currentMap(), 1);
}

void MapTabsControllerTest::unreadable()
{
	QVERIFY(QFile::remove(QDir(m_dir->path()).filePath(QStringLiteral("Map0002.lmu"))));
	QVERIFY(!m_tabs->openMap(2, false));
	QCOMPARE(m_answers.said, QStringList{QStringLiteral("Open Map")});
	QCOMPARE(m_tabs->count(), 0);
	QVERIFY(!m_project->document(2));
	QVERIFY(m_tabs->restoreMaps({2}).isEmpty());
}

void MapTabsControllerTest::closedByTheProject()
{
	// a map deleted with its child maps: their tabs go, their views first
	const QList<int> deleted = m_project->treeModel()->subtreeIds(4);
	int child = 0;
	for (int id : deleted) {
		if (id != 4 && m_project->treeModel()->type(id) == lcf::rpg::TreeMap::MapType_map)
			child = id;
	}
	QVERIFY(child);
	QVERIFY(m_tabs->openMap(1, true));
	QVERIFY(m_tabs->openMap(4, true));
	QVERIFY(m_tabs->openMap(child, true));
	m_answers.answers = {DialogService::Answer::Yes};
	QVERIFY(m_project->deleteMap(4, QStringLiteral("Generate Dungeon")));
	QCOMPARE(m_tabs->mapIds(), QList<int>{1});
	QCOMPARE(QSet<int>(m_released.cbegin(), m_released.cend()), (QSet<int>{4, child}));
	QCOMPARE(m_project->documentIds(), QList<int>{1});
	QCOMPARE(m_tabs->currentMap(), 1);
}

void MapTabsControllerTest::closeAll()
{
	for (int id : {1, 2, 4})
		QVERIFY(m_tabs->openMap(id, true));
	edit(2);
	// its changes were asked about before (closing the project)
	m_tabs->closeAll();
	QCOMPARE(m_tabs->count(), 0);
	QVERIFY(m_answers.asked.isEmpty());
	QVERIFY(m_project->documentIds().isEmpty());
	QCOMPARE(m_tabs->currentMap(), 0);
}

QTEST_MAIN(MapTabsControllerTest)
#include "map_tabs_controller_test.moc"
