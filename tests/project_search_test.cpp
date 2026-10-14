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

// The search of a project without its window (ProjectSearch) and its
// results (SearchResultsModel, checked by QAbstractItemModelTester), on a
// copy of the database and maps of TestGame-2003 (EDITOR_TEST_PROJECT,
// skipped without it). A common event, a monster group and a map open in
// the Editor (not saved) are added in memory: they use variable, switch
// and item 4000, which the game does not. Opening a result is left to the
// window.

#include <QAbstractItemModelTester>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <algorithm>
#include "core.h"
#include "common/dbstring.h"
#include "model/move_route_codec.h"
#include "model/project.h"
#include "model/project_search.h"
#include "model/search_results_model.h"

namespace {

using Cmd = lcf::rpg::EventCommand::Code;
using Kind = SearchResult::Kind;

// the variable, switch and item the events added use
constexpr int Needle = 4000;

lcf::rpg::EventCommand command(Cmd code, std::vector<int32_t> parameters, const char* text = "")
{
	lcf::rpg::EventCommand cmd;
	cmd.code = static_cast<int32_t>(code);
	cmd.string = lcf::DBString(text);
	cmd.parameters = lcf::DBArray<int32_t>(parameters.begin(), parameters.end());
	return cmd;
}

lcf::rpg::MoveRoute switchRoute(lcf::rpg::MoveCommand::Code code)
{
	lcf::rpg::MoveCommand move;
	move.command_id = static_cast<int32_t>(code);
	move.parameter_a = Needle;
	lcf::rpg::MoveRoute route;
	route.move_commands = {move};
	return route;
}

/** A result in short: where, and what refers to the query */
struct Found {
	int page;
	int line;
	QString type;
	bool operator==(const Found&) const = default;
};

char* toString(const Found& found)
{
	return QTest::toString(QStringLiteral("page %1 line %2 %3").arg(found.page).arg(found.line).arg(found.type));
}

} // namespace

class ProjectSearchTest : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase();
	void cleanupTestCase();
	void choices();
	void emptyText();
	void references();
	void references_data();
	void eventText();
	void eventNames();
	void scopes();
	void openMap();
	void resultsModel();
	void wholeGame();
	void cache();
	void suggestions();
	void openAndReset();

private:
	/** The results of the search in an event added: the common event (0),
	 *  the monster group (TroopResults) or the map open */
	QList<Found> found(const ProjectSearch& search, int map_id) const;

	std::unique_ptr<QTemporaryDir> m_dir;
	std::shared_ptr<Project> m_project;
	int m_commonEvent = 0;
	int m_troop = 0;
	int m_openMap = 0;
	ProjectSearch::MapSource m_source;
};

void ProjectSearchTest::initTestCase()
{
	const QString source = qEnvironmentVariable("EDITOR_TEST_PROJECT");
	if (source.isEmpty()) {
		QSKIP("EDITOR_TEST_PROJECT (TestGame-2003) is not set");
	}
	// the database, the maps and the CharSets (the graphics of the results)
	m_dir = std::make_unique<QTemporaryDir>();
	QVERIFY(m_dir->isValid());
	const QDir from(source);
	QDir to(m_dir->path());
	QVERIFY(to.mkdir(QStringLiteral("CharSet")));
	QStringList files = from.entryList({QStringLiteral("*.ini"), QStringLiteral("*.ldb"), QStringLiteral("*.lmt"),
		QStringLiteral("*.lmu")}, QDir::Files);
	for (const QString& file : QDir(from.filePath(QStringLiteral("CharSet"))).entryList(QDir::Files)) {
		files << QStringLiteral("CharSet/") + file;
	}
	for (const QString& file : std::as_const(files)) {
		QVERIFY2(QFile::copy(from.filePath(file), to.filePath(file)), qPrintable(file));
		// the maps are written (cache)
		QFile::setPermissions(to.filePath(file), QFile::permissions(to.filePath(file)) | QFile::WriteOwner);
	}
	m_project = Project::load(m_dir->path());
	QVERIFY(m_project);
	QVERIFY(m_project->loadDatabaseAndMapTree());
	// the texts of the commands (Stringizer) are those of the open project
	core().project() = m_project;
	auto& database = m_project->database();

	// a common event run by switch 4000, using 4000 in many ways
	lcf::rpg::CommonEvent common;
	m_commonEvent = database.commonevents.empty() ? 1 : database.commonevents.back().ID + 1;
	common.ID = m_commonEvent;
	common.name = lcf::DBString("Search Test Event");
	common.switch_flag = true;
	common.switch_id = Needle;
	lcf::rpg::EventCommand move = command(Cmd::MoveEvent, {});
	MoveRouteCodec::MoveEvent route;
	route.route = switchRoute(lcf::rpg::MoveCommand::Code::switch_on);
	MoveRouteCodec::encode(route, move, m_project->encoding().toStdString());
	common.event_commands = {
		// 1: variable 4000 set
		command(Cmd::ControlVars, {0, Needle, Needle, 0, 0, 5, 0}),
		// 2: shown in a message
		command(Cmd::ShowMessage, {}, "Value \\V[4000] here"),
		// 3: switch 4000 tested
		command(Cmd::ConditionalBranch, {0, Needle, 1, 0, 0, 0}),
		// 4: item 4000 given
		command(Cmd::ChangeItems, {0, 0, Needle, 0, 1}),
		// 5: nothing of 4000
		command(Cmd::ShowMessage, {}, "zz unique needle zz"),
		// 6: switches 3990 to 4010
		command(Cmd::ControlSwitches, {1, Needle - 10, Needle + 10, 0}),
		// 7: variable 1 set to variable 4000
		command(Cmd::ControlVars, {0, 1, 1, 0, 1, Needle}),
		// 8: switch 4000 turned on by a move
		move,
	};
	database.commonevents.push_back(common);

	// a monster group whose battle event waits for variable 4000
	lcf::rpg::Troop troop;
	m_troop = database.troops.empty() ? 1 : database.troops.back().ID + 1;
	troop.ID = m_troop;
	troop.name = lcf::DBString("Search Test Troop");
	lcf::rpg::TroopPage battle;
	battle.ID = 1;
	battle.condition.flags.variable = true;
	battle.condition.variable_id = Needle;
	battle.event_commands = {command(Cmd::ControlVars, {0, Needle, Needle, 0, 0, 1, 0})};
	troop.pages = {battle};
	database.troops.push_back(troop);

	// a map open in the Editor, its edits not saved: an event shown by switch 4000
	const auto& maps = m_project->treeMap().maps;
	const auto first = std::find_if(maps.begin(), maps.end(), [](const auto& info) {
		return info.ID > 0 && info.type == lcf::rpg::TreeMap::MapType_map;
	});
	QVERIFY(first != maps.end());
	m_openMap = first->ID;
	auto open = std::make_shared<lcf::rpg::Map>();
	lcf::rpg::Event event;
	event.ID = 1;
	event.name = lcf::DBString("Needle Event");
	lcf::rpg::EventPage page;
	page.ID = 1;
	page.condition.flags.switch_a = true;
	page.condition.switch_a_id = Needle;
	page.character_name = lcf::DBString("Actor1");
	page.character_index = 1;
	page.character_direction = 2;
	page.character_pattern = 1;
	page.move_route = switchRoute(lcf::rpg::MoveCommand::Code::switch_off);
	page.event_commands = {command(Cmd::ControlVars, {0, Needle, Needle, 0, 0, 2, 0})};
	event.pages = {page};
	open->events = {event};
	m_source = [this, open](int map_id) -> std::shared_ptr<const lcf::rpg::Map> {
		return map_id == m_openMap ? open : nullptr;
	};
}

void ProjectSearchTest::cleanupTestCase()
{
	core().project().reset();
	m_project.reset();
}

QList<Found> ProjectSearchTest::found(const ProjectSearch& search, int map_id) const
{
	const int event = map_id == ProjectSearch::CommonEventResults ? m_commonEvent : map_id == ProjectSearch::TroopResults ? m_troop : 1;
	QList<Found> list;
	for (const SearchResult& result : search.results()->results()) {
		if (result.mapId == map_id && result.eventId == event) {
			list.append({result.pageId, result.line, result.type});
		}
	}
	return list;
}

void ProjectSearchTest::choices()
{
	ProjectSearch search(m_project->projectData());
	const QVariantList types = search.typeChoices();
	QCOMPARE(types.size(), 6);
	for (int i = 0; i < types.size(); ++i) {
		QCOMPARE(types.at(i).toMap().value(QStringLiteral("value")).toInt(), i);
	}
	QCOMPARE(types.at(ProjectSearch::EventText).toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Event text"));
	const QVariantList scopes = search.scopeChoices();
	QCOMPARE(scopes.size(), 3);
	QCOMPARE(scopes.at(ProjectSearch::CurrentMap).toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Current map"));
	// what a QML window binds to
	QCOMPARE(search.property("results").value<SearchResultsModel*>(), search.results());
	QSignalSpy texts(&search, &ProjectSearch::textChanged);
	QVERIFY(search.setProperty("text", QStringLiteral("abc")));
	QVERIFY(search.setProperty("type", static_cast<int>(ProjectSearch::Items)));
	QCOMPARE(texts.count(), 1);
	QCOMPARE(search.type(), ProjectSearch::Items);
}

void ProjectSearchTest::emptyText()
{
	ProjectSearch search(m_project->projectData());
	search.setText(QStringLiteral("4000"));
	QVERIFY(search.search() > 0);
	search.setText(QStringLiteral("   "));
	QCOMPARE(search.search(), 0);
	QCOMPARE(search.results()->rowCount(), 0);
}

void ProjectSearchTest::references_data()
{
	QTest::addColumn<QString>("text");
	QTest::addColumn<int>("type");
	QTest::addColumn<QList<Found>>("common");
	QTest::addColumn<QList<Found>>("troop");
	QTest::addColumn<QList<Found>>("map");

	const QString variable = QStringLiteral("Variable");
	const QString zwitch = QStringLiteral("Switch");
	const QString item = QStringLiteral("Item");
	const QList<Found> variables = {{0, 1, variable}, {0, 2, variable}, {0, 7, variable}};
	const QList<Found> switches = {{0, 0, zwitch}, {0, 3, zwitch}, {0, 6, zwitch}, {0, 8, zwitch}};
	const QList<Found> troop = {{1, 0, variable}, {1, 1, variable}};
	// the condition of the page, its route, its command
	const QList<Found> map_variables = {{1, 1, variable}};
	const QList<Found> map_switches = {{1, 0, zwitch}, {1, 0, zwitch}};

	QTest::newRow("variables") << QStringLiteral("4000") << int(ProjectSearch::Variables)
		<< variables << troop << map_variables;
	QTest::newRow("switches") << QStringLiteral("4000") << int(ProjectSearch::Switches)
		<< switches << QList<Found>() << map_switches;
	QTest::newRow("items") << QStringLiteral("4000") << int(ProjectSearch::Items)
		<< QList<Found>{{0, 4, item}} << QList<Found>() << QList<Found>();
	// all of them, and the message also by its text
	const QList<Found> all = {{0, 0, zwitch}, {0, 1, variable}, {0, 2, QStringLiteral("Variable, Event text")},
		{0, 3, zwitch}, {0, 4, item}, {0, 6, zwitch}, {0, 7, variable}, {0, 8, zwitch}};
	const QList<Found> map_all = {{1, 0, zwitch}, {1, 0, zwitch}, {1, 1, variable}};
	QTest::newRow("all") << QStringLiteral("4000") << int(ProjectSearch::AllTypes) << all << troop << map_all;
	// as the suggestions complete it
	QTest::newRow("suggested") << QStringLiteral("4000: Gone") << int(ProjectSearch::AllTypes)
		<< QList<Found>{{0, 0, zwitch}, {0, 1, variable}, {0, 2, variable}, {0, 3, zwitch}, {0, 4, item}, {0, 6, zwitch},
			{0, 7, variable}, {0, 8, zwitch}} << troop << map_all;
	QTest::newRow("other number") << QStringLiteral("4001") << int(ProjectSearch::AllTypes)
		<< QList<Found>{{0, 6, zwitch}} << QList<Found>() << QList<Found>();
}

void ProjectSearchTest::references()
{
	QFETCH(QString, text);
	QFETCH(int, type);
	QFETCH(QList<Found>, common);
	QFETCH(QList<Found>, troop);
	QFETCH(QList<Found>, map);
	ProjectSearch search(m_project->projectData());
	search.setMapSource(m_source);
	search.setText(text);
	search.setType(static_cast<ProjectSearch::Type>(type));
	search.search();
	QCOMPARE(found(search, ProjectSearch::CommonEventResults), common);
	QCOMPARE(found(search, ProjectSearch::TroopResults), troop);
	QCOMPARE(found(search, m_openMap), map);
}

void ProjectSearchTest::eventText()
{
	ProjectSearch search(m_project->projectData());
	search.setText(QStringLiteral("UNIQUE needle"));
	search.setType(ProjectSearch::EventText);
	QCOMPARE(search.search(), 1);
	const SearchResult& result = search.results()->results().front();
	QCOMPARE(result.mapId, ProjectSearch::CommonEventResults);
	QCOMPARE(result.eventId, m_commonEvent);
	QCOMPARE(result.eventName, QStringLiteral("Search Test Event"));
	QCOMPARE(result.location, QStringLiteral("Common Events"));
	QCOMPARE(result.line, 5);
	QCOMPARE(result.type, QStringLiteral("Event text"));
	QCOMPARE(result.kind, Kind::Text);
	QVERIFY2(result.match.contains(QStringLiteral("zz unique needle zz")), qPrintable(result.match));
	// a number is a reference, not a text
	search.setText(QStringLiteral("4000"));
	search.search();
	QCOMPARE(found(search, ProjectSearch::CommonEventResults), (QList<Found>{{0, 2, QStringLiteral("Event text")}}));
}

void ProjectSearchTest::eventNames()
{
	ProjectSearch search(m_project->projectData());
	search.setMapSource(m_source);
	search.setType(ProjectSearch::EventNames);
	search.setText(QStringLiteral("needle EVENT"));
	search.search();
	QCOMPARE(found(search, m_openMap), (QList<Found>{{0, 0, QStringLiteral("Event")}}));
	search.setText(QStringLiteral("search test event"));
	search.search();
	QCOMPARE(found(search, ProjectSearch::CommonEventResults), (QList<Found>{{0, 0, QStringLiteral("Common event")}}));
	const auto& results = search.results()->results();
	const auto common = std::find_if(results.begin(), results.end(), [this](const SearchResult& result) {
		return result.mapId == ProjectSearch::CommonEventResults && result.eventId == m_commonEvent;
	});
	QCOMPARE(common->match, QStringLiteral("Search Test Event"));
	QCOMPARE(common->kind, Kind::Name);
	// the name of a monster group is not an event's
	search.setText(QStringLiteral("Search Test Troop"));
	search.search();
	QCOMPARE(found(search, ProjectSearch::TroopResults), QList<Found>());
}

void ProjectSearchTest::scopes()
{
	ProjectSearch search(m_project->projectData());
	search.setMapSource(m_source);
	search.setText(QStringLiteral("4000"));
	auto maps = [&] {
		QSet<int> ids;
		for (const SearchResult& result : search.results()->results()) {
			ids.insert(result.mapId);
		}
		return ids;
	};
	search.setScope(ProjectSearch::CurrentMap);
	search.setCurrentMapId(m_openMap);
	QCOMPARE(search.search(), 3);
	QCOMPARE(maps(), QSet<int>{m_openMap});
	// no map open
	search.setCurrentMapId(0);
	QCOMPARE(search.search(), 0);
	search.setScope(ProjectSearch::CommonEvents);
	search.search();
	QCOMPARE(maps(), QSet<int>{ProjectSearch::CommonEventResults});
	QCOMPARE(found(search, ProjectSearch::CommonEventResults).size(), 8);
	search.setScope(ProjectSearch::WholeProject);
	search.search();
	QVERIFY(maps().contains(m_openMap));
	QVERIFY(maps().contains(ProjectSearch::TroopResults));
	QVERIFY(maps().contains(ProjectSearch::CommonEventResults));
}

void ProjectSearchTest::openMap()
{
	// a map open: as it is in the Editor, with what the map shows of the event
	ProjectSearch search(m_project->projectData());
	search.setMapSource(m_source);
	search.setScope(ProjectSearch::CurrentMap);
	search.setCurrentMapId(m_openMap);
	search.setText(QStringLiteral("4000"));
	QCOMPARE(search.search(), 3);
	const auto& maps = m_project->treeMap().maps;
	QStringList path;
	for (int id = m_openMap; id > 0;) {
		const auto info = std::find_if(maps.begin(), maps.end(), [id](const auto& map) { return map.ID == id; });
		QVERIFY(info != maps.end());
		path.prepend(ToQString(info->name));
		id = info->parent_map;
	}
	const SearchResult& condition = search.results()->results().front();
	QCOMPARE(condition.location, path.join(QStringLiteral(" > ")));
	QCOMPARE(condition.eventName, QStringLiteral("Needle Event"));
	QCOMPARE(condition.match, QStringLiteral("Event page condition"));
	QCOMPARE(condition.kind, Kind::Switch);
	// CharSet Actor1, its second character, facing down, the middle pattern
	QCOMPARE(condition.charsetFrame, QRect(96, 64, 24, 32));
	QCOMPARE(QFileInfo(condition.charsetFile).fileName(), QStringLiteral("Actor1.png"));
	QCOMPARE(search.results()->results().at(1).match, QStringLiteral("Custom movement pattern"));
	QCOMPARE(search.results()->results().at(2).line, 1);
	QCOMPARE(search.results()->results().at(2).kind, Kind::Variable);
}

void ProjectSearchTest::resultsModel()
{
	ProjectSearch search(m_project->projectData());
	search.setMapSource(m_source);
	SearchResultsModel* model = search.results();
	QAbstractItemModelTester tester(model, QAbstractItemModelTester::FailureReportingMode::QtTest);
	QSignalSpy counts(model, &SearchResultsModel::countChanged);
	QSignalSpy resets(model, &QAbstractItemModel::modelReset);
	search.setText(QStringLiteral("4000"));
	const int count = search.search();
	QVERIFY(count >= 13);
	QCOMPARE(model->rowCount(), count);
	QCOMPARE(model->count(), count);
	QCOMPARE(model->columnCount(), 5);
	QCOMPARE(counts.count(), 1);
	QCOMPARE(resets.count(), 1);

	QCOMPARE(model->headerData(SearchResultsModel::MapColumn, Qt::Horizontal).toString(), QStringLiteral("Map"));
	QCOMPARE(model->headerData(SearchResultsModel::MatchColumn, Qt::Horizontal).toString(), QStringLiteral("Match"));
	const QHash<int, QByteArray> roles = model->roleNames();
	QCOMPARE(roles.value(SearchResultsModel::MapIdRole), QByteArray("mapId"));
	QCOMPARE(roles.value(SearchResultsModel::LineRole), QByteArray("line"));
	QCOMPARE(roles.value(Qt::DisplayRole), QByteArray("display"));

	// the columns of a line of the common event, and what opens it
	int row = -1;
	for (int i = 0; i < model->rowCount(); ++i) {
		const SearchResult* result = model->result(i);
		if (result->mapId == ProjectSearch::CommonEventResults && result->eventId == m_commonEvent && result->line == 4) {
			row = i;
		}
	}
	QVERIFY(row >= 0);
	auto cell = [&](int column, int role = Qt::DisplayRole) { return model->index(row, column).data(role); };
	QCOMPARE(cell(SearchResultsModel::MapColumn).toString(), QStringLiteral("Common Events"));
	QCOMPARE(cell(SearchResultsModel::MapColumn, Qt::ToolTipRole).toString(), QStringLiteral("Common Events"));
	QCOMPARE(cell(SearchResultsModel::EventColumn).toString(), QStringLiteral("%1: Search Test Event").arg(m_commonEvent));
	QCOMPARE(cell(SearchResultsModel::PageColumn).toString(), QString());
	QCOMPARE(cell(SearchResultsModel::TypeColumn).toString(), QStringLiteral("Item"));
	QVERIFY(!cell(SearchResultsModel::MatchColumn).toString().isEmpty());
	// the same roles on every column
	for (int column = 0; column < model->columnCount(); ++column) {
		QCOMPARE(cell(column, SearchResultsModel::MapIdRole).toInt(), ProjectSearch::CommonEventResults);
		QCOMPARE(cell(column, SearchResultsModel::EventIdRole).toInt(), m_commonEvent);
		QCOMPARE(cell(column, SearchResultsModel::PageIdRole).toInt(), 0);
		QCOMPARE(cell(column, SearchResultsModel::LineRole).toInt(), 4);
	}
	QCOMPARE(cell(0, SearchResultsModel::EventNameRole).toString(), QStringLiteral("Search Test Event"));
	QCOMPARE(cell(0, SearchResultsModel::MatchRole), cell(SearchResultsModel::MatchColumn));
	QCOMPARE(cell(0, SearchResultsModel::LocationIconRole).toString(), QStringLiteral("database"));
	QCOMPARE(cell(0, SearchResultsModel::EventIconRole).toString(), QStringLiteral("code"));
	QCOMPARE(cell(0, SearchResultsModel::TypeIconRole).toString(), QStringLiteral("resources"));
	QCOMPARE(cell(0, SearchResultsModel::CharsetFileRole).toString(), QString());
	QCOMPARE(model->rowCount(model->index(row, 0)), 0);
	QCOMPARE(model->result(-1), nullptr);
	QCOMPARE(model->result(count), nullptr);

	// a map's event: its page, its graphic
	for (int i = 0; i < model->rowCount(); ++i) {
		if (model->result(i)->mapId == m_openMap) {
			row = i;
			break;
		}
	}
	QCOMPARE(cell(SearchResultsModel::PageColumn).toString(), QStringLiteral("1"));
	QCOMPARE(cell(0, SearchResultsModel::LocationIconRole).toString(), QStringLiteral("tree"));
	QCOMPARE(cell(0, SearchResultsModel::EventIconRole).toString(), QStringLiteral("layer-event"));
	QCOMPARE(cell(0, SearchResultsModel::TypeIconRole).toString(), QStringLiteral("switch"));
	QCOMPARE(cell(0, SearchResultsModel::CharsetFrameRole).toRect(), QRect(96, 64, 24, 32));

	// a new search replaces them
	search.setText(QStringLiteral("unique needle"));
	QCOMPARE(search.search(), 1);
	QCOMPARE(model->rowCount(), 1);
	QCOMPARE(counts.count(), 2);
	model->clear();
	QCOMPARE(model->rowCount(), 0);
	QCOMPARE(counts.count(), 3);
	QCOMPARE(resets.count(), 3);
}

void ProjectSearchTest::wholeGame()
{
	// the maps of the game, as their files are: the results are in them
	ProjectSearch search(m_project->projectData());
	search.setText(QStringLiteral("1"));
	QVERIFY(search.search() > 0);
	const auto& maps = m_project->treeMap().maps;
	bool on_maps = false;
	for (const SearchResult& result : search.results()->results()) {
		QVERIFY(!result.location.isEmpty());
		QVERIFY(!result.type.isEmpty());
		QVERIFY(result.line >= 0);
		if (result.mapId > 0) {
			on_maps = true;
			QVERIFY(std::any_of(maps.begin(), maps.end(), [&](const auto& map) { return map.ID == result.mapId; }));
		}
	}
	QVERIFY(on_maps);
}

void ProjectSearchTest::cache()
{
	// a map of the game with a named event, other than the one open
	int map_id = 0;
	QString name;
	for (const auto& info : m_project->treeMap().maps) {
		if (info.ID <= 0 || info.ID == m_openMap || info.type != lcf::rpg::TreeMap::MapType_map) {
			continue;
		}
		const auto map = m_project->loadMap(info.ID);
		if (!map) {
			continue;
		}
		const auto event = std::find_if(map->events.begin(), map->events.end(), [](const auto& event) {
			return ToQString(event.name).trimmed().size() > 3;
		});
		if (event != map->events.end()) {
			map_id = info.ID;
			name = ToQString(event->name).trimmed();
			break;
		}
	}
	QVERIFY(map_id > 0);
	const lcf::rpg::Map original = *m_project->loadMap(map_id);
	lcf::rpg::Map empty = original;
	empty.events.clear();

	ProjectSearch search(m_project->projectData());
	search.setText(name);
	search.setType(ProjectSearch::EventNames);
	auto onMap = [&] {
		search.search();
		const auto& results = search.results()->results();
		return std::any_of(results.begin(), results.end(), [&](const SearchResult& result) { return result.mapId == map_id; });
	};
	QSignalSpy cached(&search, &ProjectSearch::cacheEnabledChanged);
	search.setCacheEnabled(true);
	QCOMPARE(cached.count(), 1);
	QVERIFY(onMap());

	// the map changed on disk: as read, until the cache is emptied
	QVERIFY(m_project->saveMap(empty, map_id, false));
	QVERIFY(onMap());
	search.clearCache();
	QVERIFY(!onMap());

	// without the cache: read each time
	search.setCacheEnabled(false);
	QCOMPARE(cached.count(), 2);
	lcf::rpg::Map restored = original;
	QVERIFY(m_project->saveMap(restored, map_id, false));
	QVERIFY(onMap());
	QVERIFY(m_project->saveMap(empty, map_id, false));
	QVERIFY(!onMap());
}

void ProjectSearchTest::suggestions()
{
	ProjectSearch search(m_project->projectData());
	SearchSuggestions* suggestions = search.suggestions();
	QAbstractItemModelTester tester(suggestions, QAbstractItemModelTester::FailureReportingMode::QtTest);
	QCOMPARE(search.property("suggestions").value<SearchSuggestions*>(), suggestions);
	// nothing typed: nothing suggested
	QCOMPARE(suggestions->rowCount(), 0);

	// a number: that entry of each type first (variable, switch, item, common event)
	suggestions->setQuery(QStringLiteral("1"));
	QVERIFY(suggestions->rowCount() >= 4);
	QCOMPARE(suggestions->labelAt(0).section(u':', 0, 0), QStringLiteral("1"));
	QVERIFY(suggestions->labelAt(0).contains(QStringLiteral("Variable")));
	QCOMPARE(suggestions->typeAt(0), static_cast<int>(ProjectSearch::Variables));
	QCOMPARE(suggestions->typeAt(1), static_cast<int>(ProjectSearch::Switches));
	QCOMPARE(suggestions->typeAt(2), static_cast<int>(ProjectSearch::Items));
	QCOMPARE(suggestions->typeAt(3), static_cast<int>(ProjectSearch::EventNames));
	QCOMPARE(suggestions->index(0).data(SearchSuggestions::IconRole).toString(), QStringLiteral("variable"));
	// a suggestion chosen ("1: Variable — ...") is that number
	suggestions->setQuery(suggestions->labelAt(1));
	QCOMPARE(suggestions->typeAt(0), static_cast<int>(ProjectSearch::Variables));
	QCOMPARE(suggestions->index(0).data(SearchSuggestions::IdRole).toInt(), 1);

	// a name: the start of it before anywhere in it; of the type chosen only
	search.setType(ProjectSearch::EventNames);
	suggestions->setQuery(QStringLiteral("search test"));
	QCOMPARE(suggestions->rowCount(), 1);
	QCOMPARE(suggestions->labelAt(0), QStringLiteral("%1: Common event — Search Test Event").arg(m_commonEvent));
	QCOMPARE(suggestions->typeAt(0), static_cast<int>(ProjectSearch::EventNames));
	QCOMPARE(suggestions->typeAt(1), -1);
	suggestions->setQuery(QStringLiteral("zz no entry zz"));
	QCOMPARE(suggestions->rowCount(), 0);
}

void ProjectSearchTest::openAndReset()
{
	ProjectSearch search(m_project->projectData());
	search.setMapSource(m_source);
	search.setCurrentMapSource([] { return 7; });
	search.setText(QStringLiteral("4000"));
	search.setType(ProjectSearch::Items);
	search.setScope(ProjectSearch::CommonEvents);
	QVERIFY(search.search() > 0);
	// the map shown is the current map of each search
	QCOMPARE(search.currentMapId(), 7);

	QSignalSpy opened(&search, &ProjectSearch::openRequested);
	search.open(0);
	QCOMPARE(opened.count(), 1);
	const SearchResult* first = search.results()->result(0);
	QCOMPARE(opened.first().at(0).toInt(), first->mapId);
	QCOMPARE(opened.first().at(1).toInt(), first->eventId);
	QCOMPARE(opened.first().at(2).toInt(), first->pageId);
	QCOMPARE(opened.first().at(3).toInt(), first->line);
	search.open(-1);
	search.open(search.results()->rowCount());
	QCOMPARE(opened.count(), 1);

	// the local state reset: a new search
	search.reset();
	QCOMPARE(search.text(), QString());
	QCOMPARE(search.type(), ProjectSearch::AllTypes);
	QCOMPARE(search.scope(), ProjectSearch::WholeProject);
	QCOMPARE(search.results()->rowCount(), 0);
}

QTEST_MAIN(ProjectSearchTest)

#include "project_search_test.moc"
