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

class QWidget;

/**
 * The scripts of a game edited with rpgsync (its Scripts folder): opens the
 * line where a script writes an event, a page or a command, in the script
 * editor of the settings (VS Code...). rpgsync finds the line (`rpgsync
 * locate`), from the script as it is now.
 */
namespace ScriptLink {
	/** The open game has rpgsync scripts */
	bool available();
	/** location: what rpgsync locate takes (--map 1 --event 2 --page 1,
	 *  --common-event 3); command: a line of the list, -1 for its start */
	void open(QWidget* parent, const QStringList& location, int command = -1);
	/** Opens file at line with the script editor of the settings */
	bool openAt(const QString& file, int line);
}
