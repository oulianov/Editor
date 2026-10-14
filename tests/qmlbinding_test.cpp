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

// The QML binding of the project's data (src/qmlbinding) without QML: the
// values read and written through JSON pointers (converted, clamped, bits),
// the signals of the changes (at, above and under a pointer, batched), the
// views and lists made once per pointer, the list operations of the
// database windows (Array Size, Copy, Paste, Clear...) checked by
// QAbstractItemModelTester, JsonField, EntryFilterModel and DatabaseSession.
// With EDITOR_TEST_PROJECT (TestGame-2003), every value of its database is
// read and written back through its pointer.

#include <QAbstractItemModelTester>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QSignalSpy>
#include <QTest>
#include <lcf/ldb/reader.h>
#include "common/dbstring.h"
#include "controller/dialog_service.h"
#include "controller/edit_locks.h"
#include "database_session.h"
#include "entry_filter_model.h"
#include "json_field.h"
#include "json_list_view.h"
#include "json_t.h"
#include "json_view.h"
#include "model/database_list_data.h"
#include "model/project_data.h"
#include "model/project_watcher.h"
#include "model/vocabulary_fields.h"

using lcf::rpg::Database;

namespace {

/** The pointers of the changes of a Json */
class Changes {
public:
	explicit Changes(QObject* sender, const char* signal = SIGNAL(valueChanged(QString))) : m_spy(sender, signal) {}
	QStringList take() {
		QStringList pointers;
		for (const auto& arguments : std::as_const(m_spy)) {
			pointers << arguments.value(0).toString();
		}
		m_spy.clear();
		return pointers;
	}

private:
	QSignalSpy m_spy;
};

/** Every value of a JSON document with its pointer */
template<class F>
void forEachLeaf(const QJsonValue& value, const QString& pointer, F&& leaf) {
	if (value.isObject()) {
		const QJsonObject object = value.toObject();
		for (auto it = object.begin(); it != object.end(); ++it) {
			forEachLeaf(it.value(), pointer + u'/' + it.key(), leaf);
		}
	} else if (value.isArray()) {
		const QJsonArray array = value.toArray();
		for (qsizetype i = 0; i < array.size(); ++i) {
			forEachLeaf(array.at(i), pointer + u'/' + QString::number(i), leaf);
		}
	} else {
		leaf(pointer, value);
	}
}

/** The file of a database that stays in memory: written when asked */
DatabaseApply::File memoryFile(int* saves) {
	DatabaseApply::File file;
	file.isStale = [] { return false; };
	file.read = [] { return std::unique_ptr<Database>(); };
	file.save = [saves] {
		++*saves;
		return true;
	};
	file.reload = [] {};
	return file;
}

} // namespace

class QmlBindingTest : public QObject
{
	Q_OBJECT

private slots:
	void init();
	void pointers();
	void reads();
	void writesConverted();
	void signalsChanges();
	void bits();
	void cachedViews();
	void viewSignals();
	void batches();
	void listModel();
	void listOperations();
	void listOperationsReachViews();
	void listRename();
	void fallbackRow();
	void choiceLists();
	void jsonField();
	void entryFilter();
	void session();
	void sessionLock();
	void vocabularyTerms();
	void testGameLeaves();

private:
	Database m_db;
	std::unique_ptr<JsonT<Database>> m_json;
};

void QmlBindingTest::init()
{
	DatabaseList::clipboard<lcf::rpg::Actor>().clear();
	m_db = Database();
	m_db.system.ldb_id = 2003;
	m_db.ldb_header = "LcfDataBase";
	const char* names[] = {"Alex", "Brian", "Cherry"};
	for (int id = 1; id <= 3; ++id) {
		auto actor = DatabaseList::defaultEntry<lcf::rpg::Actor>(true, id);
		actor.name = lcf::DBString(names[id - 1]);
		actor.state_ranks = {2, 2};
		m_db.actors.push_back(actor);
		m_db.states.push_back(DatabaseList::defaultEntry<lcf::rpg::State>(true, id));
	}
	auto item = DatabaseList::defaultEntry<lcf::rpg::Item>(true, 1);
	item.name = lcf::DBString("Potion");
	item.state_set = lcf::DBBitArray({true, false});
	m_db.items.push_back(item);
	m_json = std::make_unique<JsonT<Database>>();
	m_json->setData(&m_db);
}

void QmlBindingTest::pointers()
{
	QCOMPARE(Json::normalize(QString()), QString());
	QCOMPARE(Json::normalize(QStringLiteral("/")), QString());
	QCOMPARE(Json::normalize(QStringLiteral("actors/0/name")), QStringLiteral("/actors/0/name"));
	QCOMPARE(Json::normalize(QStringLiteral("/actors/0/")), QStringLiteral("/actors/0"));
	QCOMPARE(Json::join(QStringLiteral("/actors/0"), QStringLiteral("name")), QStringLiteral("/actors/0/name"));
	QCOMPARE(Json::join(QString(), QStringLiteral("/actors/")), QStringLiteral("/actors"));
	QVERIFY(Json::affects(QString(), QStringLiteral("/actors")));
	QVERIFY(Json::affects(QStringLiteral("/actors"), QStringLiteral("/actors/1/name")));
	QVERIFY(Json::affects(QStringLiteral("/actors/1/name"), QStringLiteral("/actors")));
	QVERIFY(!Json::affects(QStringLiteral("/actors/1"), QStringLiteral("/actors/10")));
	QVERIFY(!Json::affects(QStringLiteral("/actors/0/name"), QStringLiteral("/actors/1/name")));
}

void QmlBindingTest::reads()
{
	Json& json = *m_json;
	QCOMPARE(json.str("/actors/1/name"), QStringLiteral("Brian"));
	QCOMPARE(json.str("actors/1/name/"), QStringLiteral("Brian"));
	QCOMPARE(json.str("/ldb_header"), QStringLiteral("LcfDataBase"));
	QCOMPARE(json.num("/actors/1/ID"), 2);
	QCOMPARE(json.num("/actors/0/state_ranks/1"), 2);
	QCOMPARE(json.num("/system/ldb_id"), 2003);
	QCOMPARE(json.num("/actors/0/transparent"), 0);
	QCOMPARE(json.boolean("/actors/0/critical_hit"), true);
	QCOMPARE(json.value("/actors/0/state_ranks"), QVariant(QVariantList{2, 2}));
	QCOMPARE(json.value("/system/party").toList().size(), 1);
	QCOMPARE(json.size("/actors"), 3);
	QCOMPARE(json.size("/actors/0/name"), -1);
	QVERIFY(json.exists("/actors/2"));
	QVERIFY(!json.exists("/actors/3"));
	QVERIFY(!json.exists("/nothing"));
	QVERIFY(!json.value("/actors/0").isValid());
	// what upstream's readers give for a wrong pointer
	QCOMPARE(json.str("/actors/0/ID"), QStringLiteral("!BAD POINTER!"));
	QCOMPARE(json.num("/actors/0/name"), -0xEA51);
	// the JSON of the value, not of the whole database
	QCOMPARE(json.toJson("/actors/1/name"), QStringLiteral("\"Brian\""));
	QCOMPARE(json.toJson("/items/0/state_set"), QStringLiteral("[true,false]"));
	QVERIFY(json.toJson("/actors/1").startsWith(u'{'));
	QVERIFY(!json.toJson("/actors/1").contains(QStringLiteral("Cherry")));
}

void QmlBindingTest::writesConverted()
{
	Json& json = *m_json;
	const QString weapon = QStringLiteral("/actors/0/initial_equipment/weapon_id"); // int16_t
	QVERIFY(json.set(weapon, 70000));
	QCOMPARE(m_db.actors[0].initial_equipment.weapon_id, int16_t(32767));
	QVERIFY(json.set(weapon, -5.6));
	QCOMPARE(m_db.actors[0].initial_equipment.weapon_id, int16_t(-6));
	QVERIFY(json.set(weapon, QStringLiteral("12")));
	QCOMPARE(m_db.actors[0].initial_equipment.weapon_id, int16_t(12));
	QVERIFY(json.set(weapon, true));
	QCOMPARE(m_db.actors[0].initial_equipment.weapon_id, int16_t(1));
	QVERIFY(!json.set(weapon, QStringLiteral("twelve")));
	QCOMPARE(m_db.actors[0].initial_equipment.weapon_id, int16_t(1));

	const QString rank = QStringLiteral("/actors/0/state_ranks/0"); // uint8_t
	QVERIFY(json.set(rank, 300));
	QCOMPARE(m_db.actors[0].state_ranks[0], uint8_t(255));
	QVERIFY(json.set(rank, -1));
	QCOMPARE(m_db.actors[0].state_ranks[0], uint8_t(0));

	QVERIFY(json.set("/actors/0/initial_level", qint64(1) << 40)); // int32_t
	QCOMPARE(m_db.actors[0].initial_level, std::numeric_limits<int32_t>::max());

	QVERIFY(json.set("/actors/0/transparent", 1));
	QCOMPARE(m_db.actors[0].transparent, true);
	QVERIFY(json.set("/actors/0/transparent", QStringLiteral("false")));
	QCOMPARE(m_db.actors[0].transparent, false);

	QVERIFY(json.set("/actors/0/name", QStringLiteral("Anne-Hélène")));
	QCOMPARE(ToQString(m_db.actors[0].name), QStringLiteral("Anne-Hélène"));
	QVERIFY(json.set("/system/title_music/name", QStringLiteral("Town"))); // std::string
	QCOMPARE(m_db.system.title_music.name, std::string("Town"));
	QVERIFY(json.set("/actors/0/name", 42));
	QCOMPARE(ToQString(m_db.actors[0].name), QStringLiteral("42"));

	// arrays of numbers: element by element or whole
	QVERIFY(json.set("/system/party", QVariantList{2, 3}));
	QCOMPARE(m_db.system.party, (std::vector<int16_t>{2, 3}));
	QVERIFY(!json.set("/system/party", QVariantList{2, QStringLiteral("x")}));
	QCOMPARE(m_db.system.party, (std::vector<int16_t>{2, 3}));

	// objects: through their fields only
	QVERIFY(!json.set("/actors/0", QStringLiteral("x")));
	QVERIFY(!json.set("/actors/9/name", QStringLiteral("x")));
}

void QmlBindingTest::signalsChanges()
{
	Changes changes(m_json.get());
	QVERIFY(m_json->set("actors/1/name/", QStringLiteral("Brian")));
	QCOMPARE(changes.take(), QStringList());
	QVERIFY(m_json->set("actors/1/name/", QStringLiteral("Bob")));
	QCOMPARE(changes.take(), QStringList{QStringLiteral("/actors/1/name")});
	// clamped to the value it has: no change
	m_db.actors[0].state_ranks[0] = 255;
	QVERIFY(m_json->set("/actors/0/state_ranks/0", 1000));
	QCOMPARE(changes.take(), QStringList());
	m_json->notifyChanged("/actors/");
	QCOMPARE(changes.take(), QStringList{QStringLiteral("/actors")});
}

void QmlBindingTest::bits()
{
	Json& json = *m_json;
	QCOMPARE(json.bit("/items/0/state_set", 0), true);
	QCOMPARE(json.bit("/items/0/state_set", 1), false);
	// beyond the end: what the caller says the missing bits are
	QCOMPARE(json.bit("/items/0/state_set", 7), false);
	QCOMPARE(json.bit("/items/0/state_set", 7, true), true);
	QCOMPARE(json.boolean("/items/0/state_set/0"), true);
	QCOMPARE(json.value("/items/0/state_set/1"), QVariant(false));
	QCOMPARE(json.value("/items/0/state_set"), QVariant(QVariantList{true, false}));

	Changes changes(m_json.get());
	QVERIFY(json.setBit("/items/0/state_set", 1, true));
	QCOMPARE(m_db.items[0].state_set.size(), size_t(2));
	QCOMPARE(bool(m_db.items[0].state_set[1]), true);
	QCOMPARE(changes.take(), QStringList{QStringLiteral("/items/0/state_set")});
	QVERIFY(json.setBit("/items/0/state_set", 1, true));
	QCOMPARE(changes.take(), QStringList());

	// grown with the missing bits
	QVERIFY(json.setBit("/items/0/state_set", 4, false, true));
	QCOMPARE(m_db.items[0].state_set.size(), size_t(5));
	QCOMPARE(bool(m_db.items[0].state_set[2]), true);
	QCOMPARE(bool(m_db.items[0].state_set[3]), true);
	QCOMPARE(bool(m_db.items[0].state_set[4]), false);
	QVERIFY(json.set("/items/0/state_set/6", true));
	QCOMPARE(m_db.items[0].state_set.size(), size_t(7));
	QCOMPARE(bool(m_db.items[0].state_set[5]), false);
	QCOMPARE(bool(m_db.items[0].state_set[6]), true);
	QCOMPARE(json.size("/items/0/state_set"), 7);

	QVERIFY(!json.setBit("/items/0/name", 0, true));
	QVERIFY(!json.setBit("/items/0/state_set", -1, true));
}

void QmlBindingTest::cachedViews()
{
	Json& json = *m_json;
	JsonView* view = json.view("/actors/1");
	QVERIFY(view);
	QCOMPARE(json.view("actors/1/"), view);
	QCOMPARE(qvariant_cast<JsonView*>(json.subtree("/actors/1")), view);
	QCOMPARE(json.view(QString())->subtree("actors")->subtree("1"), view);
	QCOMPARE(view->pathPrefix(), QStringLiteral("/actors/1"));
	QCOMPARE(view->str("name"), QStringLiteral("Brian"));
	QCOMPARE(view->num("/ID"), 2);

	JsonListView* list = json.listView("/actors");
	QVERIFY(list);
	QCOMPARE(qvariant_cast<JsonListView*>(json.list("actors")), list);
	QCOMPARE(json.view(QString())->list("actors"), list);
	QCOMPARE(list->entry(1), view);
	QCOMPARE(list->str("/1/name"), QStringLiteral("Brian"));
	QCOMPARE(list->view(), json.view("/actors"));
	// no list of objects there
	QCOMPARE(json.listView("/system/party"), nullptr);
	QCOMPARE(json.listView("/actors/0/name"), nullptr);
	QVERIFY(json.listView("/actors/0/skills"));
}

void QmlBindingTest::viewSignals()
{
	JsonView* view = m_json->view("/actors/1");
	Changes changes(view);
	QSignalSpy valid(view, &JsonView::validChanged);
	QVERIFY(m_json->set("/actors/1/name", QStringLiteral("Bob")));
	QCOMPARE(changes.take(), QStringList{QStringLiteral("/name")});
	QVERIFY(m_json->set("/actors/0/name", QStringLiteral("Ann")));
	QCOMPARE(changes.take(), QStringList());
	QCOMPARE(valid.count(), 0);
	// the list holding the entry, the whole database: all of the view
	m_json->notifyChanged("/actors");
	QCOMPARE(changes.take(), QStringList{QString()});
	m_json->notifyChanged(QString());
	QCOMPARE(changes.take(), QStringList{QString()});
	QCOMPARE(valid.count(), 2);
	QVERIFY(view->isValid());
}

void QmlBindingTest::batches()
{
	Changes changes(m_json.get());
	m_json->beginBatch();
	QVERIFY(m_json->set("/actors/0/name", QStringLiteral("Ann")));
	m_json->beginBatch();
	QVERIFY(m_json->set("/actors/2/name", QStringLiteral("Cid")));
	m_json->endBatch();
	QCOMPARE(changes.take(), QStringList());
	m_json->endBatch();
	QCOMPARE(changes.take(), QStringList{QStringLiteral("/actors")});

	m_json->beginBatch();
	QVERIFY(m_json->set("/actors/0/name", QStringLiteral("Ann")));
	m_json->endBatch();
	QCOMPARE(changes.take(), QStringList());

	m_json->beginBatch();
	QVERIFY(m_json->set("/actors/0/name", QStringLiteral("Al")));
	QVERIFY(m_json->set("/items/0/name", QStringLiteral("Elixir")));
	m_json->endBatch();
	QCOMPARE(changes.take(), QStringList{QString()});
}

void QmlBindingTest::listModel()
{
	JsonListView* list = m_json->listView("/actors");
	QAbstractItemModelTester tester(list, QAbstractItemModelTester::FailureReportingMode::QtTest);
	QCOMPARE(list->rowCount(), 3);
	QCOMPARE(list->count(), 3);
	const QModelIndex second = list->index(1);
	QCOMPARE(second.data(JsonListView::NameRole), QVariant(QStringLiteral("Brian")));
	QCOMPARE(second.data(JsonListView::IdRole), QVariant(2));
	QCOMPARE(second.data(JsonListView::IndexRole), QVariant(1));
	QCOMPARE(second.data(Qt::DisplayRole), QVariant(QStringLiteral("0002: Brian")));
	QCOMPARE(second.data(JsonListView::TitleRole), QVariant(QString()));
	QCOMPARE(list->roleNames().value(JsonListView::IdRole), QByteArray("value"));

	QSignalSpy changed(list, &QAbstractItemModel::dataChanged);
	QVERIFY(m_json->set("/actors/1/name", QStringLiteral("Bob")));
	QCOMPARE(changed.count(), 1);
	QCOMPARE(changed.at(0).at(0).value<QModelIndex>(), second);
	QCOMPARE(second.data(Qt::DisplayRole), QVariant(QStringLiteral("0002: Bob")));

	// changed by C++ code, told to the Json: the rows follow
	m_db.actors.push_back(DatabaseList::defaultEntry<lcf::rpg::Actor>(true, 4));
	QCOMPARE(list->rowCount(), 3);
	m_json->notifyChanged("/actors");
	QCOMPARE(list->rowCount(), 4);
	m_db.actors.resize(2);
	m_json->notifyChanged(QString());
	QCOMPARE(list->rowCount(), 2);
	QCOMPARE(list->count(), 2);

	// a list in an entry: found again when the entry moves
	m_db.actors[1].skills.resize(2);
	JsonListView* skills = m_json->listView("/actors/1/skills");
	QAbstractItemModelTester skillTester(skills, QAbstractItemModelTester::FailureReportingMode::QtTest);
	QCOMPARE(skills->rowCount(), 2);
	QVERIFY(list->insertEntries(0, 1));
	QCOMPARE(skills->rowCount(), 0); // "/actors/1" is the new entry now
	QVERIFY(list->removeEntries(0, 1));
	QCOMPARE(skills->rowCount(), 2);
}

void QmlBindingTest::listOperations()
{
	JsonListView* list = m_json->listView("/actors");
	QAbstractItemModelTester tester(list, QAbstractItemModelTester::FailureReportingMode::QtTest);
	QSignalSpy notified(&DatabaseListNotifier::instance(), &DatabaseListNotifier::listChanged);

	// Array Size: new heroes as RPG Maker 2003 makes them
	QVERIFY(list->resize(5));
	QCOMPARE(m_db.actors.size(), size_t(5));
	QCOMPARE(m_db.actors[4].ID, 5);
	QCOMPARE(m_db.actors[4], DatabaseList::defaultEntry<lcf::rpg::Actor>(true, 5));
	QCOMPARE(list->rowCount(), 5);
	QCOMPARE(notified.count(), 1);
	QCOMPARE(notified.at(0).at(0).value<const void*>(), static_cast<const void*>(&m_db.actors));
	QVERIFY(list->isEmpty(4));
	QVERIFY(!list->isEmpty(0));
	QCOMPARE(list->entriesWithData(1), (QStringList{QStringLiteral("0002: Brian"), QStringLiteral("0003: Cherry")}));
	QVERIFY(!list->resize(5));
	QVERIFY(!list->resize(0));
	QVERIFY(list->resize(3));
	QCOMPARE(list->rowCount(), 3);

	// Copy and Paste over other entries: they keep their IDs
	list->copy(0, 2);
	QCOMPARE(list->clipboardSize(), 2);
	QCOMPARE(list->paste(2), 1); // the list does not grow
	QCOMPARE(ToQString(m_db.actors[2].name), QStringLiteral("Alex"));
	QCOMPARE(m_db.actors[2].ID, 3);
	QCOMPARE(list->index(2).data(), QVariant(QStringLiteral("0003: Alex")));

	// Clear, and the first condition RPG Maker keeps
	QVERIFY(list->clear(1));
	QCOMPARE(m_db.actors[1].ID, 2);
	QVERIFY(list->isEmpty(1));
	JsonListView* states = m_json->listView("/states");
	QVERIFY(!states->canClear(0));
	QVERIFY(!states->clear(0));
	QVERIFY(states->canClear(1));
	QVERIFY(!list->canClear(3));

	// in the middle: the IDs follow the rows
	QVERIFY(list->insertEntries(1, 2));
	QCOMPARE(m_db.actors.size(), size_t(5));
	for (int i = 0; i < 5; ++i) {
		QCOMPARE(m_db.actors[static_cast<size_t>(i)].ID, i + 1);
	}
	QVERIFY(list->moveEntry(0, 4));
	QCOMPARE(ToQString(m_db.actors[4].name), QStringLiteral("Alex"));
	QCOMPARE(m_db.actors[4].ID, 5);
	QCOMPARE(m_db.actors[0].ID, 1);
	QVERIFY(list->moveEntry(4, 0));
	QCOMPARE(ToQString(m_db.actors[0].name), QStringLiteral("Alex"));
	QVERIFY(list->removeEntries(1, 2));
	QCOMPARE(m_db.actors.size(), size_t(3));
	QCOMPARE(m_db.actors[2].ID, 3);
	QVERIFY(!list->removeEntries(2, 2));
	QVERIFY(!list->insertEntries(4, 1));
	QVERIFY(list->moveRows(QModelIndex(), 2, 1, QModelIndex(), 0));
	QCOMPARE(ToQString(m_db.actors[0].name), QStringLiteral("Alex")); // the pasted one
	QCOMPARE(m_db.actors[0].ID, 1);
}

void QmlBindingTest::listOperationsReachViews()
{
	JsonListView* list = m_json->listView("/actors");
	JsonView* third = m_json->view("/actors/2");
	Changes changes(third);
	JsonField name;
	name.setSource(third);
	name.setPath(QStringLiteral("name"));
	QCOMPARE(name.value(), QVariant(QStringLiteral("Cherry")));

	list->copy(0);
	QCOMPARE(list->paste(2), 1);
	QCOMPARE(changes.take(), QStringList{QString()});
	QCOMPARE(name.value(), QVariant(QStringLiteral("Alex")));

	// gone with a smaller list, back with a larger one
	QVERIFY(list->resize(2));
	QCOMPARE(changes.take(), QStringList{QString()});
	QVERIFY(!third->isValid());
	QVERIFY(!name.valid());
	QCOMPARE(third->str("name"), QStringLiteral("!BAD POINTER!"));
	QVERIFY(list->resize(3));
	QVERIFY(third->isValid());
	QVERIFY(name.valid());
	QCOMPARE(name.value(), QVariant(QString()));
}

void QmlBindingTest::listRename()
{
	// a combo box renames the entry it shows (its list with "(None)" first);
	// the views of the same list through another Json follow (another window)
	JsonListView* choices = m_json->choiceList("/actors", QStringLiteral("(None)"), 0);
	JsonT<Database> other;
	other.setData(&m_db);
	JsonListView* elsewhere = other.listView("/actors");
	QCOMPARE(elsewhere->index(1).data(JsonListView::NameRole), QVariant(QStringLiteral("Brian")));
	QSignalSpy changed(elsewhere, &QAbstractItemModel::dataChanged);
	JsonListView* list = m_json->listView("/actors");
	QVERIFY(list->rename(1, QStringLiteral("Bryan")));
	QCOMPARE(QString::fromStdString(std::string(m_db.actors[1].name)), QStringLiteral("Bryan"));
	QCOMPARE(choices->index(2).data(), QVariant(QStringLiteral("0002: Bryan")));
	QVERIFY(changed.count() > 0);
	QCOMPARE(elsewhere->index(1).data(JsonListView::NameRole), QVariant(QStringLiteral("Bryan")));
	QVERIFY(!list->rename(3, QStringLiteral("Nobody")));
	QVERIFY(!list->rename(-1, QStringLiteral("Nobody")));
}

void QmlBindingTest::fallbackRow()
{
	JsonListView* list = m_json->listView("/items");
	QAbstractItemModelTester tester(list, QAbstractItemModelTester::FailureReportingMode::QtTest);
	QCOMPARE(list->rowCount(), 1);
	list->setFallbackString(QStringLiteral("(None)"));
	QCOMPARE(list->rowCount(), 2);
	QCOMPARE(list->count(), 1);
	QCOMPARE(list->index(0).data(), QVariant(QStringLiteral("(None)")));
	QCOMPARE(list->index(0).data(JsonListView::IdRole), QVariant(0));
	QCOMPARE(list->index(1).data(JsonListView::NameRole), QVariant(QStringLiteral("Potion")));
	QSignalSpy changed(list, &QAbstractItemModel::dataChanged);
	QVERIFY(m_json->set("/items/0/name", QStringLiteral("Elixir")));
	QCOMPARE(changed.count(), 1);
	QCOMPARE(changed.at(0).at(0).value<QModelIndex>().row(), 1);
	// the entry operations count the entries, not the rows
	QVERIFY(list->resize(2));
	QCOMPARE(list->rowCount(), 3);
	QCOMPARE(list->index(2).data(), QVariant(QStringLiteral("0002: ")));
	list->setFallbackString(QString());
	QCOMPARE(list->rowCount(), 2);
}

void QmlBindingTest::choiceLists()
{
	// the combo boxes' list has its "(None)", the entry lists' has none
	JsonListView* entries = m_json->listView("/items");
	JsonListView* choices = m_json->choiceList("/items", QStringLiteral("(None)"), 0);
	QVERIFY(choices && choices != entries);
	QCOMPARE(m_json->choiceList("items/", QStringLiteral("(None)"), 0), choices);
	QAbstractItemModelTester tester(choices, QAbstractItemModelTester::FailureReportingMode::QtTest);
	QCOMPARE(entries->rowCount(), 1);
	QCOMPARE(choices->rowCount(), 2);
	QCOMPARE(choices->index(0).data(), QVariant(QStringLiteral("(None)")));
	QCOMPARE(choices->index(0).data(JsonListView::IdRole), QVariant(0));
	QCOMPARE(choices->index(1).data(), QVariant(QStringLiteral("0001: Potion")));
	// both follow the data: a rename, an entry added by the other
	QSignalSpy changed(choices, &QAbstractItemModel::dataChanged);
	QVERIFY(m_json->set("/items/0/name", QStringLiteral("Elixir")));
	QCOMPARE(changed.count(), 1);
	QCOMPARE(choices->index(1).data(), QVariant(QStringLiteral("0001: Elixir")));
	QVERIFY(entries->resize(3));
	QCOMPARE(choices->rowCount(), 4);
	QCOMPARE(choices->count(), 3);
	// another none: another list
	QVERIFY(m_json->choiceList("/items", QStringLiteral("(All)"), -1) != choices);
	QCOMPARE(m_json->choiceList("/system", QStringLiteral("(None)"), 0), nullptr);
}

void QmlBindingTest::jsonField()
{
	JsonField field;
	QSignalSpy values(&field, &JsonField::valueChanged);
	field.setSource(m_json->view("/actors/0"));
	field.setPath(QStringLiteral("initial_equipment/weapon_id"));
	QCOMPARE(field.jsonPointer(), QStringLiteral("/actors/0/initial_equipment/weapon_id"));
	QVERIFY(field.valid());
	QCOMPARE(field.value(), QVariant(0));

	// from the data
	QVERIFY(m_json->set("/actors/0/initial_equipment/weapon_id", 3));
	QCOMPARE(field.value(), QVariant(3));
	// into the data, converted: read back
	values.clear();
	field.setValue(4.4);
	QCOMPARE(m_db.actors[0].initial_equipment.weapon_id, int16_t(4));
	QCOMPARE(field.value(), QVariant(4));
	QVERIFY(values.count() >= 1);
	values.clear();
	field.setValue(4.2);
	QCOMPARE(field.value(), QVariant(4));
	QCOMPARE(values.count(), 1); // the control shows 4 again
	values.clear();
	field.setValue(QStringLiteral("many"));
	QCOMPARE(field.value(), QVariant(4));
	QCOMPARE(values.count(), 1);

	// a Json as source, the pointer absolute; a bit
	JsonField bit;
	bit.setSource(m_json.get());
	bit.setPath(QStringLiteral("items/0/state_set/0"));
	QCOMPARE(bit.value(), QVariant(true));
	bit.setValue(false);
	QCOMPARE(bool(m_db.items[0].state_set[0]), false);

	// a list as source
	JsonField name;
	name.setSource(m_json->listView("/actors"));
	name.setPath(QStringLiteral("1/name"));
	QCOMPARE(name.value(), QVariant(QStringLiteral("Brian")));
	m_json->notifyChanged(QString());
	m_db.actors[1].name = lcf::DBString("Bea");
	QCOMPARE(name.value(), QVariant(QStringLiteral("Brian"))); // not told
	m_json->notifyChanged("/actors");
	QCOMPARE(name.value(), QVariant(QStringLiteral("Bea")));

	JsonField missing;
	missing.setSource(m_json.get());
	missing.setPath(QStringLiteral("/actors/7/name"));
	QVERIFY(!missing.valid());
	QVERIFY(!missing.value().isValid());
	missing.setValue(QStringLiteral("x"));
	QVERIFY(!missing.valid());
}

void QmlBindingTest::entryFilter()
{
	JsonListView* list = m_json->listView("/actors");
	QVERIFY(list->resize(12));
	m_db.actors[11].name = lcf::DBString("Zed");
	m_json->notifyChanged("/actors/11/name");
	EntryFilterModel filter;
	QAbstractItemModelTester tester(&filter, QAbstractItemModelTester::FailureReportingMode::QtTest);
	filter.setSourceModel(list);
	QCOMPARE(filter.rowCount(), 12);
	filter.setFilterText(QStringLiteral("1"));
	// numbers starting with 1: 1, 10, 11, 12
	QCOMPARE(filter.rowCount(), 4);
	QCOMPARE(filter.sourceRow(1), 9);
	filter.setFilterText(QStringLiteral("012"));
	QCOMPARE(filter.rowCount(), 1);
	QCOMPARE(filter.bestRow(), 0);
	QCOMPARE(filter.sourceRow(filter.bestRow()), 11);
	filter.setFilterText(QStringLiteral("che"));
	QCOMPARE(filter.rowCount(), 1);
	QCOMPARE(filter.index(0, 0).data(JsonListView::NameRole), QVariant(QStringLiteral("Cherry")));
	filter.setFilterText(QStringLiteral("1 ze"));
	QCOMPARE(filter.rowCount(), 1);
	QCOMPARE(filter.rowOf(11), 0);
	QCOMPARE(filter.rowOf(0), -1);
	// renamed: found by its new name
	filter.setFilterText(QStringLiteral("bob"));
	QCOMPARE(filter.rowCount(), 0);
	QVERIFY(m_json->set("/actors/1/name", QStringLiteral("Bob")));
	QCOMPARE(filter.rowCount(), 1);
	filter.setFilterText(QString());
	QCOMPARE(filter.rowCount(), 12);
}

void QmlBindingTest::session()
{
	ProjectData live;
	live.database() = m_db;
	int saves = 0;
	DatabaseSession session;
	DialogService quiet;
	session.setDialogs(&quiet);
	session.setFile([&saves] { return memoryFile(&saves); });
	QSignalSpy modified(&session, &DatabaseSession::modifiedChanged);
	QSignalSpy applied(&session, &DatabaseSession::applied);
	QCOMPARE(session.database(), nullptr);
	session.open(&live);
	QVERIFY(session.database());
	QVERIFY(!session.modified());

	JsonView* database = session.database();
	QCOMPARE(database->str("actors/0/name"), QStringLiteral("Alex"));
	QVERIFY(database->set("actors/0/name", QStringLiteral("Ann")));
	QVERIFY(session.modified());
	QCOMPARE(ToQString(live.database().actors[0].name), QStringLiteral("Alex"));
	QVERIFY(database->list("actors")->resize(4));

	QCOMPARE(session.apply(), DatabaseSession::Applied);
	QCOMPARE(saves, 1);
	QCOMPARE(applied.count(), 1);
	QVERIFY(!session.modified());
	QCOMPARE(ToQString(live.database().actors[0].name), QStringLiteral("Ann"));
	QCOMPARE(live.database().actors.size(), size_t(4));

	// Cancel: the copy is the project again
	JsonField name;
	name.setSource(database);
	name.setPath(QStringLiteral("actors/0/name"));
	name.setValue(QStringLiteral("Zoe"));
	QVERIFY(session.modified());
	session.discard();
	QVERIFY(!session.modified());
	QCOMPARE(name.value(), QVariant(QStringLiteral("Ann")));
	QCOMPARE(database->list("actors")->rowCount(), 4);
	QVERIFY(modified.count() >= 4);
}

void QmlBindingTest::sessionLock()
{
	ProjectData live;
	live.database() = m_db;
	DatabaseSession session;
	session.open(&live);
	QVERIFY(session.isRpg2k3());
	// a window editing the copy: the project reads no database meanwhile
	QSignalSpy editing(&session, &DatabaseSession::editingChanged);
	QVERIFY(EditLocks::instance().canReload(ProjectWatcher::Database));
	session.setEditing(true);
	QVERIFY(session.editing());
	QVERIFY(!EditLocks::instance().canReload(ProjectWatcher::Database));
	session.setEditing(true);
	QCOMPARE(editing.count(), 1);
	session.setEditing(false);
	QVERIFY(EditLocks::instance().canReload(ProjectWatcher::Database));
	QCOMPARE(editing.count(), 2);
	// a session that goes while editing gives its lock back
	{
		DatabaseSession closing;
		closing.open(&live);
		closing.setEditing(true);
		QVERIFY(!EditLocks::instance().canReload(ProjectWatcher::Database));
	}
	QVERIFY(EditLocks::instance().canReload(ProjectWatcher::Database));
	// no project (a copy of nothing): no state to remember
	QVERIFY(session.viewState().contains(QStringLiteral("active_page")));
}

void QmlBindingTest::vocabularyTerms()
{
	// every term of the table is a text of the database, once; each one of
	// RPG Maker 2003 has its length
	Database db;
	JsonT<Database> json;
	json.setData(&db);
	QSet<QString> seen;
	int rpg2k3 = 0;
	int rpg2k = 0;
	for (const auto& page : VocabularyFields::pages()) {
		QVERIFY(page.title);
		for (const auto& group : page.groups) {
			QVERIFY(group.column == 0 || group.column == 1);
			for (const auto& row : group.rows) {
				for (const char* term : {row.term, row.second}) {
					if (!term) {
						continue;
					}
					const QString name = QString::fromLatin1(term);
					QVERIFY2(json.exists(QStringLiteral("terms/") + name), term);
					QVERIFY2(!seen.contains(name), term);
					seen.insert(name);
					if (VocabularyFields::shown(row, true)) {
						QVERIFY2(VocabularyFields::limit(term) > 0, term);
					}
				}
				rpg2k3 += VocabularyFields::shown(row, true);
				rpg2k += VocabularyFields::shown(row, false);
			}
		}
	}
	// the battle messages of RPG Maker 2000 (its own battles) and 2003
	QVERIFY(seen.contains(QStringLiteral("encounter")));
	QVERIFY(seen.contains(QStringLiteral("battle_start")));
	QVERIFY(rpg2k > rpg2k3);
	QCOMPARE(VocabularyFields::limit("victory"), 50);
	QCOMPARE(VocabularyFields::limit("inn_a_greeting_2"), 20);
	QCOMPARE(VocabularyFields::limit("attacking"), 0);
}

void QmlBindingTest::testGameLeaves()
{
	const QString game = qEnvironmentVariable("EDITOR_TEST_PROJECT");
	if (game.isEmpty()) {
		QSKIP("EDITOR_TEST_PROJECT (TestGame-2003) is not set");
	}
	const QString path = QDir(game).filePath(QStringLiteral("RPG_RT.ldb"));
	QVERIFY2(QFile::exists(path), qPrintable(path));
	auto db = lcf::LDB_Reader::Load(path.toStdString(), "1252");
	QVERIFY(db);
	const Database original = *db;
	JsonT<Database> json;
	json.setData(db.get());

	// every value of the database, read and written back through its pointer
	QJsonParseError error;
	const QJsonDocument document = QJsonDocument::fromJson(json.toJson(QString()).toUtf8(), &error);
	QVERIFY2(!document.isNull(), qPrintable(error.errorString()));
	Changes changes(&json);
	int leaves = 0;
	QStringList failures;
	forEachLeaf(document.object(), QString(), [&](const QString& pointer, const QJsonValue& expected) {
		++leaves;
		const QVariant value = json.value(pointer);
		bool same = value.isValid();
		if (same && expected.isString()) {
			same = value.toString() == expected.toString();
		} else if (same && expected.isBool()) {
			same = value.typeId() == QMetaType::Bool && value.toBool() == expected.toBool();
		} else if (same && expected.isDouble()) {
			same = value.toDouble() == expected.toDouble();
		}
		if (!same || !json.set(pointer, value)) {
			if (failures.size() < 20) {
				failures << pointer;
			}
		}
	});
	QVERIFY2(failures.isEmpty(), qPrintable(failures.join(u' ')));
	QVERIFY(leaves > 10000);
	QCOMPARE(changes.take(), QStringList());
	QVERIFY(*db == original);
	qInfo() << leaves << "values of TestGame-2003's database";

	// the lists of objects in it
	QCOMPARE(json.listView(QStringLiteral("/terms")), nullptr);
	for (const char* pointer : {"/actors", "/troops/0/members", "/troops/0/pages", "/commonevents/0/event_commands",
			"/animations/0/frames", "/animations/0/frames/0/cells", "/chipsets"}) {
		JsonListView* list = json.listView(QString::fromLatin1(pointer));
		QVERIFY2(list, pointer);
		QCOMPARE(list->rowCount(), json.size(QString::fromLatin1(pointer)));
		QAbstractItemModelTester tester(list, QAbstractItemModelTester::FailureReportingMode::QtTest);
	}
}

QTEST_GUILESS_MAIN(QmlBindingTest)

#include "qmlbinding_test.moc"
