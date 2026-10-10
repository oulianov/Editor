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

#include "edit_actions.h"

#include <QAbstractScrollArea>
#include <QApplication>
#include <QCoreApplication>
#include <QPalette>

namespace EditActions {

namespace {
const char* const kind_property = "edit_action_kind";

QString iconName(Kind kind) {
	switch (kind) {
		case Kind::Undo: return QStringLiteral("edit-undo");
		case Kind::Redo: return QStringLiteral("edit-redo");
		case Kind::Insert: return QStringLiteral("new");
		case Kind::Edit: return QStringLiteral("properties");
		case Kind::Cut: return QStringLiteral("edit-cut");
		case Kind::Copy: return QStringLiteral("edit-copy");
		case Kind::Paste: return QStringLiteral("edit-paste");
		case Kind::Delete:
		case Kind::Clear: return QStringLiteral("edit-delete");
		case Kind::SelectAll: return QStringLiteral("select-all");
	}
	return {};
}

/** The icon for the colours of the menus now (light or dark) */
QIcon icon(const QString& name) {
	const bool light = QApplication::palette().color(QPalette::Window).lightness() >= 128;
	return QIcon(QStringLiteral("%1/%2").arg(light ? ":/bright" : ":/dark", name));
}

void setup(QAction* action, QWidget* owner, const QList<QKeySequence>& keys) {
	action->setShortcuts(keys);
	// a list's keys, not the window's: the window has other lists and fields
	action->setShortcutContext(Qt::WidgetShortcut);
	action->setShortcutVisibleInContextMenu(true);
	if (!qobject_cast<QMenu*>(owner)) {
		owner->addAction(action);
	}
}
}

QString text(Kind kind) {
	switch (kind) {
		case Kind::Undo: return QCoreApplication::translate("EditActions", "Undo");
		case Kind::Redo: return QCoreApplication::translate("EditActions", "Redo");
		case Kind::Insert: return QCoreApplication::translate("EditActions", "Insert...");
		case Kind::Edit: return QCoreApplication::translate("EditActions", "Edit...");
		case Kind::Cut: return QCoreApplication::translate("EditActions", "Cut");
		case Kind::Copy: return QCoreApplication::translate("EditActions", "Copy");
		case Kind::Paste: return QCoreApplication::translate("EditActions", "Paste");
		case Kind::Delete: return QCoreApplication::translate("EditActions", "Delete");
		case Kind::Clear: return QCoreApplication::translate("EditActions", "Clear");
		case Kind::SelectAll: return QCoreApplication::translate("EditActions", "Select All");
	}
	return {};
}

// The main keys of the platform only: the others (Shift+Del for Cut on
// Windows and Linux...) are RPG Maker's keys of other actions
QList<QKeySequence> keys(Kind kind) {
	switch (kind) {
		case Kind::Undo: return {QKeySequence(QKeySequence::Undo)};
		case Kind::Redo: return {QKeySequence(QKeySequence::Redo)};
		case Kind::Insert: return {QKeySequence(Qt::Key_Return), QKeySequence(Qt::Key_Insert)};
		case Kind::Edit: return {QKeySequence(Qt::Key_Return)};
		case Kind::Cut: return {QKeySequence(QKeySequence::Cut)};
		case Kind::Copy: return {QKeySequence(QKeySequence::Copy)};
		case Kind::Paste: return {QKeySequence(QKeySequence::Paste)};
		case Kind::Delete:
		case Kind::Clear:
#ifdef Q_OS_MACOS
			// as the map tree: the Delete key of Mac keyboards is Backspace
			return {QKeySequence(Qt::Key_Delete), QKeySequence(Qt::Key_Backspace)};
#else
			return {QKeySequence(Qt::Key_Delete)};
#endif
		case Kind::SelectAll: return {QKeySequence(QKeySequence::SelectAll)};
	}
	return {};
}

void decorate(QAction* action, Kind kind, const QList<QKeySequence>& keys) {
	action->setProperty(kind_property, static_cast<int>(kind));
	action->setIcon(icon(iconName(kind)));
	action->setShortcuts(keys.isEmpty() ? EditActions::keys(kind) : keys);
	action->setShortcutVisibleInContextMenu(true);
}

QAction* create(Kind kind, QWidget* owner, const QList<QKeySequence>& keys) {
	auto* action = new QAction(text(kind), owner);
	setup(action, owner, keys.isEmpty() ? EditActions::keys(kind) : keys);
	decorate(action, kind, keys);
	return action;
}

QAction* create(QWidget* owner, const QString& text, const QList<QKeySequence>& keys, const QString& icon_name) {
	auto* action = new QAction(text, owner);
	setup(action, owner, keys);
	if (!icon_name.isEmpty()) {
		action->setProperty("edit_action_icon", icon_name);
		action->setIcon(icon(icon_name));
	}
	return action;
}

void exec(QMenu& menu, QAbstractScrollArea* view, const QPoint& pos) {
	// the icons of the colours now: the system may have gone dark since
	for (QAction* action : menu.actions()) {
		const QVariant kind = action->property(kind_property);
		const QString name = kind.isValid() ? iconName(static_cast<Kind>(kind.toInt()))
			: action->property("edit_action_icon").toString();
		if (!name.isEmpty()) {
			action->setIcon(icon(name));
		}
	}
	QList<QAction*> disabled;
	for (QAction* action : menu.actions()) {
		if (!action->isEnabled()) {
			disabled << action;
		}
	}
	menu.exec(view->viewport()->mapToGlobal(pos));
	for (QAction* action : disabled) {
		action->setEnabled(true);
	}
}

}
