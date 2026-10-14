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

#include <QImage>
#include <QList>
#include <QMetaObject>
#include <QPointer>
#include <QQuickItem>
#include <memory>
#include "model/map_document.h"

/**
 * The panorama of a map, drawn by the scene graph under its lower layer
 * (MapLayerView): from the top-left corner of the map, a pixel of the
 * picture a pixel of the map at the zoom of the tiles, repeated over the
 * map (Panorama). Magenta without a panorama; nothing when its file cannot
 * be read.
 *
 * Only the copies of the picture in sight have nodes. The picture is read
 * once per file (PictureCache) and its texture shared in the window
 * (MapTextureCache).
 */
class MapPanoramaView : public QQuickItem
{
	Q_OBJECT
	QML_ELEMENT
	Q_PROPERTY(MapDocument* document READ document WRITE setDocument NOTIFY documentChanged)
	Q_PROPERTY(qreal zoom READ zoom WRITE setZoom NOTIFY zoomChanged)
	Q_PROPERTY(bool darkened READ darkened WRITE setDarkened NOTIFY darkenedChanged)
	Q_PROPERTY(QString name READ name NOTIFY pictureChanged)
	Q_PROPERTY(bool found READ found NOTIFY pictureChanged)

public:
	explicit MapPanoramaView(QQuickItem* parent = nullptr);
	~MapPanoramaView() override;

	MapDocument* document() const;
	void setDocument(MapDocument* document);

	/** Item pixels per pixel of the map, as MapLayerView's */
	qreal zoom() const;
	void setZoom(qreal zoom);

	/** Drawn at half its brightness, with the lower layer over it
	 *  (MapLayerView::darkened) */
	bool darkened() const;
	void setDarkened(bool darkened);

	/** The panorama of the map, empty when it has none (magenta) */
	QString name() const;
	/** Its picture was read (else nothing is drawn); true without one */
	bool found() const;
	/** The picture shown, null without one */
	QImage picture() const;
	/** Read the picture again (its file or the map's panorama changed) */
	Q_INVOKABLE void reload();

	/** The copies of the picture with nodes (tests) */
	int builtCopyCount() const;

signals:
	void documentChanged();
	void zoomChanged();
	void darkenedChanged();
	void pictureChanged();

protected:
	QSGNode* updatePaintNode(QSGNode* old_node, UpdatePaintNodeData* data) override;
	void itemChange(ItemChange change, const ItemChangeData& value) override;
	void geometryChange(const QRectF& new_geometry, const QRectF& old_geometry) override;

private:
	/** The map's size in pixels of the map, empty without one */
	QSize mapPixels() const;
	void updateSize();
	void watchViewport();

	QPointer<MapDocument> m_doc;
	QList<QMetaObject::Connection> m_docConnections;
	qreal m_zoom = 1;
	QString m_name;
	std::shared_ptr<const QImage> m_picture;
	bool m_darkened = false;
	// m_picture darkened, made once darkened (of the picture whose key is
	// m_darkKey)
	QImage m_darkPicture;
	qint64 m_darkKey = 0;
	// read by updatePaintNode: what to draw changed
	bool m_changed = true;
	int m_builtCopies = 0;
	QList<QMetaObject::Connection> m_viewportConnections;
};
