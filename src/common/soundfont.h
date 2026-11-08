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

#include "audio_backend.h"

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

/**
 * What the header of a soundfont (SF2) says: its chunk table, its presets
 * and the names of its samples, never the samples themselves, so it takes
 * milliseconds on any size. Refused (not valid()) when it is no SF2 or a
 * damaged one; valid with warnings when RPG Maker's songs may not sound
 * right with it.
 */
struct SoundfontInfo {
	enum class Status {
		Valid,
		/** Not RIFF sfbk */
		NotSoundfont,
		/** A chunk missing, or past the end of the file */
		Damaged,
		/** The file cannot be read */
		Unreadable
	};

	Status status = Status::Unreadable;
	/** Why it is refused, empty when valid */
	QString error;
	/** Why RPG Maker's songs may sound wrong with it */
	QStringList warnings;
	/** INAM, empty when it has none */
	QString name;
	qint64 size = 0;
	/** Presets (bank and program), the drum kits included */
	int presets = 0;
	/** SF3: its samples compressed (Ogg Vorbis) */
	bool compressed = false;

	bool valid() const { return status == Status::Valid; }

	static SoundfontInfo read(const QString& path);
	/** "12.3 MB" */
	static QString sizeText(qint64 bytes);
};

/**
 * The soundfont the MIDI music of the project open plays with, chosen as
 * EasyRPG Player would choose it (decoder_fluidsynth.cpp), so the Editor
 * sounds as the game will:
 * 1. the game soundfont, easyrpg.soundfont in the folder of the game;
 * 2. the Editor default (Settings > Paths), as the Player's own setting;
 * 3. the system soundfont, in the standard folders of Linux;
 * 4. the synthesizer of the system: Apple's DLS bank, Windows' GS Wavetable
 *    Synth; else FmMidi.
 * A file is skipped when it is no valid SF2 (SoundfontInfo), or when it
 * failed to load (markFailed) and has not changed since.
 *
 * Every player of the Editor plans with it (AudioPlayer); the Jukebox
 * compares the sources one by one.
 */
class Soundfonts : public QObject {
	Q_OBJECT

public:
	/** What plays the MIDI music */
	enum Source {
		/** The order above */
		Automatic,
		Game,
		EditorDefault,
		System,
		/** FmMidi, EasyRPG Player's FM synthesizer */
		BuiltIn,
		/** Apple DLS, Microsoft GS Wavetable Synth */
		Platform,
		/** A file chosen (a soundfont tried before it is the game's) */
		File
	};
	Q_ENUM(Source)

	static Soundfonts& instance();

	/** easyrpg.soundfont in the folder (in any case, as the Player finds it), empty when there is none */
	static QString gameSoundfont(const QString& folder);
	/** Where the game soundfont of the folder is written: the file there, else easyrpg.soundfont */
	static QString gameSoundfontTarget(const QString& folder);
	/** The folder of the project open in the Editor, empty when none */
	static QString projectFolder();
	/** The Editor default (Settings > Paths), else EASYRPG_EDITOR_SOUNDFONT; empty when none */
	static QString editorDefault();
	/** The Editor default as chosen in the Settings (no EASYRPG_EDITOR_SOUNDFONT), empty when none */
	Q_INVOKABLE static QString editorDefaultSetting();
	Q_INVOKABLE static void setEditorDefault(const QString& path);
	/** The first soundfont of the standard folders of Linux, empty when none (always on macOS and Windows) */
	static QString systemSoundfont();
	/** The name of the file a game soundfont was copied from (remembered per project), empty when unknown */
	static QString originalName(const QString& folder);
	static void setOriginalName(const QString& folder, const QString& name);

	/** The file of a source (Game, EditorDefault, System), empty when it has none */
	Q_INVOKABLE QString path(Source source) const;
	/** Why a source cannot play here (the Jukebox greys it), empty when it can */
	Q_INVOKABLE QString unavailable(Source source) const;
	/** Why a file is refused as a soundfont (missing, no SF2, damaged), empty when it is one */
	Q_INVOKABLE QString refusal(const QString& path) const;
	/** "Game soundfont", "Apple DLS"... */
	Q_INVOKABLE static QString sourceName(Source source);

	/**
	 * The choices of the Jukebox, in their order: {source, name, available,
	 * why}; Automatic named with what it chooses ("Automatic: Game soundfont")
	 */
	Q_INVOKABLE QVariantList choices() const;
	/**
	 * What plays, as the line under the Jukebox's choice tells it: the file
	 * and its size, or the synthesizer; playing (a Source) and soundfont:
	 * what the player said (AudioPlayer::synthSource), else what the choice
	 * would play
	 */
	Q_INVOKABLE QString describe(int choice, int playing, const QString& soundfont) const;
	/** The soundfonts can be played: Apple's synthesizer (macOS), else FluidSynth */
	Q_INVOKABLE static bool soundfontsPlayable();

	/** What plays the MIDI music for this choice (file: the soundfont of File) */
	MidiPlan plan(Source choice = Automatic, const QString& file = QString()) const;
	/** What Automatic chooses now: the first source of plan() that may play */
	Source automaticSource() const;

	/** The header of a soundfont, read again when the file changes */
	SoundfontInfo info(const QString& path) const;
	/** A soundfont that could not be loaded: the plans skip it until it changes */
	void markFailed(const QString& path, const QString& why);
	/** Why it could not be loaded, empty when it can be tried */
	QString failure(const QString& path) const;

signals:
	/** The plans may change: a soundfont failed, the Editor default changed */
	void changed();

private:
	Soundfonts() = default;

	/** A soundfont of a plan: valid, not failed */
	bool usable(const QString& path) const;

	struct Stamp {
		QDateTime modified;
		qint64 size = 0;
		bool operator==(const Stamp&) const = default;
	};
	static Stamp stamp(const QString& path);

	mutable QHash<QString, std::pair<Stamp, SoundfontInfo>> m_infos;
	QHash<QString, std::pair<Stamp, QString>> m_failures;
};
