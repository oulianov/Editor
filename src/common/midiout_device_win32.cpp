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

// EasyRPG Editor: EasyRPG Player's src/platform/windows/midiout_device_win32.cpp,
// with the MIDI mapper first, a timer of 1 ms while the device is open, and
// the SysEx header given back by a reset when the device keeps it too long

#ifdef _WIN32
#include "midiout_device_win32.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>

static std::string get_error_str(MMRESULT res) {
	char errMsg[MAXERRORLENGTH] = {};
	midiOutGetErrorTextA(res, errMsg, MAXERRORLENGTH);
	return std::string(errMsg);
}

static HMIDIOUT handle(void* midi_out) {
	return static_cast<HMIDIOUT>(midi_out);
}

Win32MidiOutDevice::Win32MidiOutDevice(std::string& status_message) {
	// The Player opens the device 0. The MIDI mapper of Windows 8 and later
	// cannot be set in the Control Panel any more, but it is the device chosen
	// with third party tools, and the Microsoft GS Wavetable Synth otherwise,
	// where the device 0 may be a hardware port
	HMIDIOUT out = nullptr;
	UINT device_id = MIDI_MAPPER;
	MMRESULT err = midiOutOpen(&out, device_id, 0, 0, CALLBACK_NULL);
	if (err != MMSYSERR_NOERROR) {
		status_message = "Win32 midiOutOpen MIDI_MAPPER failed: (" + std::to_string(err) + ") " + get_error_str(err) + ". ";
		device_id = 0;
		err = midiOutOpen(&out, device_id, 0, 0, CALLBACK_NULL);
	}
	if (err != MMSYSERR_NOERROR) {
		status_message += "Win32 midiOutOpen 0 failed: (" + std::to_string(err) + ") " + get_error_str(err);
		midi_out = nullptr;
		return;
	}
	midi_out = out;

	MIDIOUTCAPSA caps = {};
	if (midiOutGetDevCapsA(device_id, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
		status_message = caps.szPname;
	} else {
		status_message = device_id == MIDI_MAPPER ? "MIDI mapper" : "device 0";
	}

	// The thread sending the messages wakes every millisecond, as the Player's
	// (under SDL, which sets the same): the default timer of Windows is 15.6 ms
	timeBeginPeriod(1);
}

Win32MidiOutDevice::~Win32MidiOutDevice() {
	if (midi_out) {
		midiOutReset(handle(midi_out));
		midiOutClose(handle(midi_out));
		midi_out = nullptr;
		timeEndPeriod(1);
	}
}

void Win32MidiOutDevice::message(uint32_t message) {
	midiOutShortMsg(handle(midi_out), message);
}

void Win32MidiOutDevice::sysex(const uint8_t* data, std::size_t size) {
	// (the Player logs the failures; the Editor's log is Qt's, which this file does without)
	MIDIHDR hdr = {};
	MMRESULT res;
	hdr.dwBufferLength = static_cast<DWORD>(size);
	hdr.dwBytesRecorded = static_cast<DWORD>(size);
	hdr.dwFlags = 0;
	hdr.lpData = reinterpret_cast<LPSTR>(const_cast<uint8_t*>(data));
	res = midiOutPrepareHeader(handle(midi_out), &hdr, sizeof(hdr));
	if (res != MMSYSERR_NOERROR) {
		return;
	}
	midiOutLongMsg(handle(midi_out), &hdr, sizeof(hdr));
	// the header is on the stack, the data the caller's: kept until the device is done with them
	for (int tries = 0; midiOutUnprepareHeader(handle(midi_out), &hdr, sizeof(hdr)) == MIDIERR_STILLPLAYING; ++tries) {
		if (tries == 100) {
			// still not done after 100 ms: the reset gives the buffers back (marked done), then they can go
			midiOutReset(handle(midi_out));
			midiOutUnprepareHeader(handle(midi_out), &hdr, sizeof(hdr));
			break;
		}
		Sleep(1);
	}
}

const char* Win32MidiOutDevice::name() const {
	return "Windows MIDI";
}

bool Win32MidiOutDevice::IsInitialized() const {
	return midi_out != nullptr;
}

#endif
