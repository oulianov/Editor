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

#include "qml_player_download.h"

QmlPlayerDownload::QmlPlayerDownload(QObject* parent) :
	PlayerDownload(parent), m_dialogs(std::make_unique<QmlDialogService>())
{
	setDialogService(m_dialogs.get());
}

QmlPlayerDownload::~QmlPlayerDownload() = default;

QWindow* QmlPlayerDownload::window() const
{
	return m_window;
}

void QmlPlayerDownload::setWindow(QWindow* window)
{
	if (m_window == window) {
		return;
	}
	m_window = window;
	m_dialogs = std::make_unique<QmlDialogService>(window);
	setDialogService(m_dialogs.get());
	emit windowChanged();
}
