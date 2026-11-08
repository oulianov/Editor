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

#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>
#include <memory>

/**
 * What plays MIDI, as Soundfonts::plan() chose it (soundfont.h): the
 * soundfonts in their order (the first one that loads plays), then the
 * synthesizer of the system (Apple's DLS bank, Windows' GS Wavetable
 * Synth) when allowed, then FmMidi
 */
struct MidiPlan {
	QStringList soundfonts;
	bool systemSynth = true;
};

/**
 * One way of playing one audio file, used by AudioPlayer (audio_player.h),
 * which adds the fade-in, the loop and the signals. Positions are in ms.
 */
class AudioBackend {
public:
	virtual ~AudioBackend() = default;

	/** What plays, for the log */
	virtual QString name() const = 0;

	/** Loads the file; false with a message when this backend cannot play it */
	virtual bool open(const QString& path, bool midi, QString& error) = 0;
	/** Whether open() can load another file after this one (quicker than a new backend) */
	virtual bool canReopen() const { return false; }
	/** What plays the next MIDI file opened, for a backend that plays several ways */
	virtual void setMidiPlan(const MidiPlan&) {}

	virtual void play() = 0;
	virtual void pause() = 0;
	/** Stops and goes back to the start */
	virtual void stop() = 0;
	virtual void seek(qint64 ms) = 0;

	/**
	 * RPG Maker's volume (0 to 1, the slider's) and how far the fade-in is
	 * (0 to 1): sampled files scale their samples with the two, MIDI its
	 * channels, as EasyRPG Player does
	 */
	virtual void setVolume(float volume, float fade) = 0;
	/** -1 left, 0 center, 1 right */
	virtual void setPan(float pan) = 0;
	/** 1 is normal speed (RPG Maker's tempo / 100) */
	virtual void setRate(float rate) = 0;
	virtual bool canPan() const { return true; }
	/** Loops by itself, without a gap (else AudioPlayer seeks to loopStart() once finished()) */
	virtual void setLooping(bool) {}

	virtual qint64 position() const = 0;
	/** 0 while unknown */
	virtual qint64 duration() const = 0;
	/** MIDI controller 111 loop point, or the beginning when none is set */
	virtual qint64 loopStart() const { return 0; }
	/** The end was reached while playing */
	virtual bool finished() const = 0;
	/** An error found after open() (decoding is asynchronous in some backends) */
	virtual QString error() const { return QString(); }

	/** What plays a MIDI file */
	enum class Synth {
		/** Not MIDI, or not known yet (a soundfont loads) */
		None,
		/** The soundfont of soundfont() */
		Soundfont,
		/** The system's (Apple DLS, Microsoft GS Wavetable Synth) */
		System,
		/** EasyRPG Player's FM synthesizer */
		FmMidi
	};
	virtual Synth synth() const { return Synth::None; }
	/** The soundfont of the plan that plays (Synth::Soundfont) */
	virtual QString soundfont() const { return QString(); }
	/** The soundfonts of the plan that could not be loaded, with why: given once */
	virtual QList<QPair<QString, QString>> takeFailures() { return {}; }

	/** The linear gain of RPG Maker's volume (0 to 1): the slider is perceptual */
	static float linearGain(float volume);
};

#ifdef Q_OS_MACOS
/**
 * AVAudioEngine: WAV, MP3, AIFF, AAC, FLAC... and MIDI with Apple's DLS
 * synthesizer, which plays the first soundfont of the plan (setMidiPlan)
 * that loads, else the system's GS sound bank (audio_player_mac.mm). A
 * MIDI file whose plan has neither cannot be opened: FmMidi plays it
 */
std::unique_ptr<AudioBackend> createAppleAudioBackend();
#endif
/**
 * MIDI as EasyRPG Player plays it, for the systems whose Qt Multimedia has
 * none (audio_player_midi.cpp): the soundfonts of the plan with FluidSynth
 * (built in on Windows, the system's elsewhere), loaded away from the
 * interface, else the MIDI device of Windows (its GS Wavetable Synth) when
 * the plan allows it, else FmMidi, through QAudioSink
 */
std::unique_ptr<AudioBackend> createMidiAudioBackend(const MidiPlan& plan);

/** The length of a MIDI file as EasyRPG Player plays it (its tempo map), 0 when it cannot be read */
qint64 midiFileDuration(const QString& path);
