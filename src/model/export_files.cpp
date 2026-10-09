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

#include "export_files.h"
#include <QBuffer>
#include <QCoreApplication>
#include <QDateTime>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QtEndian>
#include <zlib.h>
#include <algorithm>

namespace {
QString tr(const char* text)
{
	return QCoreApplication::translate("ExportFiles", text);
}

/** The lookup keys of the web Player: lowercase and NFKC with ICU, ASCII lowercase without */
QStringList webKeys(const QString& name)
{
	QStringList keys{ name.toLower().normalized(QString::NormalizationForm_KC) };
	QString ascii = name.normalized(QString::NormalizationForm_C);
	for (QChar& c : ascii) {
		if (c >= QLatin1Char('A') && c <= QLatin1Char('Z')) {
			c = QChar(c.unicode() + 32);
		}
	}
	if (!keys.contains(ascii)) {
		keys << ascii;
	}
	return keys;
}

/** A square image of the size, the picture centered and scaled (pixels kept sharp when enlarged) */
QImage iconImage(const QImage& image, int size)
{
	QImage source = image.convertToFormat(QImage::Format_ARGB32);
	const bool enlarge = std::max(source.width(), source.height()) < size;
	source = source.scaled(size, size, Qt::KeepAspectRatio, enlarge ? Qt::FastTransformation : Qt::SmoothTransformation);
	QImage square(size, size, QImage::Format_ARGB32);
	square.fill(Qt::transparent);
	for (int y = 0; y < source.height(); ++y) {
		for (int x = 0; x < source.width(); ++x) {
			square.setPixel(x + (size - source.width()) / 2, y + (size - source.height()) / 2, source.pixel(x, y));
		}
	}
	return square;
}

QByteArray png(const QImage& image)
{
	QByteArray data;
	QBuffer buffer(&data);
	buffer.open(QIODevice::WriteOnly);
	image.save(&buffer, "PNG");
	return data;
}

void putLE16(QByteArray& data, quint16 value)
{
	char bytes[2];
	qToLittleEndian(value, bytes);
	data.append(bytes, 2);
}

void putLE32(QByteArray& data, quint32 value)
{
	char bytes[4];
	qToLittleEndian(value, bytes);
	data.append(bytes, 4);
}

/**
 * An icon bitmap of the same size and colors as old (header, palette, the
 * rows from the bottom up, then the transparency mask), drawn from the
 * image; empty if old is not a bitmap that can be written again.
 */
QByteArray dibIcon(const QByteArray& old, const QImage& image)
{
	if (old.size() < 40 || qFromLittleEndian<quint32>(old.constData()) != 40 || qFromLittleEndian<quint32>(old.constData() + 16) != 0) {
		return QByteArray();
	}
	const int w = qFromLittleEndian<qint32>(old.constData() + 4);
	const int h = qFromLittleEndian<qint32>(old.constData() + 8) / 2;
	const int bpp = qFromLittleEndian<quint16>(old.constData() + 14);
	if (w <= 0 || h <= 0 || w > 1024 || h > 1024 || (bpp != 4 && bpp != 8 && bpp != 24 && bpp != 32)) {
		return QByteArray();
	}
	const quint32 used = qFromLittleEndian<quint32>(old.constData() + 32);
	const int colors = bpp > 8 ? 0 : (used ? static_cast<int>(used) : 1 << bpp);
	if (40 + colors * 4 > old.size()) {
		return QByteArray();
	}
	const QImage icon = iconImage(image, std::max(w, h)).scaled(w, h);
	QImage indexed;
	QList<QRgb> palette;
	if (colors > 0) {
		// transparent pixels are black under the mask (the mask keeps what is behind, black changes nothing)
		QImage opaque = icon.convertToFormat(QImage::Format_RGB32);
		for (int y = 0; y < h; ++y) {
			for (int x = 0; x < w; ++x) {
				if (qAlpha(icon.pixel(x, y)) < 128) {
					opaque.setPixel(x, y, qRgb(0, 0, 0));
				}
			}
		}
		if (bpp == 8 && colors == 256) {
			// a palette of its own
			indexed = opaque.convertToFormat(QImage::Format_Indexed8, Qt::ThresholdDither | Qt::AvoidDither);
			palette = indexed.colorTable();
		} else {
			// the colors of the old icon (the 16 colors of Windows, usually)
			for (int i = 0; i < colors; ++i) {
				const uchar* c = reinterpret_cast<const uchar*>(old.constData()) + 40 + i * 4;
				palette << qRgb(c[2], c[1], c[0]);
			}
			indexed = opaque.convertToFormat(QImage::Format_Indexed8, palette, Qt::ThresholdDither);
		}
		if (palette.size() > colors) {
			return QByteArray();
		}
		while (palette.size() < colors) {
			palette << qRgb(0, 0, 0);
		}
	}
	const int row = ((w * bpp + 31) / 32) * 4;
	const int mask_row = ((w + 31) / 32) * 4;
	QByteArray data = old.left(40);
	qToLittleEndian<quint32>(row * h + mask_row * h, data.data() + 20);
	for (QRgb c : palette) {
		const char bgra[4] = { char(qBlue(c)), char(qGreen(c)), char(qRed(c)), 0 };
		data.append(bgra, 4);
	}
	for (int y = h - 1; y >= 0; --y) {
		QByteArray line(row, '\0');
		for (int x = 0; x < w; ++x) {
			const QRgb c = icon.pixel(x, y);
			const bool clear = qAlpha(c) < 128;
			if (bpp == 32) {
				line[x * 4] = char(qBlue(c));
				line[x * 4 + 1] = char(qGreen(c));
				line[x * 4 + 2] = char(qRed(c));
				line[x * 4 + 3] = char(qAlpha(c));
			} else if (bpp == 24) {
				line[x * 3] = char(clear ? 0 : qBlue(c));
				line[x * 3 + 1] = char(clear ? 0 : qGreen(c));
				line[x * 3 + 2] = char(clear ? 0 : qRed(c));
			} else {
				const int index = indexed.pixelIndex(x, y);
				if (bpp == 8) {
					line[x] = char(index);
				} else {
					line[x / 2] = char(line[x / 2] | (x % 2 ? index : index << 4));
				}
			}
		}
		data.append(line);
	}
	// the mask: 1 where the icon is transparent (32-bit icons use their alpha)
	for (int y = h - 1; y >= 0; --y) {
		QByteArray line(mask_row, '\0');
		for (int x = 0; x < w && bpp != 32; ++x) {
			if (qAlpha(icon.pixel(x, y)) < 128) {
				line[x / 8] = char(line[x / 8] | (0x80 >> (x % 8)));
			}
		}
		data.append(line);
	}
	return data;
}
}

QString ExportFiles::safeFileName(const QString& name)
{
	QString result;
	for (QChar c : name.trimmed()) {
		// refused by Windows (and / everywhere)
		if (c.unicode() < 32 || QStringLiteral("<>:\"/\\|?*").contains(c)) {
			continue;
		}
		result += c;
	}
	// Windows refuses names ending with a dot or a space
	while (result.endsWith(QLatin1Char('.')) || result.endsWith(QLatin1Char(' '))) {
		result.chop(1);
	}
	return result.isEmpty() ? QStringLiteral("Game") : result;
}

QString ExportFiles::slug(const QString& name)
{
	QString result;
	const QString plain = name.normalized(QString::NormalizationForm_KD).toLower();
	for (QChar c : plain) {
		if ((c >= QLatin1Char('a') && c <= QLatin1Char('z')) || (c >= QLatin1Char('0') && c <= QLatin1Char('9'))) {
			result += c;
		} else if (c.isSpace() || c == QLatin1Char('-') || c == QLatin1Char('_')) {
			if (!result.isEmpty() && !result.endsWith(QLatin1Char('-'))) {
				result += QLatin1Char('-');
			}
		}
	}
	while (result.endsWith(QLatin1Char('-'))) {
		result.chop(1);
	}
	return result.isEmpty() ? QStringLiteral("game") : result;
}

bool ExportFiles::copyTree(const QString& source, const QString& destination, QString* error)
{
	const QDir src(source);
	if (!QDir().mkpath(destination)) {
		*error = tr("Cannot create %1.").arg(QDir::toNativeSeparators(destination));
		return false;
	}
	QDirIterator it(source, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDirIterator::Subdirectories);
	while (it.hasNext()) {
		it.next();
		const QFileInfo info = it.fileInfo();
		const QString target = destination + QLatin1Char('/') + src.relativeFilePath(info.filePath());
		bool ok;
		if (info.isSymLink()) {
			ok = QFile::link(info.readSymLink(), target);
		} else if (info.isDir()) {
			ok = QDir().mkpath(target);
		} else {
			ok = QFile::copy(info.filePath(), target);
		}
		if (!ok) {
			*error = tr("Cannot copy %1.").arg(QDir::toNativeSeparators(info.filePath()));
			return false;
		}
	}
	return true;
}

bool ExportFiles::writeZip(const QString& zip_path, const QString& root, QString* error, const std::function<bool()>& cancelled)
{
	struct Entry {
		QString name;
		quint32 crc = 0;
		quint32 size = 0;
		quint32 compressed = 0;
		quint16 method = 0;
		quint32 mode = 0;
		quint32 offset = 0;
	};

	const QDir base(root);
	QStringList paths;
	QDirIterator it(root, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDirIterator::Subdirectories);
	while (it.hasNext()) {
		paths << base.relativeFilePath(it.next());
	}
	// the same archive for the same files
	std::sort(paths.begin(), paths.end());
	if (paths.size() > 65535) {
		*error = tr("Too many files for a zip archive.");
		return false;
	}

	QSaveFile out(zip_path);
	if (!out.open(QIODevice::WriteOnly)) {
		*error = tr("Cannot write %1.").arg(QDir::toNativeSeparators(zip_path));
		return false;
	}
	const QDateTime now = QDateTime::currentDateTime();
	const quint16 dos_time = (now.time().hour() << 11) | (now.time().minute() << 5) | (now.time().second() / 2);
	const quint16 dos_date = ((now.date().year() - 1980) << 9) | (now.date().month() << 5) | now.date().day();

	QList<Entry> entries;
	for (const QString& path : paths) {
		if (cancelled && cancelled()) {
			out.cancelWriting();
			*error = tr("Cancelled.");
			return false;
		}
		const QFileInfo info(base.filePath(path));
		Entry entry;
		entry.name = path;
		QByteArray data;
		if (info.isSymLink()) {
			// a link is stored as its target, with the link type in the permissions
			data = info.readSymLink().toUtf8();
			entry.mode = 0120777;
		} else if (info.isDir()) {
			entry.name += QLatin1Char('/');
			entry.mode = 040755;
		} else {
			QFile file(info.filePath());
			if (!file.open(QIODevice::ReadOnly)) {
				out.cancelWriting();
				*error = tr("Cannot read %1.").arg(QDir::toNativeSeparators(info.filePath()));
				return false;
			}
			data = file.readAll();
			entry.mode = 0100000 | (info.permissions() & QFileDevice::ExeOwner ? 0755 : 0644);
		}
		if (static_cast<quint64>(data.size()) >= 0xFFFFFFFFull || out.pos() >= 0xFFFFFFFFll) {
			out.cancelWriting();
			*error = tr("The game is too big for a zip archive (4 GB).");
			return false;
		}
		entry.size = static_cast<quint32>(data.size());
		entry.crc = crc32(0, reinterpret_cast<const Bytef*>(data.constData()), entry.size);
		QByteArray stored = data;
		if (entry.size > 0 && !info.isSymLink()) {
			// raw deflate (no zlib header), as zip wants it
			z_stream zs = {};
			deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
			QByteArray packed(static_cast<qsizetype>(deflateBound(&zs, entry.size)), Qt::Uninitialized);
			zs.next_in = reinterpret_cast<Bytef*>(data.data());
			zs.avail_in = entry.size;
			zs.next_out = reinterpret_cast<Bytef*>(packed.data());
			zs.avail_out = static_cast<uInt>(packed.size());
			const bool ok = deflate(&zs, Z_FINISH) == Z_STREAM_END;
			packed.resize(static_cast<qsizetype>(zs.total_out));
			deflateEnd(&zs);
			// already compressed files (ogg, png) are stored as they are
			if (ok && packed.size() < data.size()) {
				stored = packed;
				entry.method = 8;
			}
		}
		entry.compressed = static_cast<quint32>(stored.size());
		entry.offset = static_cast<quint32>(out.pos());

		const QByteArray name = entry.name.toUtf8();
		QByteArray header;
		putLE32(header, 0x04034b50);
		putLE16(header, 20);
		putLE16(header, 0x0800);  // UTF-8 names
		putLE16(header, entry.method);
		putLE16(header, dos_time);
		putLE16(header, dos_date);
		putLE32(header, entry.crc);
		putLE32(header, entry.compressed);
		putLE32(header, entry.size);
		putLE16(header, static_cast<quint16>(name.size()));
		putLE16(header, 0);
		out.write(header + name);
		out.write(stored);
		entries << entry;
	}

	const quint32 directory = static_cast<quint32>(out.pos());
	for (const Entry& entry : entries) {
		const QByteArray name = entry.name.toUtf8();
		QByteArray header;
		putLE32(header, 0x02014b50);
		putLE16(header, (3 << 8) | 20);  // made on Unix: the permissions are read
		putLE16(header, 20);
		putLE16(header, 0x0800);
		putLE16(header, entry.method);
		putLE16(header, dos_time);
		putLE16(header, dos_date);
		putLE32(header, entry.crc);
		putLE32(header, entry.compressed);
		putLE32(header, entry.size);
		putLE16(header, static_cast<quint16>(name.size()));
		putLE16(header, 0);
		putLE16(header, 0);
		putLE16(header, 0);
		putLE16(header, 0);
		putLE32(header, (entry.mode << 16) | ((entry.mode & 040000) ? 0x10 : 0));
		putLE32(header, entry.offset);
		out.write(header + name);
	}
	QByteArray end;
	putLE32(end, 0x06054b50);
	putLE16(end, 0);
	putLE16(end, 0);
	putLE16(end, static_cast<quint16>(entries.size()));
	putLE16(end, static_cast<quint16>(entries.size()));
	putLE32(end, static_cast<quint32>(out.pos()) - directory);
	putLE32(end, directory);
	putLE16(end, 0);
	out.write(end);
	if (!out.commit()) {
		*error = tr("Cannot write %1.").arg(QDir::toNativeSeparators(zip_path));
		return false;
	}
	return true;
}

QJsonObject ExportFiles::webIndex(const QDir& game_dir)
{
	std::function<QJsonObject(const QDir&, bool)> index = [&](const QDir& dir, bool root) {
		QJsonObject object;
		if (!root) {
			// the real name of the folder: the key of the object is not used
			object.insert(QStringLiteral("_dirname"), dir.dirName());
		}
		const QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::Hidden, QDir::Name);
		// names with their extension first (RPG_RT.ldb, Map0001.lmu, translations)...
		for (const QFileInfo& file : files) {
			for (const QString& key : webKeys(file.fileName())) {
				if (!object.contains(key)) {
					object.insert(key, file.fileName());
				}
			}
		}
		// ...then without (the Player asks for "CharSet/Hero" and tries the extensions)
		for (const QFileInfo& file : files) {
			const int dot = file.fileName().lastIndexOf(QLatin1Char('.'));
			if (dot <= 0) {
				continue;
			}
			for (const QString& key : webKeys(file.fileName().left(dot))) {
				if (!object.contains(key)) {
					object.insert(key, file.fileName());
				}
			}
		}
		for (const QFileInfo& sub : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
			const QJsonObject child = index(QDir(sub.filePath()), false);
			for (const QString& key : webKeys(sub.fileName())) {
				object.insert(key, child);
			}
		}
		return object;
	};

	QJsonObject metadata;
	metadata.insert(QStringLiteral("version"), 2);
	metadata.insert(QStringLiteral("date"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
	QJsonObject result;
	result.insert(QStringLiteral("metadata"), metadata);
	result.insert(QStringLiteral("cache"), index(game_dir, true));
	return result;
}

QByteArray ExportFiles::protectMapTree(const QByteArray& lmt, QString* error)
{
	// what the Player checks: the header "LcfMapTree" with its length, each byte plus one
	if (!lmt.startsWith(QByteArray("\x0a" "LcfMapTree"))) {
		*error = tr("RPG_RT.lmt does not start like a map tree of RPG Maker.");
		return QByteArray();
	}
	QByteArray result = lmt;
	for (char& c : result) {
		c = static_cast<char>(static_cast<unsigned char>(c) + 1);
	}
	return result;
}

QByteArray ExportFiles::makeIcns(const QImage& image)
{
	// PNG entries, read by macOS 10.7 and later
	static const struct { const char* type; int size; } kinds[] = {
		{ "icp4", 16 }, { "icp5", 32 }, { "ic11", 32 }, { "ic12", 64 }, { "ic07", 128 },
		{ "ic13", 256 }, { "ic08", 256 }, { "ic14", 512 }, { "ic09", 512 }, { "ic10", 1024 },
	};
	QByteArray body;
	for (const auto& kind : kinds) {
		const QByteArray data = png(iconImage(image, kind.size));
		char length[4];
		qToBigEndian(static_cast<quint32>(data.size() + 8), length);
		body += QByteArray(kind.type, 4) + QByteArray(length, 4) + data;
	}
	char length[4];
	qToBigEndian(static_cast<quint32>(body.size() + 8), length);
	return QByteArray("icns", 4) + QByteArray(length, 4) + body;
}

int ExportFiles::replaceExeIcon(const QString& exe_path, const QImage& image, QString* message)
{
	QFile file(exe_path);
	if (!file.open(QIODevice::ReadOnly)) {
		*message = tr("Cannot read %1.").arg(QDir::toNativeSeparators(exe_path));
		return -1;
	}
	QByteArray pe = file.readAll();
	file.close();

	bool bad = false;
	auto u16 = [&](qint64 offset) -> quint32 {
		if (offset < 0 || offset + 2 > pe.size()) { bad = true; return 0; }
		return qFromLittleEndian<quint16>(pe.constData() + offset);
	};
	auto u32 = [&](qint64 offset) -> quint32 {
		if (offset < 0 || offset + 4 > pe.size()) { bad = true; return 0; }
		return qFromLittleEndian<quint32>(pe.constData() + offset);
	};
	auto set32 = [&](qint64 offset, quint32 value) { qToLittleEndian(value, pe.data() + offset); };
	auto invalid = [&]() {
		*message = tr("%1 is not a Windows program that can be read.").arg(QFileInfo(exe_path).fileName());
		return -1;
	};

	if (!pe.startsWith("MZ")) {
		return invalid();
	}
	const quint32 pe_offset = u32(0x3C);
	if (bad || pe.mid(pe_offset, 4) != QByteArray("PE\0\0", 4)) {
		return invalid();
	}
	const qint64 coff = pe_offset + 4;
	const quint32 sections = u16(coff + 2);
	const qint64 optional = coff + 20;
	const qint64 section_table = optional + u16(coff + 16);
	const bool pe64 = u16(optional) == 0x20b;
	const qint64 directories = optional + (pe64 ? 112 : 96);
	const quint32 directory_count = u32(optional + (pe64 ? 108 : 92));
	if (bad || directory_count < 3) {
		return invalid();
	}

	// where the data of an address of the loaded program is in the file
	auto fileOffset = [&](quint32 rva) -> qint64 {
		for (quint32 i = 0; i < sections; ++i) {
			const qint64 s = section_table + i * 40;
			const quint32 va = u32(s + 12), raw_size = u32(s + 16), raw = u32(s + 20);
			if (rva >= va && rva < va + raw_size) {
				return raw + (rva - va);
			}
		}
		bad = true;
		return -1;
	};

	const quint32 resources_rva = u32(directories + 2 * 8);
	if (resources_rva == 0) {
		*message = tr("%1 has no icon.").arg(QFileInfo(exe_path).fileName());
		return 0;
	}
	const qint64 resources = fileOffset(resources_rva);
	// the entries of a resource directory: id -> offset (from the start of the resources)
	auto entries = [&](qint64 dir) {
		QList<QPair<quint32, quint32>> list;
		const quint32 named = u16(dir + 12), ids = u16(dir + 14);
		for (quint32 i = 0; i < named + ids && !bad && i < 4096; ++i) {
			list << qMakePair(u32(dir + 16 + i * 8), u32(dir + 20 + i * 8));
		}
		return list;
	};
	// the data entries (offset of the entry in the file) of each resource of a type
	auto resourcesOf = [&](quint32 type) {
		QList<QPair<quint32, qint64>> found;
		for (const auto& [type_id, type_dir] : entries(resources)) {
			if (type_id != type || !(type_dir & 0x80000000)) {
				continue;
			}
			for (const auto& [id, id_dir] : entries(resources + (type_dir & 0x7FFFFFFF))) {
				if (!(id_dir & 0x80000000)) {
					continue;
				}
				for (const auto& language : entries(resources + (id_dir & 0x7FFFFFFF))) {
					if (!(language.second & 0x80000000)) {
						found << qMakePair(id, resources + language.second);
					}
				}
			}
		}
		return found;
	};

	const auto icons = resourcesOf(3);  // RT_ICON
	const auto groups = resourcesOf(14);  // RT_GROUP_ICON
	if (bad) {
		return invalid();
	}
	int replaced = 0;
	int skipped = 0;
	for (const auto& [id, entry] : icons) {
		const qint64 offset = fileOffset(u32(entry));
		const quint32 size = u32(entry + 4);
		if (bad || offset + size > pe.size()) {
			return invalid();
		}
		const QByteArray old = pe.mid(offset, size);
		QByteArray data;
		if (old.startsWith("\x89PNG")) {
			const int w = qFromBigEndian<quint32>(old.constData() + 16), h = qFromBigEndian<quint32>(old.constData() + 20);
			data = png(iconImage(image, std::max(w, h)).scaled(w, h));
		} else {
			data = dibIcon(old, image);
		}
		if (data.isEmpty() || data.size() > old.size()) {
			// another format, or no room: this image stays
			++skipped;
			continue;
		}
		pe.replace(offset, data.size(), data);
		pe.replace(offset + data.size(), old.size() - data.size(), QByteArray(old.size() - data.size(), '\0'));
		set32(entry + 4, static_cast<quint32>(data.size()));
		// the size is written in the icon groups too
		for (const auto& group : groups) {
			const qint64 g = fileOffset(u32(group.second));
			const quint32 count = u16(g + 4);
			for (quint32 i = 0; i < count && !bad; ++i) {
				const qint64 e = g + 6 + i * 14;
				if (u16(e + 12) == id) {
					set32(e + 8, static_cast<quint32>(data.size()));
				}
			}
		}
		++replaced;
	}
	if (bad) {
		return invalid();
	}

	if (replaced > 0) {
		// a signature no longer matches the program: removed, with the checksum (not checked for programs)
		const qint64 security = directories + 4 * 8;
		if (directory_count > 4 && u32(security + 4) > 0) {
			const quint32 cert = u32(security), cert_size = u32(security + 4);
			set32(security, 0);
			set32(security + 4, 0);
			if (cert + cert_size == static_cast<quint64>(pe.size())) {
				pe.truncate(cert);
			}
		}
		set32(optional + 64, 0);
		QSaveFile out(exe_path);
		if (!out.open(QIODevice::WriteOnly) || out.write(pe) != pe.size() || !out.commit()) {
			*message = tr("Cannot write %1.").arg(QDir::toNativeSeparators(exe_path));
			return -1;
		}
	}
	*message = skipped > 0
		? tr("%1 of %2 icon images replaced (the others have a format that cannot be written in their room).").arg(replaced).arg(replaced + skipped)
		: tr("%1 icon images replaced.").arg(replaced);
	return replaced;
}

QByteArray ExportFiles::setIniValues(QByteArray ini, const QList<QPair<QByteArray, QByteArray>>& values)
{
	QList<QByteArray> lines = ini.split('\n');
	const bool crlf = ini.contains("\r\n") || ini.isEmpty();
	int section = -1;
	int section_end = lines.size();
	for (int i = 0; i < lines.size(); ++i) {
		const QByteArray line = lines[i].trimmed();
		if (line.startsWith('[')) {
			if (section >= 0) {
				section_end = i;
				break;
			}
			if (line.compare("[RPG_RT]", Qt::CaseInsensitive) == 0) {
				section = i;
			}
		}
	}
	const QByteArray eol = crlf ? "\r" : "";
	if (section < 0) {
		// a new section first
		QList<QByteArray> head{ "[RPG_RT]" + eol };
		for (const auto& [key, value] : values) {
			head << key + '=' + value + eol;
		}
		head << eol;
		return (head + lines).join('\n');
	}
	// the section ends at its last line that is not empty
	int insert = section_end;
	while (insert - 1 > section && lines[insert - 1].trimmed().isEmpty()) {
		--insert;
	}
	for (const auto& [key, value] : values) {
		bool found = false;
		for (int i = section + 1; i < insert; ++i) {
			const QByteArray line = lines[i].trimmed();
			const int equal = line.indexOf('=');
			if (equal > 0 && line.left(equal).trimmed().compare(key, Qt::CaseInsensitive) == 0) {
				lines[i] = key + '=' + value + eol;
				found = true;
				break;
			}
		}
		if (!found) {
			lines.insert(insert++, key + '=' + value + eol);
		}
	}
	return lines.join('\n');
}
