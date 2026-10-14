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
#include <QObject>
#include <QPointer>
#include <QString>
#include <QWindow>
#include <QtQmlIntegration>
#include "model/resource_library.h"

class QQuickWindow;

/**
 * The windows the Resource Manager shows while it works (Ez.ResourceDialogs),
 * over its window: the image import's choices (256 colors, the Import Image
 * window), and what an operation could not do.
 *
 *   Ez.ResourceDialogs { library: library; window: root }
 */
class ResourceDialogs : public QObject, private ResourceImportChoices
{
	Q_OBJECT
	QML_ELEMENT
	/** The library whose imports ask here */
	Q_PROPERTY(ResourceLibraryModel* library READ library WRITE setLibrary NOTIFY libraryChanged)
	Q_PROPERTY(QWindow* window READ window WRITE setWindow NOTIFY windowChanged)

public:
	explicit ResourceDialogs(QObject* parent = nullptr);
	~ResourceDialogs() override;

	ResourceLibraryModel* library() const;
	void setLibrary(ResourceLibraryModel* library);
	QWindow* window() const;
	void setWindow(QWindow* window);

	/** Tells what an operation could not do (a warning, an information); whether it was done */
	Q_INVOKABLE bool report(const QString& title, const ResourceResult& result);

signals:
	void libraryChanged();
	void windowChanged();

private:
	Colors askColors(const QString& title, const QString& text) override;
	QImage chooseTransparent(const QImage& image, const QString& file_name) override;

	QPointer<ResourceLibraryModel> m_library;
	QPointer<QWindow> m_window;
};

/**
 * The windows of Tools (Resource Manager, Jukebox) and the image import, for
 * the project open, opened over a window. They open as large as last time.
 */
namespace ResourceWindows {
	/** Tools > Resource Manager: modal, returns once closed */
	void resourceManager(QWindow* over);
	/** Tools > Jukebox: one window, not modal, which keeps its list and
	 *  settings between openings; when open, shown again with its lists read again */
	QQuickWindow* jukebox(QWindow* over);
	/** Closes the Jukebox (its project is closed) */
	void closeJukebox();
	/** The Import Image window (modal): the image with its transparent color, null when cancelled */
	QImage chooseTransparent(QWindow* over, const QImage& image, const QString& title = QString());
	/** The Import Image window on a file, not modal (scripts) */
	QQuickWindow* openImportImage(QWindow* over, const QString& path);
}
