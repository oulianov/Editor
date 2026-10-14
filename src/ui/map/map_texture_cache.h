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
#include <memory>

class QQuickWindow;
class QSGTexture;

/**
 * The textures of the map items of a window, made once for all of them:
 * the map and the palette showing the same chipset (the images of a
 * TileAtlas), two views of a charset or of a panorama (PictureCache) share
 * them. A texture belongs to the scene graph of its window, so each window
 * has its textures.
 *
 * Called while the items update their nodes (on the render thread, the GUI
 * thread waiting). A texture lasts while a node holds it: the last node
 * deleted, on the render thread, deletes it.
 */
namespace MapTextureCache
{
	/** The texture of an image in a window, made on first use: the same
	 *  image (its cacheKey) gives the same texture */
	std::shared_ptr<QSGTexture> texture(QQuickWindow* window, const QImage& image);

	/** The textures made in a window so far (tests) */
	int uploadCount(QQuickWindow* window);
}
