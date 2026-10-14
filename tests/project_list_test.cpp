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

#include <QAbstractItemModelTester>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <memory>
#include "controller/dialog_service.h"
#include "model/project.h"
#include "model/project_list.h"

namespace {

/** Answers the questions as told, and remembers them */
class Answers : public DialogService
{
public:
	Answer answer = Answer::No;
	QStringList questions;

	Answer question(const QString&, const QString& text, Buttons) override {
		questions << text;
		return answer;
	}
};

/** A project of the test game's database, map tree and ini, with its title */
bool makeProject(const QString& game, const QString& folder, const QString& title)
{
	if (!QDir().mkpath(folder)) {
		return false;
	}
	for (const char* name : {"RPG_RT.ldb", "RPG_RT.lmt"}) {
		if (!QFile::copy(QDir(game).filePath(QLatin1String(name)), QDir(folder).filePath(QLatin1String(name)))) {
			return false;
		}
	}
	QFile ini(QDir(folder).filePath(QStringLiteral("RPG_RT.ini")));
	return ini.open(QIODevice::WriteOnly) && ini.write("[RPG_RT]\r\nGameTitle=" + title.toUtf8() + "\r\n") > 0;
}

} // namespace

/** Game Project > Open Project without its window (ProjectListModel), on copies of the test game */
class ProjectListTest : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase();
	void init();
	void cleanup();
	void list();
	void openProject();
	void trash();
	void settings();
	void legacy();

private:
	QString folder(const QString& name = QString()) const { return QDir(m_dir->path()).filePath(name); }
	/** Lists the projects and waits for them */
	void listed();

	QString m_game;
	std::unique_ptr<QTemporaryDir> m_dir;
	std::unique_ptr<ProjectListModel> m_model;
	std::unique_ptr<QAbstractItemModelTester> m_tester;
	Answers m_answers;
};

void ProjectListTest::initTestCase()
{
	m_game = qEnvironmentVariable("EDITOR_TEST_PROJECT");
	if (m_game.isEmpty()) {
		QSKIP("EDITOR_TEST_PROJECT (TestGame-2003) is not set");
	}
}

void ProjectListTest::init()
{
	m_dir = std::make_unique<QTemporaryDir>();
	QVERIFY(m_dir->isValid());
	QVERIFY(makeProject(m_game, folder(QStringLiteral("Beta")), QStringLiteral("Second Game")));
	QVERIFY(makeProject(m_game, folder(QStringLiteral("Alpha")), QStringLiteral("First Game")));
	// not a project
	QVERIFY(QDir().mkpath(folder(QStringLiteral("Other"))));
	m_answers = Answers();
	DialogService::install(&m_answers);
	m_model = std::make_unique<ProjectListModel>();
	m_model->setUseTrash(false);
	m_tester = std::make_unique<QAbstractItemModelTester>(m_model.get(), QAbstractItemModelTester::FailureReportingMode::QtTest);
}

void ProjectListTest::cleanup()
{
	m_tester.reset();
	m_model.reset();
	DialogService::install(nullptr);
	m_dir.reset();
}

void ProjectListTest::listed()
{
	QSignalSpy listed(m_model.get(), &ProjectListModel::listed);
	if (m_model->folder() != folder()) {
		m_model->setFolder(folder());
	} else {
		m_model->refresh();
	}
	QVERIFY(m_model->busy());
	QVERIFY(listed.wait(5000));
	QVERIFY(!m_model->busy());
}

void ProjectListTest::list()
{
	listed();
	QCOMPARE(m_model->rowCount(), 2);
	QCOMPARE(m_model->columnCount(), 2);
	QCOMPARE(m_model->index(0, 0).data().toString(), QStringLiteral("Alpha"));
	QCOMPARE(m_model->index(0, 1).data().toString(), QStringLiteral("First Game"));
	QCOMPARE(m_model->index(1, 0).data(ProjectListModel::TitleRole).toString(), QStringLiteral("Second Game"));
	QCOMPARE(m_model->headerData(1, Qt::Horizontal).toString(), QStringLiteral("Game Title"));
	// the first one selected
	QCOMPARE(m_model->currentRow(), 0);
	QCOMPARE(QDir(m_model->currentPath()), QDir(folder(QStringLiteral("Alpha"))));
	m_model->setCurrentRow(1);
	QCOMPARE(QDir(m_model->currentPath()), QDir(folder(QStringLiteral("Beta"))));
	m_model->setCurrentRow(5);
	QCOMPARE(m_model->currentRow(), -1);
	QVERIFY(m_model->currentPath().isEmpty());

	// another folder asked for while listing: its projects only
	QSignalSpy listed(m_model.get(), &ProjectListModel::listed);
	m_model->setFolder(folder(QStringLiteral("Other")));
	m_model->setFolder(folder());
	QVERIFY(listed.wait(5000));
	QTRY_VERIFY(!m_model->busy());
	QCOMPARE(m_model->rowCount(), 2);
}

void ProjectListTest::openProject()
{
	m_model->setOpenPath(folder(QStringLiteral("Beta")));
	listed();
	QVERIFY(!m_model->isOpen(0));
	QVERIFY(m_model->isOpen(1));
	QVERIFY(m_model->openNotice(0).isEmpty());
	QVERIFY(m_model->openNotice(1).contains(QStringLiteral("Beta is the project open in the Editor")));
	// not changed there: nothing asked
	QCOMPARE(m_model->trash(1), m_model->openNotice(1));
	QVERIFY(m_answers.questions.isEmpty());
	QVERIFY(QDir(folder(QStringLiteral("Beta"))).exists());
	bool edited = false;
	m_model->setSettingsEditor([&edited](Project&) {
		edited = true;
		return QString();
	});
	QCOMPARE(m_model->editSettings(1), m_model->openNotice(1));
	QVERIFY(!edited);
}

void ProjectListTest::trash()
{
	listed();
	// No: nothing done
	QVERIFY(m_model->trash(0).isEmpty());
	QCOMPARE(m_answers.questions.size(), 1);
	QVERIFY(m_answers.questions.first().contains(QStringLiteral("Move the project First Game")));
	QVERIFY(QDir(folder(QStringLiteral("Alpha"))).exists());

	m_answers.answer = DialogService::Answer::Yes;
	QSignalSpy listed(m_model.get(), &ProjectListModel::listed);
	QVERIFY(m_model->trash(0).isEmpty());
	QVERIFY(!QDir(folder(QStringLiteral("Alpha"))).exists());
	// listed again
	QVERIFY(listed.wait(5000));
	QCOMPARE(m_model->rowCount(), 1);
	QCOMPARE(m_model->index(0, 0).data().toString(), QStringLiteral("Beta"));
}

void ProjectListTest::settings()
{
	listed();
	// the editor of the interface, on the project of the row; its errors come back
	QString edited;
	m_model->setSettingsEditor([&edited](Project& project) {
		edited = project.projectDir().dirName();
		return project.saveGameTitle(QStringLiteral("Renamed Game")) ? QString() : QStringLiteral("not written");
	});
	QSignalSpy listed(m_model.get(), &ProjectListModel::listed);
	QVERIFY(m_model->editSettings(1).isEmpty());
	QCOMPARE(edited, QStringLiteral("Beta"));
	QVERIFY(listed.wait(5000));
	QCOMPARE(m_model->index(1, 1).data().toString(), QStringLiteral("Renamed Game"));
}

void ProjectListTest::legacy()
{
	QVERIFY(m_model->isLegacyProject(folder(QStringLiteral("Alpha"))));
	QVERIFY(!m_model->isLegacyProject(folder(QStringLiteral("Other"))));
	QVERIFY(!m_model->isLegacyProject(QString()));
}

QTEST_MAIN(ProjectListTest)
#include "project_list_test.moc"
