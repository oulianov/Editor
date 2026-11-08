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

// MIDI playback where Qt Multimedia has none (Linux, Windows), as EasyRPG
// Player plays it. Its sequencer (midi/midisequencer.h) sends the messages of
// the file to a MidiOutput (midi_synth.h), with the Player's volume (the
// controller 7 of each channel, scaled), balance (the controller 10, shifted),
// tempo (the speed of the sequencer) and loop (at the controller 111):
// MidiSequence, from the Player's audio_decoder_midi.cpp. Two drivers move it
// on, as in the Player:
// - the MIDI device of Windows (midiout_device_win32.h), with a thread that
//   moves the sequencer on with the clock every millisecond (DeviceDriver,
//   from the Player's audio_generic_midiout.cpp);
// - a software synthesizer, whose audio QAudioSink pulls (PulseAudio or
//   PipeWire, WASAPI, Core Audio), the sequencer moving on with the frames
//   made (PcmDriver): FluidSynth for a soundfont, loaded away from the
//   interface while the music waits; FmMidi on Linux without one, and on
//   Windows when its device is busy or fails.
// The audio is made ahead of what is heard (the buffer of the output): the
// position is the one of the frames played, and a file ends once its last
// notes faded out (kTail).

#include "audio_backend.h"
#include "audio_player.h"
#include "common/logging.h"
#include "midi/midisequencer.h"
#include "midi_synth.h"
#ifdef _WIN32
#include "midiout_device_win32.h"
#endif

#include <QAudio>
#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QFile>
#include <QFileInfo>
#include <QFuture>
#include <QMediaDevices>
#include <QtConcurrent>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace {

/** Frames between two passes of the sequencer: ~1.5 ms, the Player's resolution */
constexpr int kStep = 64;
/** How long the last notes of a file sound after its end (their release), before it is over */
constexpr qint64 kTailUs = 750'000;
/** Reads past the end of a file before it is damaged: a last track declared a little too long plays, as in the Player */
constexpr int kReadsPastEnd = 64 * 1024;

uint32_t controlChange(uint32_t channel, uint32_t controller, uint32_t value) {
	return 0xB0 | (channel & 0x0F) | (controller << 8) | (value << 16);
}

/**
 * The bytes of the file for the sequencer, which reads on forever past the
 * end of a damaged one (a track without its end): stopped after kReadsPastEnd
 */
struct FileReader {
	struct PastTheEnd {};

	const QByteArray& bytes;
	qsizetype next = 0;
	int pastEnd = 0;

	static int read(void* reader) {
		auto* self = static_cast<FileReader*>(reader);
		if (self->next < self->bytes.size()) {
			return static_cast<unsigned char>(self->bytes[self->next++]);
		}
		if (++self->pastEnd > kReadsPastEnd) {
			throw PastTheEnd();
		}
		return EOF;
	}
};

/**
 * A MIDI file played: the sequencer, where it is, and what the Player
 * changes in the messages for RPG Maker's settings. The drivers move it on
 * from their threads: all but position(), duration(), loopStart(), ended()
 * and afterEnd() with mutex() held.
 */
class MidiSequence final : private midisequencer::output {
public:
	MidiSequence() {
		m_channelVolumes.fill(100);
		m_channelPans.fill(64);
	}

	std::mutex& mutex() { return m_mutex; }

	bool load(const QByteArray& bytes, QString& error) {
		FileReader reader{bytes};
		bool loaded = false;
		try {
			loaded = m_sequencer.load(&reader, &FileReader::read);
		} catch (const FileReader::PastTheEnd&) {
			error = AudioPlayer::tr("The MIDI file is damaged.");
			return false;
		} catch (const std::exception&) {
			// a length read from a damaged file, too large
			error = AudioPlayer::tr("The MIDI file is damaged.");
			return false;
		}
		if (!loaded) {
			error = AudioPlayer::tr("This is not a MIDI file.");
			return false;
		}
		m_totalUs = m_sequencer.get_total_time().count();
		if (m_totalUs <= 0) {
			error = AudioPlayer::tr("The MIDI file has no music.");
			return false;
		}
		// the first message at the time of the last controller 111, the first message when none
		m_loopUs = m_sequencer.rewind_to_loop()->time.count();
		// from the first note, as RPG_RT
		m_startUs = m_sequencer.get_start_skipping_silence().count();
		rewind();
		return true;
	}

	/** Where the messages go from now on (none: nowhere) */
	void setOutput(MidiOutput* output) {
		m_output = output;
		// moved while the messages went nowhere: the programs and controllers on the way there, now
		if (m_output && m_unsent) {
			m_unsent = false;
			m_sequencer.rewind();
			m_sequencer.set_time(std::chrono::microseconds(static_cast<qint64>(m_timeUs)), this);
		}
	}
	MidiOutput* output() const { return m_output; }

	/** Moves on by some time (at the normal tempo), around the loop (the Player's UpdateMidi) */
	void update(std::chrono::microseconds delta) {
		if (m_paused || !m_output) {
			return;
		}
		if (m_ended) {
			// the last notes fade out
			m_afterEndUs += delta.count();
			return;
		}
		m_timeUs += static_cast<double>(delta.count()) * m_pitch;
		m_sequencer.play(std::chrono::microseconds(static_cast<qint64>(m_timeUs)), this);
		if (m_sequencer.is_at_end()) {
			if (!m_looping) {
				m_ended = true;
			} else if (m_loopUs < m_totalUs) {
				m_timeUs = static_cast<double>(m_sequencer.rewind_to_loop()->time.count());
			} else {
				// a loop point at the end: silent at the end, as RPG_RT
				m_timeUs = static_cast<double>(m_totalUs);
			}
		}
		m_positionMs = std::clamp<qint64>(static_cast<qint64>(m_timeUs / 1000), 0, m_totalUs / 1000);
	}

	/** 0 to 1: the controller 7 of each channel, scaled (the fade-in comes as volumes too) */
	void setVolume(float volume) {
		volume = std::clamp(volume, 0.0f, 1.0f);
		if (volume == m_volume) {
			return;
		}
		m_volume = volume;
		sendVolumes();
	}

	/** RPG Maker's balance, 0 to 100: the controller 10 of each channel, shifted */
	void setBalance(int balance) {
		balance = std::clamp(balance, 0, 100);
		if (balance == m_balance) {
			return;
		}
		m_balance = balance;
		sendPans();
	}

	/** RPG Maker's tempo / 100: the speed of the sequencer */
	void setPitch(float pitch) { m_pitch = pitch; }
	void setLooping(bool looping) { m_looping = looping; }

	/** The channels silent, the notes held (the Player's Pause) */
	void pause() {
		m_paused = true;
		sendVolumes();
	}

	void resume() {
		m_paused = false;
		sendVolumes();
	}

	/** Back where it starts, all sound off; paused. It resets the output when it plays again */
	void rewind() {
		allSoundOff();
		m_paused = true;
		m_sequencer.rewind();
		m_unsent = false;
		jumped(m_startUs);
	}

	void seek(qint64 ms) {
		// within the file: the sequencer cannot go past its last message
		const qint64 timeUs = std::clamp<qint64>(ms * 1000, 0, m_totalUs);
		allSoundOff();
		// the programs, controllers and pitch bends on the way there, without the notes
		m_sequencer.set_time(std::chrono::microseconds(timeUs), this);
		if (!m_output) {
			// sent once it has an output
			m_unsent = true;
		}
		jumped(timeUs);
	}

	/** Every note off at once (controllers 120 and 123) */
	void allSoundOff() {
		for (uint32_t channel = 0; channel < 16; ++channel) {
			send(controlChange(channel, 120, 0));
			send(controlChange(channel, 123, 0));
		}
	}

	qint64 position() const { return m_positionMs; }
	qint64 duration() const { return m_totalUs / 1000; }
	qint64 loopStart() const { return m_loopUs / 1000; }
	/** The messages of a file not looping are over; its sound goes on a little */
	bool ended() const { return m_ended; }
	/** Since the end, in the time update() was given */
	qint64 afterEndUs() const { return m_afterEndUs; }

private:
	void send(uint32_t message) {
		if (m_output) {
			m_output->message(message);
		}
	}

	void jumped(qint64 timeUs) {
		m_timeUs = static_cast<double>(timeUs);
		m_ended = false;
		m_afterEndUs = 0;
		m_positionMs = timeUs / 1000;
	}

	uint8_t channelVolume(int channel) const {
		return m_paused ? 0 : static_cast<uint8_t>(m_channelVolumes[channel] * m_volume);
	}

	uint8_t channelPan(int pan) const {
		return static_cast<uint8_t>(std::clamp(pan + (m_balance - 50) * 2, 0, 127));
	}

	void sendVolumes() {
		for (uint32_t channel = 0; channel < 16; ++channel) {
			send(controlChange(channel, 7, channelVolume(channel)));
		}
	}

	void sendPans() {
		for (uint32_t channel = 0; channel < 16; ++channel) {
			send(controlChange(channel, 10, channelPan(m_channelPans[channel])));
		}
	}

	// midisequencer::output
	void midi_message(int, uint_least32_t message) override {
		const uint32_t channel = message & 0x0F;
		const uint32_t value1 = (message >> 8) & 0xFF;
		const uint32_t value2 = (message >> 16) & 0xFF;
		// invalid messages can crash a synthesizer (the Player checks the first value)
		if (value1 >= 128 || value2 >= 128) {
			return;
		}
		if ((message & 0xF0) == 0xB0 && value1 == 7) {
			m_channelVolumes[channel] = static_cast<uint8_t>(value2);
			message = controlChange(channel, 7, channelVolume(channel));
		} else if ((message & 0xF0) == 0xB0 && value1 == 10) {
			m_channelPans[channel] = static_cast<uint8_t>(value2);
			message = controlChange(channel, 10, channelPan(value2));
		}
		send(message);
	}

	void sysex_message(int, const void* data, std::size_t size) override {
		if (m_output) {
			m_output->sysex(static_cast<const uint8_t*>(data), size);
		}
	}

	// the tempo changes are in the times of the messages already; the notes
	// skipped by a jump were all turned off before it
	void meta_event(int, const void*, std::size_t) override {}

	/** The output as new, before the file plays from its beginning (the Player's reset) */
	void reset() override {
		for (uint32_t channel = 0; channel < 16; ++channel) {
			// reset all controllers
			send(controlChange(channel, 121, 0));
		}
		// GM system on (resets most parameters)
		static const uint8_t gmReset[] = {0xF0, 0x7E, 0x7F, 0x09, 0x01, 0xF7};
		if (m_output) {
			m_output->sysex(gmReset, sizeof(gmReset));
		}
		// a pitch bend range of 2 semitones (registered parameter 0)
		for (uint32_t channel = 0; channel < 16; ++channel) {
			send(controlChange(channel, 101, 0));
			send(controlChange(channel, 100, 0));
			send(controlChange(channel, 6, 2));
			send(controlChange(channel, 38, 0));
		}
		// the volumes and pans of General MIDI after the reset, with the
		// settings: a channel whose file sets none has them too
		m_channelVolumes.fill(100);
		m_channelPans.fill(64);
		sendVolumes();
		sendPans();
	}

	midisequencer::sequencer m_sequencer;
	MidiOutput* m_output = nullptr;
	std::mutex m_mutex;

	qint64 m_totalUs = 0;
	qint64 m_loopUs = 0;
	qint64 m_startUs = 0;
	// where the sequencer is, in the time of the file (at the normal tempo)
	double m_timeUs = 0;
	float m_pitch = 1;
	float m_volume = 1;
	int m_balance = 50;
	bool m_looping = false;
	bool m_paused = true;
	// moved without an output: what comes before is sent once it has one
	bool m_unsent = false;
	// as the file sets them
	std::array<uint8_t, 16> m_channelVolumes;
	std::array<uint8_t, 16> m_channelPans;

	std::atomic<qint64> m_positionMs = 0;
	std::atomic<bool> m_ended = false;
	std::atomic<qint64> m_afterEndUs = 0;
};

/**
 * Moves the sequence on with the clock and sends its messages to a MIDI
 * device, from a thread of its own: the messages are timed better than the
 * interface could (the Player's GenericAudioMidiOut)
 */
class DeviceDriver {
public:
	explicit DeviceDriver(MidiSequence& sequence) : m_sequence(sequence) {
		m_thread = std::thread([this] { run(); });
	}

	~DeviceDriver() {
		m_stop = true;
		m_thread.join();
	}

private:
	void run() {
		using clock = std::chrono::steady_clock;
		auto last = clock::now();
		while (!m_stop) {
			const auto now = clock::now();
			{
				std::lock_guard lock(m_sequence.mutex());
				m_sequence.update(std::chrono::duration_cast<std::chrono::microseconds>(now - last));
			}
			last = now;
			std::this_thread::sleep_for(1ms);
		}
	}

	MidiSequence& m_sequence;
	std::atomic<bool> m_stop = false;
	std::thread m_thread;
};

/**
 * The audio of a software synthesizer, made as the output reads it: the
 * sequence moves on with the frames made, in steps of kStep. Silent while
 * it has no synthesizer yet (a soundfont loads): the music waits. The
 * output may read from another thread than the interface's; all with the
 * mutex of the sequence held.
 */
class PcmDriver final : public QIODevice {
public:
	PcmDriver(MidiSequence& sequence, const QAudioFormat& format) : m_sequence(sequence), m_format(format) {}

	MidiSynth* synth() const { return m_synth.get(); }
	void setSynth(std::shared_ptr<MidiSynth> synth) { m_synth = std::move(synth); }

	/** No frame made yet: the output starts */
	void restart() {
		m_made = 0;
		m_marks.clear();
		m_endFrame = -1;
	}

	/** The sequence jumped: the next frames made are there */
	void jumped() {
		m_marks.clear();
		m_marks.push_back({m_made, m_sequence.position()});
		m_endFrame = -1;
	}

	/** Where the file is in the frame played (counted from the start of the output) */
	qint64 positionAt(qint64 played) const {
		if (m_marks.empty()) {
			return m_sequence.position();
		}
		// the last frames made before it; the first when it is older (a jump not heard yet)
		auto after = std::upper_bound(m_marks.begin(), m_marks.end(), played, [](qint64 frame, const Mark& mark) {
			return frame < mark.frame;
		});
		return after == m_marks.begin() ? after->positionMs : std::prev(after)->positionMs;
	}

	/** The end of the file was played, and its last notes faded out */
	bool finishedAt(qint64 played) const {
		return m_endFrame >= 0 && played >= m_endFrame + kTailUs * m_format.sampleRate() / 1'000'000;
	}

	bool isSequential() const override { return true; }
	// endless, for the outputs that read only what is available (WASAPI)
	qint64 bytesAvailable() const override { return QIODevice::bytesAvailable() + (1 << 20); }

protected:
	qint64 readData(char* data, qint64 maxSize) override {
		const int frameBytes = m_format.bytesPerFrame();
		const int frames = frameBytes > 0 ? static_cast<int>(std::min<qint64>(maxSize / frameBytes, 1 << 20)) : 0;
		if (frames <= 0) {
			return 0;
		}
		std::lock_guard lock(m_sequence.mutex());
		m_buffer.assign(static_cast<std::size_t>(frames) * 2, 0.0f);
		for (int done = 0; done < frames; done += kStep) {
			const int step = std::min(kStep, frames - done);
			if (m_synth) {
				// the time of the frames, without losing the fractions of microseconds
				m_carryUs += step * 1'000'000.0 / m_format.sampleRate();
				const auto whole = static_cast<qint64>(m_carryUs);
				m_carryUs -= static_cast<double>(whole);
				m_sequence.update(std::chrono::microseconds(whole));
				m_synth->render(m_buffer.data() + done * 2, step);
			}
			if (m_endFrame < 0 && m_sequence.ended()) {
				m_endFrame = m_made + step;
			}
			m_marks.push_back({m_made, m_sequence.position()});
			m_made += step;
		}
		// what the output may still play: two seconds (its buffer is a fifth of one)
		const qint64 kept = m_made - 2 * m_format.sampleRate();
		while (m_marks.size() > 1 && m_marks[1].frame < kept) {
			m_marks.pop_front();
		}
		auto* floats = reinterpret_cast<float*>(data);
		auto* shorts = reinterpret_cast<int16_t*>(data);
		const bool isFloat = m_format.sampleFormat() == QAudioFormat::Float;
		for (std::size_t i = 0; i < m_buffer.size(); ++i) {
			const float sample = std::clamp(m_buffer[i], -1.0f, 1.0f);
			if (isFloat) {
				floats[i] = sample;
			} else {
				shorts[i] = static_cast<int16_t>(sample * 32767);
			}
		}
		return static_cast<qint64>(frames) * frameBytes;
	}

	qint64 writeData(const char*, qint64) override { return -1; }

private:
	/** The frames from this one on are at this position of the file */
	struct Mark {
		qint64 frame;
		qint64 positionMs;
	};

	MidiSequence& m_sequence;
	std::shared_ptr<MidiSynth> m_synth;
	const QAudioFormat m_format;
	std::vector<float> m_buffer;
	double m_carryUs = 0;
	// frames made since the output started, and where the file was in them
	qint64 m_made = 0;
	std::deque<Mark> m_marks;
	// where the file ended, -1 before
	qint64 m_endFrame = -1;
};

/** Stereo at the rate of the output, in floats or 16-bit integers: what it takes */
QAudioFormat outputFormat(const QAudioDevice& device) {
	QList<int> rates;
	if (const int preferred = device.preferredFormat().sampleRate(); preferred >= 22050 && preferred <= 96000) {
		rates << preferred;
	}
	rates << 48000 << 44100;
	QAudioFormat format;
	format.setChannelCount(2);
	for (int rate : rates) {
		format.setSampleRate(rate);
		for (const auto sampleFormat : {QAudioFormat::Float, QAudioFormat::Int16}) {
			format.setSampleFormat(sampleFormat);
			if (device.isFormatSupported(format)) {
				return format;
			}
		}
	}
	// the output tells when it cannot (error())
	format.setSampleRate(44100);
	format.setSampleFormat(QAudioFormat::Int16);
	return format;
}

/**
 * The MIDI device of the system, as the Player's native MIDI: Windows' (on
 * macOS, AVAudioEngine plays MIDI; Linux has FluidSynth or FmMidi). Not tried
 * again once it failed, as in the Player.
 */
std::unique_ptr<MidiOutput> openSystemMidiOutput() {
#ifdef _WIN32
	static bool works = true;
	if (!works) {
		return nullptr;
	}
	std::string status;
	auto device = std::make_unique<Win32MidiOutDevice>(status);
	if (device->IsInitialized()) {
		// which device: once in the log, as the others are told
		static std::once_flag logged;
		std::call_once(logged, [&status] {
			qCInfo(lcAudio).noquote() << "MIDI: Windows MIDI," << QString::fromLocal8Bit(status.c_str());
		});
		return device;
	}
	works = false;
	qCWarning(lcAudio).noquote() << "MIDI: Windows MIDI cannot be used, FmMidi plays instead:" << QString::fromLocal8Bit(status.c_str());
	return nullptr;
#else
	return nullptr;
#endif
}

/** A backend has the MIDI device of the system: one at a time (the interface's thread only) */
bool systemMidiOutputInUse = false;

/** A soundfont loaded by FluidSynth, away from the interface: the first of the plan that loads */
struct LoadedSoundfont {
	std::shared_ptr<MidiSynth> synth;
	QString soundfont;
	QList<QPair<QString, QString>> failures;
};

class MidiAudioBackend final : public AudioBackend {
public:
	explicit MidiAudioBackend(const MidiPlan& plan) : m_plan(plan) {}

	~MidiAudioBackend() override {
		stopOutput();
	}

	QString name() const override {
		const MidiOutput* output = m_device ? m_device.get() : (m_pcm ? m_pcm->synth() : nullptr);
		return output ? QString::fromLatin1(output->name()) : QStringLiteral("MIDI");
	}

	bool open(const QString& path, bool, QString& error) override {
		QFile file(path);
		if (!file.open(QIODevice::ReadOnly)) {
			error = file.errorString();
			return false;
		}
		const QByteArray bytes = file.readAll();
		m_fileName = QFileInfo(path).fileName();
		std::lock_guard lock(m_sequence.mutex());
		if (!m_sequence.load(bytes, error)) {
			return false;
		}
		qCDebug(lcAudio).noquote() << m_fileName << "length" << m_sequence.duration() << "ms, loop at"
			<< m_sequence.loopStart() << "ms";
		return true;
	}

	void play() override {
		if (m_state == State::Playing) {
			return;
		}
		if (m_state == State::Stopped && !startOutput()) {
			return;
		}
		{
			std::lock_guard lock(m_sequence.mutex());
			m_sequence.resume();
		}
		if (m_sink && m_sink->state() == QtAudio::SuspendedState) {
			m_sink->resume();
		}
		m_state = State::Playing;
	}

	void pause() override {
		if (m_state != State::Playing) {
			return;
		}
		{
			std::lock_guard lock(m_sequence.mutex());
			m_sequence.pause();
		}
		if (m_sink && m_sink->state() != QtAudio::StoppedState) {
			m_sink->suspend();
		}
		m_state = State::Paused;
	}

	void stop() override {
		stopOutput();
		std::lock_guard lock(m_sequence.mutex());
		m_sequence.rewind();
		m_state = State::Stopped;
	}

	void seek(qint64 ms) override {
		std::lock_guard lock(m_sequence.mutex());
		m_sequence.seek(ms);
		if (m_pcm) {
			m_pcm->jumped();
		}
	}

	void setVolume(float volume, float fade) override {
		// the channels scaled, as the Player does with the volume and its fade (SetFade)
		std::lock_guard lock(m_sequence.mutex());
		m_sequence.setVolume(volume * fade);
	}

	void setPan(float pan) override {
		std::lock_guard lock(m_sequence.mutex());
		m_sequence.setBalance(qRound(50 + pan * 50));
	}

	void setRate(float rate) override {
		std::lock_guard lock(m_sequence.mutex());
		m_sequence.setPitch(rate);
	}

	void setLooping(bool looping) override {
		std::lock_guard lock(m_sequence.mutex());
		m_sequence.setLooping(looping);
	}

	qint64 position() const override {
		if (!playsPcm()) {
			return m_sequence.position();
		}
		std::lock_guard lock(m_sequence.mutex());
		return m_pcm->positionAt(playedFrames());
	}

	qint64 duration() const override { return m_sequence.duration(); }
	qint64 loopStart() const override { return m_sequence.loopStart(); }

	bool finished() const override {
		if (!playsPcm()) {
			// the device plays the messages as they are sent: the tail after them
			return m_sequence.ended() && m_sequence.afterEndUs() >= kTailUs;
		}
		std::lock_guard lock(m_sequence.mutex());
		return m_pcm->finishedAt(playedFrames());
	}

	QString error() const override {
		if (!m_error.isEmpty() || !m_sink || m_device) {
			return m_error;
		}
		switch (m_sink->error()) {
			case QtAudio::OpenError:
				return AudioPlayer::tr("The audio output could not be opened.");
			case QtAudio::IOError:
			case QtAudio::FatalError:
				return AudioPlayer::tr("The audio output stopped working.");
			default:
				return QString();
		}
	}

	Synth synth() const override {
		if (m_device) {
			return Synth::System;
		}
		if (!m_pcm || !m_pcm->synth()) {
			return Synth::None;
		}
		return m_soundfont.isEmpty() ? Synth::FmMidi : Synth::Soundfont;
	}

	QString soundfont() const override { return m_soundfont; }

	QList<QPair<QString, QString>> takeFailures() override { return std::exchange(m_failures, {}); }

private:
	enum class State {
		Stopped,
		Playing,
		Paused
	};

	bool playsPcm() const {
		return m_pcm && m_sink && !m_device && m_state != State::Stopped;
	}

	/** Frames played by the output since it started */
	qint64 playedFrames() const {
		return m_sink->processedUSecs() * m_format.sampleRate() / 1'000'000;
	}

	/**
	 * Where it plays from the start: the soundfonts of the plan with
	 * FluidSynth (the music waits while it loads); else the device of the
	 * system when allowed and no other file plays on it (as the Player plays
	 * only its music on it, and the rest with its synthesizer); else FmMidi
	 */
	bool startOutput() {
		if (!m_plan.soundfonts.isEmpty() && !m_triedSoundfonts && MidiSynth::canPlaySoundfonts()) {
			if (!startPcm()) {
				return false;
			}
			if (!m_pcm->synth()) {
				loadSoundfont();
			}
			return true;
		}
		if (m_plan.systemSynth && startDevice()) {
			return true;
		}
		if (!startPcm()) {
			return false;
		}
		if (!m_pcm->synth()) {
			std::lock_guard lock(m_sequence.mutex());
			m_pcm->setSynth(MidiSynth::createFmMidi(m_format.sampleRate()));
			m_sequence.setOutput(m_pcm->synth());
		}
		return true;
	}

	bool startDevice() {
		if (systemMidiOutputInUse) {
			qCDebug(lcAudio).noquote() << "MIDI: the MIDI device plays another file, FmMidi plays" << m_fileName;
			return false;
		}
		m_device = openSystemMidiOutput();
		if (!m_device) {
			return false;
		}
		systemMidiOutputInUse = true;
		{
			std::lock_guard lock(m_sequence.mutex());
			m_sequence.setOutput(m_device.get());
		}
		m_deviceDriver = std::make_unique<DeviceDriver>(m_sequence);
		return true;
	}

	/** The output of the software synthesizers started, with the synthesizer it has (none while one loads) */
	bool startPcm() {
		if (!m_pcm) {
			const QAudioDevice device = QMediaDevices::defaultAudioOutput();
			if (device.isNull()) {
				m_error = AudioPlayer::tr("No audio output was found.");
				return false;
			}
			m_format = outputFormat(device);
			m_pcm = std::make_unique<PcmDriver>(m_sequence, m_format);
			m_pcm->open(QIODevice::ReadOnly | QIODevice::Unbuffered);
			m_sink = std::make_unique<QAudioSink>(device, m_format);
			// a fifth of a second ahead: enough while the interface is busy, and
			// the volume and the jumps are heard soon
			m_sink->setBufferSize(m_format.bytesForDuration(200'000));
			qCDebug(lcAudio).noquote() << "MIDI: to" << device.description() << m_format.sampleRate() << "Hz"
				<< (m_format.sampleFormat() == QAudioFormat::Float ? "float" : "16-bit");
		}
		{
			std::lock_guard lock(m_sequence.mutex());
			m_pcm->restart();
			m_sequence.setOutput(m_pcm->synth());
		}
		m_sink->start(m_pcm.get());
		return true;
	}

	/** The first soundfont of the plan that FluidSynth loads, away from the interface */
	void loadSoundfont() {
		if (m_loading) {
			return;
		}
		m_loading = true;
		const QStringList soundfonts = m_plan.soundfonts;
		const int rate = m_format.sampleRate();
		QtConcurrent::run([soundfonts, rate] {
			LoadedSoundfont loaded;
			for (const QString& path : soundfonts) {
				std::string why;
				if (std::unique_ptr<MidiSynth> synth = MidiSynth::createFluidSynth(rate, path.toStdString(), why)) {
					loaded.synth = std::move(synth);
					loaded.soundfont = path;
					break;
				}
				qCDebug(lcAudio).noquote() << "MIDI: FluidSynth:" << QString::fromStdString(why);
				loaded.failures.append({path, AudioPlayer::tr("FluidSynth could not load it")});
			}
			return loaded;
		}).then(&m_context, [this](LoadedSoundfont loaded) {
			soundfontLoaded(std::move(loaded));
		});
	}

	void soundfontLoaded(LoadedSoundfont loaded) {
		m_loading = false;
		m_triedSoundfonts = true;
		m_failures += loaded.failures;
		for (const auto& [path, why] : loaded.failures) {
			qCWarning(lcAudio).noquote() << "MIDI: the soundfont" << path << "cannot be played:" << why;
		}
		if (!loaded.synth) {
			// none loads: the device of the system, else FmMidi
			if (m_state != State::Stopped && m_plan.systemSynth && startDevice()) {
				m_sink->stop();
				return;
			}
			loaded.synth = MidiSynth::createFmMidi(m_format.sampleRate());
		} else {
			qCDebug(lcAudio).noquote() << "MIDI: FluidSynth with" << loaded.soundfont << "plays" << m_fileName;
		}
		m_soundfont = loaded.soundfont;
		std::lock_guard lock(m_sequence.mutex());
		m_pcm->setSynth(std::move(loaded.synth));
		if (m_state != State::Stopped) {
			m_sequence.setOutput(m_pcm->synth());
		}
	}

	/** The output given back: the device closed (as the Player's Reset), the audio stopped */
	void stopOutput() {
		// no more moving on from the thread of the device
		m_deviceDriver.reset();
		{
			std::lock_guard lock(m_sequence.mutex());
			m_sequence.allSoundOff();
			m_sequence.setOutput(nullptr);
		}
		if (m_device) {
			m_device.reset();
			systemMidiOutputInUse = false;
		}
		if (m_sink) {
			m_sink->stop();
		}
	}

	MidiPlan m_plan;
	QString m_fileName;
	QString m_error;
	State m_state = State::Stopped;
	// read by the outputs: destroyed after them
	mutable MidiSequence m_sequence;
	std::unique_ptr<MidiOutput> m_device;
	std::unique_ptr<DeviceDriver> m_deviceDriver;
	QAudioFormat m_format;
	std::unique_ptr<PcmDriver> m_pcm;
	std::unique_ptr<QAudioSink> m_sink;
	// the soundfont: loading, loaded (or none of the plan could be), which
	bool m_loading = false;
	bool m_triedSoundfonts = false;
	QString m_soundfont;
	QList<QPair<QString, QString>> m_failures;
	// the loading of the soundfont tells only a backend still there
	QObject m_context;
};

} // namespace

std::unique_ptr<AudioBackend> createMidiAudioBackend(const MidiPlan& plan) {
	return std::make_unique<MidiAudioBackend>(plan);
}

qint64 midiFileDuration(const QString& path) {
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly)) {
		return 0;
	}
	MidiSequence sequence;
	QString error;
	std::lock_guard lock(sequence.mutex());
	return sequence.load(file.readAll(), error) ? sequence.duration() : 0;
}
