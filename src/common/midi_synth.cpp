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
#ifdef EDITOR_FLUIDSYNTH_LINKED
#include <fluidsynth.h>
#endif

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QLibrary>
#include <QStringList>
#include <cstdio>
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

/**
 * FluidSynth: built in on Windows (vcpkg's, static), elsewhere the system's
 * libfluidsynth (FluidSynth 2 or 3, Linux), loaded at run time: the Editor
 * needs it neither to build nor to start there, and plays with FmMidi
 * without it. The same functions either way.
 * Its synthesizers load their soundfont, which takes time: the last ones
 * used are kept for the next files.
 */
class FluidLibrary {
public:
	// the functions used (FluidSynth's types are pointers)
	void* (*new_settings)() = nullptr;
	void (*delete_settings)(void*) = nullptr;
	int (*settings_setnum)(void*, const char*, double) = nullptr;
	int (*settings_setint)(void*, const char*, int) = nullptr;
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
	int (*set_interp_method)(void*, int, int) = nullptr;
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
		// the file loaded, as it was: another one at its place is loaded again
		QString soundfont;
		QDateTime modified;
		qint64 size = 0;
	};

	/** A synthesizer at this rate with the soundfont loaded, or none (error: why) */
	Synth take(int rate, const QString& soundfont, std::string& error) {
		const QFileInfo file(soundfont);
		{
			std::lock_guard lock(m_mutex);
			for (auto it = m_idle.begin(); it != m_idle.end(); ++it) {
				if (it->rate == rate && it->soundfont == soundfont && it->modified == file.lastModified() && it->size == file.size()) {
					Synth kept = *it;
					m_idle.erase(it);
					return kept;
				}
			}
		}
		Synth made;
		made.rate = rate;
		made.soundfont = soundfont;
		made.modified = file.lastModified();
		made.size = file.size();
		made.settings = new_settings();
		if (!made.settings) {
			error = "new_fluid_settings failed";
			return Synth();
		}
		// as EasyRPG Player sets it (decoder_fluidsynth.cpp): as loud, as
		// many voices, without reverb nor chorus, linear interpolation
		settings_setnum(made.settings, "synth.sample-rate", rate);
		settings_setnum(made.settings, "synth.gain", 0.6);
		settings_setint(made.settings, "synth.polyphony", 256);
		settings_setint(made.settings, "synth.reverb.active", 0);
		settings_setint(made.settings, "synth.chorus.active", 0);
		settings_setint(made.settings, "synth.lock-memory", 0);
		made.synth = new_synth(made.settings);
		if (!made.synth) {
			delete_settings(made.settings);
			error = "new_fluid_synth failed";
			return Synth();
		}
#ifdef EDITOR_FLUIDSYNTH_LINKED
		// the file read by Qt: the names of Windows in any script
		fluid_sfloader_t* loader = new_fluid_defsfloader(static_cast<fluid_settings_t*>(made.settings));
		fluid_sfloader_set_callbacks(loader, &fileOpen, &fileRead, &fileSeek, &fileTell, &fileClose);
		fluid_synth_add_sfloader(static_cast<fluid_synth_t*>(made.synth), loader);
		const QByteArray name = soundfont.toUtf8();
#else
		const QByteArray name = QFile::encodeName(soundfont);
#endif
		if (set_interp_method) {
			set_interp_method(made.synth, -1, 1); // FLUID_INTERP_LINEAR
		}
		if (sfload(made.synth, name.constData(), 1) == -1) {
			release(made);
			error = "FluidSynth could not load it";
			return Synth();
		}
		return made;
	}

	/** A synthesizer done with: kept for the next file (the last two) */
	void giveBack(Synth used) {
		all_sounds_off(used.synth, -1);
		system_reset(used.synth);
		std::lock_guard lock(m_mutex);
		m_idle.push_back(used);
		while (m_idle.size() > 2) {
			release(m_idle.front());
			m_idle.erase(m_idle.begin());
		}
	}

	/** synth.default-soundfont: what the distribution built FluidSynth with (Fedora: /usr/share/soundfonts/default.sf2) */
	std::string defaultSoundfont() {
		std::string path;
		void* settings = settings_dupstr ? new_settings() : nullptr;
		char* value = nullptr;
		if (settings && settings_dupstr(settings, "synth.default-soundfont", &value) == 0 && value) {
			path = value;
			if (fluid_free) {
				fluid_free(value);
			}
		}
		if (settings) {
			delete_settings(settings);
		}
		return path;
	}

private:
	FluidLibrary() {
#ifdef EDITOR_FLUIDSYNTH_LINKED
		// built in: the same functions, linked
		auto link = [](auto& function, auto* linked) {
			function = reinterpret_cast<std::remove_reference_t<decltype(function)>>(linked);
		};
		link(new_settings, &new_fluid_settings);
		link(delete_settings, &delete_fluid_settings);
		link(settings_setnum, &fluid_settings_setnum);
		link(settings_setint, &fluid_settings_setint);
		link(new_synth, &new_fluid_synth);
		link(delete_synth, &delete_fluid_synth);
		link(sfload, &fluid_synth_sfload);
		link(noteon, &fluid_synth_noteon);
		link(noteoff, &fluid_synth_noteoff);
		link(cc, &fluid_synth_cc);
		link(program_change, &fluid_synth_program_change);
		link(channel_pressure, &fluid_synth_channel_pressure);
		link(key_pressure, &fluid_synth_key_pressure);
		link(pitch_bend, &fluid_synth_pitch_bend);
		link(sysex, &fluid_synth_sysex);
		link(all_sounds_off, &fluid_synth_all_sounds_off);
		link(system_reset, &fluid_synth_system_reset);
		link(write_float, &fluid_synth_write_float);
		link(set_interp_method, &fluid_synth_set_interp_method);
		link(settings_dupstr, &fluid_settings_dupstr);
		link(fluid_free, &::fluid_free);
		link(version_str, &fluid_version_str);
		link(set_log_function, &fluid_set_log_function);
		m_library.setFileName(QStringLiteral("FluidSynth"));
#else
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
			qCInfo(lcAudio).noquote() << "MIDI: FluidSynth not found, soundfonts cannot be played:" << tried;
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
		resolve(settings_setint, "fluid_settings_setint");
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
		resolve(set_interp_method, "fluid_synth_set_interp_method", false);
		resolve(settings_dupstr, "fluid_settings_dupstr", false);
		resolve(fluid_free, "fluid_free", false);
		resolve(version_str, "fluid_version_str", false);
		resolve(set_log_function, "fluid_set_log_function", false);
		if (!all) {
			return;
		}
#endif
		m_version = version_str ? QString::fromLatin1(version_str()) : m_library.fileName();
		if (set_log_function) {
			// its warnings and information (a missing instrument...) on the terminal: only its errors stay
			for (int level : {2, 3, 4}) {
				set_log_function(level, nullptr, nullptr);
			}
		}
		qCInfo(lcAudio).noquote() << "MIDI: FluidSynth" << m_version << "plays the soundfonts";
		m_loaded = true;
	}

#ifdef EDITOR_FLUIDSYNTH_LINKED
	// the soundfont's file, for FluidSynth's loader (FLUID_OK 0, FLUID_FAILED -1)
	static void* fileOpen(const char* name) {
		auto file = std::make_unique<QFile>(QString::fromUtf8(name));
		return file->open(QIODevice::ReadOnly) ? file.release() : nullptr;
	}
	static int fileRead(void* buffer, fluid_long_long_t count, void* file) {
		return static_cast<QFile*>(file)->read(static_cast<char*>(buffer), count) == count ? 0 : -1;
	}
	static int fileSeek(void* file, fluid_long_long_t offset, int origin) {
		auto* f = static_cast<QFile*>(file);
		const qint64 base = origin == SEEK_CUR ? f->pos() : origin == SEEK_END ? f->size() : 0;
		return f->seek(base + offset) ? 0 : -1;
	}
	static fluid_long_long_t fileTell(void* file) {
		return static_cast<QFile*>(file)->pos();
	}
	static int fileClose(void* file) {
		delete static_cast<QFile*>(file);
		return 0;
	}
#endif

	void release(const Synth& synth) {
		delete_synth(synth.synth);
		delete_settings(synth.settings);
	}

	QLibrary m_library;
	bool m_loaded = false;
	QString m_version;
	std::mutex m_mutex;
	std::vector<Synth> m_idle;
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

} // namespace

std::unique_ptr<MidiSynth> MidiSynth::createFmMidi(int sampleRate) {
	static std::once_flag logged;
	std::call_once(logged, [] { qCInfo(lcAudio) << "MIDI: FmMidi synthesizer"; });
	return std::make_unique<FmMidiSynth>(sampleRate);
}

bool MidiSynth::canPlaySoundfonts() {
	return FluidLibrary::instance().loaded();
}

std::unique_ptr<MidiSynth> MidiSynth::createFluidSynth(int sampleRate, const std::string& soundfont, std::string& error) {
	FluidLibrary& library = FluidLibrary::instance();
	if (!library.loaded()) {
		error = "FluidSynth is not there";
		return nullptr;
	}
	FluidLibrary::Synth synth = library.take(sampleRate, QString::fromStdString(soundfont), error);
	return synth.synth ? std::make_unique<FluidSynth>(library, synth) : nullptr;
}

std::string MidiSynth::fluidSynthDefaultSoundfont() {
	FluidLibrary& library = FluidLibrary::instance();
	return library.loaded() ? library.defaultSoundfont() : std::string();
}
