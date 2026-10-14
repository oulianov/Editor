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

// The EasyRPG Player of this computer downloaded for test play
// (PlayerDownload): asked first, from a fake archive of this platform
// (EDITOR_FAKE_PLAYER_URL, made by tests/fake_player.cmake: no network),
// extracted in a temporary folder; cancelled, failed, then used again
// without downloading, and found by GameRunner::findPlayer at last.

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include "controller/dialog_service.h"
#include "model/game_runner.h"
#include "model/launcher_download.h"
#include "model/player_download.h"

namespace {
/** Answers the questions as told, and remembers what was asked and told */
class Answers : public DialogService
{
public:
	Answer answer = Answer::No;
	QStringList questions;
	QStringList warnings;

	Answer question(const QString&, const QString& text, Buttons) override {
		questions << text;
		return answer;
	}
	void warning(const QString&, const QString& text) override { warnings << text; }
	void error(const QString&, const QString& text) override { warnings << text; }
};
}

class PlayerDownloadTest : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase();
	void failed();
	void cancelled();
	void downloaded();
	void usedAgain();
	void partialIgnored();
	void foundAtLast();

private:
	/** Starts a download and waits for its end: the program */
	QString run(PlayerDownload& download);
	void useFake(bool fake);

	QTemporaryDir m_settings;
	QTemporaryDir m_launchers;
	QTemporaryDir m_empty;
	QString m_fake;
	Answers m_answers;
	QString m_installed;
	QString m_program;
};

void PlayerDownloadTest::initTestCase()
{
	m_fake = qEnvironmentVariable("EDITOR_FAKE_PLAYER_URL");
	if (m_fake.isEmpty()) {
		QSKIP("EDITOR_FAKE_PLAYER_URL (tests/fake_player.cmake) is not set");
	}
	// the settings and the downloads of the test
	QVERIFY(m_settings.isValid() && m_launchers.isValid() && m_empty.isValid());
	QStandardPaths::setTestModeEnabled(true);
	QSettings::setDefaultFormat(QSettings::IniFormat);
	QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings.path());
	qputenv("EASYRPG_EDITOR_LAUNCHER_DIR", m_launchers.path().toUtf8());
	DialogService::install(&m_answers);
	// a Player installed on this computer comes before the one downloaded
	m_installed = GameRunner::findPlayer();
}

void PlayerDownloadTest::useFake(bool fake)
{
	qputenv("EASYRPG_EDITOR_LAUNCHER_BASE", (fake ? m_fake : QUrl::fromLocalFile(m_empty.path() + QLatin1Char('/')).toString()).toUtf8());
}

QString PlayerDownloadTest::run(PlayerDownload& download)
{
	QSignalSpy finished(&download, &PlayerDownload::finished);
	if (!download.start()) {
		return QStringLiteral("(not started)");
	}
	if (finished.isEmpty() && !finished.wait(20000)) {
		return QStringLiteral("(not finished)");
	}
	return finished.first().first().toString();
}

void PlayerDownloadTest::failed()
{
	// no archive there: told through the dialog service
	useFake(false);
	m_answers.answer = DialogService::Answer::Yes;
	PlayerDownload download;
	QCOMPARE(run(download), QString());
	QCOMPARE(m_answers.warnings.size(), 1);
	QVERIFY(!download.isBusy());
	QVERIFY(Launchers::downloaded(Launchers::hostPlatform()).isEmpty());
}

void PlayerDownloadTest::cancelled()
{
	useFake(true);
	m_answers.warnings.clear();
	PlayerDownload download;
	QSignalSpy finished(&download, &PlayerDownload::finished);
	QSignalSpy busy(&download, &PlayerDownload::busyChanged);
	QVERIFY(download.start());
	QVERIFY(download.isBusy());
	download.cancel();
	QVERIFY(finished.size() == 1 || finished.wait(20000));
	QCOMPARE(finished.first().first().toString(), QString());
	QVERIFY(!download.isBusy());
	QCOMPARE(busy.size(), 2);
	// nothing told: the user stopped it
	QVERIFY(m_answers.warnings.isEmpty());
	QVERIFY(Launchers::downloaded(Launchers::hostPlatform()).isEmpty());
}

void PlayerDownloadTest::downloaded()
{
	useFake(true);
	m_answers.questions.clear();
	PlayerDownload download;
	QSignalSpy progress(&download, &PlayerDownload::progressChanged);
	m_program = run(download);
	QVERIFY2(!m_program.isEmpty() && !m_program.startsWith(u'('), qPrintable(m_program));
	// the button pressed is the choice: nothing asked
	QVERIFY(m_answers.questions.isEmpty());
	QVERIFY(m_answers.warnings.isEmpty());
	QVERIFY(progress.size() >= 1);
	QCOMPARE(download.progress(), download.progressMaximum());
	// the program, in the folder of the version, as the settings keep it
	QVERIFY(QFileInfo(m_program).isExecutable());
	QVERIFY(QDir::cleanPath(m_program).startsWith(QDir::cleanPath(Launchers::folder(Launchers::hostPlatform())
		+ QLatin1Char('/') + Launchers::version())));
	QCOMPARE(PlayerDownload::downloadedProgram(), m_program);
	// the archive is gone once extracted
	QVERIFY(!QFile::exists(Launchers::folder(Launchers::hostPlatform()) + QLatin1Char('/') + Launchers::version()
		+ QLatin1Char('/') + Launchers::url(Launchers::hostPlatform()).fileName()));
}

void PlayerDownloadTest::usedAgain()
{
	QVERIFY(!m_program.isEmpty());
	// nothing to download from, nothing asked: the one there
	useFake(false);
	m_answers.questions.clear();
	PlayerDownload download;
	QSignalSpy finished(&download, &PlayerDownload::finished);
	QVERIFY(download.start());
	QCOMPARE(finished.size(), 1);
	QCOMPARE(finished.first().first().toString(), m_program);
	QVERIFY(m_answers.questions.isEmpty());
	QVERIFY(!download.isBusy());
}

void PlayerDownloadTest::partialIgnored()
{
	// an archive still there: the download stopped before its end
	const QString archive = Launchers::folder(Launchers::hostPlatform()) + QLatin1Char('/') + Launchers::version()
		+ QLatin1Char('/') + Launchers::url(Launchers::hostPlatform()).fileName();
	{
		QFile file(archive);
		QVERIFY(file.open(QIODevice::WriteOnly));
	}
	QVERIFY(Launchers::downloaded(Launchers::hostPlatform()).isEmpty());
	QVERIFY(Launchers::downloads(Launchers::hostPlatform()).isEmpty());
	QVERIFY(QFile::remove(archive));
	QVERIFY(!Launchers::downloaded(Launchers::hostPlatform()).isEmpty());
}

void PlayerDownloadTest::foundAtLast()
{
	if (!m_installed.isEmpty()) {
		QSKIP("an EasyRPG Player is installed on this computer: it is found first");
	}
	QCOMPARE(GameRunner::findPlayer(), m_program);
	// the one of the settings first
	QSettings().setValue(PLAYER_PATH_KEY, QCoreApplication::applicationFilePath());
	QCOMPARE(GameRunner::findPlayer(), QFileInfo(QCoreApplication::applicationFilePath()).absoluteFilePath());
	QSettings().remove(PLAYER_PATH_KEY);
}

QTEST_GUILESS_MAIN(PlayerDownloadTest)
#include "player_download_test.moc"
