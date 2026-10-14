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

#include "curve_painter.h"

#include <QPainter>
#include <QPolygonF>
#include <algorithm>

void CurvePainter::paintArea(QPainter& painter, const QRectF& area, const std::vector<int16_t>& values,
		double max_level, double max_value, const QColor& color) {
	painter.fillRect(area, Qt::black);
	if (values.empty() || max_level <= 1 || max_value <= 0) {
		return;
	}
	QPolygonF points;
	points.append(area.bottomLeft());
	for (size_t i = 0; i < values.size(); ++i) {
		points.append(QPointF(area.left() + static_cast<double>(i) * area.width() / (max_level - 1.0),
			area.bottom() - area.height() * static_cast<double>(values[i]) / max_value));
	}
	points.append(area.bottomRight());

	painter.save();
	painter.setBrush(color);
	painter.setPen(Qt::NoPen);
	painter.drawPolygon(points);
	painter.setPen(QPen(color.lighter()));
	painter.drawPoints(points);
	painter.restore();
}

void CurvePainter::paintBars(QPainter& painter, const QRectF& area, const std::vector<int16_t>& values,
		int levels, int maximum, const QColor& color, int current_level) {
	painter.fillRect(area, Qt::black);
	levels = std::min(levels, static_cast<int>(values.size()));
	if (levels <= 0 || maximum <= 0) {
		return;
	}
	painter.save();
	// a line every 10 levels and every quarter of the maximum
	painter.setPen(QColor(64, 64, 64));
	const qreal step = area.width() / levels;
	for (int level = 10; level < levels; level += 10) {
		painter.drawLine(QPointF(area.left() + level * step, area.top()), QPointF(area.left() + level * step, area.bottom()));
	}
	for (int quarter = 1; quarter < 4; ++quarter) {
		const qreal y = area.top() + area.height() * quarter / 4;
		painter.drawLine(QPointF(area.left(), y), QPointF(area.right(), y));
	}

	// bars with a gap when they are wide enough to tell them apart
	const qreal gap = step >= 4 ? 1 : 0;
	for (int i = 0; i < levels; ++i) {
		const qreal height = area.height() * std::clamp<int>(values[static_cast<size_t>(i)], 0, maximum) / maximum;
		const QRectF bar(area.left() + i * step, area.bottom() - height, step - gap, height);
		painter.fillRect(bar, i + 1 == current_level ? color.lighter(160) : color);
	}
	// the current level, also when its value is low
	if (current_level >= 1 && current_level <= levels) {
		painter.setPen(Qt::white);
		painter.setBrush(Qt::NoBrush);
		painter.drawRect(QRectF(area.left() + (current_level - 1) * step, area.top(), step - gap, area.height() - 1));
	}
	painter.restore();
}
