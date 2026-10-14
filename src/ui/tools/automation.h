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

#include <QObject>
#include <QStringList>
#include <QTemporaryDir>
#include <memory>
#include "automation_quick_driver.h"

class QmlShell;

/**
 * Runs a list of Editor commands, one per line, to test the Editor or take
 * screenshots without anyone at the keyboard (see docs/Automation.adoc):
 *
 *     open-map 13
 *     open-event 13 15
 *     resize 1100 700
 *     screenshot out/event.png
 *     check-layout
 *     close
 *     quit
 *
 * The exit code is the number of failed commands and checks.
 */
class Automation : public QObject
{
	Q_OBJECT

public:
	/** window: the main window */
	Automation(QmlShell& window, const QString& script_file, QObject* parent = nullptr);

	/** False when the script could not be read */
	bool isValid() const;

	void start();

private:
	void step();
	bool run(const QStringList& args);
	void fail(const QString& message);
	/** qml-smoke: the QML stack in a window, saved as a PNG */
	bool qmlSmoke(const QString& file);
	/** edit-database-file, expect-database-file: a value of the database file on
	 *  disk; expect-database: of the project's, in memory */
	bool databaseFile(const QStringList& args);

	QmlShell& m_window;
	/** The commands on the windows (their items, the mouse, the keys) */
	AutomationQuickDriver m_quick;
	QString m_file;
	QStringList m_lines;
	int m_line = 0;
	int m_failures = 0;
	int m_delay = 0;
	bool m_valid = false;
	/** What the last dialog-service question was answered (expect-answer) */
	QString m_answer;
	/** The copy of the project open-project-copy made, removed at the end */
	std::unique_ptr<QTemporaryDir> m_projectCopy;
};
