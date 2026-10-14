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

#include "core.h"
#include <QDebug>
#include "common/dbstring.h"
#include "common/image_loader.h"
#include <QCoreApplication>
#include <QDir>

//define static member
Core *Core::core_instance = nullptr;

Core& core() {
	return *Core::getCore();
}

namespace {

// The chipset of this id in the database, an empty one (ID 0) when there is none
lcf::rpg::Chipset findChipset(const Project& project, int chipset_id)
{
	for (auto & chipset : project.database().chipsets)
		if (chipset.ID == chipset_id)
			return chipset;
	return lcf::rpg::Chipset();
}

}

Core::Core() :
	m_chipsetTiles(std::make_shared<const ChipsetTiles>()),
	m_tilePainter(m_chipsetTiles)
{
	m_tileSize = 16;
	// the callers connect to Core
	connect(&m_editorState, &EditorState::toolChanged, this, &Core::toolChanged);
	connect(&m_editorState, &EditorState::layerChanged, this, &Core::layerChanged);
	connect(&m_editorState, &EditorState::selectionChanged, this, &Core::selectionChanged);
}

Core *Core::getCore()
{
	if (!core_instance) {
		core_instance = new Core();
	}
	return core_instance;
}

void Core::LoadChipset(int n_chipsetid)
{
	const lcf::rpg::Chipset n_chipset = findChipset(*project(), n_chipsetid);
	// Compare the whole chipset, not its id: a database reloaded from disk
	// can have changed its image or terrain
	if (n_chipset == m_chipsetTiles->chipset() && m_chipsetProject == project().get())
		return;
	m_chipsetProject = project().get();
	m_charsetCache.clear();
	// unknown chipset: no tiles, not those of the previous one
	m_chipsetTiles = ChipsetTiles::load(*project(), n_chipset, tileSize(), m_rtpDir);
	m_tilePainter.setTiles(m_chipsetTiles);
	emit chipsetChanged();
}

int Core::chipsetId() const
{
	return m_chipsetTiles->id();
}

const std::shared_ptr<const ChipsetTiles>& Core::chipsetTiles() const
{
	return m_chipsetTiles;
}

std::shared_ptr<const ChipsetTiles> Core::loadChipsetTiles(int chipset_id)
{
	// without a project, the tiles there are
	if (!project())
		return m_chipsetTiles;
	const lcf::rpg::Chipset chipset = findChipset(*project(), chipset_id);
	if (chipset == m_chipsetTiles->chipset() && m_chipsetProject == project().get())
		return m_chipsetTiles;
	return ChipsetTiles::load(*project(), chipset, tileSize(), m_rtpDir);
}

void Core::LoadBackground(QString name)
{
	// drawn where the last one was
	m_tilePainter.setBackground(panorama(name), m_tilePainter.backgroundTransform());
}

QPixmap Core::panorama(const QString& name) const
{
	if (name.isEmpty()) {
		QPixmap background(640,480);
		background.fill(Qt::magenta);
		return background;
	}
	return ImageLoader::Load(project()->findFile(PANORAMA, name, FileFinder::FileType::Image));
}

const QPixmap& Core::background() const
{
	return m_tilePainter.background();
}

const QTransform& Core::backgroundTransform() const
{
	return m_tilePainter.backgroundTransform();
}

void Core::setBackground(const QPixmap& background, const QTransform& transform)
{
	m_tilePainter.setBackground(background, transform);
}

int Core::tileSize()
{
	return m_tileSize;
}

void Core::setTileSize(int tile_size)
{
	m_tileSize = tile_size;
}

QString Core::rtpPath(const QString &folder, const QString &filename) const
{
	return m_rtpDir+folder+filename;
}

QString Core::defDir() const
{
	return m_defDir;
}

void Core::setDefDir(const QString &defDir)
{
	m_defDir = defDir + (defDir.endsWith('/') ? "" : "/");
}

std::shared_ptr<Project>& Core::project() {
	return m_project;
}

const std::shared_ptr<Project>& Core::project() const {
	return m_project;
}

ProjectWatcher& Core::watcher() {
	return m_watcher;
}

void Core::setAnimationEnabled(bool enabled)
{
	if (enabled == animationEnabled())
		return;
	if (!enabled) {
		m_animationTimer->stop();
		// the first frame, as the tiles were drawn before
		m_tilePainter.setFrames(0, 0);
		emit animationFrameChanged();
		return;
	}
	if (!m_animationTimer) {
		m_animationTimer = new QTimer(this);
		connect(m_animationTimer, &QTimer::timeout, this, &Core::nextAnimationFrame);
	}
	// the Player changes the animated tiles every 6 frames (60 per second)
	m_animationTimer->start(100);
}

bool Core::animationEnabled() const
{
	return m_animationTimer && m_animationTimer->isActive();
}

int Core::waterFrame() const
{
	return m_tilePainter.waterFrame();
}

int Core::animatedTileFrame() const
{
	return m_tilePainter.animatedTileFrame();
}

void Core::nextAnimationFrame()
{
	if (chipsetIsNull())
		return;
	m_animationTick++;
	const int tile_frame = Autotile::animatedTileFrame(m_animationTick);
	const int water_frame = Autotile::waterFrame(m_chipsetTiles->chipset(), m_animationTick);
	if (tile_frame == animatedTileFrame() && water_frame == waterFrame())
		return;
	m_tilePainter.setFrames(water_frame, tile_frame);
	emit animationFrameChanged();
}

void Core::renderEvent(const lcf::rpg::Event& event, const QRect &dest_rect)
{
	const auto character = m_eventCache.constFind(event.ID);
	m_tilePainter.renderEvent(event, dest_rect, character != m_eventCache.constEnd() ? &character.value() : nullptr);
}

void Core::setRtpDir(const QString &n_path)
{
	m_rtpDir = n_path;
}

EditorState& Core::editorState()
{
	return m_editorState;
}

void Core::setCurrentMapEvents(QMap<int, lcf::rpg::Event *> *events)
{
	m_editorState.setCurrentMapEvents(events);

	m_eventCache.clear();
	if (!events)
		return;
	for (QMap<int, lcf::rpg::Event*>::iterator it = events->begin(); it != events->end(); ++it)
	{
		lcf::rpg::Event* ev = it.value();
		if (ev->pages.empty())
			continue;

		lcf::rpg::EventPage& evp = ev->pages[0];
		if (evp.character_name.empty())
			continue;

		const QPixmap charset = charsetImage(ToQString(evp.character_name));

		int char_index = evp.character_index;
		int src_x = (char_index%4)*72 + evp.character_pattern * 24;
		int src_y = (char_index/4)*128 + evp.character_direction * 32;
		m_eventCache[it.key()] = charset.copy(src_x, src_y, 24, 32);
	}
}

QPixmap Core::charsetImage(const QString& name)
{
	if (m_cacheProject != project().get()) {
		m_charsetCache.clear();
		m_cacheProject = project().get();
	}
	auto it = m_charsetCache.constFind(name);
	if (it != m_charsetCache.constEnd())
		return it.value();

	QPixmap charset(ImageLoader::Load(project()->findFile(CHARSET, name, FileFinder::FileType::Image)));
	if (!charset)
		charset = ImageLoader::Load(rtpPath(CHARSET, name));
	if (!charset)
	{
		qWarning()<<"CharSet"<<name<<"not found.";
		charset = TilePainter::createDummyPixmap(288,256);
	}
	m_charsetCache.insert(name, charset);
	return charset;
}
