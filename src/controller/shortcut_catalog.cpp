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

#include "shortcut_catalog.h"
#include <QHash>
#include <algorithm>

namespace {

struct Keys {
	QList<QKeySequence> keys;
	QKeySequence::StandardKey standard = QKeySequence::UnknownKey;
	ShortcutCatalog::Context context = ShortcutCatalog::Context::Window;
};

/** The first binding of a standard key, as QAction::setShortcut(StandardKey) takes it */
QList<QKeySequence> first(QKeySequence::StandardKey key)
{
	const QList<QKeySequence> bindings = QKeySequence::keyBindings(key);
	return bindings.isEmpty() ? QList<QKeySequence>() : QList<QKeySequence>{bindings.first()};
}

const QHash<QString, Keys>& table()
{
	using ShortcutCatalog::Context;
	static const QHash<QString, Keys> keys = [] {
		QHash<QString, Keys> keys;
		auto key = [&keys](const char* action, const QKeySequence& sequence, Context context = Context::Window) {
			keys.insert(QLatin1String(action), Keys{{sequence}, QKeySequence::UnknownKey, context});
		};
		key("actionProjectOpen", QKeySequence(Qt::CTRL | Qt::Key_O));
		key("actionProjectNew", QKeySequence(Qt::CTRL | Qt::Key_N));
		key("actionMapSave", QKeySequence(Qt::CTRL | Qt::Key_S));
		key("actionMapSaveAll", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
		key("actionMapRevert", QKeySequence(Qt::CTRL | Qt::Key_R));
		key("actionMapGoTo", QKeySequence(Qt::CTRL | Qt::Key_G));
		// RPG Maker 2003's
		key("actionLayerLower", QKeySequence(Qt::Key_F5));
		key("actionLayerUpper", QKeySequence(Qt::Key_F6));
		key("actionLayerEvents", QKeySequence(Qt::Key_F7));
		key("actionDatabase", QKeySequence(Qt::Key_F8));
		key("actionPlayTest", QKeySequence(Qt::Key_F9));
		key("actionZoomIn", QKeySequence(Qt::Key_Plus));
		key("actionZoomOut", QKeySequence(Qt::Key_Minus));
		key("actionZoom", QKeySequence(Qt::Key_1));
		key("actionDrawPen", QKeySequence(Qt::Key_2));
		key("actionDrawRectangle", QKeySequence(Qt::Key_3));
		key("actionDrawCircle", QKeySequence(Qt::Key_4));
		key("actionDrawFill", QKeySequence(Qt::Key_5));
		key("actionDrawSelect", QKeySequence(Qt::Key_6));
		// the map tree's: Ctrl+C elsewhere copies tiles or events
		key("actionMapCopy", QKeySequence(Qt::CTRL | Qt::Key_C), Context::MapTree);
		key("actionMapPaste", QKeySequence(Qt::CTRL | Qt::Key_V), Context::MapTree);
		key("actionMapShift", QKeySequence(Qt::CTRL | Qt::Key_H), Context::MapTree);
		// the Delete key of Mac keyboards is Backspace
		const QList<QKeySequence> remove =
#ifdef Q_OS_MACOS
			{QKeySequence(Qt::Key_Delete), QKeySequence(Qt::Key_Backspace)};
#else
			{QKeySequence(Qt::Key_Delete)};
#endif
		keys.insert(QStringLiteral("actionMapDelete"), Keys{remove, QKeySequence::UnknownKey, Context::MapTree});
		keys.insert(QStringLiteral("actionAreaDelete"), Keys{remove, QKeySequence::UnknownKey, Context::MapTree});
		// the platform's: Shift+Cmd+Z on macOS, Ctrl+Y and Ctrl+Shift+Z elsewhere...
		keys.insert(QStringLiteral("actionQuit"), Keys{QKeySequence::keyBindings(QKeySequence::Quit), QKeySequence::Quit});
		keys.insert(QStringLiteral("actionSettings"), Keys{first(QKeySequence::Preferences), QKeySequence::Preferences});
		keys.insert(QStringLiteral("actionContents"), Keys{first(QKeySequence::HelpContents), QKeySequence::HelpContents});
		keys.insert(QStringLiteral("actionUndo"), Keys{QKeySequence::keyBindings(QKeySequence::Undo), QKeySequence::Undo});
		keys.insert(QStringLiteral("actionRedo"), Keys{QKeySequence::keyBindings(QKeySequence::Redo), QKeySequence::Redo});
		return keys;
	}();
	return keys;
}

}

QList<QKeySequence> ShortcutCatalog::shortcuts(const QString& action)
{
	return table().value(action).keys;
}

QKeySequence::StandardKey ShortcutCatalog::standardKey(const QString& action)
{
	return table().value(action).standard;
}

ShortcutCatalog::Context ShortcutCatalog::context(const QString& action)
{
	return table().value(action).context;
}

QStringList ShortcutCatalog::actions()
{
	QStringList actions = table().keys();
	actions.sort();
	return actions;
}

bool ShortcutCatalog::sharesKeys(const QString& action, const QString& other)
{
	static const QStringList deletes = {QStringLiteral("actionMapDelete"), QStringLiteral("actionAreaDelete")};
	return action != other && deletes.contains(action) && deletes.contains(other);
}
