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

#include "map_texture_cache.h"
#include <QHash>
#include <QMutex>
#include <QQuickWindow>
#include <QSGTexture>

namespace {
struct WindowTextures
{
	QHash<qint64, std::weak_ptr<QSGTexture>> textures;
	int uploads = 0;
};

// Each window has its render thread: they can ask at the same time
QMutex mutex;
QHash<const QQuickWindow*, WindowTextures>& windows()
{
	static QHash<const QQuickWindow*, WindowTextures> windows;
	return windows;
}
}

std::shared_ptr<QSGTexture> MapTextureCache::texture(QQuickWindow* window, const QImage& image)
{
	if (!window || image.isNull())
		return nullptr;
	QMutexLocker lock(&mutex);
	auto it = windows().find(window);
	if (it == windows().end()) {
		it = windows().insert(window, {});
		// on the GUI thread, once its scene graph is gone
		QObject::connect(window, &QObject::destroyed, [window]() {
			QMutexLocker lock(&mutex);
			windows().remove(window);
		});
	}
	std::weak_ptr<QSGTexture>& slot = it->textures[image.cacheKey()];
	if (auto texture = slot.lock())
		return texture;
	// the others are gone with their nodes (or with the scene graph)
	it->textures.removeIf([](const QHash<qint64, std::weak_ptr<QSGTexture>>::iterator entry) {
		return entry->expired();
	});
	std::shared_ptr<QSGTexture> texture(window->createTextureFromImage(image));
	// pixel art: no smoothing (the nodes say it too)
	texture->setFiltering(QSGTexture::Nearest);
	it->textures[image.cacheKey()] = texture;
	it->uploads++;
	return texture;
}

int MapTextureCache::uploadCount(QQuickWindow* window)
{
	QMutexLocker lock(&mutex);
	return windows().value(window).uploads;
}
