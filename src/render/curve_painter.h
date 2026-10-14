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
#include <QRectF>
#include <cstdint>
#include <vector>

class QPainter;

/**
 * The graphs of the parameter curves of heroes and classes, on black as in
 * RPG Maker: the small filled curves of the database pages, and the bars of
 * the Parameter Progression window (a bar per level). The QML canvases
 * (CurveCanvas) draw them with these.
 */
namespace CurvePainter
{
	/**
	 * A curve filled down to the bottom of area, from level 1 at the left
	 * to max_level at the right; max_value at the top
	 */
	void paintArea(QPainter& painter, const QRectF& area, const std::vector<int16_t>& values,
		double max_level, double max_value, const QColor& color);

	/**
	 * The first levels values as bars, maximum at the top, with a line every
	 * 10 levels and every quarter; the bar of current_level (1 to levels)
	 * lighter and framed
	 */
	void paintBars(QPainter& painter, const QRectF& area, const std::vector<int16_t>& values,
		int levels, int maximum, const QColor& color, int current_level);
}
