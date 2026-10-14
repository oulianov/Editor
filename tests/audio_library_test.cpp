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

// The Jukebox without its window (AudioLibraryModel), on a copy of the
// music and sounds of TestGame-2003 (EDITOR_TEST_PROJECT, skipped without
// it) and an RTP made here: the lists with the database's files and their
// settings, the filter, the loop of each list. Nothing is played: only a
// file that cannot be, whose error the status shows.

#include <QAbstractItemModelTester>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <memory>
#include <set>
#include <lcf/ldb/reader.h>
#include "common/dbstring.h"
#include "common/filefinder.h"
#include "model/audio_library.h"

using Kind = AudioLibraryModel::Kind;
using Origin = AudioListModel::Origin;
using Section = AudioListModel::Section;

namespace {
bool copyFolder(const QString& from, const QString& to)
{
	if (!QDir().mkpath(to)) {
		return false;
	}
	for (const QFileInfo& info : QDir(from).entryInfoList(QDir::Files)) {
		if (!QFile::copy(info.absoluteFilePath(), QDir(to).filePath(info.fileName()))) {
			return false;
		}
	}
	return true;
}

bool touch(const QString& path)
{
	QDir().mkpath(QFileInfo(path).absolutePath());
	QFile file(path);
	return file.open(QIODevice::WriteOnly) && file.write("MThd") == 4;
}

/** The audio files of a folder, as RPG Maker counts them: by name, any case */
int audioCount(const QString& dir, FileFinder::FileType type)
{
	const QStringList suffixes = FileFinder::GetFiltersForType(type);
	std::set<QString> names;
	for (const QFileInfo& info : QDir(dir).entryInfoList(QDir::Files)) {
		if (suffixes.contains(QLatin1Char('.') + info.suffix().toLower())) {
			names.insert(info.completeBaseName().toLower());
		}
	}
	return static_cast<int>(names.size());
}

/** The database's music that the System section lists (named, not (OFF)) */
int systemMusic(const lcf::rpg::System& system)
{
	int count = 0;
	for (const lcf::rpg::Music* music : { &system.title_music, &system.battle_music, &system.battle_end_music,
			&system.inn_music, &system.boat_music, &system.ship_music, &system.airship_music, &system.gameover_music }) {
		const QString name = ToQString(music->name);
		count += !name.isEmpty() && name != QLatin1String("(OFF)");
	}
	return count;
}
}

class AudioLibraryTest : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase();
	void init();
	void cleanup();
	void lists();
	void systemSettings();
	void filter();
	void kinds();
	void settings();
	void playing();
	void playerProperties();

private:
	QString game(const QString& path = QString()) const { return QDir(m_dir->filePath(QStringLiteral("game"))).filePath(path); }
	QString rtp(const QString& path = QString()) const { return QDir(m_dir->filePath(QStringLiteral("rtp"))).filePath(path); }
	/** The first row of a section, -1 when none */
	int firstOf(const AudioListModel* list, Section section) const;

	std::unique_ptr<QTemporaryDir> m_dir;
	QTemporaryDir m_settings;
	lcf::rpg::System m_system;
	std::unique_ptr<AudioLibraryModel> m_model;
	std::unique_ptr<QAbstractItemModelTester> m_musicTester;
	std::unique_ptr<QAbstractItemModelTester> m_soundTester;
};

void AudioLibraryTest::initTestCase()
{
	// the list of last time in a settings file of the test
	QVERIFY(m_settings.isValid());
	QSettings::setDefaultFormat(QSettings::IniFormat);
	QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings.path());

	const QString test_game = qEnvironmentVariable("EDITOR_TEST_PROJECT");
	if (test_game.isEmpty()) {
		QSKIP("EDITOR_TEST_PROJECT (TestGame-2003) is not set");
	}
	m_dir = std::make_unique<QTemporaryDir>();
	QVERIFY(m_dir->isValid());
	QVERIFY(copyFolder(QDir(test_game).filePath(QStringLiteral("Music")), game(QStringLiteral("Music"))));
	QVERIFY(copyFolder(QDir(test_game).filePath(QStringLiteral("Sound")), game(QStringLiteral("Sound"))));
	auto db = lcf::LDB_Reader::Load(QDir(test_game).filePath(QStringLiteral("RPG_RT.ldb")).toStdString(), "1252");
	QVERIFY(db);
	m_system = db->system;
	QVERIFY(systemMusic(m_system) > 0);

	// an RTP: a song of its own, one the game replaces, a sound
	QVERIFY(touch(rtp(QStringLiteral("Music/Theme.mid"))));
	QVERIFY(touch(rtp(QStringLiteral("Music/Town 1.mid"))));
	QVERIFY(touch(rtp(QStringLiteral("Sound/Zap.wav"))));
	QVERIFY(QFile::exists(game(QStringLiteral("Music/Town 1.mid"))));
}

void AudioLibraryTest::init()
{
	m_model = std::make_unique<AudioLibraryModel>();
	m_musicTester = std::make_unique<QAbstractItemModelTester>(m_model->music(), QAbstractItemModelTester::FailureReportingMode::QtTest);
	m_soundTester = std::make_unique<QAbstractItemModelTester>(m_model->sound(), QAbstractItemModelTester::FailureReportingMode::QtTest);
	m_model->setGamePath(game());
	m_model->setRtpPath(rtp());
	m_model->setSystem(m_system);
	m_model->refresh();
}

void AudioLibraryTest::cleanup()
{
	m_musicTester.reset();
	m_soundTester.reset();
	m_model.reset();
}

int AudioLibraryTest::firstOf(const AudioListModel* list, Section section) const
{
	for (int row = 0; row < list->rowCount(); ++row) {
		if (list->index(row).data(AudioListModel::SectionRole).value<Section>() == section) {
			return row;
		}
	}
	return -1;
}

void AudioLibraryTest::lists()
{
	const AudioListModel* music = m_model->music();
	const int gameFiles = audioCount(game(QStringLiteral("Music")), FileFinder::FileType::Music);
	QCOMPARE(music->filesTitle(), QStringLiteral("Files (%1 in the game, 1 in the RTP)").arg(gameFiles));
	QCOMPARE(music->rowCount(), gameFiles + 1 + systemMusic(m_system));
	QCOMPARE(firstOf(music, Section::System), gameFiles + 1);

	// the files first, sorted; the game's hide the RTP's
	int theme = -1;
	for (int row = 0; row < gameFiles + 1; ++row) {
		const QModelIndex index = music->index(row);
		const QString name = index.data(AudioListModel::NameRole).toString();
		if (name == QLatin1String("Theme")) {
			theme = row;
			QCOMPARE(index.data(AudioListModel::OriginRole).value<Origin>(), Origin::Rtp);
			QCOMPARE(index.data(AudioListModel::SourceRole).toString(), QStringLiteral("RTP"));
			QCOMPARE(index.data(AudioListModel::FormatRole).toString(), QStringLiteral("MIDI"));
		} else if (name == QLatin1String("Town 1")) {
			QCOMPARE(index.data(AudioListModel::OriginRole).value<Origin>(), Origin::Game);
			QCOMPARE(index.data(AudioListModel::PathRole).toString(), game(QStringLiteral("Music/Town 1.mid")));
		}
		QVERIFY(!index.data(AudioListModel::HasSettingsRole).toBool());
		QCOMPARE(index.data(AudioListModel::SectionTitleRole).toString(), music->filesTitle());
	}
	QVERIFY(theme >= 0);

	// the database's: with their label
	const int system = firstOf(music, Section::System);
	const QModelIndex line = music->index(system);
	QVERIFY(line.data(AudioListModel::HasSettingsRole).toBool());
	QVERIFY(line.data(AudioListModel::NameRole).toString().contains(QStringLiteral(": ")));
	QCOMPARE(line.data(AudioListModel::SectionTitleRole).toString(), QStringLiteral("System (database)"));

	// the first file selected
	QCOMPARE(music->currentRow(), 0);
	QCOMPARE(m_model->sound()->currentRow(), 0);
	QVERIFY(m_model->canPlay());
	QCOMPARE(m_model->sound()->filesTitle(),
		QStringLiteral("Files (%1 in the game, 1 in the RTP)").arg(audioCount(game(QStringLiteral("Sound")), FileFinder::FileType::Sound)));

	// listed again: the same line stays selected
	m_model->music()->setCurrentRow(theme);
	m_model->refresh();
	QCOMPARE(music->currentRow(), theme);

	// no game: nothing
	m_model->setGamePath(QString());
	m_model->refresh();
	QCOMPARE(music->rowCount(), 0);
	QCOMPARE(music->currentRow(), -1);
	QVERIFY(!m_model->canPlay());
}

void AudioLibraryTest::systemSettings()
{
	AudioListModel* music = m_model->music();
	const int system = firstOf(music, Section::System);
	QVERIFY(system >= 0);
	const QModelIndex line = music->index(system);
	// a line of the database brings its settings
	QSignalSpy volume(m_model.get(), &AudioLibraryModel::volumeChanged);
	music->setCurrentRow(system);
	QCOMPARE(m_model->volume(), line.data(AudioListModel::VolumeRole).toInt());
	QCOMPARE(m_model->tempo(), line.data(AudioListModel::TempoRole).toInt());
	QCOMPARE(m_model->balance(), line.data(AudioListModel::BalanceRole).toInt());
	QCOMPARE(m_model->fadeIn(), line.data(AudioListModel::FadeInRole).toInt() / 1000);
	QCOMPARE(volume.count(), m_model->volume() == 100 ? 0 : 1);

	// a file of the database that is nowhere
	lcf::rpg::System system_copy = m_system;
	system_copy.title_music.name = "Nothing here";
	m_model->setSystem(system_copy);
	m_model->refresh();
	const QModelIndex missing = music->index(firstOf(music, Section::System));
	QCOMPARE(missing.data(AudioListModel::NameRole).toString(), QStringLiteral("Title Screen: Nothing here"));
	QCOMPARE(missing.data(AudioListModel::OriginRole).value<Origin>(), Origin::Missing);
	QCOMPARE(missing.data(AudioListModel::SourceRole).toString(), QStringLiteral("Not found"));
	QVERIFY(missing.data(AudioListModel::PathRole).toString().isEmpty());
	QVERIFY(missing.data(AudioListModel::FormatRole).toString().isEmpty());
	QVERIFY(!m_model->playRow(Kind::Music, missing.row()));
	QVERIFY(m_model->statusIsError());
	QVERIFY(m_model->status().contains(QStringLiteral("Nothing here")));
	QCOMPARE(m_model->state(), AudioPlayer::State::Stopped);
}

void AudioLibraryTest::filter()
{
	AudioListModel* music = m_model->music();
	AudioListModel* sound = m_model->sound();
	const int rows = music->rowCount();
	const int town = [&] {
		for (int row = 0; row < rows; ++row) {
			if (music->index(row).data(AudioListModel::NameRole).toString() == QLatin1String("Town 1")) {
				return row;
			}
		}
		return -1;
	}();
	QVERIFY(town >= 0);
	music->setCurrentRow(town);

	// both lists; the line selected stays when shown
	QSignalSpy reset(music, &QAbstractItemModel::modelReset);
	m_model->setFilter(QStringLiteral("TOWN"));
	QCOMPARE(reset.count(), 1);
	QVERIFY(music->rowCount() >= 1 && music->rowCount() < rows);
	for (int row = 0; row < music->rowCount(); ++row) {
		QVERIFY(music->index(row).data(AudioListModel::NameRole).toString().contains(QStringLiteral("town"), Qt::CaseInsensitive));
	}
	QCOMPARE(music->index(music->currentRow()).data(AudioListModel::NameRole).toString(), QStringLiteral("Town 1"));

	// else the first line shown
	m_model->setFilter(QStringLiteral("cat"));
	QCOMPARE(music->currentRow(), 0);
	QVERIFY(sound->rowCount() >= 1);
	QCOMPARE(sound->currentRow(), 0);

	// nothing shown: nothing to play
	m_model->setFilter(QStringLiteral("no such sound"));
	QCOMPARE(music->rowCount(), 0);
	QCOMPARE(music->currentRow(), -1);
	QVERIFY(!m_model->canPlay());
	m_model->setFilter(QString());
	QCOMPARE(music->rowCount(), rows);
	QCOMPARE(music->currentRow(), 0);
}

void AudioLibraryTest::kinds()
{
	QCOMPARE(m_model->kind(), Kind::Music);
	// loop on for music, off for sounds; each list keeps its own
	QVERIFY(m_model->loop());
	QSignalSpy loop(m_model.get(), &AudioLibraryModel::loopChanged);
	m_model->setKind(Kind::Sound);
	QCOMPARE(m_model->currentList(), m_model->sound());
	QVERIFY(!m_model->loop());
	QCOMPARE(loop.count(), 1);
	m_model->setLoop(true);
	m_model->setKind(Kind::Music);
	m_model->setLoop(false);
	m_model->setKind(Kind::Sound);
	QVERIFY(m_model->loop());

	// the list of last time
	QCOMPARE(AudioLibraryModel().kind(), Kind::Sound);
	m_model->setKind(Kind::Music);
	QCOMPARE(AudioLibraryModel().kind(), Kind::Music);
}

void AudioLibraryTest::settings()
{
	// RPG Maker's ranges
	m_model->setVolume(150);
	QCOMPARE(m_model->volume(), 100);
	m_model->setVolume(-1);
	QCOMPARE(m_model->volume(), 0);
	m_model->setTempo(10);
	QCOMPARE(m_model->tempo(), 50);
	m_model->setTempo(200);
	QCOMPARE(m_model->tempo(), 150);
	m_model->setBalance(30);
	QCOMPARE(m_model->balance(), 30);
	m_model->setFadeIn(20);
	QCOMPARE(m_model->fadeIn(), 10);
	QCOMPARE(AudioLibraryModel::timeText(0), QStringLiteral("0:00"));
	QCOMPARE(AudioLibraryModel::timeText(65000), QStringLiteral("1:05"));
	QCOMPARE(AudioLibraryModel::timeText(600500), QStringLiteral("10:00"));
}

void AudioLibraryTest::playing()
{
	// a file gone since it was listed: the player says so, nothing plays
	QVERIFY(touch(game(QStringLiteral("Sound/Gone.wav"))));
	m_model->refresh();
	AudioListModel* sound = m_model->sound();
	int gone = -1;
	for (int row = 0; row < sound->rowCount(); ++row) {
		if (sound->index(row).data(AudioListModel::NameRole).toString() == QLatin1String("Gone")) {
			gone = row;
		}
	}
	QVERIFY(gone >= 0);
	QVERIFY(QFile::remove(game(QStringLiteral("Sound/Gone.wav"))));
	m_model->setKind(Kind::Sound);
	sound->setCurrentRow(gone);
	QSignalSpy status(m_model.get(), &AudioLibraryModel::statusChanged);
	m_model->play();
	QVERIFY(status.count() >= 1);
	QVERIFY(m_model->statusIsError());
	QVERIFY(m_model->status().contains(QStringLiteral("Gone.wav")));
	QCOMPARE(m_model->state(), AudioPlayer::State::Stopped);
	QVERIFY(m_model->canPlay());
	m_model->setKind(Kind::Music);
}

void AudioLibraryTest::playerProperties()
{
	// what QML reads and calls (Ez.AudioPlayer): its properties, its functions
	AudioPlayer player;
	QSignalSpy settings(&player, &AudioPlayer::settingsChanged);
	QSignalSpy file(&player, &AudioPlayer::fileChanged);
	QVERIFY(player.setProperty("volume", 40));
	QVERIFY(player.setProperty("tempo", 200));
	QVERIFY(player.setProperty("balance", 30));
	QVERIFY(player.setProperty("fadeIn", 1500));
	QVERIFY(player.setProperty("looping", true));
	QCOMPARE(settings.count(), 5);
	QCOMPARE(player.property("volume").toInt(), 40);
	// within RPG Maker's range
	QCOMPARE(player.property("tempo").toInt(), 150);
	QCOMPARE(player.property("balance").toInt(), 30);
	QCOMPARE(player.property("fadeIn").toInt(), 1500);
	QVERIFY(player.property("looping").toBool());
	QCOMPARE(player.property("state").value<AudioPlayer::State>(), AudioPlayer::State::Stopped);

	// a file that is not there: said, nothing plays
	bool played = true;
	QVERIFY(QMetaObject::invokeMethod(&player, "play", Q_RETURN_ARG(bool, played), Q_ARG(QString, game(QStringLiteral("Music/None.wav")))));
	QVERIFY(!played);
	QVERIFY(file.count() >= 1);
	QCOMPARE(player.property("filePath").toString(), game(QStringLiteral("Music/None.wav")));
	QVERIFY(player.property("errorString").toString().contains(QStringLiteral("None.wav")));
	QVERIFY(QMetaObject::invokeMethod(&player, "stop"));
	QCOMPARE(player.property("position").toLongLong(), 0);
}

QTEST_MAIN(AudioLibraryTest)
#include "audio_library_test.moc"
