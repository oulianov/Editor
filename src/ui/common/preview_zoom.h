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

#include <QWidget>

class QGraphicsView;
class QHBoxLayout;
class QPushButton;

/**
 * The zoom of an image preview (resource pickers, Resource Manager, image
 * import): the Zoom Out and Zoom In buttons, together, and the trackpad pinch
 * on the view. The zoom is on top of the view's own scale (faces are shown
 * twice as big). The buttons go to the next whole-pixel level (x1, x2,
 * x4...), sharp for pixel art; a pinch zooms freely, under the fingers.
 */
class PreviewZoom : public QWidget
{
	Q_OBJECT

public:
	static constexpr double minimum = 1.0;
	static constexpr double maximum = 16.0;

	PreviewZoom(QGraphicsView* view, QWidget* parent);

	/**
	 * The row under a preview, the same in every dialog: what the file is
	 * (info) on the left, the zoom buttons on the right, both at the top
	 */
	static QHBoxLayout* infoRow(QWidget* info, PreviewZoom* zoom);

	double zoom() const { return m_zoom; }
	/** Zooms to zoom (within minimum and maximum) */
	void setZoom(double zoom);
	/** The next whole-pixel level up or down */
	void zoomIn();
	void zoomOut();

protected:
	bool eventFilter(QObject* watched, QEvent* event) override;

private:
	QGraphicsView* m_view;
	QPushButton* m_zoomIn;
	QPushButton* m_zoomOut;
	double m_zoom = 1.0;
};
