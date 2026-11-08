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

#include "chipset_editor_painter.h"
#include "model/chipset_editor.h"
#include <QFontMetrics>
#include <QPainterPath>
#include <QPolygonF>
#include <utility>

QSize ChipsetEditorPainter::size(const ChipsetEditor& editor) {
	const int rows = (editor.tileCount() + ChipsetEditor::Columns - 1) / ChipsetEditor::Columns;
	return {ChipsetEditor::Columns * Cell, rows * Cell};
}

void ChipsetEditorPainter::paint(QPainter& painter, const ChipsetEditor& editor, const QPixmap& graphic,
		const QColor& highlight) {
	using Mode = ChipsetEditor::Mode;
	if (!editor.chipset()) {
		return;
	}
	painter.save();
	painter.setRenderHint(QPainter::Antialiasing);
	const bool lower = editor.lowerLayer();
	const int count = editor.tileCount();
	const int size = graphic.isNull() ? 16 : graphic.width() / 30;
	const Mode mode = editor.mode();
	const auto& hover = editor.hover();
	QFont font = painter.font();
	font.setBold(true);
	font.setPixelSize(13);
	painter.setFont(font);
	QFont litFont = font;
	litFont.setPixelSize(16);

	auto cell = [](int i) {
		return QRect((i % ChipsetEditor::Columns) * Cell, (i / ChipsetEditor::Columns) * Cell, Cell, Cell);
	};
	for (int i = 0; i < count; ++i) {
		const QRect target = cell(i);
		const bool lit = i == hover.tile;
		if (!graphic.isNull()) {
			const QPoint pos = ChipsetEditor::tilePosition(lower, i);
			painter.drawPixmap(target, graphic, QRect(pos.x() * size, pos.y() * size, size, size));
		}
		if (lit) {
			painter.fillRect(target, QColor(highlight.red(), highlight.green(), highlight.blue(), 70));
		}
		// the outline only: the dots of Directions leave their brush set
		painter.setPen(QColor(0, 0, 0, 60));
		painter.setBrush(Qt::NoBrush);
		painter.drawRect(target.adjusted(0, 0, -1, -1));

		// marks with a dark outline, readable on any tile; under the
		// pointer, larger and paler in a halo of highlight
		auto mark = [&](const QString& text, QColor color) {
			const QFont& shown = lit ? litFont : font;
			QPainterPath path;
			const QRect bounds = QFontMetrics(shown).boundingRect(text);
			path.addText(target.center().x() - bounds.width() / 2.0, target.center().y() + bounds.height() / 3.0, shown, text);
			if (lit) {
				painter.strokePath(path, QPen(highlight, 6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
				color = QColor((3 * color.red() + 2 * 255) / 5, (3 * color.green() + 2 * 255) / 5, (3 * color.blue() + 2 * 255) / 5);
			}
			painter.strokePath(path, QPen(QColor(0, 0, 0, 200), 3));
			painter.fillPath(path, color);
		};
		const int flags = editor.flags(i);
		switch (mode) {
			case Mode::Terrain:
				if (lower && i < static_cast<int>(editor.chipset()->terrain_data.size())) {
					mark(QString::number(editor.terrain(i)), Qt::white);
				}
				break;
			case Mode::Passability:
				if (flags & ChipsetEditor::Above) {
					mark(QStringLiteral("☆"), QColor(255, 220, 90));
				} else if ((flags & ChipsetEditor::AllDirections) == 0) {
					mark(QStringLiteral("×"), QColor(255, 110, 110));
				} else {
					mark(QStringLiteral("○"), QColor(120, 200, 255));
				}
				break;
			case Mode::Directions: {
				// where RPG Maker 2003 puts them in its tiles of 32 pixels
				static const std::pair<int, QPoint> sides[] = {
					{ChipsetEditor::Down, {8, 16}}, {ChipsetEditor::Left, {0, 8}},
					{ChipsetEditor::Right, {16, 8}}, {ChipsetEditor::Up, {8, 0}}};
				for (const auto& [side, at] : sides) {
					const bool side_lit = lit && side == hover.side;
					drawDirection(painter, target.topLeft() + at, side, flags & side, side_lit,
						side_lit ? highlight : QColor());
				}
				break;
			}
			case Mode::Counter:
				if (flags & ChipsetEditor::Counter) {
					mark(QStringLiteral("□"), QColor(255, 220, 90));
				}
				break;
		}
	}
	// the frame of the tile under the pointer, over the outlines of the
	// tiles around it: dark outside, readable on light tiles too
	if (hover.tile >= 0 && hover.tile < count) {
		const QRectF target = cell(hover.tile);
		painter.setBrush(Qt::NoBrush);
		painter.setPen(QPen(QColor(0, 0, 0, 170), 1));
		painter.drawRect(target.adjusted(0.5, 0.5, -0.5, -0.5));
		painter.setPen(QPen(highlight, 2));
		painter.drawRect(target.adjusted(2, 2, -2, -2));
	}
	painter.restore();
}

void ChipsetEditorPainter::drawDirection(QPainter& painter, QPoint at, int side, bool open, bool hovered,
		const QColor& halo) {
	// pointing up; on pixel centers, for a crisp outline
	static const QPolygonF arrow({{8, 0.5}, {14.5, 7.5}, {10.5, 7.5}, {10.5, 12.5},
		{5.5, 12.5}, {5.5, 7.5}, {1.5, 7.5}});
	static const QRectF dot(5.5, 5.5, 5, 5);
	painter.save();
	painter.translate(at);
	if (open) {
		const int angle = side == ChipsetEditor::Right ? 90 : side == ChipsetEditor::Down ? 180
			: side == ChipsetEditor::Left ? 270 : 0;
		painter.translate(8, 8);
		painter.rotate(angle);
		painter.translate(-8, -8);
	}
	if (halo.isValid()) {
		painter.setPen(QPen(halo, 5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
		painter.setBrush(halo);
		if (open) {
			painter.drawPolygon(arrow);
		} else {
			painter.drawRect(dot);
		}
	}
	painter.setPen(QPen(hovered ? QColor(128, 0, 0) : QColor(Qt::black), 1));
	painter.setBrush(hovered ? QColor(255, 255, 0) : QColor(Qt::white));
	if (open) {
		painter.drawPolygon(arrow);
	} else {
		painter.drawRect(dot);
	}
	painter.restore();
}
