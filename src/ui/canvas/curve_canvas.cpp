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

#include "curve_canvas.h"
#include "render/curve_painter.h"

#include <QCursor>
#include <QMouseEvent>
#include <QPainter>
#include <algorithm>

CurveCanvas::CurveCanvas(QQuickItem* parent) : QQuickPaintedItem(parent) {
	setAcceptedMouseButtons(Qt::NoButton);
}

QList<int> CurveCanvas::values() const {
	return QList<int>(m_values.begin(), m_values.end());
}

void CurveCanvas::setValues(const QList<int>& values) {
	std::vector<int16_t> converted;
	converted.reserve(static_cast<size_t>(values.size()));
	for (int value : values) {
		converted.push_back(static_cast<int16_t>(std::clamp(value, -32768, 32767)));
	}
	if (converted == m_values) {
		return;
	}
	m_values = std::move(converted);
	update();
	emit valuesChanged();
}

void CurveCanvas::setCurve(int curve) {
	if (m_curve != curve) {
		m_curve = curve;
		update();
		emit curveChanged();
	}
}

void CurveCanvas::setLevels(int levels) {
	if (m_levels != levels) {
		m_levels = levels;
		update();
		emit levelsChanged();
	}
}

void CurveCanvas::setMinimum(int minimum) {
	if (m_minimum != minimum) {
		m_minimum = minimum;
		emit minimumChanged();
	}
}

void CurveCanvas::setMaximum(int maximum) {
	if (m_maximum != maximum) {
		m_maximum = maximum;
		update();
		emit maximumChanged();
	}
}

void CurveCanvas::setBars(bool bars) {
	if (m_bars != bars) {
		m_bars = bars;
		update();
		emit barsChanged();
	}
}

void CurveCanvas::setEditable(bool editable) {
	if (m_editable != editable) {
		m_editable = editable;
		setAcceptedMouseButtons(editable ? Qt::LeftButton : Qt::NoButton);
		if (editable) {
			setCursor(Qt::CrossCursor);
		} else {
			unsetCursor();
		}
		emit editableChanged();
	}
}

void CurveCanvas::setCurrentLevel(int level) {
	if (m_currentLevel != level) {
		m_currentLevel = level;
		update();
		emit currentLevelChanged();
	}
}

void CurveCanvas::paint(QPainter* painter) {
	const QRectF area(0, 0, width(), height());
	const QColor color = ParameterCurves::color(m_curve);
	if (m_bars) {
		CurvePainter::paintBars(*painter, area, m_values, m_levels, m_maximum, color, m_currentLevel);
	} else {
		CurvePainter::paintArea(*painter, area, m_values, m_levels, m_maximum, color);
	}
}

QString CurveCanvas::automationText() const {
	if (m_values.empty()) {
		return {};
	}
	const size_t last = std::min(m_values.size(), static_cast<size_t>(std::max(m_levels, 1))) - 1;
	return QStringLiteral("level 1: %1, level %2: %3").arg(m_values.front()).arg(last + 1).arg(m_values[last]);
}

void CurveCanvas::mousePressEvent(QMouseEvent* event) {
	if (!m_editable || event->button() != Qt::LeftButton) {
		event->ignore();
		return;
	}
	editAt(event->position(), false);
}

void CurveCanvas::mouseMoveEvent(QMouseEvent* event) {
	if (m_editable && (event->buttons() & Qt::LeftButton)) {
		editAt(event->position(), true);
	}
}

void CurveCanvas::editAt(const QPointF& pos, bool drag) {
	m_stroke.setCurve(&m_values, m_levels, m_minimum, m_maximum);
	const int level = m_stroke.editAt(pos.toPoint(), QSize(qRound(width()), qRound(height())), drag);
	m_stroke.setCurve(nullptr, 0, m_minimum, m_maximum);
	if (level <= 0) {
		return;
	}
	update();
	emit valuesChanged();
	emit edited(level);
}
