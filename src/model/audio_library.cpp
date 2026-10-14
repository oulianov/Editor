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

#include "audio_library.h"

#include <QCollator>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <algorithm>
#include <map>

#include <lcf/rpg/system.h>

#include "common/dbstring.h"
#include "common/filefinder.h"
#include "defines.h"

namespace {

const char TAB_KEY[] = "jukebox_tab";

using Origin = AudioListModel::Origin;
using Section = AudioListModel::Section;

QString formatOf(const QString& path) {
	const QString suffix = QFileInfo(path).suffix().toUpper();
	return suffix == QLatin1String("MID") ? QStringLiteral("MIDI") : suffix;
}

/** The audio files of a folder, by lower case base name (what RPG Maker searches) */
std::map<QString, QString> audioFiles(const QString& dir, const QStringList& suffixes) {
	std::map<QString, QString> files;
	if (dir.isEmpty()) {
		return files;
	}
	for (const QFileInfo& info : QDir(dir).entryInfoList(QDir::Files | QDir::Readable)) {
		if (suffixes.contains('.' + info.suffix().toLower())) {
			// a.mid and a.wav: the first in RPG Maker's order wins, like Project::findFile
			const QString key = info.completeBaseName().toLower();
			auto found = files.find(key);
			if (found == files.end() || suffixes.indexOf('.' + info.suffix().toLower())
					< suffixes.indexOf('.' + QFileInfo(found->second).suffix().toLower())) {
				files[key] = info.absoluteFilePath();
			}
		}
	}
	return files;
}

// The music and sound effects of the System tab of the database
struct SystemMusic {
	const char* label;
	lcf::rpg::Music lcf::rpg::System::* music;
};
const SystemMusic system_music[] = {
	{ QT_TRANSLATE_NOOP("JukeboxDialog", "Title Screen"), &lcf::rpg::System::title_music },
	{ QT_TRANSLATE_NOOP("JukeboxDialog", "Battle"), &lcf::rpg::System::battle_music },
	{ QT_TRANSLATE_NOOP("JukeboxDialog", "Victory"), &lcf::rpg::System::battle_end_music },
	{ QT_TRANSLATE_NOOP("JukeboxDialog", "Inn"), &lcf::rpg::System::inn_music },
	{ QT_TRANSLATE_NOOP("JukeboxDialog", "Skiff"), &lcf::rpg::System::boat_music },
	{ QT_TRANSLATE_NOOP("JukeboxDialog", "Ship"), &lcf::rpg::System::ship_music },
	{ QT_TRANSLATE_NOOP("JukeboxDialog", "Airship"), &lcf::rpg::System::airship_music },
	{ QT_TRANSLATE_NOOP("JukeboxDialog", "Game Over Screen"), &lcf::rpg::System::gameover_music },
};

struct SystemSound {
	const char* label;
	lcf::rpg::Sound lcf::rpg::System::* sound;
};
const SystemSound system_sounds[] = {
	{ QT_TRANSLATE_NOOP("JukeboxDialog", "Cursor"), &lcf::rpg::System::cursor_se },
	{ QT_TRANSLATE_NOOP("JukeboxDialog", "Select Choice"), &lcf::rpg::System::decision_se },
	{ QT_TRANSLATE_NOOP("JukeboxDialog", "Cancel"), &lcf::rpg::System::cancel_se },
	{ QT_TRANSLATE_NOOP("JukeboxDialog", "Buzzer"), &lcf::rpg::System::buzzer_se },
	{ QT_TRANSLATE_NOOP("JukeboxDialog", "Battle Transition"), &lcf::rpg::System::battle_se },
	{ QT_TRANSLATE_NOOP("JukeboxDialog", "Flee Battle"), &lcf::rpg::System::escape_se },
	{ QT_TRANSLATE_NOOP("JukeboxDialog", "Enemy Attacks"), &lcf::rpg::System::enemy_attack_se },
	{ QT_TRANSLATE_NOOP("JukeboxDialog", "Enemy Damaged"), &lcf::rpg::System::enemy_damaged_se },
	{ QT_TRANSLATE_NOOP("JukeboxDialog", "Ally Damaged"), &lcf::rpg::System::actor_damaged_se },
	{ QT_TRANSLATE_NOOP("JukeboxDialog", "Evade Attack"), &lcf::rpg::System::dodge_se },
	{ QT_TRANSLATE_NOOP("JukeboxDialog", "Enemy Defeated"), &lcf::rpg::System::enemy_death_se },
	{ QT_TRANSLATE_NOOP("JukeboxDialog", "Item Used"), &lcf::rpg::System::item_se },
};

} // namespace

AudioListModel::AudioListModel(QObject* parent) : QAbstractListModel(parent) {
}

int AudioListModel::rowCount(const QModelIndex& parent) const {
	return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant AudioListModel::data(const QModelIndex& index, int role) const {
	if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
		return QVariant();
	}
	const Entry& line = m_entries[m_rows[index.row()]];
	switch (role) {
		case Qt::DisplayRole:
		case NameRole:
			return line.name;
		case Qt::ToolTipRole:
			return line.path.isEmpty()
				? QCoreApplication::translate("JukeboxDialog", "No file of this name in the game or in the RTP")
				: QDir::toNativeSeparators(line.path);
		case PathRole:
			return line.path;
		case FormatRole:
			return line.path.isEmpty() ? QString() : formatOf(line.path);
		case SourceRole:
			return line.origin == Origin::Missing ? QCoreApplication::translate("JukeboxDialog", "Not found")
				: line.origin == Origin::Rtp ? QCoreApplication::translate("JukeboxDialog", "RTP")
				: QCoreApplication::translate("JukeboxDialog", "Game");
		case OriginRole:
			return QVariant::fromValue(line.origin);
		case SectionRole:
			return QVariant::fromValue(line.section);
		case SectionTitleRole:
			return line.section == Section::System ? systemTitle() : filesTitle();
		case HasSettingsRole:
			return line.settings;
		case FadeInRole:
			return line.fadein;
		case VolumeRole:
			return line.volume;
		case TempoRole:
			return line.tempo;
		case BalanceRole:
			return line.balance;
		default:
			return QVariant();
	}
}

QHash<int, QByteArray> AudioListModel::roleNames() const {
	QHash<int, QByteArray> names = QAbstractListModel::roleNames();
	names.insert(NameRole, "name");
	names.insert(PathRole, "path");
	names.insert(FormatRole, "format");
	names.insert(SourceRole, "source");
	names.insert(OriginRole, "origin");
	names.insert(SectionRole, "section");
	names.insert(SectionTitleRole, "sectionTitle");
	names.insert(HasSettingsRole, "hasSettings");
	names.insert(FadeInRole, "fadeIn");
	names.insert(VolumeRole, "volume");
	names.insert(TempoRole, "tempo");
	names.insert(BalanceRole, "balance");
	return names;
}

const AudioListModel::Entry* AudioListModel::entry(int row) const {
	return row >= 0 && row < m_rows.size() ? &m_entries[m_rows[row]] : nullptr;
}

int AudioListModel::currentRow() const {
	return m_current < 0 ? -1 : static_cast<int>(m_rows.indexOf(m_current));
}

void AudioListModel::setCurrentRow(int row) {
	select(row >= 0 && row < m_rows.size() ? m_rows[row] : -1);
}

const AudioListModel::Entry* AudioListModel::current() const {
	return m_current < 0 ? nullptr : &m_entries[m_current];
}

QString AudioListModel::filesTitle() const {
	return QCoreApplication::translate("JukeboxDialog", "Files (%1 in the game, %2 in the RTP)").arg(m_gameFiles).arg(m_rtpFiles);
}

QString AudioListModel::systemTitle() const {
	return QCoreApplication::translate("JukeboxDialog", "System (database)");
}

void AudioListModel::select(int entry) {
	if (entry == m_current) {
		return;
	}
	m_current = entry;
	emit currentRowChanged();
}

void AudioListModel::setEntries(const QList<Entry>& entries, int game_files, int rtp_files) {
	// the same line as before, else the first file
	const Entry* before = current();
	const QString name = before ? before->name : QString();
	const QString path = before ? before->path : QString();
	const bool titles = game_files != m_gameFiles || rtp_files != m_rtpFiles;

	beginResetModel();
	m_entries = entries;
	m_gameFiles = game_files;
	m_rtpFiles = rtp_files;
	int found = -1;
	for (int i = 0; i < m_entries.size() && !path.isEmpty(); ++i) {
		if (m_entries[i].path == path && (found < 0 || m_entries[i].name == name)) {
			found = i;
		}
	}
	m_current = -1;
	applyFilter();
	if (found >= 0 && m_rows.contains(found)) {
		m_current = found;
	} else if (!m_rows.isEmpty()) {
		m_current = m_rows.first();
	}
	endResetModel();
	if (titles) {
		emit filesTitleChanged();
	}
	emit currentRowChanged();
}

void AudioListModel::setFilter(const QString& text) {
	if (text == m_filter) {
		return;
	}
	const int before = m_current;
	beginResetModel();
	m_filter = text;
	applyFilter();
	// the selection follows the filter: the first line shown
	if (!m_rows.contains(m_current)) {
		m_current = m_rows.isEmpty() ? -1 : m_rows.first();
	}
	endResetModel();
	if (before != m_current) {
		emit currentRowChanged();
	}
}

void AudioListModel::applyFilter() {
	m_rows.clear();
	const QString text = m_filter.trimmed();
	for (int i = 0; i < m_entries.size(); ++i) {
		if (text.isEmpty() || m_entries[i].name.contains(text, Qt::CaseInsensitive)) {
			m_rows.append(i);
		}
	}
}

AudioLibraryModel::AudioLibraryModel(QObject* parent) : QObject(parent) {
	m_music = new AudioListModel(this);
	m_sound = new AudioListModel(this);
	m_player = new AudioPlayer(this);

	connect(m_music, &AudioListModel::currentRowChanged, this, [this] { lineSelected(Kind::Music); });
	connect(m_sound, &AudioListModel::currentRowChanged, this, [this] { lineSelected(Kind::Sound); });
	connect(m_player, &AudioPlayer::stateChanged, this, [this](AudioPlayer::State state) {
		switch (state) {
			case AudioPlayer::State::Playing:
				setStatus(QCoreApplication::translate("JukeboxDialog", "Playing %1").arg(m_playingName));
				break;
			case AudioPlayer::State::Paused:
				setStatus(QCoreApplication::translate("JukeboxDialog", "Paused %1").arg(m_playingName));
				break;
			case AudioPlayer::State::Stopped:
				// also at the end of a file played once; an error follows when it failed
				setStatus(QCoreApplication::translate("JukeboxDialog", "Stopped"));
				break;
		}
		emit stateChanged();
		emit canPlayChanged();
	});
	connect(m_player, &AudioPlayer::positionChanged, this, &AudioLibraryModel::positionChanged);
	connect(m_player, &AudioPlayer::durationChanged, this, &AudioLibraryModel::durationChanged);
	connect(m_player, &AudioPlayer::errorOccurred, this, [this](const QString& message) {
		setStatus(message, true);
	});

	// the list of last time
	m_kind = QSettings().value(QLatin1String(TAB_KEY), 0).toInt() == 1 ? Kind::Sound : Kind::Music;
	m_player->setVolume(m_volume);
	m_player->setTempo(m_tempo);
	m_player->setBalance(m_balance);
	m_player->setLooping(loop());
	m_player->setFadeIn(playerFadeIn());
}

AudioLibraryModel::~AudioLibraryModel() = default;

AudioListModel* AudioLibraryModel::music() const {
	return m_music;
}

AudioListModel* AudioLibraryModel::sound() const {
	return m_sound;
}

AudioListModel* AudioLibraryModel::list(Kind kind) const {
	return kind == Kind::Music ? m_music : m_sound;
}

AudioListModel* AudioLibraryModel::currentList() const {
	return list(m_kind);
}

AudioPlayer* AudioLibraryModel::player() const {
	return m_player;
}

QString AudioLibraryModel::gamePath() const {
	return m_gamePath;
}

void AudioLibraryModel::setGamePath(const QString& path) {
	if (path != m_gamePath) {
		m_gamePath = path;
		emit gamePathChanged();
	}
}

bool AudioLibraryModel::hasGame() const {
	return !m_gamePath.isEmpty();
}

QString AudioLibraryModel::rtpPath() const {
	return m_rtpPath;
}

void AudioLibraryModel::setRtpPath(const QString& path) {
	if (path != m_rtpPath) {
		m_rtpPath = path;
		emit rtpPathChanged();
	}
}

void AudioLibraryModel::setSystem(const lcf::rpg::System& system) {
	clearSystem();
	for (const SystemMusic& line : system_music) {
		const lcf::rpg::Music& music = system.*line.music;
		m_systemMusic.append({ line.label, ToQString(music.name), music.fadein, music.volume, music.tempo, music.balance });
	}
	for (const SystemSound& line : system_sounds) {
		const lcf::rpg::Sound& sound = system.*line.sound;
		m_systemSound.append({ line.label, ToQString(sound.name), 0, sound.volume, sound.tempo, sound.balance });
	}
}

void AudioLibraryModel::clearSystem() {
	m_systemMusic.clear();
	m_systemSound.clear();
}

void AudioLibraryModel::refresh() {
	fill(Kind::Music);
	fill(Kind::Sound);
	emit canPlayChanged();
}

void AudioLibraryModel::fill(Kind kind) {
	AudioListModel* lines = list(kind);
	if (!hasGame()) {
		lines->setEntries({}, 0, 0);
		return;
	}
	const QString folder = kind == Kind::Music ? QStringLiteral(MUSIC) : QStringLiteral(SOUND);
	const auto type = kind == Kind::Music ? FileFinder::FileType::Music : FileFinder::FileType::Sound;
	const QStringList suffixes = FileFinder::GetFiltersForType(type);

	// The game's files, then the RTP's not replaced by one of the game (the game uses its own)
	const QString gameDir = FileFinder::Find(QDir(m_gamePath), folder, FileFinder::FileType::Default, QDir::Dirs);
	const QString rtpDir = m_rtpPath.isEmpty() ? QString()
		: FileFinder::Find(QDir(m_rtpPath), folder, FileFinder::FileType::Default, QDir::Dirs);
	auto files = audioFiles(gameDir, suffixes);
	const int gameCount = static_cast<int>(files.size());
	std::map<QString, bool> fromRtp;
	for (const auto& [key, path] : audioFiles(rtpDir, suffixes)) {
		if (files.emplace(key, path).second) {
			fromRtp[key] = true;
		}
	}

	std::vector<std::pair<QString, QString>> sorted;
	for (const auto& [key, path] : files) {
		sorted.emplace_back(QFileInfo(path).completeBaseName(), path);
	}
	QCollator collator;
	collator.setNumericMode(true);
	collator.setCaseSensitivity(Qt::CaseInsensitive);
	std::sort(sorted.begin(), sorted.end(), [&](const auto& a, const auto& b) {
		return collator.compare(a.first, b.first) < 0;
	});
	QList<AudioListModel::Entry> entries;
	for (const auto& [name, path] : sorted) {
		AudioListModel::Entry line;
		line.name = name;
		line.path = path;
		line.origin = fromRtp.count(name.toLower()) > 0 ? Origin::Rtp : Origin::Game;
		entries.append(line);
	}

	// The music and sound effects of the System tab of the database, with their settings
	for (const SystemFile& file : kind == Kind::Music ? m_systemMusic : m_systemSound) {
		if (file.name.isEmpty() || file.name == QLatin1String("(OFF)")) {
			continue;
		}
		// the game's file first, then the RTP's (as Project::findFile)
		QString path = FileFinder::Find(QDir(m_gamePath), folder, file.name, type);
		if (path.isEmpty() && !m_rtpPath.isEmpty()) {
			path = FileFinder::Find(QDir(m_rtpPath), folder, file.name, type);
		}
		AudioListModel::Entry line;
		line.name = QStringLiteral("%1: %2").arg(QCoreApplication::translate("JukeboxDialog", file.label), file.name);
		line.path = path;
		line.origin = path.isEmpty() ? Origin::Missing
			: QFileInfo(path).absolutePath() != QFileInfo(gameDir).absoluteFilePath() ? Origin::Rtp
			: Origin::Game;
		line.section = Section::System;
		line.settings = true;
		line.fadein = file.fadein;
		line.volume = file.volume;
		line.tempo = file.tempo;
		line.balance = file.balance;
		entries.append(line);
	}
	lines->setEntries(entries, gameCount, static_cast<int>(files.size()) - gameCount);
}

AudioLibraryModel::Kind AudioLibraryModel::kind() const {
	return m_kind;
}

void AudioLibraryModel::setKind(Kind kind) {
	if (kind == m_kind) {
		return;
	}
	m_kind = kind;
	// RPG Maker's sound effects have no fade-in
	m_player->setFadeIn(playerFadeIn());
	QSettings().setValue(QLatin1String(TAB_KEY), kind == Kind::Sound ? 1 : 0);
	emit kindChanged();
	emit loopChanged();
	emit canPlayChanged();
}

QString AudioLibraryModel::filter() const {
	return m_filter;
}

void AudioLibraryModel::setFilter(const QString& text) {
	if (text == m_filter) {
		return;
	}
	m_filter = text;
	m_music->setFilter(text);
	m_sound->setFilter(text);
	emit filterChanged();
	emit canPlayChanged();
}

int AudioLibraryModel::fadeIn() const {
	return m_fadeIn;
}

void AudioLibraryModel::setFadeIn(int seconds) {
	seconds = std::clamp(seconds, 0, 10);
	if (seconds == m_fadeIn) {
		return;
	}
	m_fadeIn = seconds;
	// for the next start: a fade-in is not heard in the middle of a file
	m_player->setFadeIn(playerFadeIn());
	emit fadeInChanged();
}

int AudioLibraryModel::volume() const {
	return m_volume;
}

void AudioLibraryModel::setVolume(int volume) {
	volume = std::clamp(volume, 0, 100);
	if (volume == m_volume) {
		return;
	}
	m_volume = volume;
	m_player->setVolume(volume);
	emit volumeChanged();
}

int AudioLibraryModel::tempo() const {
	return m_tempo;
}

void AudioLibraryModel::setTempo(int tempo) {
	tempo = std::clamp(tempo, 50, 150);
	if (tempo == m_tempo) {
		return;
	}
	m_tempo = tempo;
	m_player->setTempo(tempo);
	emit tempoChanged();
}

int AudioLibraryModel::balance() const {
	return m_balance;
}

void AudioLibraryModel::setBalance(int balance) {
	balance = std::clamp(balance, 0, 100);
	if (balance == m_balance) {
		return;
	}
	m_balance = balance;
	m_player->setBalance(balance);
	emit balanceChanged();
}

bool AudioLibraryModel::loop() const {
	return m_kind == Kind::Music ? m_loopMusic : m_loopSound;
}

void AudioLibraryModel::setLoop(bool loop) {
	if (loop == this->loop()) {
		return;
	}
	(m_kind == Kind::Music ? m_loopMusic : m_loopSound) = loop;
	// a list shown does not change the file of the other list playing
	if (m_playingKind == m_kind) {
		m_player->setLooping(loop);
	}
	emit loopChanged();
}

AudioPlayer::State AudioLibraryModel::state() const {
	return m_player->state();
}

bool AudioLibraryModel::canPlay() const {
	return currentList()->current() || m_player->state() == AudioPlayer::State::Paused;
}

qint64 AudioLibraryModel::position() const {
	return m_player->position();
}

qint64 AudioLibraryModel::duration() const {
	return m_player->duration();
}

QString AudioLibraryModel::status() const {
	return m_status;
}

bool AudioLibraryModel::statusIsError() const {
	return m_statusError;
}

void AudioLibraryModel::setStatus(const QString& text, bool error) {
	m_status = text;
	m_statusError = error;
	emit statusChanged();
}

int AudioLibraryModel::playerFadeIn() const {
	return m_kind == Kind::Music ? m_fadeIn * 1000 : 0;
}

void AudioLibraryModel::lineSelected(Kind kind) {
	// a file of the database: its settings, like the database plays it
	if (const AudioListModel::Entry* line = list(kind)->current(); line && line->settings) {
		setFadeIn(line->fadein / 1000);
		setVolume(line->volume);
		setTempo(line->tempo);
		setBalance(line->balance);
	}
	emit canPlayChanged();
}

bool AudioLibraryModel::playRow(Kind kind, int row) {
	const AudioListModel::Entry* line = list(kind)->entry(row);
	if (!line) {
		return false;
	}
	if (line->path.isEmpty()) {
		setStatus(QCoreApplication::translate("JukeboxDialog", "%1: no file of this name in the game or in the RTP.").arg(line->name), true);
		return false;
	}
	m_playingKind = kind;
	m_player->setFadeIn(kind == Kind::Music ? m_fadeIn * 1000 : 0);
	m_player->setLooping(kind == Kind::Music ? m_loopMusic : m_loopSound);
	m_playingName = QStringLiteral("%1 (%2)").arg(QFileInfo(line->path).fileName(),
		list(kind)->data(list(kind)->index(row), AudioListModel::SourceRole).toString());
	// a file again from the start, also when it is the one playing
	return m_player->play(line->path);
}

void AudioLibraryModel::play() {
	const int row = currentList()->currentRow();
	if (row >= 0) {
		playRow(m_kind, row);
	} else if (m_player->state() == AudioPlayer::State::Paused) {
		m_player->play();
	}
}

void AudioLibraryModel::pauseOrResume() {
	if (m_player->state() == AudioPlayer::State::Playing) {
		m_player->pause();
	} else if (m_player->state() == AudioPlayer::State::Paused) {
		m_player->play();
	}
}

void AudioLibraryModel::stop() {
	m_player->stop();
}

void AudioLibraryModel::seek(qint64 ms) {
	m_player->setPosition(ms);
}

QString AudioLibraryModel::timeText(qint64 ms) {
	const qint64 seconds = ms / 1000;
	return QStringLiteral("%1:%2").arg(seconds / 60).arg(seconds % 60, 2, 10, QLatin1Char('0'));
}
