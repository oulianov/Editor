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

#include <QObject>
#include <type_traits>
#include <vector>
#include <lcf/rpg/database.h>
#include "model/project_data.h"

/**
 * Tells every view of a database list that it changed: entries added or
 * removed (Array Size, Paste...) or renamed. Lists are identified by the
 * address of their vector, so the copies edited by the database window and
 * by the "..." pickers do not mix.
 */
class DatabaseListNotifier : public QObject {
	Q_OBJECT
public:
	static DatabaseListNotifier& instance();

	/** The list changed; source is the widget that changed it (it refreshes itself) */
	static void notify(const void* list, QObject* source = nullptr);

	/** Copies a project (OK of a picker) and notifies the lists that differ */
	static void assign(ProjectData& target, const ProjectData& source, QObject* sender = nullptr);

signals:
	void listChanged(const void* list, QObject* source);
};

namespace DatabaseList {

/** A new or cleared entry, as RPG Maker makes it */
template<class LCF>
LCF defaultEntry(const ProjectData& project, int id) {
	LCF entry;
	entry.ID = id;
	if constexpr (std::is_same_v<LCF, lcf::rpg::Actor>) {
		// the stat curves depend on the engine, like when liblcf loads a database
		entry.Setup(project.database().system.ldb_id == 2003);
	}
	return entry;
}

/** Copied entries of a type: shared by the database window and the pickers */
template<class LCF>
std::vector<LCF>& clipboard() {
	static std::vector<LCF> entries;
	return entries;
}

}
