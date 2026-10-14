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

class EventEditorController;
class ProjectData;
class QWindow;

/**
 * The event window of a map event (EventEditorWindow.qml) over the event
 * edited by an EventEditorController: the map makes the controller, gives
 * it the event and puts on the map what it applies; this shows it, modal,
 * and remembers the window (its size, the page shown, the line and scroll
 * of each page) per event of the game.
 */
namespace EventWindow {
	struct Options {
		/** The chipset of the map: the tiles an event can show */
		int chipsetId = 0;
		/** The page shown first (from 1), else the one shown last time */
		int page = 0;
		/** Its line selected (from 1), 0: as last time */
		int line = 0;
	};

	/** Shows the window and returns once it is closed (the controller's
	 *  result says how); it holds the controller's EditLock meanwhile */
	void exec(QWindow* over, ProjectData& project, EventEditorController& controller, const Options& options = {});

	/** The commands of every page with the indents of their blocks (the
	 *  lists normalize them when shown): the event as shown is not changed */
	void normalizeCommands(EventEditorController& controller);
}
