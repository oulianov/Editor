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

#include <QDateTime>
#include <QObject>
#include <QStringList>
#include <QUrl>
#include <memory>
#include <optional>

/**
 * A crash of the EasyRPG Player a test play started (GameRunner), as the
 * window that tells it shows it (PlayerCrashWindow): what the system kept of
 * it (the Player's minidump on Windows, Player_<date>.dmp in the game folder;
 * the crash report of macOS), the end of the Player's log, and a report to
 * copy or to open as a GitHub issue.
 */
class PlayerCrash : public QObject
{
	Q_OBJECT
	Q_PROPERTY(QString report READ report CONSTANT)
	Q_PROPERTY(QString dumpPath READ dumpPath CONSTANT)
	Q_PROPERTY(QString dumpName READ dumpName CONSTANT)
	Q_PROPERTY(QString cause READ cause CONSTANT)

public:
	/** A Player started: what finding its crash needs */
	struct Launch {
		qint64 pid = 0;
		QDateTime started;
		/** The program */
		QString player;
		QStringList arguments;
		/** Its working folder: the game's, where the Windows Player writes its dump */
		QString projectDir;
		/** Its log file, and its size when it started (what it wrote since) */
		QString logPath;
		qint64 logSize = 0;
	};

	/**
	 * The crash of the Player launch started once it ended: a dump of it
	 * written since, or an exit code of a crash (Windows: an exception,
	 * 0xC0000005...). nullptr when it ended normally.
	 */
	static std::unique_ptr<PlayerCrash> find(const Launch& launch, std::optional<quint32> exitCode);

	/** The log the Player writes by default (or the one --log-file gives in arguments) */
	static QString logPathOf(const QStringList& arguments);

	/** The text of a Windows exit code of a crash ("access violation"); empty when it is not one */
	static QString exceptionName(quint32 code);

	/** What to copy or report: the Player, the system, how it crashed, the end of its log */
	QString report() const;
	/** The dump of the crash (empty: none was written) */
	QString dumpPath() const;
	QString dumpName() const;
	/** How it crashed: "exception 0xC0000005 (access violation)", "crash report of macOS"... */
	QString cause() const;

	/** Shows the dump in the file manager (the game folder without one) */
	Q_INVOKABLE void reveal() const;
	/** Puts the report in the clipboard */
	Q_INVOKABLE void copyReport() const;
	/** Opens a new GitHub issue in the browser, the report in it */
	Q_INVOKABLE void openIssue() const;
	/** The address of that issue */
	QUrl issueUrl() const;

private:
	PlayerCrash() = default;

	Launch m_launch;
	QString m_dump;
	QString m_cause;
	QString m_playerVersion;
	QStringList m_log;
};
