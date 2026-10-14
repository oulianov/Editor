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

#include "database_apply.h"
#include "common/logging.h"
#include "controller/dialog_service.h"
#include "model/data_merge.h"
#include "model/database_list_data.h"
#include "model/project_data.h"

DatabaseApply::Result DatabaseApply::apply(ProjectData& live, ProjectData& copy, lcf::rpg::Database& base,
	const File& file, DialogService& dialogs, QObject* sender)
{
	// Another program (rpgsync...) changed the database since it was
	// read: what it changed is merged entry by entry; an entry both
	// sides changed is asked, never replaced silently
	QStringList conflicts;
	if (file.isStale() && !mergeFromDisk(copy, base, file, &conflicts, sender)) {
		switch (dialogs.askStale(tr("The database"), true, conflicts)) {
		case StaleChoice::Cancel:
			return {Status::Cancelled, {}};
		case StaleChoice::Reload:
			return {Status::Reload, {}};
		case StaleChoice::Overwrite:
			break;
		}
	}
	// the windows showing the project list what changed again
	DatabaseListNotifier::assign(live, copy, sender);
	// save counts the save
	if (!file.save()) {
		return {Status::SaveFailed, tr("The database could not be written.")};
	}
	// the copy edited takes the count saved: the next Apply neither
	// writes it back nor sees it as a change of the system to merge
	copy.database().system.save_count = live.database().system.save_count;
	base = live.database();
	return {Status::Applied, {}};
}

bool DatabaseApply::mergeFromDisk(ProjectData& copy, lcf::rpg::Database& base, const File& file,
	QStringList* conflicts, QObject* sender)
{
	auto theirs = file.read();
	if (!theirs) {
		return false;
	}
	lcf::rpg::Database merged;
	const auto result = DataMerge::database(base, copy.database(), *theirs, merged);
	if (!result.ok()) {
		*conflicts = result.conflicts;
		return false;
	}
	qCInfo(lcReload) << "database merged with the changes on disk";
	// the pages show the merge: their lists are told what changed
	ProjectData shown = copy;
	shown.database() = std::move(merged);
	DatabaseListNotifier::assign(copy, shown, sender);
	base = std::move(*theirs);
	return true;
}
