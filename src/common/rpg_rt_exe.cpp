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

#include "rpg_rt_exe.h"

#include <QCoreApplication>
#include <QFile>
#include <QtEndian>
#include <lcf/encoder.h>
#include <algorithm>

namespace {

/** The Incredibly Dumb PE parser of EasyRPG Player (exe_reader.cpp) */
class ExeResources {
public:
	explicit ExeResources(QByteArray data) : m_data(std::move(data)) {
		const quint32 pe = u32(0x3C);
		const quint16 machine = u16(pe + 4);
		if (machine != 0x14c && machine != 0x8664) {
			return;
		}
		// the largest known RPG_RT.exe has 11 sections
		quint16 sections = std::min<quint16>(u16(pe + 6), 11);
		const quint32 optional = pe + 0x18;
		const quint16 magic = u16(optional);
		if (magic != 0x10b && magic != 0x20b) {
			return;
		}
		m_info.program = true;
		quint32 section = optional + u16(pe + 0x14);
		const quint32 rva = u32(optional + (magic == 0x10b ? 0x60 : 0x70) + 16);
		m_rva = rva;
		for (; sections; --sections, section += 0x28) {
			quint32 virtualSize = u32(section + 0x08);
			virtualSize = std::max(virtualSize, u32(section + 0x10));
			switch (u32(section)) {
			case 0x45444F43: // CODE
				m_info.codeSize = virtualSize;
				break;
			case 0x52454843: // CHER(RY)
				m_info.cherrySize = virtualSize;
				break;
			case 0x50454547: // GEEP
				m_info.geepSize = virtualSize;
				break;
			case 0x30585055: // UPX0
				m_info.upx = true;
				break;
			}
			const quint32 sectionRva = u32(section + 0x0C);
			if (rva && !m_offset && sectionRva <= rva && sectionRva + virtualSize > rva) {
				m_offset = u32(section + 0x14) + (rva - sectionRva);
			}
		}
	}

	/** The bitmap resource named EXFONT, with the file header resources lack */
	QByteArray exfont() const {
		const quint32 bitmaps = byType(2);
		if (!bitmaps) {
			return {};
		}
		quint32 entries = u16(bitmaps + 0x0C) + u16(bitmaps + 0x0E);
		for (quint32 entry = bitmaps + 0x10; entries; --entries, entry += 8) {
			const quint32 name = u32(entry);
			if (!(name & 0x80000000) || !nameIs(relative(name), "EXFONT")) {
				continue;
			}
			const auto [position, size] = dataOf(u32(entry + 4));
			return withFileHeader(position, size);
		}
		return {};
	}

	/** EXEReader::GetFileInfo, then GetEngineType */
	RpgRtExe::Info info() const {
		RpgRtExe::Info info = m_info;
		if (!info.program) {
			return info;
		}
		info.logos = logoCount();
		const quint32 versions = byType(16);
		quint32 entries = versions ? u16(versions + 0x0C) + u16(versions + 0x0E) : 0;
		for (quint32 entry = versions + 0x10; entries; --entries, entry += 8) {
			if (u32(entry) != 1) {
				continue;
			}
			const auto [position, size] = dataOf(u32(entry + 4));
			const QByteArray resource = m_data.mid(position, size);
			// VS_FIXEDFILEINFO, aligned: found by its signature
			const int signature = resource.indexOf(QByteArray("\xBD\x04\xEF\xFE", 4));
			if (signature >= 0 && signature + 24 <= resource.size()) {
				const quint32 high = qFromLittleEndian<quint32>(resource.constData() + signature + 16);
				const quint32 low = qFromLittleEndian<quint32>(resource.constData() + signature + 20);
				info.version = QStringLiteral("%1.%2.%3.%4").arg(high >> 16).arg(high & 0xFFFF).arg(low >> 16).arg(low & 0xFFFF);
			}
			const int strings = resource.indexOf(utf16("StringFileInfo"));
			if (strings >= 0) {
				if (resource.indexOf(utf16("EasyRPGPlayer"), strings) >= 0) {
					info.easyrpgPlayer = true;
				} else {
					const QByteArray maniac = utf16("Maniacs, v");
					const int at = resource.indexOf(maniac, strings);
					if (at >= 0 && at + maniac.size() + 12 < resource.size()) {
						QByteArray number = resource.mid(at + maniac.size(), 12);
						number.replace('\0', QByteArray());
						info.maniac = std::atoi(number.constData());
					}
				}
			}
			break;
		}
		info.engine = engineType(info);
		return info;
	}

private:
	static QByteArray utf16(const char* text) {
		QByteArray bytes;
		for (; *text; ++text) {
			bytes.append(*text).append('\0');
		}
		return bytes;
	}

	static int engineType(RpgRtExe::Info& info) {
		using namespace RpgRtExe;
		if (info.easyrpgPlayer) {
			return EngineNone;
		}
		if (info.version.isEmpty()) {
			// RPG Maker 2000, and 2003 before 1.0.2.1: no VERSIONINFO
			if (info.logos == 3) {
				return EngineRpg2k;
			}
			if (info.logos == 1) {
				// VALUE!, or 2003: the size of its code tells
				if (info.codeSize > 0xB0000) {
					return info.codeSize >= 0xC7400 ? EngineRpg2k3 | EngineMajorUpdated : EngineRpg2k3;
				}
				return EngineRpg2k | EngineMajorUpdated;
			}
			return EngineNone;
		}
		const QStringList parts = info.version.split(QLatin1Char('.'));
		auto part = [&](int i) { return parts.value(i).toInt() & 0xFF; };
		const bool v1121 = part(0) == 1 && part(1) == 1 && part(2) == 2 && part(3) == 1;
		if (info.logos == 0) {
			// the Maniac Patch rewritten: 1.1.2.1 without logos nor CODE and CHERRY sections
			if (v1121 && info.codeSize == 0 && info.cherrySize == 0) {
				info.maniac = std::max(info.maniac, 1);
				return EngineRpg2k3 | EngineMajorUpdated | EngineEnglish;
			}
			return EngineNone;
		}
		if (info.logos != 1 || part(0) != 1) {
			return EngineNone;
		}
		switch (part(1)) {
		case 6:
			// the official English RPG Maker 2000
			return EngineRpg2k | EngineMajorUpdated | EngineEnglish;
		case 0:
			return part(2) < 5 ? EngineRpg2k3 : EngineRpg2k3 | EngineMajorUpdated;
		case 1:
			// the first Maniac Patches: a hacked 1.1.2.1 (a GEEP section, or a large CHERRY one)
			if (v1121 && !info.maniac && (info.geepSize > 0 || info.cherrySize > 0x10000)) {
				info.maniac = 1;
			}
			return EngineRpg2k3 | EngineMajorUpdated | EngineEnglish;
		}
		return EngineNone;
	}

	/** EXEReader::GetLogoCount: the resources named XYZ, alone at the top */
	int logoCount() const {
		if (!m_offset || u16(m_offset + 0x0C) != 1) {
			return 0;
		}
		const quint32 entry = m_offset + 0x10;
		if (!nameIs(relative(u32(entry)), "XYZ")) {
			return 0;
		}
		return u16(relative(u32(entry + 4)) + 0x0C);
	}

	/** Where a resource is in the file, and its size */
	std::pair<quint32, quint32> dataOf(quint32 data) const {
		if (data & 0x80000000) {
			// a directory of languages: the first one
			data = m_offset + u32(relative(data) + 0x14);
		}
		return {u32(data) - m_rva + m_offset, u32(data + 4)};
	}

	quint16 u16(quint32 at) const {
		return at + 2 <= static_cast<quint32>(m_data.size()) ? qFromLittleEndian<quint16>(m_data.constData() + at) : 0;
	}
	quint32 u32(quint32 at) const {
		return at + 4 <= static_cast<quint32>(m_data.size()) ? qFromLittleEndian<quint32>(m_data.constData() + at) : 0;
	}
	quint32 relative(quint32 offset) const {
		return m_offset + (offset ^ 0x80000000);
	}
	quint32 byType(quint32 type) const {
		if (!m_offset) {
			return 0;
		}
		quint32 entries = u16(m_offset + 0x0C) + u16(m_offset + 0x0E);
		for (quint32 entry = m_offset + 0x10; entries; --entries, entry += 8) {
			if (u32(entry) == type) {
				return relative(u32(entry + 4));
			}
		}
		return 0;
	}
	bool nameIs(quint32 at, const char* name) const {
		if (u16(at) != qstrlen(name)) {
			return false;
		}
		for (; *name; ++name) {
			at += 2;
			if (u16(at) != static_cast<quint16>(*name)) {
				return false;
			}
		}
		return true;
	}
	QByteArray withFileHeader(quint32 position, quint32 size) const {
		if (size < 40 || position + size > static_cast<quint32>(m_data.size())) {
			return {};
		}
		const QByteArray bitmap = m_data.mid(position, size);
		// BITMAPINFOHEADER: its size, the bits per pixel, the colors
		const quint32 infoSize = qFromLittleEndian<quint32>(bitmap.constData());
		const quint16 depth = qFromLittleEndian<quint16>(bitmap.constData() + 14);
		quint32 colors = qFromLittleEndian<quint32>(bitmap.constData() + 32);
		if (depth != 8) {
			return {};
		}
		if (colors == 0) {
			colors = 256;
		}
		QByteArray file(14, '\0');
		file[0] = 'B';
		file[1] = 'M';
		qToLittleEndian<quint32>(14 + size, file.data() + 2);
		qToLittleEndian<quint32>(14 + infoSize + colors * 4, file.data() + 10);
		return file + bitmap;
	}

	QByteArray m_data;
	quint32 m_rva = 0;
	quint32 m_offset = 0;
	RpgRtExe::Info m_info;
};

QByteArray readFile(const QString& path) {
	QFile file(path);
	return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

} // namespace

RpgRtExe::Info RpgRtExe::read(const QString& path) {
	return ExeResources(readFile(path)).info();
}

QByteArray RpgRtExe::exfont(const QString& path) {
	return ExeResources(readFile(path)).exfont();
}

QStringList RpgRtExe::fontNames(const QString& path) {
	// constant strings of Delphi: -1 (no reference count), the length, the text.
	// The names of the system graphics come first ("System", "System2", "Frame"), then
	// those of font 1 and font 2; translations pad theirs with zeros.
	const QByteArray data = readFile(path);
	QList<QByteArray> strings;
	for (qsizetype at = data.indexOf("\xFF\xFF\xFF\xFF"); at >= 0 && at + 8 < data.size();
			at = data.indexOf("\xFF\xFF\xFF\xFF", at + 4)) {
		const quint32 length = qFromLittleEndian<quint32>(data.constData() + at + 4);
		if (length == 0 || length > 64 || at + 8 + length >= static_cast<quint32>(data.size())) {
			continue;
		}
		// its length tells (a translation may leave spaces after it), text and zeros
		QByteArray text = data.mid(at + 8, length);
		const int end = text.indexOf('\0');
		if (end >= 0) {
			text.truncate(end);
		}
		if (text.isEmpty() || std::any_of(text.begin(), text.end(), [](char c) { return static_cast<uchar>(c) < 0x20; })) {
			continue;
		}
		strings << text;
		if (strings.size() >= 3 && strings[strings.size() - 3] == "Frame") {
			// in the code page of Japanese Windows (Shift-JIS)
			QStringList names;
			for (const QByteArray& name : {strings[strings.size() - 2], strings.back()}) {
				std::string bytes = name.toStdString();
				lcf::Encoder("932").Encode(bytes);
				names << QString::fromStdString(bytes).trimmed();
			}
			return names;
		}
	}
	return {};
}

QString RpgRtExe::engineName(int engine) {
	const char* name = nullptr;
	switch (engine) {
	case EngineRpg2k:
		name = QT_TRANSLATE_NOOP("RpgRtExe", "RPG Maker 2000 (Japanese)");
		break;
	case EngineRpg2k | EngineMajorUpdated:
		name = QT_TRANSLATE_NOOP("RpgRtExe", "RPG Maker 2000 1.5x (VALUE!)");
		break;
	case EngineRpg2k | EngineMajorUpdated | EngineEnglish:
		name = QT_TRANSLATE_NOOP("RpgRtExe", "RPG Maker 2000 1.6x (English)");
		break;
	case EngineRpg2k3:
		name = QT_TRANSLATE_NOOP("RpgRtExe", "RPG Maker 2003 1.00-1.04");
		break;
	case EngineRpg2k3 | EngineMajorUpdated:
		name = QT_TRANSLATE_NOOP("RpgRtExe", "RPG Maker 2003 1.05-1.09");
		break;
	case EngineRpg2k3 | EngineMajorUpdated | EngineEnglish:
		name = QT_TRANSLATE_NOOP("RpgRtExe", "RPG Maker 2003 1.10-1.12 (English)");
		break;
	default:
		return QCoreApplication::translate("RpgRtExe", "Unknown");
	}
	return QCoreApplication::translate("RpgRtExe", name);
}

int RpgRtExe::engineFromSetting(const QString& value) {
	// Game_ConfigGame::Initialize
	const QString v = value.trimmed();
	if (v == QLatin1String("rpg2k") || v == QLatin1String("2000")) {
		return EngineRpg2k;
	}
	if (v == QLatin1String("rpg2kv150") || v == QLatin1String("2000v150")) {
		return EngineRpg2k | EngineMajorUpdated;
	}
	if (v == QLatin1String("rpg2ke") || v == QLatin1String("2000e")) {
		return EngineRpg2k | EngineMajorUpdated | EngineEnglish;
	}
	if (v == QLatin1String("rpg2k3") || v == QLatin1String("2003")) {
		return EngineRpg2k3;
	}
	if (v == QLatin1String("rpg2k3v105") || v == QLatin1String("2003v105")) {
		return EngineRpg2k3 | EngineMajorUpdated;
	}
	if (v == QLatin1String("rpg2k3e") || v == QLatin1String("2003e")) {
		return EngineRpg2k3 | EngineMajorUpdated | EngineEnglish;
	}
	return EngineNone;
}
