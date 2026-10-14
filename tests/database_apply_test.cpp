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

// OK and Apply of the database windows (DatabaseApply) on a copy of the
// database of TestGame-2003 (EDITOR_TEST_PROJECT), which "another program"
// rewrites meanwhile: merged without a question when the two sides changed
// other entries, asked on a conflict (Overwrite, Reload, Cancel), and the
// save count kept in step for the next Apply. The file is written here
// like Project::saveDatabase does and watched by a ProjectWatcher; the
// project's own file (core()) and the window closing before a reload are
// left to the script tests.

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <lcf/ldb/reader.h>
#include "common/dbstring.h"
#include "controller/database_apply.h"
#include "controller/dialog_service.h"
#include "model/database_list_data.h"
#include "model/project_data.h"
#include "model/project_watcher.h"

using Status = DatabaseApply::Status;

namespace {
constexpr const char* Encoding = "1252";

/** Answers the question of a change on disk as told, and remembers it */
class AnsweringDialogs : public DialogService
{
public:
	StaleChoice answer = StaleChoice::Cancel;
	int asked = 0;
	QString what;
	QStringList conflicts;

	StaleChoice askStale(const QString& asked_what, bool, const QStringList& asked_conflicts) override {
		++asked;
		what = asked_what;
		conflicts = asked_conflicts;
		return answer;
	}
};

QString heroName(const lcf::rpg::Database& db, int id)
{
	return ToQString(db.actors[static_cast<size_t>(id - 1)].name);
}

void rename(lcf::rpg::Database& db, int id, const char* name)
{
	db.actors[static_cast<size_t>(id - 1)].name = lcf::DBString(name);
}
}

class DatabaseApplyTest : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase();
	void init();
	void unchangedOnDisk();
	void mergesOtherEntries();
	void conflictOverwrite();
	void conflictReload();
	void conflictCancel();
	void unreadableOnDisk();
	void saveFails();
	void applyAgain();

private:
	/** The database file as the project has it: written like
	 *  Project::saveDatabase, its version known to the watcher */
	DatabaseApply::File file();
	/** Another program writes the file (a new file renamed over it, as rpgsync) */
	void writeTheirs(const lcf::rpg::Database& db);
	std::unique_ptr<lcf::rpg::Database> onDisk() const;
	DatabaseApply::Result apply();

	QString m_source;
	std::unique_ptr<QTemporaryDir> m_dir;
	QString m_path;
	std::unique_ptr<ProjectWatcher> m_watcher;
	bool m_saveFails = false;
	int m_reloads = 0;
	/** The project, the window's copy, the database as the window read it */
	ProjectData m_live;
	ProjectData m_copy;
	lcf::rpg::Database m_base;
	AnsweringDialogs m_dialogs;
	/** Hero names of TestGame-2003 */
	QString m_hero1, m_hero2, m_hero3;
};

void DatabaseApplyTest::initTestCase()
{
	const QString game = qEnvironmentVariable("EDITOR_TEST_PROJECT");
	if (game.isEmpty()) {
		QSKIP("EDITOR_TEST_PROJECT (TestGame-2003) is not set");
	}
	m_source = QDir(game).filePath(QStringLiteral("RPG_RT.ldb"));
	QVERIFY2(QFile::exists(m_source), qPrintable(m_source));
}

void DatabaseApplyTest::init()
{
	m_dir = std::make_unique<QTemporaryDir>();
	QVERIFY(m_dir->isValid());
	m_path = m_dir->filePath(QStringLiteral("RPG_RT.ldb"));
	QVERIFY(QFile::copy(m_source, m_path));
	m_saveFails = false;
	m_reloads = 0;
	m_dialogs.answer = StaleChoice::Cancel;
	m_dialogs.asked = 0;
	m_dialogs.what.clear();
	m_dialogs.conflicts.clear();

	// the project loads it: the version the Editor has
	m_watcher = std::make_unique<ProjectWatcher>();
	auto db = onDisk();
	QVERIFY(db);
	m_watcher->acknowledge(m_path, ProjectWatcher::stampOf(m_path));
	m_live = ProjectData();
	m_live.database() = std::move(*db);
	// the window opens
	m_copy = m_live;
	m_base = m_live.database();
	m_hero1 = heroName(m_base, 1);
	m_hero2 = heroName(m_base, 2);
	m_hero3 = heroName(m_base, 3);
	QVERIFY(m_hero1 != m_hero2 && m_hero2 != m_hero3);
}

DatabaseApply::File DatabaseApplyTest::file()
{
	DatabaseApply::File file;
	file.isStale = [this] { return m_watcher->isStale(m_path); };
	file.read = [this] { return onDisk(); };
	file.save = [this] {
		lcf::LDB_Reader::PrepareSave(m_live.database());
		if (m_saveFails || !lcf::LDB_Reader::Save(m_path.toStdString(), m_live.database(), Encoding)) {
			--m_live.database().system.save_count;
			return false;
		}
		m_watcher->acknowledge(m_path, ProjectWatcher::stampOf(m_path));
		return true;
	};
	file.reload = [this] { ++m_reloads; };
	return file;
}

void DatabaseApplyTest::writeTheirs(const lcf::rpg::Database& db)
{
	const QString written = m_path + QStringLiteral(".theirs");
	QVERIFY(lcf::LDB_Reader::Save(written.toStdString(), db, Encoding));
	QVERIFY(QFile::remove(m_path));
	QVERIFY(QFile::rename(written, m_path));
	QVERIFY(m_watcher->isStale(m_path));
}

std::unique_ptr<lcf::rpg::Database> DatabaseApplyTest::onDisk() const
{
	return lcf::LDB_Reader::Load(m_path.toStdString(), Encoding);
}

DatabaseApply::Result DatabaseApplyTest::apply()
{
	return DatabaseApply::apply(m_live, m_copy, m_base, file(), m_dialogs, this);
}

void DatabaseApplyTest::unchangedOnDisk()
{
	const int saves = m_live.database().system.save_count;
	rename(m_copy.database(), 1, "Mine");
	QSignalSpy lists(&DatabaseListNotifier::instance(), &DatabaseListNotifier::listChanged);

	const auto result = apply();
	QCOMPARE(result.status, Status::Applied);
	QVERIFY(result.message.isEmpty());
	QCOMPARE(m_dialogs.asked, 0);
	QCOMPARE(heroName(m_live.database(), 1), QStringLiteral("Mine"));
	// the project's hero list is told, from the window
	QCOMPARE(lists.count(), 1);
	QCOMPARE(lists[0][0].value<const void*>(), static_cast<const void*>(&m_live.database().actors));
	QCOMPARE(lists[0][1].value<QObject*>(), static_cast<QObject*>(this));
	// written, counted, and the copy and the base follow
	QCOMPARE(m_live.database().system.save_count, saves + 1);
	QCOMPARE(m_copy.database().system.save_count, saves + 1);
	QVERIFY(m_base == m_live.database());
	QVERIFY(m_copy.database() == m_live.database());
	auto written = onDisk();
	QCOMPARE(heroName(*written, 1), QStringLiteral("Mine"));
	QCOMPARE(written->system.save_count, saves + 1);
	QVERIFY(!m_watcher->isStale(m_path));
}

void DatabaseApplyTest::mergesOtherEntries()
{
	auto theirs = m_base;
	rename(theirs, 2, "Theirs");
	writeTheirs(theirs);
	rename(m_copy.database(), 1, "Mine");
	QSignalSpy lists(&DatabaseListNotifier::instance(), &DatabaseListNotifier::listChanged);

	QCOMPARE(apply().status, Status::Applied);
	QCOMPARE(m_dialogs.asked, 0);
	for (const auto* db : {&m_live.database(), &m_copy.database(), &m_base}) {
		QCOMPARE(heroName(*db, 1), QStringLiteral("Mine"));
		QCOMPARE(heroName(*db, 2), QStringLiteral("Theirs"));
		QCOMPARE(heroName(*db, 3), m_hero3);
	}
	// the window's list shows the merge, then the project's the result
	QCOMPARE(lists.count(), 2);
	QCOMPARE(lists[0][0].value<const void*>(), static_cast<const void*>(&m_copy.database().actors));
	QCOMPARE(lists[1][0].value<const void*>(), static_cast<const void*>(&m_live.database().actors));
	auto written = onDisk();
	QCOMPARE(heroName(*written, 1), QStringLiteral("Mine"));
	QCOMPARE(heroName(*written, 2), QStringLiteral("Theirs"));
	QVERIFY(!m_watcher->isStale(m_path));
}

void DatabaseApplyTest::conflictOverwrite()
{
	auto theirs = m_base;
	rename(theirs, 1, "Theirs");
	rename(theirs, 3, "Theirs too");
	writeTheirs(theirs);
	rename(m_copy.database(), 1, "Mine");
	m_dialogs.answer = StaleChoice::Overwrite;

	QCOMPARE(apply().status, Status::Applied);
	QCOMPARE(m_dialogs.asked, 1);
	QCOMPARE(m_dialogs.what, DatabaseApply::tr("The database"));
	QCOMPARE(m_dialogs.conflicts, QStringList{QStringLiteral("Hero 1")});
	// the window's database replaces the file, what the other program
	// changed elsewhere too (the question said so)
	auto written = onDisk();
	QCOMPARE(heroName(*written, 1), QStringLiteral("Mine"));
	QCOMPARE(heroName(*written, 3), m_hero3);
	QCOMPARE(heroName(m_live.database(), 1), QStringLiteral("Mine"));
	QVERIFY(m_base == m_live.database());
	QVERIFY(!m_watcher->isStale(m_path));
}

void DatabaseApplyTest::conflictReload()
{
	auto theirs = m_base;
	rename(theirs, 1, "Theirs");
	writeTheirs(theirs);
	rename(m_copy.database(), 1, "Mine");
	const auto live = m_live.database();
	const auto base = m_base;
	m_dialogs.answer = StaleChoice::Reload;

	QCOMPARE(apply().status, Status::Reload);
	QCOMPARE(m_dialogs.asked, 1);
	// nothing taken nor written: the caller closes its window, then reloads
	QCOMPARE(m_reloads, 0);
	QVERIFY(m_live.database() == live);
	QVERIFY(m_base == base);
	QCOMPARE(heroName(m_copy.database(), 1), QStringLiteral("Mine"));
	QCOMPARE(heroName(*onDisk(), 1), QStringLiteral("Theirs"));
	QVERIFY(m_watcher->isStale(m_path));
}

void DatabaseApplyTest::conflictCancel()
{
	auto theirs = m_base;
	rename(theirs, 1, "Theirs");
	writeTheirs(theirs);
	rename(m_copy.database(), 1, "Mine");
	const auto live = m_live.database();
	m_dialogs.answer = StaleChoice::Cancel;

	QCOMPARE(apply().status, Status::Cancelled);
	QVERIFY(m_live.database() == live);
	QCOMPARE(heroName(m_copy.database(), 1), QStringLiteral("Mine"));
	QCOMPARE(heroName(*onDisk(), 1), QStringLiteral("Theirs"));
	// the window stays: the next OK asks again
	QCOMPARE(apply().status, Status::Cancelled);
	QCOMPARE(m_dialogs.asked, 2);
}

void DatabaseApplyTest::unreadableOnDisk()
{
	QFile::remove(m_path);
	QFile garbage(m_path);
	QVERIFY(garbage.open(QIODevice::WriteOnly));
	garbage.write("not a database");
	garbage.close();
	QVERIFY(m_watcher->isStale(m_path));
	rename(m_copy.database(), 1, "Mine");
	m_dialogs.answer = StaleChoice::Overwrite;

	// nothing to merge with: asked without conflicts, then written
	QCOMPARE(apply().status, Status::Applied);
	QCOMPARE(m_dialogs.asked, 1);
	QVERIFY(m_dialogs.conflicts.isEmpty());
	QCOMPARE(heroName(*onDisk(), 1), QStringLiteral("Mine"));
}

void DatabaseApplyTest::saveFails()
{
	const int saves = m_live.database().system.save_count;
	const auto base = m_base;
	rename(m_copy.database(), 1, "Mine");
	m_saveFails = true;

	const auto result = apply();
	QCOMPARE(result.status, Status::SaveFailed);
	QCOMPARE(result.message, DatabaseApply::tr("The database could not be written."));
	// in the project, not counted; the next Apply tries again from the same base
	QCOMPARE(heroName(m_live.database(), 1), QStringLiteral("Mine"));
	QCOMPARE(m_live.database().system.save_count, saves);
	QCOMPARE(m_copy.database().system.save_count, saves);
	QVERIFY(m_base == base);
	QCOMPARE(heroName(*onDisk(), 1), m_hero1);

	m_saveFails = false;
	QCOMPARE(apply().status, Status::Applied);
	QCOMPARE(onDisk()->system.save_count, saves + 1);
}

void DatabaseApplyTest::applyAgain()
{
	const int saves = m_live.database().system.save_count;
	rename(m_copy.database(), 1, "Mine");
	QCOMPARE(apply().status, Status::Applied);
	// the window stays open: its copy took the count, the next Apply counts on
	rename(m_copy.database(), 2, "Mine too");
	QCOMPARE(apply().status, Status::Applied);
	QCOMPARE(onDisk()->system.save_count, saves + 2);

	// another program saves the file (counted), the window changes another hero
	auto theirs = *onDisk();
	rename(theirs, 3, "Theirs");
	lcf::LDB_Reader::PrepareSave(theirs);
	writeTheirs(theirs);
	rename(m_copy.database(), 4, "Mine again");
	QCOMPARE(apply().status, Status::Applied);
	QCOMPARE(m_dialogs.asked, 0);
	auto written = onDisk();
	QCOMPARE(heroName(*written, 1), QStringLiteral("Mine"));
	QCOMPARE(heroName(*written, 2), QStringLiteral("Mine too"));
	QCOMPARE(heroName(*written, 3), QStringLiteral("Theirs"));
	QCOMPARE(heroName(*written, 4), QStringLiteral("Mine again"));
	QCOMPARE(written->system.save_count, saves + 4);
}

QTEST_GUILESS_MAIN(DatabaseApplyTest)
#include "database_apply_test.moc"
