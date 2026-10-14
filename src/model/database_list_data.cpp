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

#include "database_list_data.h"

DatabaseListNotifier& DatabaseListNotifier::instance() {
	static DatabaseListNotifier notifier;
	return notifier;
}

void DatabaseListNotifier::notify(const void* list, QObject* source) {
	emit instance().listChanged(list, source);
}

void DatabaseListNotifier::assign(ProjectData& target, const ProjectData& source, QObject* sender) {
	using lcf::rpg::Database;
	std::vector<const void*> changed;
	auto check = [&](auto member) {
		if (!(target.database().*member == source.database().*member)) {
			changed.push_back(&(target.database().*member));
		}
	};
	check(&Database::actors);
	check(&Database::skills);
	check(&Database::items);
	check(&Database::enemies);
	check(&Database::troops);
	check(&Database::terrains);
	check(&Database::attributes);
	check(&Database::states);
	check(&Database::animations);
	check(&Database::chipsets);
	check(&Database::commonevents);
	check(&Database::classes);
	check(&Database::battleranimations);
	check(&Database::switches);
	check(&Database::variables);

	// the vectors keep their address: the views find their list again
	target = source;
	for (const void* list : changed) {
		notify(list, sender);
	}
}
