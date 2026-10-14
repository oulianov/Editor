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

class QObject;

/**
 * The scripts of a game edited with rpgsync (its Scripts folder): opens the
 * line where a script writes an event, a page or a command, in the script
 * editor of the settings (VS Code...). rpgsync finds the line (`rpgsync
 * locate`), from the script as it is now.
 */
namespace ScriptLink {
	/** The open game has rpgsync scripts */
	bool available();
	/** Why scripts cannot open for the open game (shown beside the greyed
	 *  Open in Script Editor), empty when they can */
	QString unavailableReason();
	/** The title of what tells why a script did not open */
	QString title();
	/** location: what rpgsync locate takes (--map 1 --event 2 --page 1,
	 *  --common-event 3); command: a line of the list, -1 for its start.
	 *  rpgsync runs in the background (while context lives): failed gets
	 *  why the script did not open, for the interface to tell */
	void open(QObject* context, const QStringList& location, int command,
		std::function<void(const QString& error)> failed);

	/** What a node of the map tree opens (type: its
	 *  lcf::rpg::TreeMap::MapType): a map its script (MapNNNN.py) and its
	 *  entry in map_tree.py, an area its entry, the project (0) the map
	 *  tree at its start positions. Empty: nothing to open there */
	struct MapTreeLocations {
		QStringList script;
		QStringList entry;
	};
	MapTreeLocations mapTreeLocations(int id, int type);
	/** Opens file at line with the script editor of the settings */
	bool openAt(const QString& file, int line);
}
