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

#include "dialog_scope.h"
#include "common/logging.h"
#include "qml_dialog_service.h"

DialogScope::DialogScope(QObject* parent) : QObject(parent)
{
}

QWindow* DialogScope::window() const
{
	return m_window;
}

void DialogScope::setWindow(QWindow* window)
{
	if (m_window != window) {
		m_window = window;
		emit windowChanged();
	}
}

QJSValue DialogScope::run(QJSValue function)
{
	if (!function.isCallable()) {
		return QJSValue();
	}
	QmlDialogService dialogs(m_window);
	DialogService* previous = &DialogService::instance();
	DialogService::install(&dialogs);
	const QJSValue result = function.call();
	DialogService::install(previous);
	if (result.isError()) {
		qCWarning(lcQml).noquote() << result.toString();
	}
	return result;
}
