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

class QWidget;

namespace Screenshot {
	/** The window the user is working in: the open dialog, else the active window */
	QWidget* activeWindow();

	/** Save widget (default: activeWindow()) as a PNG. Without a path, it goes in
	 *  Pictures/EasyRPG Editor. Returns the file written, or an empty string. */
	QString save(QWidget* widget = nullptr, const QString& path = QString());

	/** Ctrl+Alt+S (Cmd+Option+S on macOS) saves the active window, dialogs included */
	void installShortcut();
}
