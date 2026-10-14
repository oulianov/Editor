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

#include <QString>
#include <QStringList>
#include "model/stale_choice.h"

/**
 * How the controllers ask the user and tell them about failures, without
 * knowing the interface: the main window installs one (QmlShellDialogService).
 * The questions wait for the answer.
 *
 * Without one, nothing is asked: questions get their safe answer (Cancel,
 * No, nothing overwritten) and messages are only logged.
 */
class DialogService
{
public:
	enum class Answer { Yes, No, Cancel };
	enum class Buttons { YesNo, YesNoCancel };

	virtual ~DialogService();

	/** The service of the interface (a quiet one when none is installed) */
	static DialogService& instance();
	/** Installs the service of the interface (nullptr: the quiet one); it
	 *  stays the interface's */
	static void install(DialogService* service);

	/** What to do with data whose file another program changed (see
	 *  StaleChoice); what: "The map ..."; conflicts: what both sides
	 *  changed differently */
	virtual StaleChoice askStale(const QString& what, bool can_reload, const QStringList& conflicts);
	virtual Answer question(const QString& title, const QString& text, Buttons buttons = Buttons::YesNo);
	virtual void information(const QString& title, const QString& text);
	virtual void warning(const QString& title, const QString& text);
	virtual void error(const QString& title, const QString& text);
};
