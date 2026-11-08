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

// Without Qt: the MIDI device of Windows (midiout_device_win32.h) uses it

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

/**
 * Where the MIDI backend (audio_player_midi.cpp) sends the messages of a MIDI
 * file, as EasyRPG Player's MidiDecoder: a software synthesizer, or the MIDI
 * device of the system (Windows)
 */
class MidiOutput {
public:
	virtual ~MidiOutput() = default;

	/** "Windows MIDI", "FluidSynth", "FmMidi" */
	virtual const char* name() const = 0;

	/** A channel message: status | data1 << 8 | data2 << 16 */
	virtual void message(uint32_t message) = 0;
	/** A system exclusive message, F0 to F7 included */
	virtual void sysex(const uint8_t* data, std::size_t size) = 0;
};

/**
 * A software synthesizer: FluidSynth with a soundfont (SF2), or FmMidi,
 * EasyRPG Player's FM synthesizer (src/common/midi), which needs nothing
 */
class MidiSynth : public MidiOutput {
public:
	/** Writes frames of stereo (interleaved, -1 to 1) into the buffer */
	virtual void render(float* buffer, int frames) = 0;

	/** FmMidi at this sample rate */
	static std::unique_ptr<MidiSynth> createFmMidi(int sampleRate);
	/** FluidSynth is there: built in (Windows), else the system's libfluidsynth (the first time, the log says) */
	static bool canPlaySoundfonts();
	/**
	 * FluidSynth at this sample rate with the soundfont (UTF-8 path), as
	 * the Player sets it (no reverb nor chorus); null when it cannot load
	 * it (error says why). Loading takes time: away from the interface's
	 * thread. The last synthesizers used are kept for the next files.
	 */
	static std::unique_ptr<MidiSynth> createFluidSynth(int sampleRate, const std::string& soundfont, std::string& error);
	/** The soundfont the system's FluidSynth was built with (synth.default-soundfont, UTF-8), empty when none */
	static std::string fluidSynthDefaultSoundfont();
};
