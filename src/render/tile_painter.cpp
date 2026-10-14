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

#include "tile_painter.h"
#include <QFont>
#include <QImage>
#include <QList>
#include <QPainterPath>
#include <QPen>
#include <algorithm>
#include "model/autotile.h"

TilePainter::TilePainter(std::shared_ptr<const ChipsetTiles> tiles)
{
	setTiles(std::move(tiles));
}

const std::shared_ptr<const ChipsetTiles>& TilePainter::tiles() const
{
	return m_tiles;
}

void TilePainter::setTiles(std::shared_ptr<const ChipsetTiles> tiles)
{
	// no chipset: no tiles, never a null pointer
	m_tiles = tiles ? std::move(tiles) : std::make_shared<const ChipsetTiles>();
}

const QPixmap& TilePainter::background() const
{
	return m_background;
}

const QTransform& TilePainter::backgroundTransform() const
{
	return m_backgroundTransform;
}

void TilePainter::setBackground(const QPixmap& background, const QTransform& transform)
{
	m_background = background;
	m_backgroundTransform = transform;
}

int TilePainter::waterFrame() const
{
	return m_waterFrame;
}

int TilePainter::animatedTileFrame() const
{
	return m_animatedTileFrame;
}

void TilePainter::setFrames(int water_frame, int animated_tile_frame)
{
	m_waterFrame = water_frame;
	m_animatedTileFrame = animated_tile_frame;
}

QPixmap TilePainter::paletteTiles(EditorState::Layer layer, int size)
{
	const bool lower = layer == EditorState::LOWER;
	const int count = lower ? 162 : 144;
	QPixmap tiles(6 * size, (count / 6) * size);
	tiles.fill(Qt::transparent);
	begin(tiles);
	for (int i = 0; i < count; i++) {
		const QRect rect((i % 6) * size, (i / 6) * size, size, size);
		renderTile(lower ? Autotile::translate(i, 15) : Autotile::translate(i + 162), rect);
	}
	end();
	return tiles;
}

void TilePainter::begin(QPixmap &dest)
{
	m_painter.begin(&dest);
	if (m_painter.isActive()) {
		QBrush panorama(m_background);
		panorama.setTransform(m_backgroundTransform);
		m_painter.setBackground(panorama);
	}
	m_painter.setPen(Qt::yellow);
}

void TilePainter::renderTile(short tile_id, const QRect &dest_rect)
{
	if (tile_id < 10000) {
		// the panorama, at the place and zoom of the map (setBackground)
		QBrush panorama(m_background);
		panorama.setTransform(m_backgroundTransform);
		m_painter.fillRect(dest_rect, panorama);
	}
	const int frame = !Autotile::isAnimatedTile(tile_id) ? 0 : tile_id < 3000 ? m_waterFrame : m_animatedTileFrame;
	m_painter.drawPixmap(dest_rect, m_tiles->tile(tile_id, frame));
}

QRect TilePainter::renderEventFrame(const QRect &dest_rect)
{
	// the middle of the cell, in a thin frame of the pen's color: filled
	// lines of a pixel, the same on pixmaps of any resolution
	const int fact = dest_rect.width();
	QRect final_rect = dest_rect.adjusted(fact/6,fact/6,-fact/6,-fact/6);
	const QRect frame = final_rect.adjusted(-1,-1,1,1);
	const QColor color = m_painter.pen().color();
	m_painter.fillRect(QRect(frame.left(), frame.top(), frame.width(), 1), color);
	m_painter.fillRect(QRect(frame.left(), frame.bottom(), frame.width(), 1), color);
	m_painter.fillRect(QRect(frame.left(), frame.top(), 1, frame.height()), color);
	m_painter.fillRect(QRect(frame.right(), frame.top(), 1, frame.height()), color);
	return final_rect;
}

void TilePainter::renderEvent(const lcf::rpg::Event& event, const QRect &dest_rect, const QPixmap* character)
{
	if (event.pages.empty())
		return;

	const QRect final_rect = renderEventFrame(dest_rect);
	if (event.pages[0].character_name.empty())
		renderTile(static_cast<short>(event.pages[0].character_index+10000), final_rect);
	else {
		if (!character)
			return;
		m_painter.drawPixmap(final_rect, *character, QRect(0,6,24,24));
	}
}

void TilePainter::renderMarker(StartMarker marker, const QString& letter, const QRect &dest_rect)
{
	const QColor black(0, 0, 0);
	const QColor white(255, 255, 255);
	const QColor cyan(128, 255, 255);
	const QColor ink(40, 0, 80);
	const QColor badge(64, 0, 128);
	const QColor yellow(255, 255, 0);
	// the two ends of each gradient, bottom left to top right: light enough
	// for the dark drawing
	static const QColor ends[4][2] = {
		{QColor(72, 144, 248), QColor(240, 80, 72)},
		{QColor(248, 144, 40), QColor(72, 144, 248)},
		{QColor(240, 80, 72), QColor(80, 200, 96)},
		{QColor(72, 144, 248), QColor(248, 216, 48)},
	};

	// framed like an event, the marker taking the place of its graphic
	const QRect area = renderEventFrame(dest_rect);
	if (area.isEmpty())
		return;
	const qreal dpr = m_painter.device()->devicePixelRatioF();
	// the drawing is pixel art on a grid of 22 units (a pixel each in cells
	// of 32 pixels): a border of black, light and black lines around a field
	// of 16, from
	// cells of 16 pixels; smaller cells have the gradient alone, a unit per
	// pixel
	const bool detailed = area.width() >= 11;
	const int units = detailed ? 22 : area.width();
	const int border = detailed ? 3 : 0;

	// a diagonal gradient as pixel art: each unit takes one of the two
	// colors, after a 4x4 ordered (Bayer) dither
	static const int bayer[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
	QImage image(units, units, QImage::Format_ARGB32_Premultiplied);
	image.fill(black);
	if (detailed) {
		QPainter painter(&image);
		painter.fillRect(QRect(1, 1, units - 2, units - 2), cyan);
		painter.fillRect(QRect(2, 2, units - 4, units - 4), black);
	}
	const int field = units - 2 * border;
	const int steps = std::max(1, 2 * field - 2);
	for (int y = 0; y < field; y++)
		for (int x = 0; x < field; x++) {
			const int level = 16 * (x + field - 1 - y) / steps;
			image.setPixelColor(border + x, border + y, ends[marker][level > bayer[y & 3][x & 3] ? 1 : 0]);
		}

	if (detailed) {
		// in units of the field, in its middle
		QList<QRect> shape;
		switch (marker) {
		case PARTY:
			// a hero holding a staff
			shape = {{4, 1, 1, 14}, {3, 1, 3, 2}, {8, 1, 3, 3}, {9, 4, 1, 1}, {7, 5, 5, 4},
				{5, 6, 2, 1}, {12, 6, 1, 3}, {7, 9, 2, 5}, {10, 9, 2, 5}, {6, 14, 3, 1}, {10, 14, 3, 1}};
			break;
		case BOAT:
			// two crossed oars over a low hull
			shape = {{1, 2, 2, 2}, {3, 4, 1, 1}, {4, 5, 1, 1}, {5, 6, 1, 1}, {6, 7, 1, 1}, {7, 8, 1, 1}, {8, 9, 1, 1},
				{13, 2, 2, 2}, {12, 4, 1, 1}, {11, 5, 1, 1}, {10, 6, 1, 1}, {9, 7, 1, 1}, {8, 8, 1, 1}, {7, 9, 1, 1},
				{0, 9, 1, 1}, {15, 9, 1, 1}, {0, 10, 16, 1}, {1, 11, 14, 1}, {3, 12, 10, 1}};
			break;
		case SHIP:
			// a mast with two sails, on a tall hull
			shape = {{7, 2, 1, 7}, {8, 2, 2, 1},
				{9, 3, 2, 1}, {9, 4, 3, 1}, {9, 5, 4, 1}, {9, 6, 5, 1}, {9, 7, 6, 1},
				{5, 4, 1, 1}, {4, 5, 2, 1}, {3, 6, 3, 1}, {2, 7, 4, 1},
				{0, 9, 16, 1}, {1, 10, 14, 1}, {2, 11, 12, 1}, {3, 12, 10, 1}};
			break;
		case AIRSHIP:
			// a balloon with a tail, its gondola hanging below
			shape = {{4, 2, 7, 1}, {2, 3, 11, 1}, {1, 4, 13, 3}, {2, 7, 11, 1}, {4, 8, 7, 1},
				{14, 4, 1, 3}, {15, 3, 1, 5}, {4, 9, 1, 2}, {10, 9, 1, 2}, {3, 11, 9, 1}, {4, 12, 7, 1}};
			break;
		}
		QPainter painter(&image);
		painter.translate(border, border);
		for (const QRect& part : shape)
			painter.fillRect(part, ink);
	}

	m_painter.save();
	// the badges may pass the event's frame, not the cell
	m_painter.setClipRect(dest_rect);
	// square pixels, unless the cell has fewer pixels than the drawing
	m_painter.setRenderHint(QPainter::SmoothPixmapTransform, area.width() * dpr < units);
	m_painter.drawImage(area, image);
	if (!detailed) {
		// the lines of the border, of at least a pixel of the screen each;
		// the light one alone when there is no room for three
		const qreal width = std::max(1 / dpr, area.width() / 22.0);
		const bool three = area.width() * dpr >= 12;
		auto ring = [&](qreal inset, const QColor& color) {
			QPainterPath path;
			path.addRect(QRectF(area).adjusted(inset, inset, -inset, -inset));
			path.addRect(QRectF(area).adjusted(inset + width, inset + width, -inset - width, -inset - width));
			m_painter.fillPath(path, color);
		};
		if (three) {
			ring(0, black);
			ring(2 * width, black);
		}
		ring(three ? width : 0, cyan);
	}

	// a text as large as its box allows (a box in units of the drawing),
	// with a dark outline of a unit of the 22 unit grid, at least a pixel of
	// the screen
	const qreal unit = static_cast<qreal>(area.width()) / units;
	auto drawText = [&](const QString& text, const QRectF& units_box, const QColor& color, const QColor& outline) {
		QFont font = m_painter.font();
		font.setPixelSize(64);
		font.setBold(true);
		QPainterPath path;
		path.addText(0, 0, font, text);
		// the outlines of a glyph can overlap
		path.setFillRule(Qt::WindingFill);
		const QRectF bounds = path.boundingRect();
		if (bounds.isEmpty())
			return;
		const QRectF box(area.x() + units_box.x() * unit, area.y() + units_box.y() * unit,
			units_box.width() * unit, units_box.height() * unit);
		const qreal scale = std::min(box.width() / bounds.width(), box.height() / bounds.height());
		m_painter.save();
		m_painter.setRenderHint(QPainter::Antialiasing, true);
		m_painter.translate(box.center());
		m_painter.scale(scale, scale);
		m_painter.translate(-bounds.center());
		if (outline.isValid()) {
			const qreal width = std::max(1 / dpr, area.width() / 22.0);
			m_painter.strokePath(path, QPen(outline, 2 * width / scale, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
		}
		m_painter.fillPath(path, color);
		m_painter.restore();
	};
	if (detailed) {
		// on the border, astride the middle of its bottom corners: the
		// vehicles' number at the left, the badge of the letter at the right
		if (marker != PARTY)
			drawText(QString::number(static_cast<int>(marker)), QRectF(-1.5, 17, 6, 7), yellow, black);
		m_painter.save();
		m_painter.translate(area.topLeft());
		m_painter.scale(unit, unit);
		m_painter.fillRect(QRect(17, 16, 8, 9), white);
		m_painter.fillRect(QRect(18, 17, 6, 7), badge);
		m_painter.restore();
		drawText(letter, QRectF(19, 18, 4, 5), cyan, QColor());
	} else if (area.width() * dpr >= 12) {
		// the letter alone, when the screen has the pixels for it
		drawText(letter, QRectF(units / 5.0, units / 5.0, units * 3 / 5.0, units * 3 / 5.0), white, black);
	}
	m_painter.restore();
}

void TilePainter::end()
{
	m_painter.end();
}

QBrush TilePainter::transparencyBrush()
{
	static const QBrush brush = [] {
		QPixmap squares(16, 16);
		squares.fill(QColor(0x80, 0x80, 0x80));
		QPainter p(&squares);
		p.fillRect(0, 0, 8, 8, QColor(0x66, 0x66, 0x66));
		p.fillRect(8, 8, 8, 8, QColor(0x66, 0x66, 0x66));
		return QBrush(squares);
	}();
	return brush;
}

QPixmap TilePainter::eraserRow()
{
	// transparent cells crossed out in red: the tile is removed
	QPixmap row(192, 32);
	QPainter p(&row);
	p.fillRect(row.rect(), transparencyBrush());
	p.setRenderHint(QPainter::Antialiasing);
	p.setPen(QPen(QColor(0xd0, 0x30, 0x30), 2));
	for (int x = 0; x < 192; x += 32) {
		p.drawLine(x + 6, 26, x + 26, 6);
		p.drawRect(QRectF(x + 0.5, 0.5, 31, 31));
	}
	return row;
}

QPixmap TilePainter::createDummyPixmap(int width, int height)
{
	QPixmap dummy(width, height);
	QPainter p(&dummy);
	p.fillRect(0, 0, width, height, transparencyBrush());
	p.end();
	return dummy;
}
