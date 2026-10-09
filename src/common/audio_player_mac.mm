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

#include "audio_backend.h"

#include <algorithm>

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

class AppleAudioBackend final : public AudioBackend {
public:
	~AppleAudioBackend() override {
		close();
	}

	bool open(const QString& path, bool midi, QString& error) override {
		close();
		m_midi = midi;
		@autoreleasepool {
			NSURL* url = [NSURL fileURLWithPath:path.toNSString()];
			NSError* err = nil;

			m_engine = [[AVAudioEngine alloc] init];
			m_mixer = [[AVAudioMixerNode alloc] init];
			[m_engine attachNode:m_mixer];
			[m_engine connect:m_mixer to:m_engine.mainMixerNode format:nil];

			if (midi) {
				AudioComponentDescription desc = {};
				desc.componentType = kAudioUnitType_MusicDevice;
				desc.componentSubType = kAudioUnitSubType_DLSSynth;
				desc.componentManufacturer = kAudioUnitManufacturer_Apple;
				m_synth = [[AVAudioUnitMIDIInstrument alloc] initWithAudioComponentDescription:desc];
				[m_engine attachNode:m_synth];
				[m_engine connect:m_synth to:m_mixer format:nil];

				m_sequencer = [[AVAudioSequencer alloc] initWithAudioEngine:m_engine];
				if (![m_sequencer loadFromURL:url options:AVMusicSequenceLoadSMF_PreserveTracks error:&err]) {
					error = describe(err);
					close();
					return false;
				}
				NSTimeInterval length = 0;
				__block AVMusicTimeStamp loopBeat = 0;
				for (AVMusicTrack* track in m_sequencer.tracks) {
					track.destinationAudioUnit = m_synth;
					length = std::max(length, track.lengthInSeconds);
					[track enumerateEventsInRange:AVMakeBeatRange(0, AVMusicTimeStampEndOfTrack)
						usingBlock:^(AVMusicEvent* event, AVMusicTimeStamp* timeStamp, BOOL*) {
							if ([event isKindOfClass:[AVMIDIControlChangeEvent class]] &&
								static_cast<AVMIDIControlChangeEvent*>(event).messageType == 111) {
								loopBeat = std::max(loopBeat, *timeStamp);
							}
						}];
				}
				m_durationMs = static_cast<qint64>(length * 1000);
				m_loopStartMs = static_cast<qint64>([m_sequencer secondsForBeats:loopBeat] * 1000);
			} else {
				m_file = [[AVAudioFile alloc] initForReading:url error:&err];
				if (!m_file) {
					error = describe(err);
					close();
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

			[m_engine prepare];
			if (![m_engine startAndReturnError:&err]) {
				error = describe(err);
				close();
				return false;
			}
			if (m_midi) {
				[m_sequencer prepareToPlay];
			}
		}
		return true;
	}

	void play() override {
		if (!m_engine) {
			return;
		}
		@autoreleasepool {
			// a new output device stops the engine
			if (!m_engine.isRunning) {
				[m_engine startAndReturnError:nil];
			}
			if (m_midi) {
				[m_sequencer startAndReturnError:nil];
			} else {
				if (!m_scheduled) {
					schedule();
				}
				[m_node play];
			}
		}
		m_playing = true;
	}

	void pause() override {
		if (!m_engine || !m_playing) {
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
		if (!m_engine) {
			return;
		}
		if (m_midi) {
			[m_sequencer stop];
			m_sequencer.currentPositionInSeconds = 0;
			notesOff();
		} else {
			[m_node stop];
			m_scheduled = false;
			m_startFrame = 0;
		}
		m_heldMs = 0;
		m_playing = false;
	}

	void seek(qint64 ms) override {
		if (!m_engine) {
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
	void schedule() {
		const AVAudioFramePosition left = m_file.length - m_startFrame;
		if (left > 0) {
			[m_node scheduleSegment:m_file startingFrame:m_startFrame
				frameCount:static_cast<AVAudioFrameCount>(left) atTime:nil completionHandler:nil];
		}
		m_scheduled = true;
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

	void close() {
		if (m_sequencer) {
			[m_sequencer stop];
		}
		if (m_node) {
			[m_node stop];
		}
		if (m_engine) {
			[m_engine stop];
		}
		m_sequencer = nil;
		m_synth = nil;
		m_node = nil;
		m_varispeed = nil;
		m_file = nil;
		m_mixer = nil;
		m_engine = nil;
		m_playing = false;
		m_scheduled = false;
		m_startFrame = 0;
		m_heldMs = 0;
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

	bool m_midi = false;
	bool m_playing = false;
	// the position while paused or stopped
	qint64 m_heldMs = 0;
	qint64 m_durationMs = 0;
	qint64 m_loopStartMs = 0;
};

} // namespace

std::unique_ptr<AudioBackend> createAppleAudioBackend() {
	return std::make_unique<AppleAudioBackend>();
}
