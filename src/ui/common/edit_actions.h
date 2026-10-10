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

#include <QAction>
#include <QKeySequence>
#include <QList>
#include <QMenu>

class QAbstractScrollArea;

/**
 * The actions of the right-click menus, the same in every list of the
 * Editor: the words, the icons of the map editor, the shortcut written beside
 * them (macOS hides it in context menus unless asked).
 *
 * An action created for a widget is added to it: its shortcut works while
 * the widget has the focus. Created for a menu, it only shows the shortcut,
 * for widgets that read their keys themselves (keyPressEvent).
 */
namespace EditActions {

enum class Kind {
	Undo,
	Redo,
	Insert, // "Insert...": opens the window of the new line
	Edit, // "Edit...": opens the window of the line
	Cut,
	Copy,
	Paste,
	Delete,
	Clear, // empties an entry kept in its list
	SelectAll,
};

/** The words of the action, translated */
QString text(Kind kind);

/** The keys of the action; the first is the one shown */
QList<QKeySequence> keys(Kind kind);

/**
 * Gives action the icon and the shortcut of kind (keys when not empty), its
 * text left as is (QUndoStack's "Undo Delete")
 */
void decorate(QAction* action, Kind kind, const QList<QKeySequence>& keys = {});

/** A standard action of owner: a widget (active shortcut) or a menu (shown) */
QAction* create(Kind kind, QWidget* owner, const QList<QKeySequence>& keys = {});

/** Another action of a context menu, by the same rules */
QAction* create(QWidget* owner, const QString& text, const QList<QKeySequence>& keys = {},
	const QString& icon = {});

/** A standard action added to menu, calling slot */
template <class Slot>
QAction* add(QMenu& menu, Kind kind, const QObject* context, Slot slot,
	const QList<QKeySequence>& keys = {})
{
	QAction* action = create(kind, &menu, keys);
	menu.addAction(action);
	QObject::connect(action, &QAction::triggered, context, slot);
	return action;
}

/**
 * Shows menu at pos of view. The actions it disabled are enabled again
 * after: shortcuts check what they act on themselves
 */
void exec(QMenu& menu, QAbstractScrollArea* view, const QPoint& pos);

}
