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

#include "controller/dialog_service.h"

#include <QPointer>
#include <QWindow>

/**
 * The questions and messages of the controllers in windows of the QML
 * interface (MessageWindow), over a window: they wait for the answer, as
 * message boxes do.
 */
class QmlDialogService : public DialogService
{
public:
	explicit QmlDialogService(QWindow* over = nullptr);

	StaleChoice askStale(const QString& what, bool can_reload, const QStringList& conflicts) override;
	Answer question(const QString& title, const QString& text, Buttons buttons = Buttons::YesNo) override;
	void information(const QString& title, const QString& text) override;
	void warning(const QString& title, const QString& text) override;
	void error(const QString& title, const QString& text) override;

	/** The text of the question of askStale (also logged) */
	static QString staleText(const QString& what, const QStringList& conflicts);

private:
	/**
	 * Shows a MessageWindow and waits
	 *
	 * @param kind question, information, warning or error: its icon
	 * @param standard The standard buttons (QQuickDialogButtonBox flags)
	 * @param extra Other buttons: {answer, text, role}
	 * @return The answer chosen, "" when the window was closed
	 */
	QString ask(const QString& kind, const QString& title, const QString& text, int standard, const QVariantList& extra = {});

	QPointer<QWindow> m_over;
};
