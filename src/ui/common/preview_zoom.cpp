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

#include "preview_zoom.h"

#include <QGraphicsView>
#include <QNativeGestureEvent>
#include <QPushButton>
#include <algorithm>
#include <cmath>

PreviewZoom::PreviewZoom(QGraphicsView* view, QPushButton* zoom_in, QPushButton* zoom_out) :
	QObject(view), m_view(view), m_zoomIn(zoom_in), m_zoomOut(zoom_out)
{
	connect(m_zoomIn, &QPushButton::clicked, this, &PreviewZoom::zoomIn);
	connect(m_zoomOut, &QPushButton::clicked, this, &PreviewZoom::zoomOut);
	// the pinch keeps the pixel under the fingers in place; the buttons,
	// away from the view, keep its centre
	m_view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
	m_view->viewport()->installEventFilter(this);
	setZoom(1.0);
}

void PreviewZoom::setZoom(double zoom) {
	zoom = std::clamp(zoom, minimum, maximum);
	m_view->scale(zoom / m_zoom, zoom / m_zoom);
	m_zoom = zoom;
	m_zoomIn->setEnabled(m_zoom < maximum);
	m_zoomOut->setEnabled(m_zoom > minimum);
}

// a level just reached by a pinch counts as reached (x1.999 goes to x2, not x4)
void PreviewZoom::zoomIn() {
	setZoom(std::exp2(std::floor(std::log2(m_zoom) + 1e-6) + 1));
}

void PreviewZoom::zoomOut() {
	setZoom(std::exp2(std::ceil(std::log2(m_zoom) - 1e-6) - 1));
}

bool PreviewZoom::eventFilter(QObject* watched, QEvent* event) {
	if (event->type() == QEvent::NativeGesture) {
		auto* gesture = static_cast<QNativeGestureEvent*>(event);
		if (gesture->gestureType() == Qt::ZoomNativeGesture) {
			// value: how much the fingers spread since the last event
			setZoom(m_zoom * (1.0 + gesture->value()));
			return true;
		}
	}
	return QObject::eventFilter(watched, event);
}
