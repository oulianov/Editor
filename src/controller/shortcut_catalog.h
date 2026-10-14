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

#include <QKeySequence>
#include <QList>
#include <QString>
#include <QStringList>

/**
 * The keys of the main window's actions (by their object name): RPG
 * Maker 2003's (F5 to F9, the tools on 1 to 6...) and the platform's for
 * Quit, Settings, Help, Undo and Redo. Not translated: a key is the same in
 * every language.
 */
namespace ShortcutCatalog {

/** Where the keys of an action work */
enum class Context {
	/** Anywhere in the main window */
	Window,
	/** While the map tree has the focus (Ctrl+C copies a map there only) */
	MapTree,
};

/** The keys of an action, the one shown in the menus first; none without */
QList<QKeySequence> shortcuts(const QString& action);
/** The platform's standard key of an action (QKeySequence::Quit...), for
 *  QML's StandardKey; QKeySequence::UnknownKey for the others */
QKeySequence::StandardKey standardKey(const QString& action);
Context context(const QString& action);
/** The actions that have keys */
QStringList actions();
/** Both actions have a key in common because they are never enabled
 *  together: Delete Map on a map, Delete Area on an area */
bool sharesKeys(const QString& action, const QString& other);

}
