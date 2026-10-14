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

#include <QCoreApplication>
#include <QStringList>
#include <functional>
#include <memory>
#include <lcf/rpg/database.h>

class DialogService;
class ProjectData;
class QObject;

/**
 * OK and Apply of the database window, whatever interface shows it: the
 * copy of the project the window edits goes into the project, then into
 * its file.
 *
 * Another program (rpgsync...) can have changed the file since the window
 * read it: what it changed is merged entry by entry, against the database
 * as the window read it; an entry both sides changed is asked through the
 * DialogService, never replaced silently.
 */
class DatabaseApply
{
	Q_DECLARE_TR_FUNCTIONS(DatabaseApply)

public:
	/** The file of the database: the project's (ProjectController::databaseFile()), a test's */
	struct File {
		/** Another program changed it since the Editor read or wrote it */
		std::function<bool()> isStale;
		/** It as it is now, to merge (nullptr: not readable) */
		std::function<std::unique_ptr<lcf::rpg::Database>()> read;
		/** Writes the database of the project, counting the save
		 *  (save_count); the version written is the Editor's */
		std::function<bool()> save;
		/** Reads it again into the project, once the windows editing it allow */
		std::function<void()> reload;
	};

	enum class Status {
		/** In the project and written */
		Applied,
		/** Changed on disk and cancelled: nothing changed, the window stays */
		Cancelled,
		/** Changed on disk, the window's changes discarded: the window
		 *  closes, then File::reload reads the file */
		Reload,
		/** In the project but not written: message says it */
		SaveFailed,
	};

	struct Result {
		Status status = Status::Applied;
		QString message;
	};

	/**
	 * Puts copy (the window's) into live (the project's) and writes it.
	 * base is the database as the window read it, the base of a merge with
	 * the file; once written, it is the database written. copy takes what
	 * is merged and the save count. The lists that changed are notified
	 * (DatabaseListNotifier) from sender, the window.
	 */
	static Result apply(ProjectData& live, ProjectData& copy, lcf::rpg::Database& base,
		const File& file, DialogService& dialogs, QObject* sender = nullptr);

private:
	/** Merges what another program changed in the file into copy; false
	 *  when it cannot be read, or both sides changed an entry (conflicts) */
	static bool mergeFromDisk(ProjectData& copy, lcf::rpg::Database& base, const File& file,
		QStringList* conflicts, QObject* sender);
};
