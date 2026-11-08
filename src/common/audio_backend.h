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

#include <QString>
#include <memory>

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

	virtual void play() = 0;
	virtual void pause() = 0;
	/** Stops and goes back to the start */
	virtual void stop() = 0;
	virtual void seek(qint64 ms) = 0;

	/** Linear gain, 0 to 1 */
	virtual void setGain(float gain) = 0;
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
};

#ifdef Q_OS_MACOS
/** AVAudioEngine: WAV, MP3, AIFF, AAC, FLAC... and MIDI with the system's GS sound bank (audio_player_mac.mm) */
std::unique_ptr<AudioBackend> createAppleAudioBackend();
#endif
/**
 * MIDI as EasyRPG Player plays it, for the systems whose Qt Multimedia has
 * none (audio_player_midi.cpp): on the MIDI device of Windows (its GS
 * Wavetable Synth), else with a software synthesizer through QAudioSink,
 * FluidSynth when found with a soundfont (Linux), else FmMidi; fmMidiOnly:
 * FmMidi always
 */
std::unique_ptr<AudioBackend> createMidiAudioBackend(bool fmMidiOnly);
