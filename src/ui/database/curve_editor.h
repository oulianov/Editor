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

#pragma once

#include <QColor>
#include <QWidget>
#include <cstdint>
#include <vector>

// The graph of a parameter, one bar per level: clicking or dragging sets the
// values under the pointer, like the bars of RPG Maker's Parameter Progression
class CurveEditor : public QWidget
{
	Q_OBJECT

public:
	explicit CurveEditor(QWidget* parent = nullptr);

	// The first levels values of the vector are shown and edited
	void setCurve(std::vector<int16_t>* values, int levels, int minimum, int maximum, QColor color);

	// 1 to the number of levels
	int currentLevel() const { return m_level; }
	void setCurrentLevel(int level);

	QSize sizeHint() const override;
	QSize minimumSizeHint() const override;

signals:
	// the mouse changed values; the level is the one under the pointer
	void edited(int level);

protected:
	void paintEvent(QPaintEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;

private:
	void editAt(const QPoint& pos, bool drag);

	std::vector<int16_t>* m_values = nullptr;
	int m_levels = 0;
	int m_minimum = 0;
	int m_maximum = 999;
	QColor m_color = Qt::red;
	int m_level = 1;
	int m_lastLevel = 1;
	int m_lastValue = 0;
};
