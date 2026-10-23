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

// AVAudioEngine playback for macOS (compiled with -fobjc-arc).
// Sampled files: AVAudioFile (Core Audio's decoders) -> player node -> varispeed
// (tempo changes the pitch too, like RPG Maker) -> a mixer for the balance.
// MIDI: AVAudioSequencer -> Apple's DLS synth, which uses the system's GS sound
// bank (gs_instruments.dls); the sequencer's rate changes the tempo only.
// The engine stays from file to file, open() replacing the nodes of the file:
// the output goes on, quicker than a new engine for each file.

#include "audio_backend.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

#import <AVFoundation/AVFoundation.h>
#import <AudioToolbox/AudioToolbox.h>

// the AVFoundation objects are members of a C++ class: ARC keeps them alive
#if !__has_feature(objc_arc)
#error "audio_player_mac.mm must be compiled with -fobjc-arc"
#endif

namespace {

QString describe(NSError* error) {
	return error ? QString::fromNSString(error.localizedDescription) : QString();
}

/** Where a MIDI file starts: seconds before its first note */
constexpr Float64 kMidiLeadIn = 0.05;

/**
 * Where a MIDI file starts, and what comes before (in beats).
 *
 * Most RTP files begin with a silent measure of setup (programs, controllers,
 * a GS reset), one or two seconds long. Like RPG_RT, which starts at the first
 * note, or at the loop point when that comes first (EasyRPG Player's
 * get_start_skipping_silence), the preview skips it: what comes before is sent
 * at once, and the file starts right before its first note. (RPG_RT starts
 * half a beat before, and plays the whole measure when a SysEx comes first.)
 */
struct MidiStart {
	MusicTimeStamp beat = 0;
	/** The last controller 111, the beginning when none */
	MusicTimeStamp loopBeat = 0;
	/** The channel messages and SysEx before the start, in their order */
	std::vector<std::vector<uint8_t>> setup;
};

MidiStart findMidiStart(NSURL* url) {
	struct Message {
		MusicTimeStamp beat;
		std::vector<uint8_t> bytes;
	};
	MidiStart start;
	std::vector<Message> messages;
	MusicTimeStamp firstNote = std::numeric_limits<MusicTimeStamp>::max();
	MusicTimeStamp firstLoop = std::numeric_limits<MusicTimeStamp>::max();

	// AVMusicTrack does not give the bytes of a SysEx: the file is read again by AudioToolbox
	MusicSequence sequence = nullptr;
	if (NewMusicSequence(&sequence) != noErr) {
		return start;
	}
	UInt32 count = 0;
	if (MusicSequenceFileLoad(sequence, (__bridge CFURLRef)url, kMusicSequenceFile_MIDIType,
			kMusicSequenceLoadSMF_PreserveTracks) != noErr
			|| MusicSequenceGetTrackCount(sequence, &count) != noErr) {
		count = 0;
	}
	for (UInt32 i = 0; i < count; ++i) {
		MusicTrack track = nullptr;
		MusicEventIterator iterator = nullptr;
		if (MusicSequenceGetIndTrack(sequence, i, &track) != noErr || NewMusicEventIterator(track, &iterator) != noErr) {
			continue;
		}
		Boolean current = false;
		while (MusicEventIteratorHasCurrentEvent(iterator, &current) == noErr && current) {
			MusicTimeStamp beat = 0;
			MusicEventType type = kMusicEventType_NULL;
			const void* data = nullptr;
			UInt32 size = 0;
			if (MusicEventIteratorGetEventInfo(iterator, &beat, &type, &data, &size) == noErr && data) {
				if (type == kMusicEventType_MIDINoteMessage) {
					firstNote = std::min(firstNote, beat);
				} else if (type == kMusicEventType_MIDIChannelMessage) {
					const auto* message = static_cast<const MIDIChannelMessage*>(data);
					const uint8_t kind = message->status & 0xF0;
					if (kind == 0xB0 && message->data1 == 111) {
						firstLoop = std::min(firstLoop, beat);
						start.loopBeat = std::max(start.loopBeat, beat);
					}
					// program change and channel pressure: one byte of data
					if (kind == 0xC0 || kind == 0xD0) {
						messages.push_back({beat, {message->status, message->data1}});
					} else {
						messages.push_back({beat, {message->status, message->data1, message->data2}});
					}
				} else if (type == kMusicEventType_MIDIRawData) {
					const auto* raw = static_cast<const MIDIRawData*>(data);
					if (raw->length > 0 && raw->data[0] == 0xF0) {
						messages.push_back({beat, std::vector<uint8_t>(raw->data, raw->data + raw->length)});
					}
				}
			}
			MusicEventIteratorNextEvent(iterator);
		}
		DisposeMusicEventIterator(iterator);
	}
	const MusicTimeStamp first = std::min(firstNote, firstLoop);
	Float64 seconds = 0;
	if (first != std::numeric_limits<MusicTimeStamp>::max()
			&& MusicSequenceGetSecondsForBeats(sequence, first, &seconds) == noErr && seconds > kMidiLeadIn) {
		MusicSequenceGetBeatsForSeconds(sequence, seconds - kMidiLeadIn, &start.beat);
	}
	DisposeMusicSequence(sequence);

	if (start.beat > 0) {
		// the tracks one after another: in the order of time, then of the tracks
		std::stable_sort(messages.begin(), messages.end(), [](const Message& a, const Message& b) {
			return a.beat < b.beat;
		});
		for (Message& message : messages) {
			if (message.beat >= start.beat) {
				break;
			}
			start.setup.push_back(std::move(message.bytes));
		}
	}
	return start;
}

class AppleAudioBackend final : public AudioBackend {
public:
	~AppleAudioBackend() override {
		unload();
		if (m_engine) {
			[m_engine stop];
		}
		m_sequencer = nil;
		m_mixer = nil;
		m_engine = nil;
	}

	bool canReopen() const override { return true; }

	bool open(const QString& path, bool midi, QString& error) override {
		unload();
		m_midi = midi;
		@autoreleasepool {
			NSURL* url = [NSURL fileURLWithPath:path.toNSString()];
			NSError* err = nil;

			if (!m_engine) {
				m_engine = [[AVAudioEngine alloc] init];
				m_mixer = [[AVAudioMixerNode alloc] init];
				[m_engine attachNode:m_mixer];
				[m_engine connect:m_mixer to:m_engine.mainMixerNode format:nil];
			}

			if (midi) {
				// a new synth for each file: no note, program or controller of the last one stays
				AudioComponentDescription desc = {};
				desc.componentType = kAudioUnitType_MusicDevice;
				desc.componentSubType = kAudioUnitSubType_DLSSynth;
				desc.componentManufacturer = kAudioUnitManufacturer_Apple;
				m_synth = [[AVAudioUnitMIDIInstrument alloc] initWithAudioComponentDescription:desc];
				[m_engine attachNode:m_synth];
				[m_engine connect:m_synth to:m_mixer format:nil];

				// one sequencer loading each file: releasing a sequencer after another one
				// is made on the same engine stops the new one
				if (!m_sequencer) {
					m_sequencer = [[AVAudioSequencer alloc] initWithAudioEngine:m_engine];
				}
				if (![m_sequencer loadFromURL:url options:AVMusicSequenceLoadSMF_PreserveTracks error:&err]) {
					error = describe(err);
					unload();
					return false;
				}
				NSTimeInterval length = 0;
				for (AVMusicTrack* track in m_sequencer.tracks) {
					track.destinationAudioUnit = m_synth;
					length = std::max(length, track.lengthInSeconds);
				}
				MidiStart start = findMidiStart(url);
				m_startBeat = start.beat;
				m_setup = std::move(start.setup);
				m_durationMs = static_cast<qint64>(length * 1000);
				m_loopStartMs = static_cast<qint64>([m_sequencer secondsForBeats:start.loopBeat] * 1000);
				m_startMs = static_cast<qint64>([m_sequencer secondsForBeats:m_startBeat] * 1000);
			} else {
				m_file = [[AVAudioFile alloc] initForReading:url error:&err];
				if (!m_file) {
					error = describe(err);
					unload();
					return false;
				}
				AVAudioFormat* format = m_file.processingFormat;
				m_sampleRate = format.sampleRate > 0 ? format.sampleRate : 44100;
				m_durationMs = static_cast<qint64>(m_file.length * 1000 / m_sampleRate);

				m_node = [[AVAudioPlayerNode alloc] init];
				m_varispeed = [[AVAudioUnitVarispeed alloc] init];
				[m_engine attachNode:m_node];
				[m_engine attachNode:m_varispeed];
				[m_engine connect:m_node to:m_varispeed format:format];
				[m_engine connect:m_varispeed to:m_mixer format:format];
			}

			if (!m_engine.isRunning) {
				[m_engine prepare];
				if (![m_engine startAndReturnError:&err]) {
					error = describe(err);
					unload();
					return false;
				}
			}
			if (m_midi) {
				m_sequencer.currentPositionInBeats = m_startBeat;
				[m_sequencer prepareToPlay];
			}
			m_heldMs = m_startMs;
			m_fromStart = true;
		}
		return true;
	}

	void play() override {
		if (!loaded()) {
			return;
		}
		@autoreleasepool {
			// a new output device stops the engine
			if (!m_engine.isRunning) {
				[m_engine startAndReturnError:nil];
			}
			if (m_midi) {
				if (m_fromStart) {
					sendSetup();
					m_sequencer.currentPositionInBeats = m_startBeat;
				}
				[m_sequencer startAndReturnError:nil];
			} else {
				if (!m_scheduled) {
					schedule();
				}
				[m_node play];
			}
		}
		m_fromStart = false;
		m_playing = true;
	}

	void pause() override {
		if (!loaded() || !m_playing) {
			return;
		}
		m_heldMs = position();
		if (m_midi) {
			[m_sequencer stop];
			notesOff();
		} else {
			[m_node pause];
		}
		m_playing = false;
	}

	void stop() override {
		if (!loaded()) {
			return;
		}
		if (m_midi) {
			[m_sequencer stop];
			m_sequencer.currentPositionInBeats = m_startBeat;
			notesOff();
		} else {
			[m_node stop];
			m_scheduled = false;
			m_startFrame = 0;
		}
		m_heldMs = m_startMs;
		m_fromStart = true;
		m_playing = false;
	}

	void seek(qint64 ms) override {
		if (!loaded()) {
			return;
		}
		ms = std::clamp<qint64>(ms, 0, m_durationMs);
		if (m_midi) {
			notesOff();
			m_sequencer.currentPositionInSeconds = ms / 1000.0;
		} else {
			// the player node cannot jump: schedule the rest of the file again
			[m_node stop];
			m_scheduled = false;
			m_startFrame = static_cast<AVAudioFramePosition>(ms * m_sampleRate / 1000);
			if (m_playing) {
				schedule();
				[m_node play];
			}
		}
		m_heldMs = ms;
		m_fromStart = false;
	}

	void setGain(float gain) override {
		if (m_engine) {
			m_engine.mainMixerNode.outputVolume = gain;
		}
	}

	void setPan(float pan) override {
		if (m_mixer) {
			m_mixer.pan = pan;
		}
	}

	void setRate(float rate) override {
		if (m_sequencer) {
			m_sequencer.rate = rate;
		}
		if (m_varispeed) {
			m_varispeed.rate = rate;
		}
	}

	qint64 position() const override {
		if (!m_playing) {
			return m_heldMs;
		}
		qint64 ms = 0;
		if (m_midi) {
			ms = static_cast<qint64>(m_sequencer.currentPositionInSeconds * 1000);
		} else {
			AVAudioFramePosition frame = m_startFrame;
			AVAudioTime* nodeTime = m_node.lastRenderTime;
			AVAudioTime* playerTime = nodeTime ? [m_node playerTimeForNodeTime:nodeTime] : nil;
			if (playerTime && playerTime.sampleTimeValid) {
				// frames of the file: the varispeed after the node pulls them faster or slower
				frame += std::max<AVAudioFramePosition>(0, playerTime.sampleTime);
			}
			ms = static_cast<qint64>(frame * 1000 / m_sampleRate);
		}
		return std::clamp<qint64>(ms, 0, m_durationMs);
	}

	qint64 duration() const override {
		return m_durationMs;
	}

	qint64 loopStart() const override {
		return m_loopStartMs;
	}

	bool finished() const override {
		// the node and the sequencer go on playing silence after the end
		return m_playing && m_durationMs > 0 && position() >= m_durationMs;
	}

private:
	bool loaded() const {
		return m_synth || m_file;
	}

	void schedule() {
		const AVAudioFramePosition left = m_file.length - m_startFrame;
		if (left > 0) {
			[m_node scheduleSegment:m_file startingFrame:m_startFrame
				frameCount:static_cast<AVAudioFrameCount>(left) atTime:nil completionHandler:nil];
		}
		m_scheduled = true;
	}

	/** What the MIDI file sends before where it starts (MidiStart) */
	void sendSetup() {
		for (const std::vector<uint8_t>& bytes : m_setup) {
			if (bytes[0] == 0xF0) {
				[m_synth sendMIDISysExEvent:[NSData dataWithBytes:bytes.data() length:bytes.size()]];
			} else if (bytes.size() == 2) {
				[m_synth sendMIDIEvent:bytes[0] data1:bytes[1]];
			} else {
				[m_synth sendMIDIEvent:bytes[0] data1:bytes[1] data2:bytes[2]];
			}
		}
	}

	void notesOff() {
		if (!m_synth) {
			return;
		}
		for (uint8_t channel = 0; channel < 16; ++channel) {
			[m_synth sendController:120 withValue:0 onChannel:channel]; // all sound off
			[m_synth sendController:123 withValue:0 onChannel:channel]; // all notes off
		}
	}

	/** Stops the file and takes its nodes out of the engine, which stays for the next one */
	void unload() {
		if (m_synth) {
			[m_sequencer stop];
		}
		if (m_node) {
			[m_node stop];
		}
		// out of the engine, nothing of them is heard any more
		if (m_synth) {
			[m_engine detachNode:m_synth];
		}
		if (m_node) {
			[m_engine detachNode:m_node];
		}
		if (m_varispeed) {
			[m_engine detachNode:m_varispeed];
		}
		m_synth = nil;
		m_node = nil;
		m_varispeed = nil;
		m_file = nil;
		m_setup.clear();
		m_playing = false;
		m_scheduled = false;
		m_fromStart = true;
		m_startFrame = 0;
		m_startBeat = 0;
		m_heldMs = 0;
		m_startMs = 0;
		m_durationMs = 0;
		m_loopStartMs = 0;
	}

	AVAudioEngine* m_engine = nil;
	// the balance of the file: the pan of its input in the main mixer
	AVAudioMixerNode* m_mixer = nil;

	// sampled audio
	AVAudioFile* m_file = nil;
	AVAudioPlayerNode* m_node = nil;
	AVAudioUnitVarispeed* m_varispeed = nil;
	AVAudioFramePosition m_startFrame = 0;
	double m_sampleRate = 44100;
	bool m_scheduled = false;

	// MIDI
	AVAudioUnitMIDIInstrument* m_synth = nil;
	AVAudioSequencer* m_sequencer = nil;
	AVMusicTimeStamp m_startBeat = 0;
	std::vector<std::vector<uint8_t>> m_setup;

	bool m_midi = false;
	bool m_playing = false;
	// the next play() starts the file: a MIDI file sends its setup first
	bool m_fromStart = true;
	// the position while paused or stopped
	qint64 m_heldMs = 0;
	// where the file starts, after the silence at the beginning of a MIDI file
	qint64 m_startMs = 0;
	qint64 m_durationMs = 0;
	qint64 m_loopStartMs = 0;
};

} // namespace

std::unique_ptr<AudioBackend> createAppleAudioBackend() {
	return std::make_unique<AppleAudioBackend>();
}
