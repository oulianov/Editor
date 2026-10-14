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

#include <QStringList>
#include <functional>
#include <optional>

class QQuickWindow;

/**
 * The commands of the scripts (docs/Automation.adoc) on the Qt Quick map
 * canvas of a QML window (MapCanvas.qml, objectName mapView) and on its
 * palette (Palette.qml, graphicsPalette): the mouse on cells and the keys
 * sent through the window system, the state read from the MapEditor, its
 * document and the palette; the actions of the menus of the map.
 */
namespace AutomationQuickMap {
	using Fail = std::function<void(const QString&)>;

	/**
	 * Runs a command on the map of window
	 *
	 * @param timestamp The time of the last input event sent, moved on
	 * @return Whether it succeeded; nullopt when it is not a command of the
	 *  map, or the window has no map
	 */
	std::optional<bool> run(QQuickWindow* window, const QStringList& args, const Fail& fail, ulong& timestamp);
}
