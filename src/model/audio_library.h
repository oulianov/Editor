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

#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QString>
#include "common/audio_player.h"

namespace lcf::rpg {
class System;
}

/**
 * The music or the sound effects of a game, as filtered: its files and the
 * RTP's it does not replace (section Files), then the music and sounds of
 * the System tab of the database with their settings (section System).
 */
class AudioListModel : public QAbstractListModel
{
	Q_OBJECT
	Q_PROPERTY(int currentRow READ currentRow WRITE setCurrentRow NOTIFY currentRowChanged)
	Q_PROPERTY(QString filesTitle READ filesTitle NOTIFY filesTitleChanged)
	Q_PROPERTY(QString systemTitle READ systemTitle CONSTANT)

public:
	enum class Origin { Game, Rtp, Missing };
	Q_ENUM(Origin)
	enum class Section { Files, System };
	Q_ENUM(Section)

	enum Role {
		/** The file's name; "Battle: Battle1" for the database's (DisplayRole too) */
		NameRole = Qt::UserRole + 1,
		/** Empty when the game and the RTP have no such file */
		PathRole,
		/** MIDI, WAV, OGG... */
		FormatRole,
		/** Game, RTP or Not found, as the list shows it */
		SourceRole,
		/** Origin */
		OriginRole,
		/** Section */
		SectionRole,
		/** "Files (12 in the game, 3 in the RTP)", "System (database)" */
		SectionTitleRole,
		/** A file of the database, played with its settings */
		HasSettingsRole,
		/** The database's settings: fade-in in ms, volume, tempo, balance */
		FadeInRole,
		VolumeRole,
		TempoRole,
		BalanceRole,
	};

	struct Entry {
		QString name;
		QString path;
		Origin origin = Origin::Game;
		Section section = Section::Files;
		bool settings = false;
		int fadein = 0;
		int volume = 100;
		int tempo = 100;
		int balance = 50;
	};

	// QAbstractItemModel
	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	QHash<int, QByteArray> roleNames() const override;

	/** The line of a row, nullptr out of the list */
	const Entry* entry(int row) const;
	/** The line selected, -1 when none */
	int currentRow() const;
	void setCurrentRow(int row);
	const Entry* current() const;

	QString filesTitle() const;
	QString systemTitle() const;

signals:
	void currentRowChanged();
	void filesTitleChanged();

private:
	friend class AudioLibraryModel;
	explicit AudioListModel(QObject* parent);
	/** The lines listed again: the same line as before, else the first */
	void setEntries(const QList<Entry>& entries, int game_files, int rtp_files);
	/** The selection follows the filter: the first line shown */
	void setFilter(const QString& text);
	void applyFilter();
	/** Selects a line of m_entries (-1: none) */
	void select(int entry);

	QList<Entry> m_entries;
	QList<int> m_rows;
	int m_current = -1;
	QString m_filter;
	int m_gameFiles = 0;
	int m_rtpFiles = 0;
};

/**
 * Tools > Jukebox, without its window: the music and the sound effects of
 * the game and of the RTP, the file selected in each list, and the player
 * with RPG Maker's settings (fade-in, volume, tempo, balance) and the loop
 * of each list. A file of the database selected brings its settings.
 */
class AudioLibraryModel : public QObject
{
	Q_OBJECT
	Q_PROPERTY(AudioListModel* music READ music CONSTANT)
	Q_PROPERTY(AudioListModel* sound READ sound CONSTANT)
	Q_PROPERTY(AudioPlayer* player READ player CONSTANT)
	Q_PROPERTY(QString gamePath READ gamePath WRITE setGamePath NOTIFY gamePathChanged)
	Q_PROPERTY(QString rtpPath READ rtpPath WRITE setRtpPath NOTIFY rtpPathChanged)
	Q_PROPERTY(Kind kind READ kind WRITE setKind NOTIFY kindChanged)
	Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)
	Q_PROPERTY(int fadeIn READ fadeIn WRITE setFadeIn NOTIFY fadeInChanged)
	Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged)
	Q_PROPERTY(int tempo READ tempo WRITE setTempo NOTIFY tempoChanged)
	Q_PROPERTY(int balance READ balance WRITE setBalance NOTIFY balanceChanged)
	Q_PROPERTY(bool loop READ loop WRITE setLoop NOTIFY loopChanged)
	Q_PROPERTY(AudioPlayer::State state READ state NOTIFY stateChanged)
	Q_PROPERTY(bool canPlay READ canPlay NOTIFY canPlayChanged)
	Q_PROPERTY(qint64 position READ position NOTIFY positionChanged)
	Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
	Q_PROPERTY(QString status READ status NOTIFY statusChanged)
	Q_PROPERTY(bool statusIsError READ statusIsError NOTIFY statusChanged)

public:
	enum class Kind { Music, Sound };
	Q_ENUM(Kind)

	explicit AudioLibraryModel(QObject* parent = nullptr);
	~AudioLibraryModel() override;

	AudioListModel* music() const;
	AudioListModel* sound() const;
	AudioListModel* list(Kind kind) const;
	/** The list shown */
	AudioListModel* currentList() const;
	AudioPlayer* player() const;

	/** The folder of the game, empty when no game is open (the lists are empty) */
	QString gamePath() const;
	void setGamePath(const QString& path);
	bool hasGame() const;
	/** The RTP folder of the settings, empty when none */
	QString rtpPath() const;
	void setRtpPath(const QString& path);
	/** The music and the sounds of the System tab of the database */
	void setSystem(const lcf::rpg::System& system);
	void clearSystem();
	/** Lists the files again, keeping the lines selected */
	Q_INVOKABLE void refresh();

	/** The list shown (remembered for the next time) */
	Kind kind() const;
	void setKind(Kind kind);
	/** Only the lines whose name has it, in both lists */
	QString filter() const;
	void setFilter(const QString& text);

	/** Seconds of fade-in of the music, 0 to 10 (sound effects have none) */
	int fadeIn() const;
	void setFadeIn(int seconds);
	/** 0 to 100 % */
	int volume() const;
	void setVolume(int volume);
	/** 50 to 150 % */
	int tempo() const;
	void setTempo(int tempo);
	/** 0 left, 50 center, 100 right */
	int balance() const;
	void setBalance(int balance);
	/** Repeat at the end: the loop of the list shown (music on, sounds off at first) */
	bool loop() const;
	void setLoop(bool loop);

	AudioPlayer::State state() const;
	/** A line is selected, or a file is paused */
	bool canPlay() const;
	qint64 position() const;
	qint64 duration() const;
	/** Playing, paused, stopped, or what went wrong */
	QString status() const;
	bool statusIsError() const;

	/** Plays a line from its start, also the one playing; false when it has no file */
	Q_INVOKABLE bool playRow(AudioLibraryModel::Kind kind, int row);
	/** Plays the line selected, else resumes the file paused */
	Q_INVOKABLE void play();
	Q_INVOKABLE void pauseOrResume();
	Q_INVOKABLE void stop();
	Q_INVOKABLE void seek(qint64 ms);

	/** "1:05" */
	Q_INVOKABLE static QString timeText(qint64 ms);

signals:
	void gamePathChanged();
	void rtpPathChanged();
	void kindChanged();
	void filterChanged();
	void fadeInChanged();
	void volumeChanged();
	void tempoChanged();
	void balanceChanged();
	void loopChanged();
	void stateChanged();
	void canPlayChanged();
	void positionChanged(qint64 ms);
	void durationChanged(qint64 ms);
	void statusChanged();

private:
	struct SystemFile {
		/** The line's label, untranslated (context JukeboxDialog) */
		const char* label;
		QString name;
		int fadein;
		int volume;
		int tempo;
		int balance;
	};

	void fill(Kind kind);
	/** A line of the database selected: its settings, like the database plays it */
	void lineSelected(Kind kind);
	void setStatus(const QString& text, bool error = false);
	int playerFadeIn() const;

	AudioListModel* m_music;
	AudioListModel* m_sound;
	AudioPlayer* m_player;
	QString m_gamePath;
	QString m_rtpPath;
	QList<SystemFile> m_systemMusic;
	QList<SystemFile> m_systemSound;
	QString m_filter;
	Kind m_kind = Kind::Music;
	// the file shown as playing, for the status, and its list
	QString m_playingName;
	Kind m_playingKind = Kind::Music;
	QString m_status;
	bool m_statusError = false;

	int m_fadeIn = 0;
	int m_volume = 100;
	int m_tempo = 100;
	int m_balance = 50;
	// loop on for music, off for sounds; remembered per list
	bool m_loopMusic = true;
	bool m_loopSound = false;
};
