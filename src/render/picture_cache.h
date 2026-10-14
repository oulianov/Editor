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
#include <QString>
#include <memory>

/**
 * Pictures of the project read once per file and shared while they are
 * used: the charsets of the events and the panoramas, for every map view.
 * The same picture is then the same QImage (one cacheKey), so the views of
 * a window share its texture (MapTextureCache). A file changed on disk
 * since it was read is read again.
 *
 * On the GUI thread.
 */
namespace PictureCache
{
	/** The picture of a file as ImageLoader reads it; nullptr when there is
	 *  none or it cannot be read */
	std::shared_ptr<const QImage> load(const QString& path);
}
