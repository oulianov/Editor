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

class ProjectController;
class QQuickWindow;
class QWindow;

/**
 * The Qt Quick map canvas in a window of its own (MapCanvasWindow.qml,
 * the automation's qml-map), apart from the main window's tabs.
 */
namespace MapCanvasHost {
	/**
	 * A window with the map canvas and the palette on a map of the project
	 *
	 * The map is the controller's document, the one its tab edits when it
	 * is open (read here otherwise); the window has its own MapEditor on it,
	 * with the editing state of the Editor (layer, tool, tiles picked).
	 *
	 * @return The window, nullptr without such a map
	 */
	QQuickWindow* open(ProjectController& controller, int id, QWindow* over = nullptr);
}
