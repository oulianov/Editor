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

#include <QList>
#include <QString>
#include <QVariantMap>
#include <functional>

class QQuickWindow;
class QWindow;

/**
 * The windows of the QML interface (types of the module org.easyrpg.editor:
 * "SettingsWindow"...), opened from C++: over another window, centered on it.
 *
 * A dialog window says how it was closed in its `result` property
 * (Rejected or Accepted), see DialogWindow.qml.
 */
namespace QmlHost {
	/** The `result` of a dialog window */
	enum Result { Rejected = 0, Accepted = 1 };

	/**
	 * Shows a window of the module; it is deleted once closed. A window
	 * with a stateKey property opens where and as large as it was when last
	 * closed (WorkspaceStore::dialogGeometry), else centered on over.
	 *
	 * @param type The QML type ("HelpWindow")
	 * @param over The window it belongs to (nullptr: none)
	 * @param properties Its initial properties
	 * @return The window, nullptr when it could not be made (logged)
	 */
	QQuickWindow* open(const QString& type, QWindow* over = nullptr, const QVariantMap& properties = {});

	/**
	 * Makes a window of the module without showing it, kept until its
	 * owner deletes it (the main window); it is one of its windows once shown
	 *
	 * @return The window, nullptr when it could not be made (logged)
	 */
	QQuickWindow* make(const QString& type, const QVariantMap& properties = {});

	/**
	 * Shows a window of the module, modal, and returns once it is closed.
	 * Meanwhile it holds an EditLock::Dialog (unless the window has its own,
	 * ownEditLock): the project is not read again from disk under it.
	 *
	 * @param closed Reads the window once closed, before it goes (its answer...)
	 * @return Its `result` (0 when it could not be made)
	 */
	int exec(const QString& type, QWindow* over = nullptr, const QVariantMap& properties = {},
		const std::function<void(QQuickWindow*)>& closed = {});

	/**
	 * The guide (HelpWindow) at a topic (an anchor of it), next to over: one
	 * guide per window, usable while that window is modal
	 */
	QQuickWindow* help(QWindow* over, const QString& topic = QString());

	/** The windows it opened that are shown, the last opened last */
	QList<QQuickWindow*> windows();

	/**
	 * The window of the QML interface the user works in, nullptr when it
	 * is not one: the modal window, else the active one (with keyboard
	 * focus), when it is a QML window it opened
	 */
	QQuickWindow* frontWindow();
}
