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
 * A software synthesizer: FluidSynth with a General MIDI soundfont when the
 * system has both (Linux), else FmMidi, EasyRPG Player's FM synthesizer
 * (src/common/midi), which needs nothing
 */
class MidiSynth : public MidiOutput {
public:
	/** Writes frames of stereo (interleaved, -1 to 1) into the buffer */
	virtual void render(float* buffer, int frames) = 0;

	/**
	 * A synthesizer at this sample rate: FluidSynth when allowed and found
	 * with a soundfont (the first time, the log says which and why), else FmMidi
	 */
	static std::unique_ptr<MidiSynth> create(int sampleRate, bool allowFluidSynth);
};
