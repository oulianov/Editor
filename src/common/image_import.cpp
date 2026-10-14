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

#include "image_import.h"
#include <QFile>
#include <QHash>
#include <QSaveFile>
#include <QtEndian>
#include <cstring>
#include <vector>
#include <zlib.h>

namespace {
	/** A color not used by the palette, for the transparent index */
	QRgb unusedColor(const QVector<QRgb>& table, int skip) {
		const QRgb candidates[] = { qRgb(255, 0, 255), qRgb(0, 255, 255), qRgb(0, 255, 0), qRgb(255, 255, 0) };
		auto used = [&](QRgb c) {
			for (int i = 0; i < table.size(); ++i) {
				if (i != skip && (table[i] | 0xff000000u) == c) {
					return true;
				}
			}
			return false;
		};
		for (QRgb c : candidates) {
			if (!used(c)) {
				return c;
			}
		}
		for (int v = 1; v < 0x1000000; v += 0x010101) {
			if (!used(0xff000000u | v)) {
				return 0xff000000u | v;
			}
		}
		return qRgb(255, 0, 255);
	}
}

QImage ImageImport::opaquePalette(QImage image)
{
	QVector<QRgb> table = image.colorTable();
	for (QRgb& color : table) {
		color |= 0xff000000u;
	}
	image.setColorTable(table);
	return image;
}

void ImageImport::swapIndexes(QImage& image, int a, int b)
{
	if (a == b) {
		return;
	}
	QVector<QRgb> table = image.colorTable();
	if (a >= table.size() || b >= table.size()) {
		table.resize(qMax(a, b) + 1);
	}
	std::swap(table[a], table[b]);
	image.setColorTable(table);
	for (int y = 0; y < image.height(); ++y) {
		uchar* line = image.scanLine(y);
		for (int x = 0; x < image.width(); ++x) {
			if (line[x] == a) {
				line[x] = static_cast<uchar>(b);
			} else if (line[x] == b) {
				line[x] = static_cast<uchar>(a);
			}
		}
	}
}

QImage ImageImport::loadImage(const QString& path)
{
	QFile file(path);
	if (!file.open(QFile::ReadOnly)) {
		return QImage();
	}
	const QByteArray data = file.readAll();
	if (!data.startsWith("XYZ1")) {
		QImage image = QImage::fromData(data);
		if (image.format() == QImage::Format_Mono || image.format() == QImage::Format_MonoLSB) {
			image = image.convertToFormat(QImage::Format_Indexed8);
		}
		return image;
	}

	// XYZ: "XYZ1", width and height (16 bits, little endian), then zlib
	// compressed 256 RGB palette entries and one byte per pixel
	if (data.size() <= 8) {
		return QImage();
	}
	const int w = qFromLittleEndian<quint16>(data.constData() + 4);
	const int h = qFromLittleEndian<quint16>(data.constData() + 6);
	if (w == 0 || h == 0) {
		return QImage();
	}
	uLongf size = 768 + static_cast<uLongf>(w) * h;
	std::vector<Bytef> raw(size);
	if (uncompress(raw.data(), &size, reinterpret_cast<const Bytef*>(data.constData() + 8),
			static_cast<uLong>(data.size() - 8)) != Z_OK || size != raw.size()) {
		return QImage();
	}
	QImage image(w, h, QImage::Format_Indexed8);
	QVector<QRgb> table(256);
	for (int i = 0; i < 256; ++i) {
		table[i] = qRgb(raw[i * 3], raw[i * 3 + 1], raw[i * 3 + 2]);
	}
	image.setColorTable(table);
	for (int y = 0; y < h; ++y) {
		memcpy(image.scanLine(y), raw.data() + 768 + y * w, w);
	}
	return image;
}

bool ImageImport::saveXyz(const QImage& image, const QString& path)
{
	if (image.format() != QImage::Format_Indexed8 || image.width() > 0xffff || image.height() > 0xffff) {
		return false;
	}
	const int w = image.width();
	const int h = image.height();
	std::vector<Bytef> raw(768 + static_cast<size_t>(w) * h, 0);
	for (int i = 0; i < qMin(256, image.colorCount()); ++i) {
		const QRgb c = image.color(i);
		raw[i * 3] = static_cast<Bytef>(qRed(c));
		raw[i * 3 + 1] = static_cast<Bytef>(qGreen(c));
		raw[i * 3 + 2] = static_cast<Bytef>(qBlue(c));
	}
	for (int y = 0; y < h; ++y) {
		memcpy(raw.data() + 768 + y * w, image.constScanLine(y), w);
	}
	uLongf size = compressBound(static_cast<uLong>(raw.size()));
	std::vector<Bytef> packed(size);
	if (compress2(packed.data(), &size, raw.data(), static_cast<uLong>(raw.size()), Z_BEST_COMPRESSION) != Z_OK) {
		return false;
	}

	QSaveFile file(path);
	if (!file.open(QFile::WriteOnly)) {
		return false;
	}
	char header[8] = { 'X', 'Y', 'Z', '1' };
	qToLittleEndian<quint16>(static_cast<quint16>(w), header + 4);
	qToLittleEndian<quint16>(static_cast<quint16>(h), header + 6);
	file.write(header, 8);
	file.write(reinterpret_cast<const char*>(packed.data()), static_cast<qint64>(size));
	return file.commit();
}

int ImageImport::paletteSize(const QImage& image)
{
	return image.format() == QImage::Format_Indexed8 ? image.colorCount() : 0;
}

QImage ImageImport::toIndexed(const QImage& image)
{
	if (image.isNull()) {
		return QImage();
	}
	if (image.format() == QImage::Format_Indexed8) {
		return image;
	}
	if (image.format() == QImage::Format_Mono || image.format() == QImage::Format_MonoLSB) {
		return image.convertToFormat(QImage::Format_Indexed8);
	}

	const QImage src = image.convertToFormat(QImage::Format_ARGB32);
	bool transparent = false;
	for (int y = 0; y < src.height() && !transparent; ++y) {
		const auto* line = reinterpret_cast<const QRgb*>(src.constScanLine(y));
		for (int x = 0; x < src.width(); ++x) {
			if (qAlpha(line[x]) == 0) {
				transparent = true;
				break;
			}
		}
	}

	QImage out(src.size(), QImage::Format_Indexed8);
	QVector<QRgb> table;
	QHash<QRgb, int> indexes;
	if (transparent) {
		table << 0; // chosen below, once the colors are known
	}
	for (int y = 0; y < src.height(); ++y) {
		const auto* line = reinterpret_cast<const QRgb*>(src.constScanLine(y));
		uchar* dst = out.scanLine(y);
		for (int x = 0; x < src.width(); ++x) {
			if (qAlpha(line[x]) == 0) {
				dst[x] = 0;
				continue;
			}
			const QRgb color = line[x] | 0xff000000u;
			auto it = indexes.constFind(color);
			if (it == indexes.constEnd()) {
				if (table.size() == 256) {
					return QImage();
				}
				it = indexes.insert(color, static_cast<int>(table.size()));
				table << color;
			}
			dst[x] = static_cast<uchar>(*it);
		}
	}
	if (transparent) {
		table[0] = unusedColor(table, 0);
	}
	out.setColorTable(table);
	return out;
}

QImage ImageImport::reduceColors(const QImage& image)
{
	const QImage src = image.convertToFormat(QImage::Format_ARGB32);
	QImage out = src.convertToFormat(QImage::Format_RGB32)
		.convertToFormat(QImage::Format_Indexed8, Qt::DiffuseDither | Qt::ColorOnly);
	out = opaquePalette(out);

	// the transparent pixels of the source get an index of their own, moved to 0
	std::vector<bool> used(256, false);
	bool transparent = false;
	for (int y = 0; y < out.height(); ++y) {
		const uchar* line = out.constScanLine(y);
		const auto* alpha = reinterpret_cast<const QRgb*>(src.constScanLine(y));
		for (int x = 0; x < out.width(); ++x) {
			if (qAlpha(alpha[x]) == 0) {
				transparent = true;
			} else {
				used[line[x]] = true;
			}
		}
	}
	if (!transparent) {
		return out;
	}
	QVector<QRgb> table = out.colorTable();
	int free_index = -1;
	if (table.size() < 256) {
		free_index = static_cast<int>(table.size());
		table << 0;
	} else {
		for (int i = 0; i < 256 && free_index < 0; ++i) {
			if (!used[i]) {
				free_index = i;
			}
		}
	}
	if (free_index < 0) {
		return out;
	}
	table[free_index] = unusedColor(table, free_index);
	out.setColorTable(table);
	for (int y = 0; y < out.height(); ++y) {
		uchar* line = out.scanLine(y);
		const auto* alpha = reinterpret_cast<const QRgb*>(src.constScanLine(y));
		for (int x = 0; x < out.width(); ++x) {
			if (qAlpha(alpha[x]) == 0) {
				line[x] = static_cast<uchar>(free_index);
			}
		}
	}
	swapIndexes(out, 0, free_index);
	return out;
}

QImage ImageImport::withTransparentIndex0(const QImage& image)
{
	if (image.format() != QImage::Format_Indexed8) {
		return image;
	}
	QImage shown = image.convertToFormat(QImage::Format_ARGB32);
	for (int y = 0; y < image.height(); ++y) {
		const uchar* src = image.constScanLine(y);
		auto* dst = reinterpret_cast<QRgb*>(shown.scanLine(y));
		for (int x = 0; x < image.width(); ++x) {
			dst[x] = src[x] == 0 ? 0 : (dst[x] | 0xff000000u);
		}
	}
	return shown;
}
