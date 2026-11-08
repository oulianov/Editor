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

#include "audio_player.h"
#include "audio_backend.h"
#include "common/logging.h"
#include "soundfont.h"

#include <QAudio>
#include <QAudioOutput>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QMediaPlayer>
#include <QSet>
#include <QUrl>
#include <algorithm>

namespace {

/**
 * EASYRPG_EDITOR_AUDIO_BACKEND, to test the backends of other systems:
 * "qt" plays on macOS as on Linux (Qt Multimedia, and MIDI with FluidSynth
 * when found, else FmMidi); "fmmidi" does the same with FmMidi for MIDI on
 * every system (on Windows, instead of its MIDI device).
 * EASYRPG_EDITOR_SOUNDFONT is the Editor default soundfont while Settings
 * has none (soundfont.h).
 */
QString backendOverride() {
	return qEnvironmentVariable("EASYRPG_EDITOR_AUDIO_BACKEND").toLower();
}

/** Qt Multimedia: what the platform's backend decodes (FFmpeg, AVFoundation, Media Foundation, GStreamer) */
class QtMediaBackend final : public AudioBackend {
public:
	QString name() const override { return QStringLiteral("Qt Multimedia"); }

	bool open(const QString& path, bool, QString& error) override {
		// no media plugin of Qt Multimedia (its FFmpeg plugin, missing from a package): nothing would happen
		if (!m_player.isAvailable()) {
			error = AudioPlayer::tr("Qt Multimedia cannot play files on this system: its media plugin is missing.");
			return false;
		}
		m_player.setAudioOutput(&m_output);
		// loads asynchronously: errors come through error()
		m_player.setSource(QUrl::fromLocalFile(path));
		m_opened.start();
		return true;
	}

	void play() override { m_player.play(); }
	void pause() override { m_player.pause(); }
	void stop() override { m_player.stop(); }
	void seek(qint64 ms) override { m_player.setPosition(ms); }

	void setVolume(float volume, float fade) override { m_output.setVolume(linearGain(volume) * fade); }
	void setPan(float) override {}
	void setRate(float rate) override { m_player.setPlaybackRate(rate); }
	bool canPan() const override { return false; }

	qint64 position() const override { return m_player.position(); }
	qint64 duration() const override { return m_player.duration(); }
	bool finished() const override { return m_player.mediaStatus() == QMediaPlayer::EndOfMedia; }

	QString error() const override {
		if (m_player.error() != QMediaPlayer::NoError) {
			return m_player.errorString();
		}
		// some backends (AVFoundation) never answer for a format they do not know
		const auto status = m_player.mediaStatus();
		if (status == QMediaPlayer::InvalidMedia
				|| ((status == QMediaPlayer::LoadingMedia || status == QMediaPlayer::NoMedia) && m_opened.elapsed() > 5000)) {
			return AudioPlayer::tr("The format of this file is not supported.");
		}
		return QString();
	}

private:
	QElapsedTimer m_opened;
	// the player uses the output: destroyed first
	QAudioOutput m_output;
	QMediaPlayer m_player;
};

}

float AudioBackend::linearGain(float volume) {
	return QtAudio::convertVolume(volume, QtAudio::LogarithmicVolumeScale, QtAudio::LinearVolumeScale);
}

AudioPlayer::AudioPlayer(QObject* parent) : QObject(parent) {
	// position, end of file, fade-in
	m_timer.setInterval(40);
	connect(&m_timer, &QTimer::timeout, this, &AudioPlayer::tick);
}

AudioPlayer::~AudioPlayer() = default;

bool AudioPlayer::isMidi(const QString& path) {
	const QString suffix = QFileInfo(path).suffix().toLower();
	return suffix == QLatin1String("mid") || suffix == QLatin1String("midi");
}

bool AudioPlayer::play(const QString& path) {
	// the backend of macOS loads the next file in its engine, whose output goes on: quicker
	std::unique_ptr<AudioBackend> previous = std::move(m_backend);
	if (previous) {
		previous->stop();
		if (!previous->canReopen()) {
			previous.reset();
		}
	}
	m_path = path;
	m_error.clear();
	m_lastDuration = 0;
	setState(State::Stopped);
	emit fileChanged();

	if (!QFileInfo::exists(path)) {
		fail(tr("The file %1 does not exist.").arg(QFileInfo(path).fileName()));
		return false;
	}

	const bool midi = isMidi(path);
	const QString fileName = QFileInfo(path).fileName();
	const QString forced = backendOverride();
	// MIDI: the soundfonts to try, as the Player would for the project open
	MidiPlan plan;
	if (midi) {
		plan = Soundfonts::instance().plan(static_cast<Soundfonts::Source>(m_soundfontChoice), m_soundfontFile);
		if (forced == QLatin1String("fmmidi")) {
			plan = MidiPlan{{}, false};
		}
	}
	QString error;
#ifdef Q_OS_MACOS
	// Apple's synthesizer plays the soundfonts and its own bank, not FmMidi
	if (forced.isEmpty() && (!midi || plan.systemSynth || !plan.soundfonts.isEmpty())) {
		m_backend = previous ? std::move(previous) : createAppleAudioBackend();
		m_backend->setMidiPlan(plan);
		if (!m_backend->open(path, midi, error)) {
			qCWarning(lcAudio).noquote() << "AVAudioEngine cannot play" << fileName << ":" << error;
			updateSynth();
			m_backend.reset();
		}
	}
#endif
	previous.reset();
	// Qt Multimedia has no synthesizer: the Editor's
	if (!m_backend) {
		m_backend = midi ? createMidiAudioBackend(plan) : std::make_unique<QtMediaBackend>();
		if (!m_backend->open(path, midi, error)) {
			fail(tr("%1 cannot be played: %2").arg(fileName, error));
			return false;
		}
	}

	// the fade-in starts silent
	m_started.start();
	applyGain();
	m_backend->setPan((std::clamp(m_balance, 0, 100) - 50) / 50.0f);
	m_backend->setRate(std::clamp(m_tempo, 50, 150) / 100.0f);
	m_backend->setLooping(m_looping);
	m_backend->play();

	// which plays (a MIDI backend chooses as it starts): once each, for those
	// who tell what they hear, or not
	static QSet<QString> logged;
	if (!logged.contains(m_backend->name())) {
		logged.insert(m_backend->name());
		qCInfo(lcAudio).noquote() << "Audio:" << m_backend->name() << "plays" << fileName;
	} else {
		qCDebug(lcAudio).noquote() << "Audio:" << m_backend->name() << "plays" << fileName;
	}
	setState(State::Playing);
	updateSynth();
	tick();
	return true;
}

void AudioPlayer::play() {
	if (!m_backend) {
		if (!m_path.isEmpty()) {
			play(m_path);
		}
		return;
	}
	if (m_state == State::Stopped) {
		// from the start: fade in again
		m_started.start();
		applyGain();
	}
	m_backend->play();
	setState(State::Playing);
}

void AudioPlayer::pause() {
	if (m_backend && m_state == State::Playing) {
		m_backend->pause();
		setState(State::Paused);
	}
}

void AudioPlayer::stop() {
	if (m_backend) {
		m_backend->stop();
	}
	setState(State::Stopped);
	emit positionChanged(0);
}

qint64 AudioPlayer::position() const {
	return m_backend ? m_backend->position() : 0;
}

qint64 AudioPlayer::duration() const {
	return m_backend ? m_backend->duration() : 0;
}

void AudioPlayer::setPosition(qint64 ms) {
	if (m_backend) {
		m_backend->seek(ms);
		emit positionChanged(m_backend->position());
	}
}

void AudioPlayer::setVolume(int volume) {
	m_volume = std::clamp(volume, 0, 100);
	applyGain();
	emit settingsChanged();
}

void AudioPlayer::setTempo(int tempo) {
	m_tempo = std::clamp(tempo, 50, 150);
	if (m_backend) {
		m_backend->setRate(m_tempo / 100.0f);
	}
	emit settingsChanged();
}

void AudioPlayer::setBalance(int balance) {
	m_balance = std::clamp(balance, 0, 100);
	if (m_backend) {
		m_backend->setPan((m_balance - 50) / 50.0f);
	}
	emit settingsChanged();
}

void AudioPlayer::setFadeIn(int ms) {
	m_fadeIn = std::max(0, ms);
	emit settingsChanged();
}

void AudioPlayer::setLooping(bool looping) {
	m_looping = looping;
	if (m_backend) {
		m_backend->setLooping(looping);
	}
	emit settingsChanged();
}

bool AudioPlayer::canPan() const {
	return !m_backend || m_backend->canPan();
}

void AudioPlayer::setSoundfontChoice(int choice) {
	if (choice == m_soundfontChoice) {
		return;
	}
	m_soundfontChoice = choice;
	emit soundfontChoiceChanged();
	replayMidi();
}

void AudioPlayer::setSoundfontFile(const QString& file) {
	if (file == m_soundfontFile) {
		return;
	}
	m_soundfontFile = file;
	emit soundfontChoiceChanged();
	if (m_soundfontChoice == Soundfonts::File) {
		replayMidi();
	}
}

void AudioPlayer::replayMidi() {
	if (!m_backend || !isMidi(m_path) || m_state == State::Stopped) {
		return;
	}
	const qint64 position = m_backend->position();
	const bool paused = m_state == State::Paused;
	if (!play(m_path)) {
		return;
	}
	// where it was, without its fade-in again
	m_started.invalidate();
	applyGain();
	setPosition(position);
	if (paused) {
		pause();
	}
}

void AudioPlayer::updateSynth() {
	Soundfonts& soundfonts = Soundfonts::instance();
	int source = Soundfonts::Automatic;
	QString soundfont;
	if (m_backend) {
		for (const auto& [path, why] : m_backend->takeFailures()) {
			soundfonts.markFailed(path, why);
		}
		switch (m_backend->synth()) {
			case AudioBackend::Synth::Soundfont:
				soundfont = m_backend->soundfont();
				source = m_soundfontChoice == Soundfonts::File ? Soundfonts::File
					: soundfont == soundfonts.path(Soundfonts::Game) ? Soundfonts::Game
					: soundfont == soundfonts.path(Soundfonts::EditorDefault) ? Soundfonts::EditorDefault
					: soundfont == soundfonts.path(Soundfonts::System) ? Soundfonts::System : Soundfonts::File;
				break;
			case AudioBackend::Synth::System:
				source = Soundfonts::Platform;
				break;
			case AudioBackend::Synth::FmMidi:
				source = Soundfonts::BuiltIn;
				break;
			case AudioBackend::Synth::None:
				break;
		}
	}
	if (source != m_synthSource || soundfont != m_synthSoundfont) {
		m_synthSource = source;
		m_synthSoundfont = soundfont;
		qCDebug(lcAudio).noquote() << "MIDI:" << Soundfonts::sourceName(static_cast<Soundfonts::Source>(source)) << soundfont;
		emit synthChanged();
	}
}

void AudioPlayer::setState(State state) {
	if (state == State::Playing) {
		m_timer.start();
	} else {
		m_timer.stop();
	}
	if (m_state != state) {
		m_state = state;
		emit stateChanged(state);
	}
}

void AudioPlayer::fail(const QString& message) {
	qCWarning(lcAudio).noquote() << message;
	if (m_backend) {
		m_backend->stop();
	}
	m_backend.reset();
	m_error = message;
	setState(State::Stopped);
	emit errorOccurred(message);
	emit fileChanged();
}

void AudioPlayer::applyGain() {
	if (!m_backend) {
		return;
	}
	// the volume and the fade separately: MIDI scales its channels with them as the Player
	float fade = 1;
	if (m_fadeIn > 0 && m_started.isValid() && m_started.elapsed() < m_fadeIn) {
		fade = static_cast<float>(m_started.elapsed()) / m_fadeIn;
	}
	m_backend->setVolume(m_volume / 100.0f, fade);
}

void AudioPlayer::tick() {
	if (!m_backend) {
		return;
	}
	if (const QString error = m_backend->error(); !error.isEmpty()) {
		fail(tr("%1 cannot be played: %2").arg(QFileInfo(m_path).fileName(), error));
		return;
	}
	if (m_fadeIn > 0 && m_started.isValid() && m_started.elapsed() <= m_fadeIn + m_timer.interval()) {
		applyGain();
	}
	updateSynth();
	if (const qint64 length = m_backend->duration(); length != m_lastDuration) {
		m_lastDuration = length;
		emit durationChanged(length);
	}
	if (m_state == State::Playing && m_backend->finished()) {
		if (m_looping) {
			m_backend->seek(m_backend->loopStart());
			m_backend->play();
		} else {
			stop();
			return;
		}
	}
	emit positionChanged(m_backend->position());
}
