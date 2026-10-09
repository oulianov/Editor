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

#include "map_cursors.h"
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QPixmap>

namespace {
	// Black shapes outlined in white: readable on every tile
	QCursor makeCursor(QPainterPath shape, QPoint hotspot) {
		// overlapping parts stay filled
		shape.setFillRule(Qt::WindingFill);
		constexpr int size = 32;
		constexpr qreal ratio = 2; // sharp on high density screens
		QPixmap pixmap(size * ratio, size * ratio);
		pixmap.setDevicePixelRatio(ratio);
		pixmap.fill(Qt::transparent);
		QPainter painter(&pixmap);
		painter.setRenderHint(QPainter::Antialiasing);
		painter.strokePath(shape, QPen(Qt::white, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
		painter.fillPath(shape, Qt::black);
		painter.end();
		return QCursor(pixmap, hotspot.x(), hotspot.y());
	}

	// A line as an area, to be filled with the other shapes
	void addLine(QPainterPath& path, QPointF from, QPointF to, qreal width) {
		QPainterPath line(from);
		line.lineTo(to);
		QPainterPathStroker stroker;
		stroker.setWidth(width);
		path.addPath(stroker.createStroke(line));
	}

	void addOutline(QPainterPath& path, const QPainterPath& shape, qreal width) {
		QPainterPathStroker stroker;
		stroker.setWidth(width);
		path.addPath(stroker.createStroke(shape));
	}

	// The arrow of the rectangle and ellipse tools
	QPainterPath arrow() {
		QPainterPath path;
		path.addPolygon(QPolygonF({{1, 1}, {1, 18}, {5.5, 14}, {9, 21}, {12, 19.5}, {8.5, 13}, {14.5, 13}, {1, 1}}));
		return path;
	}

	QCursor pencil() {
		QPainterPath path;
		path.addPolygon(QPolygonF({{2, 30}, {4.5, 22}, {22, 4.5}, {27.5, 10}, {10, 27.5}, {2, 30}}));
		return makeCursor(path, {2, 30});
	}

	QCursor bucket() {
		QPainterPath path;
		path.addPolygon(QPolygonF({{9, 15}, {18, 6}, {28, 16}, {19, 25}, {9, 15}}));
		addLine(path, {9, 15}, {4, 22}, 2.5);
		path.addEllipse(QPointF(4, 25), 2.5, 3);
		return makeCursor(path, {4, 28});
	}

	QCursor magnifier() {
		QPainterPath path, ring;
		ring.addEllipse(QPointF(12, 12), 8, 8);
		addOutline(path, ring, 3);
		addLine(path, {18, 18}, {28, 28}, 5);
		return makeCursor(path, {12, 12});
	}

	QCursor arrowWith(const QPainterPath& shape) {
		QPainterPath path = arrow();
		addOutline(path, shape, 2.5);
		return makeCursor(path, {1, 1});
	}

	QCursor crosshair() {
		QPainterPath path;
		addLine(path, {16, 2}, {16, 12}, 2);
		addLine(path, {16, 20}, {16, 30}, 2);
		addLine(path, {2, 16}, {12, 16}, 2);
		addLine(path, {20, 16}, {30, 16}, 2);
		return makeCursor(path, {16, 16});
	}
}

QCursor MapCursors::forTool(Core::Tool tool) {
	switch (tool) {
		case Core::PENCIL: {
			static const QCursor cursor = pencil();
			return cursor;
		}
		case Core::RECTANGLE: {
			QPainterPath rect;
			rect.addRect(17, 17, 12, 12);
			static const QCursor cursor = arrowWith(rect);
			return cursor;
		}
		case Core::CIRCLE: {
			QPainterPath ellipse;
			ellipse.addEllipse(QPointF(23, 23), 6.5, 6.5);
			static const QCursor cursor = arrowWith(ellipse);
			return cursor;
		}
		case Core::FILL: {
			static const QCursor cursor = bucket();
			return cursor;
		}
		case Core::ZOOM: {
			static const QCursor cursor = magnifier();
			return cursor;
		}
		case Core::SELECT: {
			static const QCursor cursor = crosshair();
			return cursor;
		}
	}
	return QCursor();
}

QCursor MapCursors::move() {
	static const QCursor cursor = [] {
		QPainterPath path;
		addLine(path, {16, 5}, {16, 27}, 2);
		addLine(path, {5, 16}, {27, 16}, 2);
		path.addPolygon(QPolygonF({{16, 1}, {11, 7}, {21, 7}, {16, 1}}));
		path.addPolygon(QPolygonF({{16, 31}, {11, 25}, {21, 25}, {16, 31}}));
		path.addPolygon(QPolygonF({{1, 16}, {7, 11}, {7, 21}, {1, 16}}));
		path.addPolygon(QPolygonF({{31, 16}, {25, 11}, {25, 21}, {31, 16}}));
		return makeCursor(path, {16, 16});
	}();
	return cursor;
}
