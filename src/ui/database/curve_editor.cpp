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

#include "curve_editor.h"

#include <QMouseEvent>
#include <QPainter>
#include <algorithm>
#include <cmath>

CurveEditor::CurveEditor(QWidget* parent) :
	QWidget(parent)
{
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	setCursor(Qt::CrossCursor);
	setToolTip(tr("Click or drag to set the values of the levels"));
}

void CurveEditor::setCurve(std::vector<int16_t>* values, int levels, int minimum, int maximum, QColor color) {
	m_values = values;
	m_levels = std::min(levels, values ? static_cast<int>(values->size()) : 0);
	m_minimum = minimum;
	m_maximum = std::max(maximum, 1);
	m_color = color;
	m_level = std::clamp(m_level, 1, std::max(m_levels, 1));
	update();
}

void CurveEditor::setCurrentLevel(int level) {
	m_level = std::clamp(level, 1, std::max(m_levels, 1));
	update();
}

QSize CurveEditor::sizeHint() const {
	return {420, 300};
}

QSize CurveEditor::minimumSizeHint() const {
	return {200, 120};
}

void CurveEditor::paintEvent(QPaintEvent*) {
	QPainter painter(this);
	const QRectF area = rect();
	painter.fillRect(area, Qt::black);
	if (!m_values || m_levels <= 0) {
		return;
	}

	// a line every 10 levels and every quarter of the maximum
	painter.setPen(QColor(64, 64, 64));
	const qreal step = area.width() / m_levels;
	for (int level = 10; level < m_levels; level += 10) {
		painter.drawLine(QPointF(level * step, 0), QPointF(level * step, area.height()));
	}
	for (int quarter = 1; quarter < 4; ++quarter) {
		const qreal y = area.height() * quarter / 4;
		painter.drawLine(QPointF(0, y), QPointF(area.width(), y));
	}

	// bars with a gap when they are wide enough to tell them apart
	const qreal gap = step >= 4 ? 1 : 0;
	for (int i = 0; i < m_levels; ++i) {
		const qreal height = area.height() * std::clamp<int>((*m_values)[static_cast<size_t>(i)], 0, m_maximum) / m_maximum;
		const QRectF bar(i * step, area.height() - height, step - gap, height);
		painter.fillRect(bar, i + 1 == m_level ? m_color.lighter(160) : m_color);
	}
	// the current level, also when its value is low
	painter.setPen(Qt::white);
	painter.drawRect(QRectF((m_level - 1) * step, 0, step - gap, area.height() - 1));
}

void CurveEditor::mousePressEvent(QMouseEvent* event) {
	if (event->button() == Qt::LeftButton) {
		editAt(event->position().toPoint(), false);
	}
}

void CurveEditor::mouseMoveEvent(QMouseEvent* event) {
	if (event->buttons() & Qt::LeftButton) {
		editAt(event->position().toPoint(), true);
	}
}

void CurveEditor::editAt(const QPoint& pos, bool drag) {
	if (!m_values || m_levels <= 0 || width() <= 0 || height() <= 0) {
		return;
	}
	const int level = std::clamp(pos.x() * m_levels / width() + 1, 1, m_levels);
	const int value = std::clamp(static_cast<int>(std::lround(static_cast<double>(height() - pos.y()) * m_maximum / height())),
		m_minimum, m_maximum);
	// a fast drag skips levels: fill them on the line from the last point
	const int from = drag ? m_lastLevel : level;
	const int first = std::min(from, level);
	const int last = std::max(from, level);
	for (int l = first; l <= last; ++l) {
		int v = value;
		if (last > first) {
			const double t = static_cast<double>(l - from) / (level - from);
			v = static_cast<int>(std::lround(m_lastValue + (value - m_lastValue) * t));
		}
		(*m_values)[static_cast<size_t>(l) - 1] = static_cast<int16_t>(v);
	}
	m_lastLevel = level;
	m_lastValue = value;
	m_level = level;
	update();
	emit edited(level);
}
