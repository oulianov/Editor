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

#include "soundfont.h"
#include "core.h"
#include "common/logging.h"
#include "midi_synth.h"
#include "model/project.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QSet>
#include <QSettings>
#include <QVariantMap>
#include <cstring>

namespace {

/** The Editor default (Settings > Paths) */
constexpr const char* kEditorDefaultKey = "audio_soundfont";
/** The names of the files the game soundfonts were copied from, by project folder */
constexpr const char* kOriginalNamesKey = "soundfont_names";
constexpr const char* kGameSoundfont = "easyrpg.soundfont";

QString projectKey(const QString& folder) {
	return QDir::cleanPath(QFileInfo(folder).absoluteFilePath());
}

/** EASYRPG_EDITOR_AUDIO_BACKEND (audio_player.cpp): "qt" plays MIDI with FluidSynth on macOS too, "fmmidi" with FmMidi */
QString backendOverride() {
	return qEnvironmentVariable("EASYRPG_EDITOR_AUDIO_BACKEND").toLower();
}

quint16 le16(const char* p) {
	return static_cast<quint16>(static_cast<unsigned char>(p[0]) | static_cast<unsigned char>(p[1]) << 8);
}

quint32 le32(const char* p) {
	return static_cast<quint32>(le16(p)) | static_cast<quint32>(le16(p + 2)) << 16;
}

/** A chunk of the RIFF file: its id (or the type of a LIST), where its data starts, its size */
struct Chunk {
	QByteArray id;
	qint64 data = 0;
	qint64 size = 0;
};

/** The chunks of a part of the file (from, to): their headers only. false: one goes past the end */
bool readChunks(QFile& file, qint64 from, qint64 to, QList<Chunk>& chunks) {
	qint64 at = from;
	while (at + 8 <= to) {
		file.seek(at);
		const QByteArray header = file.read(12);
		if (header.size() < 8) {
			return false;
		}
		Chunk chunk;
		chunk.id = header.left(4);
		chunk.size = le32(header.constData() + 4);
		chunk.data = at + 8;
		if (chunk.data + chunk.size > to) {
			return false;
		}
		if (chunk.id == "LIST" && header.size() == 12 && chunk.size >= 4) {
			chunk.id = header.mid(8, 4);
			chunk.data += 4;
			chunk.size -= 4;
		}
		chunks << chunk;
		// chunks are padded to an even size
		at = chunk.data + chunk.size + (chunk.size & 1);
	}
	return true;
}

const Chunk* find(const QList<Chunk>& chunks, const char* id) {
	for (const Chunk& chunk : chunks) {
		if (chunk.id == id) {
			return &chunk;
		}
	}
	return nullptr;
}

} // namespace

SoundfontInfo SoundfontInfo::read(const QString& path) {
	SoundfontInfo info;
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly)) {
		info.error = QCoreApplication::translate("Soundfonts", "The soundfont cannot be read: %1").arg(file.errorString());
		return info;
	}
	info.size = file.size();
	const QByteArray riff = file.read(12);
	if (riff.size() < 12 || !riff.startsWith("RIFF") || riff.mid(8, 4) != "sfbk") {
		info.status = Status::NotSoundfont;
		info.error = QCoreApplication::translate("Soundfonts", "This is not an SF2 soundfont");
		return info;
	}
	auto damaged = [&info] {
		info.status = Status::Damaged;
		info.error = QCoreApplication::translate("Soundfonts", "The soundfont is damaged");
		return info;
	};
	// its three parts: the information, the samples, the presets (some files
	// say a RIFF size larger than they are: the parts must be there, whole)
	QList<Chunk> parts;
	readChunks(file, 12, info.size, parts);
	const Chunk* about = find(parts, "INFO");
	const Chunk* samples = find(parts, "sdta");
	const Chunk* presets = find(parts, "pdta");
	if (!about || !samples || !presets) {
		return damaged();
	}

	// the version (3: SF3) and the name
	QList<Chunk> fields;
	if (!readChunks(file, about->data, about->data + about->size, fields)) {
		return damaged();
	}
	if (const Chunk* version = find(fields, "ifil"); version && version->size >= 4) {
		file.seek(version->data);
		info.compressed = le16(file.read(2).constData()) >= 3;
	}
	if (const Chunk* name = find(fields, "INAM"); name && name->size > 0 && name->size < 1024) {
		file.seek(name->data);
		const QByteArray bytes = file.read(name->size);
		info.name = QString::fromLatin1(bytes.constData(), qstrnlen(bytes.constData(), bytes.size())).trimmed();
	}

	QList<Chunk> data;
	if (!readChunks(file, samples->data, samples->data + samples->size, data)) {
		return damaged();
	}
	QList<Chunk> tables;
	if (!readChunks(file, presets->data, presets->data + presets->size, tables)) {
		return damaged();
	}
	// the presets: 38 bytes each, the last one ends the list (EOP)
	const Chunk* headers = find(tables, "phdr");
	if (!headers || headers->size < 2 * 38 || headers->size % 38 != 0 || headers->size > 16 * 1024 * 1024) {
		return damaged();
	}
	file.seek(headers->data);
	const QByteArray list = file.read(headers->size);
	if (list.size() != headers->size) {
		return damaged();
	}
	info.presets = static_cast<int>(list.size() / 38) - 1;
	QSet<int> melodic;
	bool drums = false;
	for (int i = 0; i < info.presets; ++i) {
		const char* preset = list.constData() + i * 38;
		const int program = le16(preset + 20);
		const int bank = le16(preset + 22);
		if (bank == 0 && program < 128) {
			melodic.insert(program);
		} else if (bank == 128) {
			drums = true;
		}
	}
	// SF3 also by its samples, compressed (sfSampleType 0x10): 46 bytes each
	if (!info.compressed) {
		if (const Chunk* sampleHeaders = find(tables, "shdr"); sampleHeaders && sampleHeaders->size <= 16 * 1024 * 1024) {
			file.seek(sampleHeaders->data);
			const QByteArray table = file.read(sampleHeaders->size);
			for (qsizetype at = 0; at + 46 <= table.size() && !info.compressed; at += 46) {
				info.compressed = le16(table.constData() + at + 44) & 0x10;
			}
		}
	}

	info.status = Status::Valid;
	if (!melodic.contains(0)) {
		info.warnings << QCoreApplication::translate("Soundfonts", "Not a General MIDI soundfont: RPG Maker songs may sound wrong");
	}
	if (!melodic.isEmpty() && melodic.size() < 128) {
		info.warnings << QCoreApplication::translate("Soundfonts", "%n General MIDI instrument(s) missing: songs using them will be silent",
			nullptr, 128 - static_cast<int>(melodic.size()));
	}
	if (!drums) {
		info.warnings << QCoreApplication::translate("Soundfonts", "No drum kit: percussion will be silent");
	}
	if (info.compressed) {
		info.warnings << QCoreApplication::translate("Soundfonts", "Compressed SF3: EasyRPG Player may not load it");
	}
	return info;
}

QString SoundfontInfo::sizeText(qint64 bytes) {
	return QCoreApplication::translate("Soundfonts", "%1 MB").arg(QLocale().toString(static_cast<double>(bytes) / 1'000'000, 'f', 1));
}

Soundfonts& Soundfonts::instance() {
	static Soundfonts soundfonts;
	return soundfonts;
}

QString Soundfonts::gameSoundfont(const QString& folder) {
	if (folder.isEmpty()) {
		return {};
	}
	// FileFinder of the Player: in any case
	const QDir dir(folder);
	const QStringList found = dir.entryList({QString::fromLatin1(kGameSoundfont)}, QDir::Files | QDir::Hidden);
	for (const QString& name : found) {
		if (name.compare(QLatin1String(kGameSoundfont), Qt::CaseInsensitive) == 0) {
			return dir.filePath(name);
		}
	}
	return {};
}

QString Soundfonts::gameSoundfontTarget(const QString& folder) {
	const QString found = gameSoundfont(folder);
	return found.isEmpty() ? QDir(folder).filePath(QString::fromLatin1(kGameSoundfont)) : found;
}

QString Soundfonts::projectFolder() {
	const auto& project = core().project();
	return project ? project->projectDir().absolutePath() : QString();
}

QString Soundfonts::editorDefault() {
	const QString chosen = editorDefaultSetting();
	return chosen.isEmpty() ? qEnvironmentVariable("EASYRPG_EDITOR_SOUNDFONT") : chosen;
}

QString Soundfonts::editorDefaultSetting() {
	return QSettings().value(QLatin1String(kEditorDefaultKey)).toString();
}

void Soundfonts::setEditorDefault(const QString& path) {
	QSettings settings;
	if (path.isEmpty()) {
		settings.remove(QLatin1String(kEditorDefaultKey));
	} else {
		settings.setValue(QLatin1String(kEditorDefaultKey), QDir::cleanPath(path));
	}
	emit instance().changed();
}

QString Soundfonts::systemSoundfont() {
#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
	return {};
#else
	// as the Player looks: what the distribution built FluidSynth with
	// (synth.default-soundfont), then FluidR3_GM.sf2 in the usual folders
	static const QStringList paths = [] {
		QStringList list;
		if (const std::string built = MidiSynth::fluidSynthDefaultSoundfont(); !built.empty()) {
			list << QFile::decodeName(built.c_str());
		}
		list << QStringLiteral("/usr/share/soundfonts/default.sf2")
			<< QStringLiteral("/usr/share/soundfonts/FluidR3_GM.sf2")
			<< QStringLiteral("/usr/share/sounds/sf2/FluidR3_GM.sf2");
		list.removeDuplicates();
		return list;
	}();
	for (const QString& path : paths) {
		if (QFileInfo(path).isFile()) {
			return path;
		}
	}
	return {};
#endif
}

QString Soundfonts::originalName(const QString& folder) {
	return QSettings().value(QLatin1String(kOriginalNamesKey)).toMap().value(projectKey(folder)).toString();
}

void Soundfonts::setOriginalName(const QString& folder, const QString& name) {
	QSettings settings;
	QVariantMap names = settings.value(QLatin1String(kOriginalNamesKey)).toMap();
	if (name.isEmpty()) {
		names.remove(projectKey(folder));
	} else {
		names.insert(projectKey(folder), name);
	}
	settings.setValue(QLatin1String(kOriginalNamesKey), names);
}

QString Soundfonts::path(Source source) const {
	switch (source) {
		case Game:
			return gameSoundfont(projectFolder());
		case EditorDefault:
			return editorDefault();
		case System:
			return systemSoundfont();
		default:
			return {};
	}
}

bool Soundfonts::soundfontsPlayable() {
	const QString forced = backendOverride();
	if (forced == QLatin1String("fmmidi")) {
		return false;
	}
#ifdef Q_OS_MACOS
	if (forced.isEmpty()) {
		return true;
	}
#endif
	return MidiSynth::canPlaySoundfonts();
}

QString Soundfonts::unavailable(Source source) const {
	switch (source) {
		case Automatic:
		case BuiltIn:
			return {};
		case Platform:
#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
			if (backendOverride().isEmpty()) {
				return {};
			}
			return tr("Not with EASYRPG_EDITOR_AUDIO_BACKEND");
#else
			return tr("Linux has no synthesizer of its own: FmMidi plays");
#endif
		default:
			break;
	}
	const QString file = path(source);
	if (file.isEmpty()) {
		switch (source) {
			case Game:
				return projectFolder().isEmpty() ? tr("No project is open")
					: tr("The game has no easyrpg.soundfont (Project Settings > EasyRPG Player)");
			case EditorDefault:
				return tr("None is chosen in Settings > Paths");
			case System:
#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
				return tr("This system has no standard soundfont folder");
#else
				return tr("None in /usr/share/soundfonts or /usr/share/sounds/sf2");
#endif
			default:
				return tr("No file is chosen");
		}
	}
	if (!soundfontsPlayable()) {
		return tr("FluidSynth is not installed: soundfonts cannot be played");
	}
	if (!QFileInfo(file).isFile()) {
		return tr("%1 does not exist").arg(QDir::toNativeSeparators(file));
	}
	if (const SoundfontInfo header = info(file); !header.valid()) {
		return header.error;
	}
	return failure(file);
}

QString Soundfonts::refusal(const QString& path) const {
	if (path.isEmpty()) {
		return {};
	}
	if (!QFileInfo(path).isFile()) {
		return tr("%1 does not exist").arg(QDir::toNativeSeparators(path));
	}
	return info(path).error;
}

QString Soundfonts::sourceName(Source source) {
	switch (source) {
		case Automatic:
			return tr("Automatic");
		case Game:
			return tr("Game soundfont");
		case EditorDefault:
			return tr("Editor default");
		case System:
			return tr("System soundfont");
		case BuiltIn:
			return tr("Built-in (FmMidi)");
		case Platform:
#ifdef Q_OS_MACOS
			return tr("Apple DLS");
#elif defined(Q_OS_WIN)
			return tr("Microsoft GS Wavetable Synth");
#else
			return tr("System synthesizer");
#endif
		case File:
			return tr("Soundfont file");
	}
	return {};
}

QVariantList Soundfonts::choices() const {
	QVariantList list;
	for (Source source : {Automatic, Game, EditorDefault, System, BuiltIn, Platform}) {
		const QString why = unavailable(source);
		const QString name = source == Automatic ? tr("Automatic: %1").arg(sourceName(automaticSource())) : sourceName(source);
		list << QVariantMap{
			{QStringLiteral("source"), static_cast<int>(source)},
			{QStringLiteral("name"), name},
			{QStringLiteral("available"), why.isEmpty()},
			{QStringLiteral("why"), why},
		};
	}
	return list;
}

QString Soundfonts::describe(int choice, int playing, const QString& soundfont) const {
	Source source = static_cast<Source>(playing);
	QString file = soundfont;
	if (source == Automatic) {
		// nothing played yet: what the choice would play
		source = choice == Automatic ? automaticSource() : static_cast<Source>(choice);
		file = path(source);
	}
	QString text;
	if (!file.isEmpty() && (source == Game || source == EditorDefault || source == System || source == File)) {
		const QFileInfo info(file);
		text = tr("%1 — %2").arg(info.fileName(), SoundfontInfo::sizeText(info.size()));
	} else if (source == BuiltIn) {
		text = tr("FmMidi, EasyRPG Player's FM synthesizer, always there");
	} else {
#ifdef Q_OS_MACOS
		text = tr("Apple DLS, the instruments of macOS");
#elif defined(Q_OS_WIN)
		text = tr("Microsoft GS Wavetable Synth, the instruments of Windows");
#else
		text = tr("FmMidi, EasyRPG Player's FM synthesizer, always there");
#endif
	}
	// the choice could not play: what plays instead
	if (choice != Automatic && playing != Automatic && playing != choice) {
		const QString why = m_failures.value(path(static_cast<Source>(choice))).second;
		text = tr("%1 could not be played%2. Playing: %3")
			.arg(sourceName(static_cast<Source>(choice)), why.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(why), text);
	}
	return text;
}

MidiPlan Soundfonts::plan(Source choice, const QString& file) const {
	MidiPlan plan;
	switch (choice) {
		case Automatic:
			for (Source source : {Game, EditorDefault, System}) {
				if (const QString path = this->path(source); usable(path) && !plan.soundfonts.contains(path)) {
					plan.soundfonts << path;
				}
			}
			break;
		case Game:
		case EditorDefault:
		case System:
			// compared on purpose: tried even after a failure, which the Jukebox then tells
			if (const QString path = this->path(choice); !path.isEmpty()) {
				plan.soundfonts << path;
			}
			break;
		case File:
			if (!file.isEmpty()) {
				plan.soundfonts << file;
			}
			break;
		case BuiltIn:
			plan.systemSynth = false;
			break;
		case Platform:
			break;
	}
	if (!soundfontsPlayable()) {
		plan.soundfonts.clear();
	}
	return plan;
}

Soundfonts::Source Soundfonts::automaticSource() const {
	for (Source source : {Game, EditorDefault, System}) {
		if (unavailable(source).isEmpty()) {
			return source;
		}
	}
	return unavailable(Platform).isEmpty() ? Platform : BuiltIn;
}

bool Soundfonts::usable(const QString& path) const {
	return !path.isEmpty() && QFileInfo(path).isFile() && info(path).valid() && failure(path).isEmpty();
}

Soundfonts::Stamp Soundfonts::stamp(const QString& path) {
	const QFileInfo file(path);
	return {file.lastModified(), file.size()};
}

SoundfontInfo Soundfonts::info(const QString& path) const {
	const Stamp now = stamp(path);
	auto it = m_infos.constFind(path);
	if (it != m_infos.constEnd() && it->first == now) {
		return it->second;
	}
	SoundfontInfo read = SoundfontInfo::read(path);
	if (!read.valid()) {
		qCInfo(lcAudio).noquote() << "Soundfont" << path << "skipped:" << read.error;
	}
	m_infos.insert(path, {now, read});
	return read;
}

void Soundfonts::markFailed(const QString& path, const QString& why) {
	m_failures.insert(path, {stamp(path), why});
	emit changed();
}

QString Soundfonts::failure(const QString& path) const {
	auto it = m_failures.constFind(path);
	if (it == m_failures.constEnd() || !(it->first == stamp(path))) {
		return {};
	}
	return tr("It could not be loaded: %1").arg(it->second);
}
