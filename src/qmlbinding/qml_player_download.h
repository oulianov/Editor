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

#include <QPointer>
#include <QWindow>
#include <QtQmlIntegration>
#include <memory>
#include "model/player_download.h"
#include "qml_dialog_service.h"

/**
 * The download of the EasyRPG Player for QML windows (Ez.PlayerDownload):
 * its failures are shown over the window given.
 */
class QmlPlayerDownload : public PlayerDownload
{
	Q_OBJECT
	QML_NAMED_ELEMENT(PlayerDownload)
	/** The window the failures are shown over */
	Q_PROPERTY(QWindow* window READ window WRITE setWindow NOTIFY windowChanged)

public:
	explicit QmlPlayerDownload(QObject* parent = nullptr);
	~QmlPlayerDownload() override;

	QWindow* window() const;
	void setWindow(QWindow* window);

signals:
	void windowChanged();

private:
	QPointer<QWindow> m_window;
	std::unique_ptr<QmlDialogService> m_dialogs;
};
