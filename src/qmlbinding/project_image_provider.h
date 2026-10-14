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

#include <QQuickImageProvider>
#include <functional>

/**
 * The pictures of the project for QML: image://project/<Folder>/<File>,
 * found like the Editor finds them (the game, then the RTP; with or
 * without extension; XYZ files too).
 *
 *   Image { source: "image://project/CharSet/" + name }
 *
 * Palette color 0 is transparent, as the Player shows them, in the folders
 * where the game makes it so; "?transparent=0" or "=1" decides it. The
 * size asked (sourceSize) scales the picture, without smoothing.
 */
class ProjectImageProvider : public QQuickImageProvider {
public:
	/** Finds the file of a picture (folder, name): empty when there is none */
	using Finder = std::function<QString(const QString& folder, const QString& name)>;

	explicit ProjectImageProvider(Finder finder);

	QPixmap requestPixmap(const QString& id, QSize* size, const QSize& requestedSize) override;

	/** Folders whose pictures have palette color 0 transparent in the game */
	static bool isTransparentFolder(const QString& folder);
	/** A picture of the game, palette color 0 transparent when asked */
	static QImage load(const QString& path, bool transparent);

private:
	Finder m_finder;
};
