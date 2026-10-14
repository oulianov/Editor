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

#include <QRectF>
#include <QStringList>
#include <functional>

class QQuickItem;
class QQuickWindow;

/**
 * The commands of the scripts (docs/Automation.adoc) on the windows of the
 * QML interface: items found by their objectName (or the text of a button),
 * the mouse and the keys sent as the window system sends them, texts and
 * states read from the properties of the Qt Quick Controls.
 */
class AutomationQuickDriver
{
public:
	/** Reports a failure of the script (with its line) */
	using Fail = std::function<void(const QString&)>;

	explicit AutomationQuickDriver(Fail fail);

	/**
	 * The window the user works in: a modal one, else the last one
	 * QmlHost opened that is shown
	 */
	static QQuickWindow* frontWindow();

	/**
	 * Runs a command on window
	 *
	 * @param handled false when the command is not one of the windows'
	 *  (it is left to the commands of Automation)
	 * @return Whether it succeeded
	 */
	bool run(QQuickWindow* window, const QStringList& args, bool* handled);

	/** What check-layout finds wrong in window: each is a failure */
	int checkLayout(QQuickWindow* window);
	/** check-disabled: the disabled controls of window drawn as enabled ones, each a failure */
	int checkDisabled(QQuickWindow* window);

	/** The text an item shows (a field, a list, the tabs of a bar...) */
	static QString itemText(QQuickItem* item);

private:
	QQuickItem* findItem(QQuickWindow* window, const QString& name) const;
	QObject* findObject(QQuickWindow* window, const QString& name) const;
	/** Scrolls what item is in (a Flickable) to show rect, of the item */
	void scrollTo(QQuickItem* item, const QRectF& rect);
	void click(QQuickWindow* window, QQuickItem* item, bool double_click = false);
	void sendKey(QQuickWindow* window, int key, Qt::KeyboardModifiers modifiers, const QString& text);
	bool setControl(QQuickItem* item, const QString& value);

	Fail m_fail;
	ulong m_timestamp = 0;
};
