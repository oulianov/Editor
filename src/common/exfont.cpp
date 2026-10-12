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

#include "exfont.h"
#include "common/image_loader.h"
#include "model/project.h"

#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QtEndian>

namespace {

/** Reads an RPG_RT.exe the way EasyRPG Player does (exe_reader.cpp) */
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
		quint32 section = optional + u16(pe + 0x14);
		const quint32 rva = u32(optional + (magic == 0x10b ? 0x60 : 0x70) + 16);
		m_rva = rva;
		if (!rva) {
			return;
		}
		for (; sections; --sections, section += 0x28) {
			quint32 virtualSize = u32(section + 0x08);
			virtualSize = std::max(virtualSize, u32(section + 0x10));
			const quint32 sectionRva = u32(section + 0x0C);
			if (sectionRva <= rva && sectionRva + virtualSize > rva) {
				m_offset = u32(section + 0x14) + (rva - sectionRva);
				break;
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
			quint32 data = u32(entry + 4);
			if (data & 0x80000000) {
				// a directory of languages: the first one
				data = m_offset + u32(relative(data) + 0x14);
			}
			const quint32 position = u32(data) - m_rva + m_offset;
			const quint32 size = u32(data + 4);
			return withFileHeader(position, size);
		}
		return {};
	}

private:
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
};

} // namespace

QByteArray ExFont::fromExe(const QString& path) {
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly)) {
		return {};
	}
	return ExeResources(file.readAll()).exfont();
}

ExFont::Sheet ExFont::load(const Project& project) {
	// an image of the game (Font/ExFont first), as the Player
	for (const QString& path : {project.findFile(QStringLiteral("Font"), QStringLiteral("ExFont"), FileFinder::FileType::Image),
			project.findFile(QStringLiteral("ExFont"), FileFinder::FileType::Image)}) {
		if (path.isEmpty()) {
			continue;
		}
		// BMP and PNG as they are (their palette tells the strokes), XYZ converted
		QImage image(path);
		if (image.isNull()) {
			image = ImageLoader::Load(path).toImage();
		}
		if (!image.isNull()) {
			return {image, QFileInfo(path).fileName()};
		}
	}
	const QString exe = project.findFile(QStringLiteral("RPG_RT.exe"));
	if (!exe.isEmpty()) {
		const QImage image = QImage::fromData(fromExe(exe), "BMP");
		if (!image.isNull()) {
			return {image, QStringLiteral("RPG_RT.exe")};
		}
	}
	// EasyRPG's, by filosofiamanga (Player/resources/exfont.png)
	return {QImage(QStringLiteral(":/app/exfont.png")), QStringLiteral("EasyRPG")};
}

QString ExFont::code(int index) {
	const QChar letter = index < 26 ? QChar(u'A' + index) : QChar(u'a' + index - 26);
	return QStringLiteral("$") + letter;
}

QImage ExFont::glyph(const QImage& sheet, int index, const QColor& color) {
	const QRect cell((index % Columns) * GlyphSize, (index / Columns) * GlyphSize, GlyphSize, GlyphSize);
	if (index < 0 || index >= Count || !sheet.rect().contains(cell)) {
		return {};
	}
	// the background: the color most of the sheet has (black, or transparent)
	const QImage all = sheet.convertToFormat(QImage::Format_ARGB32);
	QHash<QRgb, int> counts;
	for (int y = 0; y < all.height(); ++y) {
		for (int x = 0; x < all.width(); ++x) {
			++counts[all.pixel(x, y)];
		}
	}
	QRgb background = 0;
	int most = -1;
	for (auto it = counts.cbegin(); it != counts.cend(); ++it) {
		if (it.value() > most) {
			most = it.value();
			background = it.key();
		}
	}

	// as the Player: gray strokes take the color of the text, colored ones
	// (an EasyRPG extension) keep theirs
	QImage result(GlyphSize, GlyphSize, QImage::Format_ARGB32);
	result.fill(Qt::transparent);
	for (int y = 0; y < GlyphSize; ++y) {
		for (int x = 0; x < GlyphSize; ++x) {
			const QRgb pixel = all.pixel(cell.x() + x, cell.y() + y);
			if (pixel == background || qAlpha(pixel) == 0) {
				continue;
			}
			const bool gray = qRed(pixel) == qGreen(pixel) && qGreen(pixel) == qBlue(pixel);
			result.setPixelColor(x, y, gray ? color : QColor::fromRgba(pixel | 0xFF000000));
		}
	}
	return result;
}
