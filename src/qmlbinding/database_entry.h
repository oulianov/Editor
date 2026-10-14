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

#include <QString>

class DatabaseSession;
class JsonView;
class ProjectData;

/**
 * The entry a page of a database window shows, found from its view
 * ("/troops/2"): the window's session, its copy of the project, the list and
 * the index. The rich pages edit that copy in C++ (a canvas, a command list)
 * and tell its views with changed().
 */
struct DatabaseEntry
{
	DatabaseSession* session = nullptr;
	/** The copy edited (the session's), nullptr without a session */
	ProjectData* project = nullptr;
	/** The list ("troops"), empty for the database itself */
	QString list;
	int index = -1;

	static DatabaseEntry of(const JsonView* view);
	bool valid() const {
		return project && index >= 0;
	}
	/** Tells the views that something under the entry changed ("pages/0") */
	void changed(const QString& path = QString()) const;
};
