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
#include <QString>
#include <functional>

class QQuickWindow;

namespace Screenshot {
	/** Save a window as a PNG. Without a path, it goes in Pictures/EasyRPG
	 *  Editor (a script's in its folder). Returns the file written, or an
	 *  empty string. */
	QString save(QQuickWindow* window, const QString& path = QString());

	/** Ctrl+Alt+S (Cmd+Option+S on macOS) saves the window with the
	 *  keyboard, dialogs included; saved: says where (the status bar of the
	 *  main window) */
	void installShortcut(std::function<void(const QString& message)> saved = {});
}
