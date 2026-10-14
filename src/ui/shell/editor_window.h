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

class ProjectController;
class ShellController;
class QWindow;

namespace lcf::rpg {
class Map;
}

/**
 * The main window (QmlShell), as the rest of the Editor sees it: the
 * commands of the scripts, the Search window, the pickers that start on the
 * map shown.
 */
class EditorWindow
{
public:
	virtual ~EditorWindow();

	/** The main window of the Editor, nullptr before it is made */
	static EditorWindow* main();

	/** Its window, for the windows over it */
	virtual QWindow* handle() const = 0;
	/** What the window does: the project, its map tabs, the actions */
	virtual ShellController* shell() const = 0;
	virtual ProjectController* projectController() const = 0;

	virtual void LoadProject(QString folder) = 0;
	/** A map or an area of the map tree */
	virtual bool hasMap(int id) const = 0;
	/** The map shown in the temporary tab, as a click in the tree */
	virtual void openScene(int id) = 0;
	/** The data of a map open in a tab, with its unsaved changes; nullptr
	 *  when no tab shows it */
	virtual const lcf::rpg::Map* openMapData(int id) = 0;
	/** Shows an event: its map, its cell selected and centered, then its
	 *  window at a page and a line (0: as last shown), modal. false when
	 *  the map has no such event */
	virtual bool openEvent(int map, int event, int page = 0, int line = 0) = 0;

protected:
	EditorWindow();
};
