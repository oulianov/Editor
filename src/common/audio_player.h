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

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <memory>

class AudioBackend;

/**
 * Plays one music or sound file of a game with RPG Maker's settings (volume,
 * tempo, balance, fade-in), for the previews of the pickers and the Jukebox.
 *
 * On macOS, AVAudioEngine plays WAV, MP3, OGG, Opus... and MIDI with the
 * system's sound bank; elsewhere (or for what it cannot read) QMediaPlayer
 * plays what the Qt Multimedia backend can decode, and MIDI is not supported.
 *
 * In QML (Ez.AudioPlayer) its properties and play(path), play(), pause()
 * and stop() are the same.
 */
class AudioPlayer : public QObject {
	Q_OBJECT
	Q_PROPERTY(State state READ state NOTIFY stateChanged)
	Q_PROPERTY(QString filePath READ filePath NOTIFY fileChanged)
	/** Why the file loaded cannot be played, empty when it can */
	Q_PROPERTY(QString errorString READ errorString NOTIFY fileChanged)
	Q_PROPERTY(qint64 position READ position WRITE setPosition NOTIFY positionChanged)
	Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
	Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY settingsChanged)
	Q_PROPERTY(int tempo READ tempo WRITE setTempo NOTIFY settingsChanged)
	Q_PROPERTY(int balance READ balance WRITE setBalance NOTIFY settingsChanged)
	Q_PROPERTY(int fadeIn READ fadeIn WRITE setFadeIn NOTIFY settingsChanged)
	Q_PROPERTY(bool looping READ isLooping WRITE setLooping NOTIFY settingsChanged)

public:
	enum class State {
		Stopped,
		Playing,
		Paused
	};
	Q_ENUM(State)

	explicit AudioPlayer(QObject* parent = nullptr);
	~AudioPlayer() override;

	/** Plays a file from the start (the path with its extension); false and an error when it cannot */
	Q_INVOKABLE bool play(const QString& path);
	/** Plays the last file again, or resumes it when paused */
	Q_INVOKABLE void play();
	Q_INVOKABLE void pause();
	Q_INVOKABLE void stop();

	State state() const { return m_state; }
	/** The file loaded (played, paused or stopped), empty when none */
	QString filePath() const { return m_path; }
	QString errorString() const { return m_error; }

	qint64 position() const;
	qint64 duration() const;
	void setPosition(qint64 ms);

	/** 0 to 100 % */
	int volume() const { return m_volume; }
	void setVolume(int volume);
	/** 50 to 150 %: speed and pitch of sampled files, speed of MIDI */
	int tempo() const { return m_tempo; }
	void setTempo(int tempo);
	/** RPG Maker's balance: 0 left, 50 center, 100 right */
	int balance() const { return m_balance; }
	void setBalance(int balance);
	/** Milliseconds of fade-in when a file starts */
	int fadeIn() const { return m_fadeIn; }
	void setFadeIn(int ms);
	/** Repeats from the MIDI loop point, or the beginning when none is set */
	void setLooping(bool looping);
	bool isLooping() const { return m_looping; }

	/** Whether the balance is applied to the file loaded (not by QMediaPlayer) */
	bool canPan() const;

	static bool isMidi(const QString& path);

signals:
	void stateChanged(AudioPlayer::State state);
	void positionChanged(qint64 ms);
	void durationChanged(qint64 ms);
	void errorOccurred(const QString& message);
	/** Another file was loaded, or it cannot be played (filePath, errorString) */
	void fileChanged();
	/** The volume, the tempo, the balance, the fade-in or the loop */
	void settingsChanged();

private:
	void setState(State state);
	void fail(const QString& message);
	void applyGain();
	void tick();

	std::unique_ptr<AudioBackend> m_backend;
	QString m_path;
	QString m_error;
	State m_state = State::Stopped;
	QTimer m_timer;
	// since the start, for the fade-in
	QElapsedTimer m_started;
	qint64 m_lastDuration = 0;

	int m_volume = 100;
	int m_tempo = 100;
	int m_balance = 50;
	int m_fadeIn = 0;
	bool m_looping = false;
};
