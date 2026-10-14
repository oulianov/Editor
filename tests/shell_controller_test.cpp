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

// What the main window does without a window (ShellController) on copies
// of TestGame-2003 (EDITOR_TEST_PROJECT, skipped without it): opening a
// project while one is open, closing it, the states of the actions, the
// status texts, the map tree's actions through stand-ins of the editing
// windows (ShellDialogs) and of the questions (DialogService), and what is
// asked of the views of the maps. The settings are written to a folder of
// their own.

#include <QDir>
#include <QFile>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include "core.h"
#include "defines.h"
#include "controller/action_catalog.h"
#include "controller/dialog_service.h"
#include "controller/map_tabs_controller.h"
#include "controller/project_controller.h"
#include "controller/shell_controller.h"
#include "controller/shell_dialogs.h"
#include "common/dbstring.h"
#include "model/game_runner.h"
#include "model/map_document.h"

namespace {

class Answers : public DialogService
{
public:
	Answer question(const QString& title, const QString&, Buttons buttons) override
	{
		asked << title;
		if (answers.isEmpty())
			return buttons == Buttons::YesNoCancel ? Answer::Cancel : Answer::No;
		return answers.takeFirst();
	}
	void warning(const QString& title, const QString&) override { said << title; }
	void error(const QString& title, const QString&) override { said << title; }

	QList<Answer> answers;
	QStringList asked;
	QStringList said;
};

/** The editing windows: what they were asked, what they answer */
class Dialogs : public ShellDialogs
{
public:
	std::optional<NewProject> newProject(const QString&) override { return std::nullopt; }
	OpenProject openProject(const QString& def_dir) override { return {OpenProject::Choice::Cancelled, {}, def_dir}; }
	ImportProject importProject(const QString& def_dir) override { return {false, {}, {}, def_dir, false}; }
	void withImportProgress(const std::function<void(const ProjectController::ImportProgress&)>&) override {}
	std::optional<ProjectConvert::Options> convertProject(const Project&) override { return std::nullopt; }
	void convertResult(const QString&, const QString&) override {}
	bool mapProperties(lcf::rpg::MapInfo& info, lcf::rpg::Map&) override
	{
		asked << QStringLiteral("mapProperties");
		if (!accept)
			return false;
		info.name = lcf::DBString(name.toStdString());
		return true;
	}
	bool areaProperties(lcf::rpg::MapInfo&, const QSize&) override
	{
		asked << QStringLiteral("areaProperties");
		return accept;
	}
	std::optional<Dungeon> generateDungeon(const lcf::rpg::Map&, const QString&) override { return std::nullopt; }
	std::optional<QPoint> shiftMap() override
	{
		asked << QStringLiteral("shiftMap");
		return accept ? std::optional<QPoint>(QPoint(1, 0)) : std::nullopt;
	}

	bool accept = false;
	QString name = QStringLiteral("Renamed");
	QStringList asked;
};

}

class ShellControllerTest : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase();
	void init();
	void cleanup();
	void openWhileOpenClosesFirst();
	void closeProject();
	void actionsWithoutProject();
	void actionsOnTheNodeSelected();
	void actionsOfTheMapShown();
	void layersToolsAndZoom();
	void statusTexts();
	void newMap();
	void propertiesOfAnOpenMap();
	void shift();
	void revert();
	void areaOnItsMap();
	void testPlayOptions();

private:
	/** A copy of the test game */
	QString copyGame();
	void edit(int id);
	bool enabled(const char* name) const;
	bool checked(const char* name) const;

	QTemporaryDir m_settingsDir;
	std::vector<std::unique_ptr<QTemporaryDir>> m_games;
	std::unique_ptr<ShellController> m_shell;
	Answers m_answers;
	Dialogs m_dialogs;
};

void ShellControllerTest::initTestCase()
{
	if (qEnvironmentVariableIsEmpty("EDITOR_TEST_PROJECT"))
		QSKIP("EDITOR_TEST_PROJECT (TestGame-2003) is not set");
	// settings of its own, not the user's
	QVERIFY(m_settingsDir.isValid());
	QCoreApplication::setOrganizationName(QStringLiteral("EasyRPG Test"));
	QCoreApplication::setApplicationName(QStringLiteral("shell_controller_test"));
	QSettings::setDefaultFormat(QSettings::IniFormat);
	QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settingsDir.path());
	DialogService::install(&m_answers);
}

void ShellControllerTest::init()
{
	m_shell = std::make_unique<ShellController>();
	m_shell->setDialogs(&m_dialogs);
	m_answers.answers.clear();
	m_answers.asked.clear();
	m_answers.said.clear();
	m_dialogs.accept = false;
	m_dialogs.asked.clear();
}

void ShellControllerTest::cleanup()
{
	if (m_shell && core().project()) {
		m_answers.answers = {DialogService::Answer::No};
		m_shell->closeProject();
	}
	m_shell.reset();
	m_games.clear();
}

QString ShellControllerTest::copyGame()
{
	auto dir = std::make_unique<QTemporaryDir>();
	const QDir from(qEnvironmentVariable("EDITOR_TEST_PROJECT"));
	for (const QString& file : from.entryList({QStringLiteral("*.ini"), QStringLiteral("*.ldb"),
			QStringLiteral("*.lmt"), QStringLiteral("*.lmu")}, QDir::Files)) {
		const QString to = QDir(dir->path()).filePath(file);
		QFile::copy(from.filePath(file), to);
		QFile::setPermissions(to, QFile::permissions(to) | QFile::WriteOwner);
	}
	const QString path = dir->path();
	m_games.push_back(std::move(dir));
	return path;
}

void ShellControllerTest::edit(int id)
{
	MapDocument* doc = m_shell->project()->document(id);
	if (!doc->hasMap())
		doc->load();
	MapDocument::Brush brush;
	brush.tiles = {doc->tileAt(EditorState::LOWER, 0, 0) == 1000 ? short(2000) : short(1000)};
	doc->drawPen(EditorState::LOWER, QPoint(0, 0), QPoint(0, 0), brush);
	doc->commitStroke();
	QVERIFY(doc->isModified());
}

bool ShellControllerTest::enabled(const char* name) const
{
	return m_shell->actions()->isEnabled(QLatin1String(name));
}

bool ShellControllerTest::checked(const char* name) const
{
	return m_shell->actions()->isChecked(QLatin1String(name));
}

void ShellControllerTest::openWhileOpenClosesFirst()
{
	const QString game = copyGame();
	QSignalSpy opened(m_shell.get(), &ShellController::projectOpened);
	QSignalSpy closing(m_shell.get(), &ShellController::projectClosing);
	QVERIFY(m_shell->openProject(game));
	QCOMPARE(opened.size(), 1);
	QCOMPARE(QSettings().value(CURRENT_PROJECT_KEY).toString(), QDir(game).absolutePath());
	const auto first = core().project();
	QVERIFY(m_shell->tabs()->openMap(1, true));
	edit(1);

	// Cancel: the project open stays, with its map and its changes
	m_answers.answers = {DialogService::Answer::Cancel};
	QVERIFY(!m_shell->openProject(game));
	QCOMPARE(m_answers.asked, QStringList{QStringLiteral("Save map changes")});
	QCOMPARE(core().project(), first);
	QVERIFY(m_shell->tabs()->isOpen(1));
	QCOMPARE(closing.size(), 0);

	// No: its tabs and documents go before the other one opens
	m_answers.answers = {DialogService::Answer::No};
	connect(m_shell.get(), &ShellController::projectClosing, this, [this] {
		QVERIFY(m_shell->tabs()->isOpen(1));
	});
	QVERIFY(m_shell->openProject(game));
	QCOMPARE(closing.size(), 1);
	QCOMPARE(opened.size(), 2);
	QVERIFY(core().project() != first);
	QCOMPARE(m_shell->tabs()->count(), 0);
	QVERIFY(m_shell->project()->documentIds().isEmpty());
	// its map tree is the new project's: read again, nothing breaks
	QVERIFY(m_shell->tabs()->openMap(1, true));
	QVERIFY(m_shell->project()->reloadTreeMap());
	QVERIFY(m_shell->tabs()->isOpen(1));

	// a folder that cannot be read: the project open stays, nothing asked
	const auto second = core().project();
	m_answers.asked.clear();
	QVERIFY(!m_shell->openProject(QDir(game).filePath(QStringLiteral("missing"))));
	QCOMPARE(core().project(), second);
	QVERIFY(m_shell->tabs()->isOpen(1));
	QVERIFY(m_answers.asked.isEmpty());
	QCOMPARE(m_answers.said, QStringList{QStringLiteral("Error loading project")});
}

void ShellControllerTest::closeProject()
{
	QVERIFY(m_shell->openProject(copyGame()));
	QVERIFY(m_shell->tabs()->openMap(2, true));
	edit(2);
	QSignalSpy title(m_shell.get(), &ShellController::windowTitleChanged);
	m_answers.answers = {DialogService::Answer::Cancel};
	QVERIFY(!m_shell->closeProject());
	QVERIFY(core().project());
	m_answers.answers = {DialogService::Answer::No};
	QVERIFY(m_shell->closeProject());
	QVERIFY(!core().project());
	QCOMPARE(m_shell->tabs()->count(), 0);
	QVERIFY(QSettings().value(CURRENT_PROJECT_KEY).toString().isEmpty());
	QCOMPARE(m_shell->windowTitle(), QStringLiteral("EasyRPG Editor"));
	QCOMPARE(title.size(), 1);
	// nothing to close
	QVERIFY(m_shell->closeProject());
}

void ShellControllerTest::actionsWithoutProject()
{
	QVERIFY(enabled("actionProjectNew"));
	QVERIFY(enabled("actionProjectOpen"));
	QVERIFY(enabled("actionSettings"));
	for (const char* name : {"actionProjectClose", "actionDatabase", "actionMapSave", "actionMapNew", "actionZoomIn",
			"actionZoom1_2", "actionPlayTest", "actionLayerLower", "actionUndo", "actionMapPaste"})
		QVERIFY2(!enabled(name), name);
}

void ShellControllerTest::actionsOnTheNodeSelected()
{
	QVERIFY(m_shell->openProject(copyGame()));
	QVERIFY(enabled("actionProjectClose"));
	QVERIFY(!enabled("actionProjectNew"));
	QVERIFY(enabled("actionDatabase"));
	// nothing selected: the project
	m_shell->setSelectedMap(-1);
	m_shell->updateActions();
	QVERIFY(enabled("actionMapNew"));
	QVERIFY(!enabled("actionMapDelete"));
	QVERIFY(!enabled("actionMapProperties"));
	// a map
	m_shell->setSelectedMap(4);
	m_shell->updateActions();
	for (const char* name : {"actionMapNew", "actionMapCopy", "actionMapDelete", "actionMapShift", "actionAreaNew",
			"actionMapGenerateDungeon", "actionMapProperties"})
		QVERIFY2(enabled(name), name);
	QVERIFY(!enabled("actionAreaDelete"));
	QVERIFY(!enabled("actionMapPaste"));
	// an area
	m_shell->setSelectedMap(9);
	m_shell->updateActions();
	QVERIFY(enabled("actionAreaProperties"));
	QVERIFY(enabled("actionAreaDelete"));
	QVERIFY(!enabled("actionMapNew"));
	QVERIFY(!enabled("actionMapDelete"));
	// a map copied: pasted on a map or the project
	m_shell->setSelectedMap(4);
	m_shell->copyMap();
	QVERIFY(enabled("actionMapPaste"));
	m_shell->setSelectedMap(0);
	m_shell->updateActions();
	QVERIFY(enabled("actionMapPaste"));
	// the view's own selection, asked when needed
	m_shell->setSelectionSource([] { return 9; });
	QCOMPARE(m_shell->selectedMap(), 9);
	m_shell->updateActions();
	QVERIFY(!enabled("actionMapPaste"));
	m_shell->setSelectionSource({});
}

void ShellControllerTest::actionsOfTheMapShown()
{
	QVERIFY(m_shell->openProject(copyGame()));
	QVERIFY(m_shell->tabs()->openMap(1, true));
	m_shell->updateActions();
	QVERIFY(enabled("actionZoomIn"));
	QVERIFY(enabled("actionZoom1_8"));
	QVERIFY(!enabled("actionMapSave"));
	QVERIFY(!enabled("actionUndo"));
	// the project selected: Map Properties edits the map shown
	m_shell->setSelectedMap(0);
	m_shell->updateActions();
	QVERIFY(enabled("actionMapProperties"));
	edit(1);
	m_shell->updateActions();
	QVERIFY(enabled("actionMapSave"));
	QVERIFY(enabled("actionMapRevert"));
	QVERIFY(enabled("actionUndo"));
	QVERIFY(!enabled("actionRedo"));
	m_shell->project()->document(1)->undo();
	m_shell->updateActions();
	QVERIFY(enabled("actionRedo"));
	QVERIFY(!enabled("actionMapSave"));
}

void ShellControllerTest::layersToolsAndZoom()
{
	m_shell->setLayer(EditorState::UPPER);
	QVERIFY(checked("actionLayerUpper"));
	QVERIFY(!checked("actionLayerLower"));
	QVERIFY(!checked("actionLayerEvents"));
	// unchecked by a click on it: checked again
	m_shell->actions()->setChecked(QStringLiteral("actionLayerUpper"), false);
	m_shell->setLayer(EditorState::UPPER);
	QVERIFY(checked("actionLayerUpper"));
	m_shell->setTool(EditorState::FILL);
	QVERIFY(checked("actionDrawFill"));
	QVERIFY(!checked("actionDrawPen"));
	m_shell->setTool(EditorState::PENCIL);
	QVERIFY(checked("actionDrawPen"));
	m_shell->setLayer(EditorState::LOWER);
	// the ratio the view shows the map at
	QCOMPARE(ShellController::zoomRatio(QStringLiteral("actionZoom1_2")), 2.0);
	QCOMPARE(ShellController::zoomRatio(QStringLiteral("actionZoom1_8")), 0.5);
	m_shell->setZoomRatio(4.0);
	QVERIFY(checked("actionZoom1_1"));
	m_shell->setZoomRatio(0);
	for (const QString& ratio : m_shell->actions()->groupActions(ActionCatalog::Group::Zoom))
		QVERIFY(!m_shell->actions()->isChecked(ratio));
}

void ShellControllerTest::statusTexts()
{
	QSignalSpy status(m_shell.get(), &ShellController::statusChanged);
	QVERIFY(m_shell->editModeText().isEmpty());
	QVERIFY(m_shell->openProject(copyGame()));
	QCOMPARE(m_shell->windowTitle(), QStringLiteral("EasyRPG Editor - ") + core().project()->gameTitle());
	QVERIFY(m_shell->mapText().isEmpty());
	QVERIFY(m_shell->tabs()->openMap(1, true));
	QCOMPARE(m_shell->mapText(), QStringLiteral("0001: Map events"));
	m_shell->setLayer(EditorState::LOWER);
	m_shell->setTool(EditorState::RECTANGLE);
	QCOMPARE(m_shell->editModeText(), QStringLiteral("Lower Layer Editing Mode - Rectangle Tool"));
	m_shell->setLayer(EditorState::EVENT);
	QCOMPARE(m_shell->editModeText(), QStringLiteral("Event Editing Mode"));
	QVERIFY(!status.isEmpty());
	m_shell->setLayer(EditorState::LOWER);
	m_shell->setTool(EditorState::PENCIL);
}

void ShellControllerTest::newMap()
{
	QVERIFY(m_shell->openProject(copyGame()));
	QSignalSpy select(m_shell.get(), &ShellController::selectMapRequested);
	const int count = m_shell->project()->treeModel()->ids().size();
	m_shell->setSelectedMap(1);
	// Cancel: nothing made
	m_shell->newMap();
	QCOMPARE(m_dialogs.asked, QStringList{QStringLiteral("mapProperties")});
	QCOMPARE(m_shell->project()->treeModel()->ids().size(), count);
	// OK: a child of the map selected, selected and opened (kept)
	m_dialogs.accept = true;
	m_dialogs.name = QStringLiteral("New One");
	m_shell->newMap();
	QCOMPARE(m_shell->project()->treeModel()->ids().size(), count + 1);
	QCOMPARE(select.size(), 1);
	const int id = select.first().first().toInt();
	QCOMPARE(m_shell->project()->treeModel()->parentId(id), 1);
	QCOMPARE(ToQString(m_shell->project()->treeModel()->mapInfo(id)->name), QStringLiteral("New One"));
	QVERIFY(m_shell->tabs()->isOpen(id));
	QCOMPARE(m_shell->tabs()->temporaryMap(), 0);
	QCOMPARE(m_shell->tabs()->currentMap(), id);
	// deleted: asked first, its tab goes
	m_shell->setSelectedMap(id);
	m_answers.answers = {DialogService::Answer::Yes};
	m_shell->deleteMap();
	QVERIFY(!m_shell->project()->treeModel()->contains(id));
	QVERIFY(!m_shell->tabs()->isOpen(id));
}

void ShellControllerTest::propertiesOfAnOpenMap()
{
	QVERIFY(m_shell->openProject(copyGame()));
	QVERIFY(m_shell->tabs()->openMap(2, true));
	m_shell->project()->document(2)->load();
	QSignalSpy floating(m_shell.get(), &ShellController::floatingCommitRequested);
	m_dialogs.accept = true;
	m_dialogs.name = QStringLiteral("Renamed");
	m_shell->setSelectedMap(2);
	m_shell->mapProperties();
	// its view put down what floats first; one undo step, not saved
	QCOMPARE(floating.size(), 1);
	QCOMPARE(floating.first().first().toInt(), 2);
	MapDocument* doc = m_shell->project()->document(2);
	QCOMPARE(doc->name(), QStringLiteral("Renamed"));
	QVERIFY(doc->isModified());
	QCOMPARE(m_shell->tabs()->title(2), QStringLiteral("Renamed *"));
	doc->undo();
	QCOMPARE(doc->name(), QStringLiteral("Picture events"));
	// the project selected: the map shown
	m_shell->setSelectedMap(0);
	m_shell->mapProperties();
	QCOMPARE(floating.size(), 2);
	QCOMPARE(doc->name(), QStringLiteral("Renamed"));
}

void ShellControllerTest::shift()
{
	const QString game = copyGame();
	QVERIFY(m_shell->openProject(game));
	QSignalSpy shifted(m_shell.get(), &ShellController::shiftRequested);
	// cancelled: nothing
	m_shell->setSelectedMap(1);
	m_shell->shiftMap();
	QCOMPARE(m_dialogs.asked, QStringList{QStringLiteral("shiftMap")});
	QCOMPARE(shifted.size(), 0);
	// an open map: its view shifts it (with undo)
	m_dialogs.accept = true;
	QVERIFY(m_shell->tabs()->openMap(1, true));
	m_shell->shiftMap();
	QCOMPARE(shifted.size(), 1);
	QCOMPARE(shifted.first(), (QList<QVariant>{1, 1, 0}));
	// another one on disk
	const QString file = QDir(game).filePath(QStringLiteral("Map0002.lmu"));
	QFile before(file);
	QVERIFY(before.open(QIODevice::ReadOnly));
	const QByteArray data = before.readAll();
	before.close();
	m_shell->setSelectedMap(2);
	m_shell->shiftMap();
	QCOMPARE(shifted.size(), 1);
	QFile after(file);
	QVERIFY(after.open(QIODevice::ReadOnly));
	QVERIFY(after.readAll() != data);
	// an area: nothing
	m_shell->setSelectedMap(9);
	m_shell->shiftMap();
	QCOMPARE(m_dialogs.asked.size(), 3);
}

void ShellControllerTest::revert()
{
	QVERIFY(m_shell->openProject(copyGame()));
	QVERIFY(m_shell->tabs()->openMap(1, true));
	edit(1);
	QSignalSpy floating(m_shell.get(), &ShellController::floatingCancelRequested);
	m_answers.answers = {DialogService::Answer::No};
	m_shell->revertMap();
	QCOMPARE(m_answers.asked, QStringList{QStringLiteral("Revert Map")});
	QVERIFY(m_shell->project()->document(1)->isModified());
	QCOMPARE(floating.size(), 0);
	m_answers.answers = {DialogService::Answer::Yes};
	m_shell->revertMap();
	QVERIFY(!m_shell->project()->document(1)->isModified());
	QCOMPARE(floating.size(), 1);
}

void ShellControllerTest::areaOnItsMap()
{
	QVERIFY(m_shell->openProject(copyGame()));
	QSignalSpy select(m_shell.get(), &ShellController::selectMapRequested);
	QSignalSpy area(m_shell.get(), &ShellController::showAreaRequested);
	const int map = m_shell->project()->treeModel()->parentId(9);
	QVERIFY(map > 0);
	// an area has no file: its map opens, the area is selected and shown
	QVERIFY(m_shell->openNode(9, false));
	QVERIFY(m_shell->tabs()->isOpen(map));
	QCOMPARE(m_shell->tabs()->temporaryMap(), map);
	QCOMPARE(select.size(), 1);
	QCOMPARE(select.first().first().toInt(), 9);
	QCOMPARE(area.size(), 1);
	QCOMPARE(area.first().first().toInt(), map);
	QVERIFY(area.first().at(1).toRect().isValid());
	// the project opens nothing
	QVERIFY(!m_shell->openNode(0, true));
}

void ShellControllerTest::testPlayOptions()
{
	m_shell->setTestPlayFullScreen(true);
	QVERIFY(QSettings().value(TESTPLAY_FULLSCREEN_KEY).toBool());
	QVERIFY(checked("actionFullScreenToggle"));
	m_shell->setTestPlayShowTitle(false);
	QVERIFY(!QSettings().value(TESTPLAY_SHOW_TITLE_KEY).toBool());
	QVERIFY(!checked("actionTitleBackgroundToggle"));
	// remembered
	ShellController again;
	QVERIFY(again.actions()->isChecked(QStringLiteral("actionFullScreenToggle")));
	QVERIFY(!again.actions()->isChecked(QStringLiteral("actionTitleBackgroundToggle")));
	m_shell->setTestPlayFullScreen(false);
	m_shell->setTestPlayShowTitle(true);
}

QTEST_MAIN(ShellControllerTest)
#include "shell_controller_test.moc"
