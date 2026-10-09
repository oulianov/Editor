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

#include <QScrollArea>
#include <QScrollBar>

/** A page of a window (database...): as wide as it needs, its height scrolled
 *  when the window is lower than the page, so the tallest page does not set
 *  the minimum height of the window */
class PageScrollArea : public QScrollArea {
public:
	explicit PageScrollArea(QWidget* page, QWidget* parent = nullptr) : QScrollArea(parent) {
		setWidget(page);
		setWidgetResizable(true);
		setFrameShape(QFrame::NoFrame);
		setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	}
	QSize sizeHint() const override {
		return widget()->sizeHint() + QSize(verticalScrollBar()->sizeHint().width(), 0);
	}
	QSize minimumSizeHint() const override {
		return {widget()->minimumSizeHint().width() + verticalScrollBar()->sizeHint().width(), 6 * fontMetrics().height()};
	}
};
