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
#include <QPainterPath>
#include <QPolygonF>
#include <utility>

QSize ChipsetEditorPainter::size(const ChipsetEditor& editor) {
	const int rows = (editor.tileCount() + ChipsetEditor::Columns - 1) / ChipsetEditor::Columns;
	return {ChipsetEditor::Columns * Cell, rows * Cell};
}

void ChipsetEditorPainter::paint(QPainter& painter, const ChipsetEditor& editor, const QPixmap& graphic) {
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

	for (int i = 0; i < count; ++i) {
		const QRect target((i % ChipsetEditor::Columns) * Cell, (i / ChipsetEditor::Columns) * Cell, Cell, Cell);
		if (!graphic.isNull()) {
			const QPoint pos = ChipsetEditor::tilePosition(lower, i);
			painter.drawPixmap(target, graphic, QRect(pos.x() * size, pos.y() * size, size, size));
		}
		// the outline only: the dots of Directions leave their brush set
		painter.setPen(QColor(0, 0, 0, 60));
		painter.setBrush(Qt::NoBrush);
		painter.drawRect(target.adjusted(0, 0, -1, -1));

		// marks with a dark outline, readable on any tile
		auto mark = [&](const QString& text, QColor color) {
			QPainterPath path;
			const QRect bounds = painter.fontMetrics().boundingRect(text);
			path.addText(target.center().x() - bounds.width() / 2.0, target.center().y() + bounds.height() / 3.0, font, text);
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
					drawDirection(painter, target.topLeft() + at, side, flags & side,
						i == hover.tile && side == hover.side);
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
	painter.restore();
}

void ChipsetEditorPainter::drawDirection(QPainter& painter, QPoint at, int side, bool open, bool hovered) {
	// pointing up; on pixel centers, for a crisp outline
	static const QPolygonF arrow({{8, 0.5}, {14.5, 7.5}, {10.5, 7.5}, {10.5, 12.5},
		{5.5, 12.5}, {5.5, 7.5}, {1.5, 7.5}});
	painter.save();
	painter.translate(at);
	painter.setPen(QPen(hovered ? QColor(128, 0, 0) : QColor(Qt::black), 1));
	painter.setBrush(hovered ? QColor(255, 255, 0) : QColor(Qt::white));
	if (open) {
		const int angle = side == ChipsetEditor::Right ? 90 : side == ChipsetEditor::Down ? 180
			: side == ChipsetEditor::Left ? 270 : 0;
		painter.translate(8, 8);
		painter.rotate(angle);
		painter.translate(-8, -8);
		painter.drawPolygon(arrow);
	} else {
		painter.drawRect(QRectF(5.5, 5.5, 5, 5));
	}
	painter.restore();
}
