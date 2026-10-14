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

// MapDocument: reading and writing the file of the map, and the map tree
// saved with it (the project and its watcher)

#include "map_document.h"
#include <QUndoStack>
#include "autotile.h"
#include "core.h"
#include "data_merge.h"
#include "project.h"
#include "project_watcher.h"
#include "common/logging.h"

void MapDocument::load(bool revert)
{
	if (revert) {
		// The start positions are in the map tree, not in the map file
		cancelFloating();
		while (m_undoStack->canUndo())
			m_undoStack->undo();
	}
	auto map = m_project.project().loadMap(id(), true);
	if (!map) {
		if (m_map)
			return; // the map shown stays
		// unreadable when opened: an empty map, whose layers can be drawn
		map = std::make_unique<lcf::rpg::Map>();
		map->lower_layer.assign(static_cast<size_t>(map->width * map->height), 0);
		map->upper_layer.assign(static_cast<size_t>(map->width * map->height), 10000);
	}
	setMap(std::move(map));
}

void MapDocument::reloadFromDisk(bool ask)
{
	if (ask && isModified() && !(m_reloadHandler && m_reloadHandler()))
		return;

	// read once: a file caught while written stays as shown (the next
	// change reloads it)
	auto map = m_project.project().loadMap(id(), true);
	if (!map)
		return;
	// The undo steps belong to the version replaced: undoing them would
	// write their old values into the map tree just read
	cancelFloating();
	m_undoStack->clear();
	setMap(std::move(map));
	emit reloaded();
}

bool MapDocument::mergeFromDisk(QStringList* conflicts)
{
	auto theirs = m_project.project().loadMap(id());
	if (!theirs || !m_savedMap)
		return false;
	lcf::rpg::Map merged;
	const auto result = DataMerge::map(*m_savedMap, *m_map, *theirs, merged);
	if (!result.ok()) {
		*conflicts = result.conflicts;
		return false;
	}
	qCInfo(lcReload) << "map" << id() << "merged with the changes on disk";
	// the map shows both sides: what the other program changed is drawn
	const bool size_changed = merged.width != m_map->width || merged.height != m_map->height;
	*m_map = std::move(merged);
	m_savedMap = std::move(*theirs);
	m_lower = m_map->lower_layer;
	m_upper = m_map->upper_layer;
	m_moveContents = {};
	m_moveBackground = {};
	emit mapReplaced(size_changed);
	return true;
}

bool MapDocument::save(bool properties_changed)
{
	emit aboutToSave();
	commitFloating();
	m_lastError.clear();
	if (!isModified() && !properties_changed)
		return true;

	// Another program (rpgsync...) deleted the map while it was edited: a
	// file the map tree does not list is never written
	if (!hasTreeEntry()) {
		switch (m_deletedHandler ? m_deletedHandler() : StaleChoice::Cancel) {
		case StaleChoice::Cancel:
			return false;
		case StaleChoice::Reload:
			// the edits go with the map: nothing is left to save
			m_undoStack->clear();
			return true;
		case StaleChoice::Overwrite:
			// put back in the tree by the handler, unless it could not be
			if (!hasTreeEntry())
				return false;
			break;
		}
	}

	// Another program (rpgsync...) changed the map since it was read:
	// never replace its changes without asking
	Project& project = m_project.project();
	ProjectWatcher& watcher = core().watcher();
	QStringList conflicts;
	if (watcher.isStale(project.mapFile(id())) && !mergeFromDisk(&conflicts)) {
		switch (askStale(tr("The map “%1”").arg(name()), true, conflicts)) {
		case StaleChoice::Cancel:
			return false;
		case StaleChoice::Reload:
			// the edits are discarded on purpose: nothing is left to save
			reloadFromDisk(false);
			return true;
		case StaleChoice::Overwrite:
			break;
		}
	}

	auto& treeMap = m_project.treeMap();
	for (unsigned int i = 1; i < treeMap.maps.size(); i++)
		if (treeMap.maps[i].ID == id())
		{
			// the tree owns the hierarchy (it can have changed by drag and drop)
			m_info.parent_map = treeMap.maps[i].parent_map;
			m_info.indentation = treeMap.maps[i].indentation;
			m_info.expanded_node = treeMap.maps[i].expanded_node;
			treeMap.maps[i] = m_info; //Apply info changes
			break;
		}
	// Remember last active map
	treeMap.active_node = id();
	// The tree is written only when it changed (map properties, start
	// positions): another program can have changed it meanwhile
	if (project.treeMapModified()) {
		StaleChoice choice = StaleChoice::Overwrite;
		bool merged = false;
		if (watcher.isStale(project.treeMapFile())) {
			QStringList tree_conflicts;
			merged = project.mergeTreeMap(&tree_conflicts);
			if (!merged)
				choice = askStale(tr("The map tree"), true, tree_conflicts);
		}
		if (choice == StaleChoice::Cancel)
			return false;
		if (choice == StaleChoice::Reload) {
			watcher.requestReload(ProjectWatcher::TreeMap);
		} else {
			if (!project.saveTreeMap()) {
				// the map stays modified: saving can be tried again
				fail(tr("The map tree could not be written."));
				return false;
			}
			// the tree shown takes what the other program changed in it
			if (merged)
				watcher.requestReload(ProjectWatcher::TreeMap);
		}
	}
	// Tiles the Player cannot draw (the old palette eraser wrote 0x7FFD)
	// become the empty tiles: water below, the transparent tile above
	auto repair = [](std::vector<short>& lower, std::vector<short>& upper) {
		for (auto& tile : lower) {
			const int terrain = Autotile::translate(tile);
			if (terrain < 0 || Autotile::isFblock(terrain))
				tile = 0;
		}
		for (auto& tile : upper) {
			if (!Autotile::isFblock(Autotile::translate(tile)))
				tile = 10000;
		}
	};
	repair(m_map->lower_layer, m_map->upper_layer);
	repair(m_lower, m_upper);
	// saveMap increases the save count
	if (!project.saveMap(*m_map, id())) {
		// the edits stay, modified and undoable: saving can be tried again
		fail(tr("%1 could not be written.").arg(name()));
		return false;
	}
	m_savedMap = *m_map;
	m_savedInfo = m_info;
	m_undoStack->clear();
	emit saved();
	return true;
}

QString MapDocument::lastError() const
{
	return m_lastError;
}

void MapDocument::setStaleHandler(StaleHandler handler)
{
	m_staleHandler = std::move(handler);
}

void MapDocument::setReloadHandler(ReloadHandler handler)
{
	m_reloadHandler = std::move(handler);
}

void MapDocument::setDeletedHandler(DeletedHandler handler)
{
	m_deletedHandler = std::move(handler);
}

StaleChoice MapDocument::askStale(const QString& what, bool can_reload, const QStringList& conflicts) const
{
	// nobody to ask: what the other program wrote is not replaced
	return m_staleHandler ? m_staleHandler(what, can_reload, conflicts) : StaleChoice::Cancel;
}

void MapDocument::fail(const QString& message)
{
	m_lastError = message;
	emit saveFailed(message);
}
