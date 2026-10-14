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

#include "picture_cache.h"
#include <QDateTime>
#include <QFileInfo>
#include <QHash>
#include <QPixmap>
#include "common/image_loader.h"

namespace {
struct Entry
{
	// the file as it was read: another date or size, it changed
	QDateTime modified;
	qint64 size = -1;
	std::weak_ptr<const QImage> picture;
};
}

std::shared_ptr<const QImage> PictureCache::load(const QString& path)
{
	if (path.isEmpty())
		return nullptr;
	static QHash<QString, Entry> entries;
	const QFileInfo info(path);
	if (!info.isFile())
		return nullptr;
	Entry& entry = entries[info.absoluteFilePath()];
	if (auto picture = entry.picture.lock(); picture && entry.modified == info.lastModified() && entry.size == info.size())
		return picture;
	const QImage image = ImageLoader::Load(path).toImage();
	if (image.isNull()) {
		entries.remove(info.absoluteFilePath());
		return nullptr;
	}
	auto picture = std::make_shared<const QImage>(image.convertToFormat(QImage::Format_ARGB32_Premultiplied));
	entry = {info.lastModified(), info.size(), picture};
	// the pictures no view shows any more
	entries.removeIf([](const QHash<QString, Entry>::iterator it) { return it->picture.expired(); });
	return picture;
}
