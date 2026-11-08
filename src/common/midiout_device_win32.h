/*
 * This file is part of EasyRPG Player.
 *
 * EasyRPG Player is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * EasyRPG Player is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with EasyRPG Player. If not, see <http://www.gnu.org/licenses/>.
 */

// EasyRPG Editor: EasyRPG Player's src/platform/windows/midiout_device_win32.h,
// a MidiOutput of the MIDI backend (audio_player_midi.cpp). Without Qt, and
// without windows.h here: the Qt files that include it do not get its macros

#ifndef EP_MIDIOUT_WIN32_H
#define EP_MIDIOUT_WIN32_H

#ifdef _WIN32

// Headers
#include <string>
#include "midi_synth.h"

/**
 * Plays MIDI through the Windows API: the MIDI mapper (the Microsoft GS
 * Wavetable Synth, unless another device was chosen), else the first device
 */
class Win32MidiOutDevice : public MidiOutput {
public:
	/** Opens the device; status_message: its name, or why it cannot be opened */
	Win32MidiOutDevice(std::string& status_message);
	~Win32MidiOutDevice();

	void message(uint32_t message) override;
	void sysex(const uint8_t* data, std::size_t size) override;
	const char* name() const override;
	bool IsInitialized() const;

private:
	// HMIDIOUT
	void* midi_out = nullptr;
};
#endif

#endif
