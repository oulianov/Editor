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

class EditorWindow;

/** Where the database window opens: a page, an entry of it, a line of its commands */
struct DatabaseTarget {
	enum class Page {
		/** As it was last closed */
		Last,
		CommonEvents,
		/** Monster Groups */
		Troops,
	};
	Page page = Page::Last;
	/** The entry (common event, monster group), by its ID */
	int id = 0;
	/** The battle event page of a monster group (1-based), 0 for the first */
	int troopPage = 0;
	/** The line of the commands (1-based), 0 for none */
	int line = 0;
};

/**
 * The database window, whichever window asks for it (Game > Database of
 * either main window, the common-event and monster-group results of Search):
 * DatabaseWindow.qml, modal. One window: asked again while it is open (from a window over it), it
 * comes to the front.
 */
namespace DatabaseWindows {
	/** Shows the database, modal over the main window, at a target */
	void open(EditorWindow& main, const DatabaseTarget& target = {});
}
