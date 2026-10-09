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
#include <QPainter>
#include <QWidget>
#include <vector>

/** RPG Maker's "Sample" of the tint and flash commands: bands of colors */
class ColorSample : public QWidget
{
public:
	explicit ColorSample(QWidget* parent = nullptr) : QWidget(parent) {
		setObjectName(QStringLiteral("colorSample"));
		setMinimumSize(48, 48);
	}

	void setColors(std::vector<QColor> colors) {
		m_colors = std::move(colors);
		update();
	}

protected:
	void paintEvent(QPaintEvent*) override {
		QPainter painter(this);
		const int count = std::max<int>(1, static_cast<int>(m_colors.size()));
		for (int i = 0; i < static_cast<int>(m_colors.size()); ++i) {
			const int top = height() * i / count;
			painter.fillRect(0, top, width(), height() * (i + 1) / count - top, m_colors[i]);
		}
		painter.setPen(palette().color(QPalette::Mid));
		painter.drawRect(rect().adjusted(0, 0, -1, -1));
	}

private:
	std::vector<QColor> m_colors;
};
