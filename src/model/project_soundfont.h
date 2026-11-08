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

#include "common/soundfont.h"

#include <QObject>
#include <QString>
#include <atomic>
#include <memory>
#include <optional>

/**
 * The game soundfont, as the Soundfont section of Project Settings shows,
 * chooses and removes it: easyrpg.soundfont in the folder of the game, which
 * EasyRPG Player plays the MIDI music with (Soundfonts). Nothing else is
 * written; the name of the file it was copied from is remembered in the
 * settings of the Editor, to show it.
 *
 * Choose and Remove wait for OK (apply()): the file chosen is copied away
 * from the interface, into a temporary file renamed once whole (cancel():
 * the old one stays); the old one goes to the trash where the system has
 * one. The header of the file is checked (SoundfontInfo) when it is chosen
 * and when the section opens.
 *
 * The texts are in the context ProjectSettingsDialog, as the dialog's.
 */
class ProjectSoundfont : public QObject
{
	Q_OBJECT
	/** There will be a game soundfont (the one there, or the one chosen) */
	Q_PROPERTY(bool present READ present NOTIFY changed)
	/** The file it will be: the one chosen, else the game's; empty when none */
	Q_PROPERTY(QString path READ path NOTIFY changed)
	/** Its name, size and presets; or that players hear their own */
	Q_PROPERTY(QString summary READ summary NOTIFY changed)
	/** Why it is refused, or what may sound wrong with it, a line each; empty when nothing */
	Q_PROPERTY(QString warning READ warning NOTIFY changed)
	/** It is refused: no SF2, or damaged */
	Q_PROPERTY(bool invalid READ invalid NOTIFY changed)
	/** Choose or Remove waits for OK: what it will do, empty when nothing waits */
	Q_PROPERTY(QString pendingText READ pendingText NOTIFY changed)
	Q_PROPERTY(bool pending READ pending NOTIFY changed)
	/** The copy runs */
	Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
	/** 0 to 1 */
	Q_PROPERTY(double progress READ progress NOTIFY progressChanged)

public:
	explicit ProjectSoundfont(const QString& folder, QObject* parent = nullptr);
	~ProjectSoundfont() override;

	bool present() const;
	QString path() const;
	QString summary() const;
	QString warning() const;
	bool invalid() const;
	QString pendingText() const;
	bool pending() const { return m_choice.has_value(); }
	bool busy() const { return m_busy; }
	double progress() const { return m_progress; }

	/** Why the file cannot be the game soundfont (no SF2, damaged), empty when it can */
	Q_INVOKABLE QString check(const QString& file) const;
	/** What to confirm before choosing it (replacing the one there, a large file), empty for nothing */
	Q_INVOKABLE QString chooseConfirmation(const QString& file) const;
	/** The file becomes the game soundfont on OK */
	Q_INVOKABLE void choose(const QString& file);
	/** What to confirm before removing the game soundfont, empty when nothing is removed */
	Q_INVOKABLE QString removeConfirmation() const;
	/** No game soundfont on OK (a choice waiting is dropped) */
	Q_INVOKABLE void remove();
	/** Nothing waits for OK any more: the game soundfont stays as it is */
	Q_INVOKABLE void revert();
	/** Shows the file in Finder, Explorer, the file manager */
	Q_INVOKABLE void reveal() const;
	/** A short MIDI file to hear the soundfont with: the smallest of the Music folder, else a sample of the Editor */
	Q_INVOKABLE QString sampleMidi() const;

	/**
	 * Does what waits (OK): the error at once when it cannot (the folder is
	 * read-only...), else empty and finished() once done; nothing waiting,
	 * finished() comes too
	 */
	Q_INVOKABLE QString apply();
	/** Stops the copy: the old file stays (finished(), cancelled) */
	Q_INVOKABLE void cancel();
	/** apply() and its end, for the scripts: the error, empty when it worked */
	QString applyAndWait();

	/** The file name a soundfont is shown with: INAM, else the name it was copied from, else its own */
	static QString displayName(const SoundfontInfo& info, const QString& original, const QString& file);

signals:
	void changed();
	void busyChanged();
	void progressChanged();
	/** apply() is done: the error, empty when it worked or was cancelled */
	void finished(const QString& error, bool cancelled);

private:
	/** The game soundfont there, read again */
	void readGame();
	void setBusy(bool busy);
	void done(const QString& error, bool cancelled);
	/** The folder cannot be written: the message, else empty */
	QString readOnlyError() const;

	QString m_folder;
	/** The game soundfont there, empty when none */
	QString m_game;
	SoundfontInfo m_gameInfo;
	/** Waiting for OK: a file to copy, or empty to remove */
	std::optional<QString> m_choice;
	SoundfontInfo m_choiceInfo;

	bool m_busy = false;
	double m_progress = 0;
	/** The copy, shared with its thread: stop it, how far it is */
	struct Copy {
		std::atomic<bool> cancelled = false;
		std::atomic<qint64> written = 0;
		qint64 size = 0;
	};
	std::shared_ptr<Copy> m_copy;
};
