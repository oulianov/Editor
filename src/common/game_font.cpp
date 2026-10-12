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

#include "game_font.h"
#include "bitmapfont/bitmapfont_rmg2000.h"
#include "bitmapfont/bitmapfont_ttyp0.h"
#include "bitmapfont/shinonome_gothic.h"
#include "bitmapfont/shinonome_mincho.h"

#include <QFontDatabase>
#include <QHash>
#include <QPainter>
#include <algorithm>

namespace {

enum { Height = 12, HalfWidth = 6 };

template <typename T>
const BitmapFontGlyph* find(const T& glyphs, char32_t code) {
	auto it = std::lower_bound(std::begin(glyphs), std::end(glyphs), code);
	return it != std::end(glyphs) && it->code == code ? &*it : nullptr;
}

/** The chains of font.cpp, without the Korean and Chinese fonts */
const BitmapFontGlyph* gothic(char32_t code) {
	return find(SHINONOME_GOTHIC, code);
}
const BitmapFontGlyph* mincho(char32_t code) {
	auto* glyph = find(SHINONOME_MINCHO, code);
	return glyph ? glyph : gothic(code);
}
const BitmapFontGlyph* ttyp0(char32_t code) {
	auto* glyph = find(BITMAPFONT_TTYP0, code);
	return glyph ? glyph : gothic(code);
}
const BitmapFontGlyph* rmg2000(char32_t code) {
	auto* glyph = find(BITMAPFONT_RMG2000, code);
	if (!glyph) {
		glyph = find(BITMAPFONT_TTYP0, code);
	}
	return glyph ? glyph : mincho(code);
}

QImage renderBitmap(const QString& text, int font, bool cjk, const QColor& color) {
	auto glyphOf = [&](char32_t code) {
		const BitmapFontGlyph* glyph = cjk ? (font ? mincho(code) : gothic(code)) : (font ? rmg2000(code) : ttyp0(code));
		return glyph ? glyph : &BITMAPFONT_REPLACEMENT_GLYPH;
	};
	const QList<uint> codes = text.toUcs4();
	int width = 0;
	for (uint code : codes) {
		width += glyphOf(code)->is_full ? 2 * HalfWidth : HalfWidth;
	}
	QImage image(std::max(width, 1), Height, QImage::Format_ARGB32_Premultiplied);
	image.fill(Qt::transparent);
	const QRgb pixel = color.rgba();
	int x = 0;
	for (uint code : codes) {
		const BitmapFontGlyph* glyph = glyphOf(code);
		const int glyph_width = glyph->is_full ? 2 * HalfWidth : HalfWidth;
		for (int y = 0; y < Height; ++y) {
			for (int dx = 0; dx < glyph_width; ++dx) {
				if (glyph->data[y] & (1 << dx)) {
					image.setPixel(x + dx, y, pixel);
				}
			}
		}
		x += glyph_width;
	}
	return image;
}

} // namespace

QImage GameFont::render(const QString& text, int font, const QString& file, bool cjk, const QColor& color) {
	if (file.isEmpty()) {
		return renderBitmap(text, font, cjk, color);
	}
	// a font file of the game, at the size of the Player (12 pixels), not smoothed
	static QHash<QString, QString> families;
	auto it = families.find(file);
	if (it == families.end()) {
		const QStringList found = QFontDatabase::applicationFontFamilies(QFontDatabase::addApplicationFont(file));
		it = families.insert(file, found.value(0));
	}
	if (it->isEmpty()) {
		return renderBitmap(text, font, cjk, color);
	}
	QFont qfont(*it);
	qfont.setPixelSize(Height);
	qfont.setStyleStrategy(QFont::NoAntialias);
	const QFontMetrics metrics(qfont);
	QImage image(std::max(metrics.horizontalAdvance(text), 1), Height, QImage::Format_ARGB32_Premultiplied);
	image.fill(Qt::transparent);
	QPainter painter(&image);
	painter.setFont(qfont);
	painter.setPen(color);
	painter.drawText(QRect(0, 0, image.width(), Height), Qt::AlignLeft | Qt::AlignVCenter, text);
	return image;
}
