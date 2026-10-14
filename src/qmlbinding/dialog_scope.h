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

#include <QJSValue>
#include <QObject>
#include <QPointer>
#include <QWindow>
#include <QtQmlIntegration>

/**
 * The questions and messages of the models over a QML window
 * (Ez.DialogScope): run(function) calls the function with the
 * DialogService of the window installed, the one before back after.
 *
 *   Ez.DialogScope { id: dialogs; window: root }
 *   onClicked: dialogs.run(() => controller.start())
 */
class DialogScope : public QObject
{
	Q_OBJECT
	QML_ELEMENT
	Q_PROPERTY(QWindow* window READ window WRITE setWindow NOTIFY windowChanged)

public:
	explicit DialogScope(QObject* parent = nullptr);

	QWindow* window() const;
	void setWindow(QWindow* window);

	/** Calls function with the questions over the window: what it returns */
	Q_INVOKABLE QJSValue run(QJSValue function);

signals:
	void windowChanged();

private:
	QPointer<QWindow> m_window;
};
