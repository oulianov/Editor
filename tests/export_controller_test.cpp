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

// Export Game without its window (ExportController): a copy of TestGame-2003
// (EDITOR_TEST_PROJECT, skipped without it) exported for the web and for
// Windows into a temporary folder, with its progress and log, then again
// over the folders written (asked first), and cancelled. The Players are
// the game's RPG_RT.exe and a web Player made here: nothing is downloaded,
// only the question before a download is checked.

#include <QAbstractItemModelTester>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <memory>
#include "controller/dialog_service.h"
#include "model/export_controller.h"
#include "model/launcher_download.h"

namespace {
/** Answers the questions as told, and remembers what was asked and told */
class Answers : public DialogService
{
public:
	Answer answer = Answer::No;
	QStringList questions;
	QStringList informations;
	QStringList warnings;

	Answer question(const QString&, const QString& text, Buttons) override {
		questions << text;
		return answer;
	}
	void information(const QString&, const QString& text) override { informations << text; }
	void warning(const QString&, const QString& text) override { warnings << text; }
	void error(const QString&, const QString& text) override { warnings << text; }
};

/** The game's files, without its tool folders (rpgsync's Scripts, hidden ones) */
bool copyGame(const QString& from, const QString& to)
{
	QDirIterator it(from, QDir::Files, QDirIterator::Subdirectories);
	while (it.hasNext()) {
		const QString source = it.next();
		const QString path = QDir(from).relativeFilePath(source);
		if (path.startsWith(QLatin1String("Scripts/")) || path.startsWith(QLatin1Char('.')) || path.contains(QLatin1String("/."))
				|| path == QLatin1String(ExportSettings::kFileName)) {
			continue;
		}
		const QString target = QDir(to).filePath(path);
		if (!QDir().mkpath(QFileInfo(target).absolutePath()) || !QFile::copy(source, target)) {
			return false;
		}
	}
	return true;
}

bool write(const QString& path, const QByteArray& data)
{
	QDir().mkpath(QFileInfo(path).absolutePath());
	QFile file(path);
	return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}
}

class ExportControllerTest : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase();
	void init();
	void cleanup();
	void settings();
	void launchers();
	void refused();
	void exportGame();
	void exportAgain();
	void download();

private:
	QString path(const QString& name) const { return m_dir->filePath(name); }
	/** Web and Windows (the game's RPG_RT.exe) into out/ */
	void webAndWindows();

	std::unique_ptr<QTemporaryDir> m_dir;
	QTemporaryDir m_settings;
	ExportSource m_source;
	std::unique_ptr<ExportController> m_controller;
	std::unique_ptr<QAbstractItemModelTester> m_tester;
	Answers m_answers;
};

void ExportControllerTest::initTestCase()
{
	// the settings and the downloaded Players of the test
	QVERIFY(m_settings.isValid());
	QStandardPaths::setTestModeEnabled(true);
	QSettings::setDefaultFormat(QSettings::IniFormat);
	QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings.path());

	const QString test_game = qEnvironmentVariable("EDITOR_TEST_PROJECT");
	if (test_game.isEmpty()) {
		QSKIP("EDITOR_TEST_PROJECT (TestGame-2003) is not set");
	}
	m_dir = std::make_unique<QTemporaryDir>();
	QVERIFY(m_dir->isValid());
	QVERIFY(copyGame(test_game, path(QStringLiteral("game"))));
	QVERIFY(QFile::exists(path(QStringLiteral("game/RPG_RT.exe"))));
	m_source.dir = QDir(path(QStringLiteral("game")));
	m_source.type = FileFinder::ProjectType::Legacy;
	m_source.encoding = QStringLiteral("1252");

	// a web Player, as easyrpg.org has it
	QVERIFY(write(path(QStringLiteral("players/web/index.html")),
		"<html><head><title>EasyRPG Player</title></head><body><script>createEasyRpgPlayer({ game: undefined })</script></body></html>"));
	QVERIFY(write(path(QStringLiteral("players/web/easyrpg-player.js")), "// js"));
	QVERIFY(write(path(QStringLiteral("players/web/easyrpg-player.wasm")), "wasm"));
}

void ExportControllerTest::init()
{
	m_answers = Answers();
	DialogService::install(&m_answers);
	m_controller = std::make_unique<ExportController>();
	m_tester = std::make_unique<QAbstractItemModelTester>(m_controller->platforms(), QAbstractItemModelTester::FailureReportingMode::QtTest);
	m_controller->setSource(m_source, QStringLiteral("TestGame"));
}

void ExportControllerTest::cleanup()
{
	m_tester.reset();
	m_controller.reset();
	DialogService::install(nullptr);
}

void ExportControllerTest::webAndWindows()
{
	for (const ExportPreset& preset : m_controller->settings().presets) {
		m_controller->setPlatformEnabled(preset.platform, false);
	}
	m_controller->setPlatformEnabled(ExportPlatform::Web, true);
	m_controller->setPlatformPath(ExportPlatform::Web, path(QStringLiteral("out/Web")));
	QVERIFY(m_controller->locateLauncher(ExportPlatform::Web, path(QStringLiteral("players"))).isEmpty());
	m_controller->setPlatformEnabled(ExportPlatform::Windows, true);
	m_controller->setPlatformPath(ExportPlatform::Windows, path(QStringLiteral("out/Windows")));
	m_controller->setLauncher(ExportPlatform::Windows, QString::fromLatin1(ExportPreset::kGameRuntime));
	m_controller->setZip(false);
	m_controller->setIncludeRtp(false);
	m_controller->setTitle(QStringLiteral("Test Game"));
	m_controller->setVersion(QStringLiteral("1.2"));
}

void ExportControllerTest::settings()
{
	// the defaults: no settings saved yet
	QCOMPARE(m_controller->title(), QStringLiteral("TestGame"));
	QVERIFY(!m_controller->isDirty());
	QVERIFY(m_controller->canProtect());
	QVERIFY(m_controller->hasRuntime());
	QCOMPARE(m_controller->platforms()->rowCount(), 4);
	QCOMPARE(m_controller->progress(), 0);
	QVERIFY(!m_controller->isBusy());

	// changed through the platforms too (a QML window), dirty until saved
	ExportPlatformModel* platforms = m_controller->platforms();
	int web = -1;
	for (int row = 0; row < platforms->rowCount(); ++row) {
		if (platforms->index(row).data(ExportPlatformModel::IdRole).toString() == QLatin1String("web")) {
			web = row;
		}
	}
	QVERIFY(web >= 0);
	QSignalSpy changed(platforms, &QAbstractItemModel::dataChanged);
	QVERIFY(platforms->setData(platforms->index(web), path(QStringLiteral("elsewhere")), ExportPlatformModel::PathRole));
	QCOMPARE(changed.count(), 1);
	QCOMPARE(m_controller->platformPath(ExportPlatform::Web), path(QStringLiteral("elsewhere")));
	QVERIFY(m_controller->isDirty());
	m_controller->setZip(true);
	QVERIFY(m_controller->zip());
	m_controller->setIcon(QStringLiteral("System/System.png"));
	QCOMPARE(m_controller->iconPath(), path(QStringLiteral("game/System/System.png")));
	QVERIFY(m_controller->save());
	QVERIFY(!m_controller->isDirty());
	QVERIFY(QFile::exists(path(QStringLiteral("game/export_presets.json"))));

	// read again by the next window
	ExportController next;
	next.setSource(m_source, QStringLiteral("TestGame"));
	// relative to the game, so that the settings travel with it
	QCOMPARE(QDir::cleanPath(m_source.dir.absoluteFilePath(next.platformPath(ExportPlatform::Web))), path(QStringLiteral("elsewhere")));
	QCOMPARE(next.icon(), QStringLiteral("System/System.png"));
	QVERIFY(next.zip());
	QVERIFY(QFile::remove(path(QStringLiteral("game/export_presets.json"))));

	QCOMPARE(ExportController::logLine(QStringLiteral("a"), GameExporter::Added), QStringLiteral("+ a"));
	QCOMPARE(ExportController::logLine(QStringLiteral("a"), GameExporter::Error), QStringLiteral("Error: a"));
}

void ExportControllerTest::launchers()
{
	// the game's RPG_RT.exe is a choice for Windows
	QVERIFY(m_controller->launcherChoices(ExportPlatform::Windows).contains(QStringLiteral("RPG_RT.exe")));
	m_controller->setLauncher(ExportPlatform::Windows, QString::fromLatin1(ExportPreset::kGameRuntime));
	QVERIFY(m_controller->launcherReady(ExportPlatform::Windows));
	QCOMPARE(m_controller->launcherText(ExportPlatform::Windows), QStringLiteral("RPG_RT.exe of the game"));
	QCOMPARE(m_controller->launcherToolTip(ExportPlatform::Windows),
		QDir::toNativeSeparators(path(QStringLiteral("game/RPG_RT.exe"))));

	// not a Player; a folder with one inside
	m_controller->setLauncher(ExportPlatform::Web, QString());
	QVERIFY(!m_controller->launcherReady(ExportPlatform::Web));
	QCOMPARE(m_controller->launcherText(ExportPlatform::Web), QStringLiteral("Player missing"));
	QVERIFY(!m_controller->locateLauncher(ExportPlatform::Web, path(QStringLiteral("game/Music"))).isEmpty());
	QVERIFY(m_controller->launcher(ExportPlatform::Web).isEmpty());
	QSignalSpy changed(m_controller.get(), &ExportController::platformChanged);
	QVERIFY(m_controller->locateLauncher(ExportPlatform::Web, path(QStringLiteral("players"))).isEmpty());
	QCOMPARE(changed.count(), 1);
	QCOMPARE(m_controller->launcher(ExportPlatform::Web), path(QStringLiteral("players/web")));
	QVERIFY(m_controller->launcherReady(ExportPlatform::Web));
	QCOMPARE(m_controller->launcherText(ExportPlatform::Web), QStringLiteral("web"));
	QVERIFY(m_controller->launcherChoices(ExportPlatform::Web).contains(path(QStringLiteral("players/web"))));
}

void ExportControllerTest::refused()
{
	webAndWindows();
	// an export folder in the game
	m_controller->setPlatformPath(ExportPlatform::Web, QStringLiteral("Music"));
	QVERIFY(!m_controller->start());
	QCOMPARE(m_answers.warnings.size(), 1);
	QVERIFY(m_answers.warnings.first().contains(QStringLiteral("cannot be in the game folder")));
	QVERIFY(!m_controller->isRunning());

	// nothing checked
	m_controller->setPlatformEnabled(ExportPlatform::Web, false);
	m_controller->setPlatformEnabled(ExportPlatform::Windows, false);
	QVERIFY(!m_controller->start());
	QCOMPARE(m_answers.informations.size(), 1);
	QVERIFY(m_answers.questions.isEmpty());
}

void ExportControllerTest::exportGame()
{
	webAndWindows();
	QSignalSpy finished(m_controller.get(), &ExportController::finished);
	QSignalSpy failed(m_controller.get(), &ExportController::failed);
	QSignalSpy progress(m_controller.get(), &ExportController::progressChanged);
	QSignalSpy lines(m_controller.get(), &ExportController::lineAdded);
	QVERIFY(m_controller->start());
	QVERIFY(m_controller->isRunning());
	QVERIFY(m_controller->isBusy());
	QVERIFY(!m_controller->canDownload());
	// saved first, nothing asked: the folders do not exist yet
	QVERIFY(!m_controller->isDirty());
	QVERIFY(m_answers.questions.isEmpty());
	QVERIFY(!m_controller->start());

	QVERIFY(finished.wait(120000));
	QCOMPARE(failed.count(), 0);
	QVERIFY(!m_controller->isRunning());
	QVERIFY(progress.count() > 2);
	QVERIFY(m_controller->progressMaximum() > 2);
	QCOMPARE(m_controller->progress(), m_controller->progressMaximum());
	QCOMPARE(lines.count(), m_controller->log().size());
	QCOMPARE(m_controller->log().last(), QStringLiteral("Export finished."));
	QVERIFY2(!m_controller->log().join(QLatin1Char('\n')).contains(QStringLiteral("Error: ")), qPrintable(m_controller->log().join(QLatin1Char('\n'))));

	// the game for the web, with the Player's page, and for Windows with its RPG_RT.exe
	QCOMPARE(m_controller->folders().size(), 2);
	QCOMPARE(m_controller->openFolderPath(), path(QStringLiteral("out")));
	QVERIFY(QFile::exists(path(QStringLiteral("out/Web/html/index.html"))));
	const QStringList games = QDir(path(QStringLiteral("out/Web/html/games"))).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
	QCOMPARE(games.size(), 1);
	const QDir web(path(QStringLiteral("out/Web/html/games/")) + games.first());
	QVERIFY(web.exists(QStringLiteral("index.json")));
	QVERIFY(web.exists(QStringLiteral("RPG_RT.ldb")));
	QVERIFY(web.exists(QStringLiteral("CharSet/Actor1.png")));
	QVERIFY(QFile::exists(path(QStringLiteral("out/Windows/RPG_RT.exe"))));
	QVERIFY(QFile::exists(path(QStringLiteral("out/Windows/RPG_RT.lmt"))));
	QVERIFY(!QFile::exists(path(QStringLiteral("out/Windows/export_presets.json"))));
}

void ExportControllerTest::exportAgain()
{
	webAndWindows();
	QVERIFY(QDir(path(QStringLiteral("out/Web"))).exists());
	// over the folders written: asked first
	QVERIFY(!m_controller->start());
	QCOMPARE(m_answers.questions.size(), 1);
	QVERIFY(m_answers.questions.first().contains(QDir::toNativeSeparators(path(QStringLiteral("out/Web")))));
	QVERIFY(!m_controller->isRunning());

	// cancelled at once: it ends with errors
	m_answers.answer = DialogService::Answer::Yes;
	QSignalSpy finished(m_controller.get(), &ExportController::finished);
	QSignalSpy failed(m_controller.get(), &ExportController::failed);
	QVERIFY(m_controller->start());
	m_controller->cancel();
	QVERIFY(failed.wait(120000));
	QCOMPARE(finished.count(), 0);
	QVERIFY(!m_controller->isBusy());
	QCOMPARE(m_controller->log().last(), QStringLiteral("Error: Export finished with errors."));
}

void ExportControllerTest::download()
{
	// the archive of the platform, from a mirror when one is set
	const QString base = QStringLiteral("file:///nowhere/players/");
	qputenv("EASYRPG_EDITOR_LAUNCHER_BASE", base.toUtf8());
	const QUrl url = Launchers::url(ExportPlatform::Web);
	qunsetenv("EASYRPG_EDITOR_LAUNCHER_BASE");
	QVERIFY(url.toString().startsWith(base));
	QVERIFY(url.fileName().contains(Launchers::version()));

	// started at once, nothing asked: from a mirror with no archive (the
	// tests never download), the failure in the log
	QVERIFY(m_controller->canDownload());
	qputenv("EASYRPG_EDITOR_LAUNCHER_BASE", base.toUtf8());
	QVERIFY(m_controller->download(ExportPlatform::Web));
	qunsetenv("EASYRPG_EDITOR_LAUNCHER_BASE");
	QVERIFY(m_answers.questions.isEmpty());
	QTRY_VERIFY_WITH_TIMEOUT(!m_controller->isDownloading(), 20000);
	QVERIFY(!m_controller->log().isEmpty());
	QVERIFY(!m_controller->platforms()->download(-1));
}

QTEST_MAIN(ExportControllerTest)
#include "export_controller_test.moc"
