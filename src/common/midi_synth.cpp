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

#include "midi_synth.h"
#include "common/logging.h"
#include "midi/midisynth.h"

#include <QFile>
#include <QFileInfo>
#include <QLibrary>
#include <QStringList>
#include <mutex>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

/** EasyRPG Player's FM synthesizer, with its instruments (midi/midiprogram.h) */
class FmMidiSynth final : public MidiSynth {
public:
	explicit FmMidiSynth(int sampleRate) : m_rate(static_cast<float>(sampleRate)) {
		// the names midiprogram.h uses
		midisynth::fm_note_factory* note_factory = &m_factory;
		midisynth::DRUMPARAMETER p;
		#include "midi/midiprogram.h"
	}

	const char* name() const override { return "FmMidi"; }
	void message(uint32_t message) override { m_synth.midi_event(message); }
	void sysex(const uint8_t* data, std::size_t size) override { m_synth.sysex_message(data, size); }

	void render(float* buffer, int frames) override {
		m_mix.assign(static_cast<std::size_t>(frames) * 2, 0);
		m_synth.synthesize_mixing(m_mix.data(), frames, m_rate);
		// 16-bit samples, which it would clip
		for (std::size_t i = 0; i < m_mix.size(); ++i) {
			buffer[i] = static_cast<float>(m_mix[i]) * (1.0f / 32768);
		}
	}

private:
	float m_rate;
	// the synthesizer makes its notes with the factory: built first
	midisynth::fm_note_factory m_factory;
	midisynth::synthesizer m_synth{&m_factory};
	std::vector<int_least32_t> m_mix;
};

#ifndef Q_OS_WIN
/**
 * FluidSynth, loaded at run time when the system has it (libfluidsynth of
 * FluidSynth 2 or 3, Linux): the Editor needs it neither to build nor to
 * start, and plays with FmMidi without it, or without a soundfont.
 * Its synthesizers load the soundfont, which takes time: the last one used
 * is kept for the next file.
 */
class FluidLibrary {
public:
	// the functions used (FluidSynth's types are pointers)
	void* (*new_settings)() = nullptr;
	void (*delete_settings)(void*) = nullptr;
	int (*settings_setnum)(void*, const char*, double) = nullptr;
	void* (*new_synth)(void*) = nullptr;
	void (*delete_synth)(void*) = nullptr;
	int (*sfload)(void*, const char*, int) = nullptr;
	int (*noteon)(void*, int, int, int) = nullptr;
	int (*noteoff)(void*, int, int) = nullptr;
	int (*cc)(void*, int, int, int) = nullptr;
	int (*program_change)(void*, int, int) = nullptr;
	int (*channel_pressure)(void*, int, int) = nullptr;
	int (*key_pressure)(void*, int, int, int) = nullptr;
	int (*pitch_bend)(void*, int, int) = nullptr;
	int (*sysex)(void*, const char*, int, char*, int*, int*, int) = nullptr;
	int (*all_sounds_off)(void*, int) = nullptr;
	int (*system_reset)(void*) = nullptr;
	int (*write_float)(void*, int, void*, int, int, void*, int, int) = nullptr;
	// not required
	int (*settings_dupstr)(void*, const char*, char**) = nullptr;
	void (*fluid_free)(void*) = nullptr;
	char* (*version_str)() = nullptr;
	void* (*set_log_function)(int, void*, void*) = nullptr;

	/** Loaded once; never unloaded: synthesizers are kept until the Editor quits */
	static FluidLibrary& instance() {
		static FluidLibrary* library = new FluidLibrary;
		return *library;
	}

	bool loaded() const { return m_loaded; }

	struct Synth {
		void* settings = nullptr;
		void* synth = nullptr;
		int rate = 0;
	};

	/** A synthesizer at this rate with the soundfont loaded, or none (then, the first time, logs why) */
	Synth take(int rate) {
		std::lock_guard lock(m_mutex);
		if (m_idle.synth && m_idle.rate == rate) {
			return std::exchange(m_idle, Synth());
		}
		if (m_noSoundfont) {
			return Synth();
		}
		Synth made;
		made.rate = rate;
		made.settings = new_settings();
		if (!made.settings) {
			return Synth();
		}
		settings_setnum(made.settings, "synth.sample-rate", rate);
		// as loud as in the Player; its reverb and chorus stay
		settings_setnum(made.settings, "synth.gain", 0.6);
		made.synth = new_synth(made.settings);
		if (!made.synth) {
			delete_settings(made.settings);
			return Synth();
		}
		const QStringList candidates = m_soundfont.isEmpty() ? soundfonts(made.settings) : QStringList{m_soundfont};
		for (const QString& path : candidates) {
			if (QFileInfo(path).isFile() && sfload(made.synth, QFile::encodeName(path).constData(), 1) != -1) {
				if (m_soundfont.isEmpty()) {
					m_soundfont = path;
					qCInfo(lcAudio).noquote() << "MIDI: FluidSynth" << m_version << "with the soundfont" << path;
				}
				return made;
			}
		}
		qCInfo(lcAudio).noquote() << "MIDI: FluidSynth" << m_version << "found, but no soundfont among"
			<< candidates.join(QLatin1String(", ")) << "(EASYRPG_EDITOR_SOUNDFONT names one)";
		m_noSoundfont = true;
		release(made);
		return Synth();
	}

	/** A synthesizer done with: kept for the next file, unless one is */
	void giveBack(Synth used) {
		all_sounds_off(used.synth, -1);
		system_reset(used.synth);
		std::lock_guard lock(m_mutex);
		if (m_idle.synth) {
			release(std::exchange(m_idle, used));
		} else {
			m_idle = used;
		}
	}

private:
	FluidLibrary() {
		// FluidSynth 2.2 and later (libfluidsynth.so.3), then 2.0 and 2.1; all
		// symbols resolved at once: a library missing some is not used at all
		QString tried;
		for (int version : {3, 2}) {
			m_library.setFileNameAndVersion(QStringLiteral("fluidsynth"), version);
			m_library.setLoadHints(QLibrary::ResolveAllSymbolsHint);
			if (m_library.load()) {
				break;
			}
			tried += QLatin1String(tried.isEmpty() ? "" : "; ") + m_library.errorString();
		}
		if (!m_library.isLoaded()) {
			qCInfo(lcAudio).noquote() << "MIDI: FluidSynth not found:" << tried;
			return;
		}
		bool all = true;
		auto resolve = [this, &all](auto& function, const char* name, bool required = true) {
			function = reinterpret_cast<std::remove_reference_t<decltype(function)>>(m_library.resolve(name));
			if (!function && required) {
				qCInfo(lcAudio) << "MIDI: FluidSynth not used," << m_library.fileName() << "has no" << name;
				all = false;
			}
		};
		resolve(new_settings, "new_fluid_settings");
		resolve(delete_settings, "delete_fluid_settings");
		resolve(settings_setnum, "fluid_settings_setnum");
		resolve(new_synth, "new_fluid_synth");
		resolve(delete_synth, "delete_fluid_synth");
		resolve(sfload, "fluid_synth_sfload");
		resolve(noteon, "fluid_synth_noteon");
		resolve(noteoff, "fluid_synth_noteoff");
		resolve(cc, "fluid_synth_cc");
		resolve(program_change, "fluid_synth_program_change");
		resolve(channel_pressure, "fluid_synth_channel_pressure");
		resolve(key_pressure, "fluid_synth_key_pressure");
		resolve(pitch_bend, "fluid_synth_pitch_bend");
		resolve(sysex, "fluid_synth_sysex");
		resolve(all_sounds_off, "fluid_synth_all_sounds_off");
		resolve(system_reset, "fluid_synth_system_reset");
		resolve(write_float, "fluid_synth_write_float");
		resolve(settings_dupstr, "fluid_settings_dupstr", false);
		resolve(fluid_free, "fluid_free", false);
		resolve(version_str, "fluid_version_str", false);
		resolve(set_log_function, "fluid_set_log_function", false);
		if (!all) {
			return;
		}
		m_version = version_str ? QString::fromLatin1(version_str()) : m_library.fileName();
		if (set_log_function) {
			// its warnings and information (a missing instrument...) on the terminal: only its errors stay
			for (int level : {2, 3, 4}) {
				set_log_function(level, nullptr, nullptr);
			}
		}
		m_loaded = true;
	}

	/** Where the soundfonts of General MIDI are: the one chosen, the distribution's default, the usual files */
	QStringList soundfonts(void* settings) const {
		QStringList paths;
		if (const QString chosen = qEnvironmentVariable("EASYRPG_EDITOR_SOUNDFONT"); !chosen.isEmpty()) {
			paths << chosen;
		}
		// what the distribution built FluidSynth with (Fedora: /usr/share/soundfonts/default.sf2)
		char* value = nullptr;
		if (settings_dupstr && settings_dupstr(settings, "synth.default-soundfont", &value) == 0 && value) {
			paths << QFile::decodeName(value);
			if (fluid_free) {
				fluid_free(value);
			}
		}
		// Fedora and Arch, then Debian and Ubuntu (fluid-soundfont-gm, timgm6mb-soundfont, musescore-general-soundfont)
		paths << QStringLiteral("/usr/share/soundfonts/default.sf2")
			<< QStringLiteral("/usr/share/soundfonts/FluidR3_GM.sf2")
			<< QStringLiteral("/usr/share/soundfonts/freepats-general-midi.sf2")
			<< QStringLiteral("/usr/share/sounds/sf2/default-GM.sf2")
			<< QStringLiteral("/usr/share/sounds/sf2/FluidR3_GM.sf2")
			<< QStringLiteral("/usr/share/sounds/sf2/TimGM6mb.sf2")
			<< QStringLiteral("/usr/share/sounds/sf3/default-GM.sf3")
			<< QStringLiteral("/usr/local/share/soundfonts/default.sf2");
		paths.removeDuplicates();
		return paths;
	}

	void release(const Synth& synth) {
		delete_synth(synth.synth);
		delete_settings(synth.settings);
	}

	QLibrary m_library;
	bool m_loaded = false;
	QString m_version;
	std::mutex m_mutex;
	QString m_soundfont;
	bool m_noSoundfont = false;
	Synth m_idle;
};

class FluidSynth final : public MidiSynth {
public:
	FluidSynth(FluidLibrary& library, FluidLibrary::Synth synth) : m_library(library), m_synth(synth) {}
	~FluidSynth() override { m_library.giveBack(m_synth); }

	const char* name() const override { return "FluidSynth"; }

	void message(uint32_t message) override {
		void* synth = m_synth.synth;
		const int channel = message & 0x0F;
		const int data1 = (message >> 8) & 0x7F;
		const int data2 = (message >> 16) & 0x7F;
		switch (message & 0xF0) {
			case 0x80:
				m_library.noteoff(synth, channel, data1);
				break;
			case 0x90:
				if (data2 == 0) {
					m_library.noteoff(synth, channel, data1);
				} else {
					m_library.noteon(synth, channel, data1, data2);
				}
				break;
			case 0xA0:
				m_library.key_pressure(synth, channel, data1, data2);
				break;
			case 0xB0:
				m_library.cc(synth, channel, data1, data2);
				break;
			case 0xC0:
				m_library.program_change(synth, channel, data1);
				break;
			case 0xD0:
				m_library.channel_pressure(synth, channel, data1);
				break;
			case 0xE0:
				m_library.pitch_bend(synth, channel, data1 | (data2 << 7));
				break;
		}
	}

	void sysex(const uint8_t* data, std::size_t size) override {
		// without F0 and F7
		if (size > 2 && data[0] == 0xF0 && data[size - 1] == 0xF7) {
			m_library.sysex(m_synth.synth, reinterpret_cast<const char*>(data + 1), static_cast<int>(size - 2),
				nullptr, nullptr, nullptr, 0);
		}
	}

	void render(float* buffer, int frames) override {
		m_library.write_float(m_synth.synth, frames, buffer, 0, 2, buffer, 1, 2);
	}

private:
	FluidLibrary& m_library;
	FluidLibrary::Synth m_synth;
};
#endif

} // namespace

std::unique_ptr<MidiSynth> MidiSynth::create(int sampleRate, bool allowFluidSynth) {
#ifndef Q_OS_WIN
	if (allowFluidSynth) {
		FluidLibrary& library = FluidLibrary::instance();
		if (library.loaded()) {
			if (FluidLibrary::Synth synth = library.take(sampleRate); synth.synth) {
				return std::make_unique<FluidSynth>(library, synth);
			}
		}
	}
#else
	Q_UNUSED(allowFluidSynth)
#endif
	static std::once_flag logged;
	std::call_once(logged, [] { qCInfo(lcAudio) << "MIDI: FmMidi synthesizer"; });
	return std::make_unique<FmMidiSynth>(sampleRate);
}
