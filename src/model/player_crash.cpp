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

#include "player_crash.h"
#include <QClipboard>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QProcess>
#include <QRegularExpression>
#include <QSysInfo>
#include <algorithm>
#include "model/editor_update.h"

namespace {

/** The lines of the log kept in the report */
constexpr int kLogLines = 60;
/** The longest address of a new issue browsers and GitHub take */
constexpr int kIssueUrlLength = 7000;
/** The repository of the issues when the Editor has none for its updates */
constexpr char kIssueRepository[] = "oulianov/Editor";

/** The user's home folder written ~: the report is meant to be published */
QString withoutHome(QString text) {
	const QString home = QDir::homePath();
	if (!home.isEmpty() && home != QLatin1String("/")) {
		text.replace(home, QStringLiteral("~"));
		text.replace(QDir::toNativeSeparators(home), QStringLiteral("~"));
	}
	return text;
}

/** The newest file of folder matching patterns, written since started (a moment before: the clocks of file systems) */
QString newestSince(const QString& folder, const QStringList& patterns, const QDateTime& started) {
	const QFileInfoList files = QDir(folder).entryInfoList(patterns, QDir::Files, QDir::Time);
	if (files.isEmpty() || files.first().lastModified() < started.addSecs(-2)) {
		return {};
	}
	return files.first().absoluteFilePath();
}

#ifdef Q_OS_MACOS
/** The crash report macOS wrote for the process pid of program, since started */
QString macCrashReport(const QString& program, qint64 pid, const QDateTime& started) {
	const QString folder = QDir::home().filePath(QStringLiteral("Library/Logs/DiagnosticReports"));
	const QString name = QFileInfo(program).fileName();
	const QFileInfoList files = QDir(folder).entryInfoList({name + QStringLiteral("*.ips"), name + QStringLiteral("*.crash")},
		QDir::Files, QDir::Time);
	const QRegularExpression pidField(QStringLiteral("\"pid\"\\s*:\\s*(\\d+)|^Process:.*\\[(\\d+)\\]"),
		QRegularExpression::MultilineOption);
	for (const QFileInfo& file : files) {
		if (file.lastModified() < started.addSecs(-2)) {
			break;
		}
		QFile report(file.absoluteFilePath());
		if (!report.open(QIODevice::ReadOnly)) {
			continue;
		}
		const auto match = pidField.match(QString::fromUtf8(report.read(64 * 1024)));
		const QString found = match.captured(1).isEmpty() ? match.captured(2) : match.captured(1);
		if (found.isEmpty() || found.toLongLong() == pid) {
			return file.absoluteFilePath();
		}
	}
	return {};
}
#endif

/** What the Player wrote in its log since it started (its end); the whole end when the log was cut meanwhile */
QStringList logSince(const QString& path, qint64 size) {
	QFile file(path);
	if (path.isEmpty() || !file.open(QIODevice::ReadOnly)) {
		return {};
	}
	const qint64 now = file.size();
	if (now < size || !file.seek(size < now ? std::max(size, now - 256 * 1024) : now)) {
		file.seek(std::max<qint64>(0, now - 64 * 1024));
	}
	QStringList lines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'));
	for (QString& line : lines) {
		line = line.trimmed();
	}
	lines.removeAll(QString());
	return lines;
}

} // namespace

QString PlayerCrash::logPathOf(const QStringList& arguments) {
	const qsizetype given = arguments.indexOf(QStringLiteral("--log-file"));
	if (given >= 0 && given + 1 < arguments.size()) {
		return arguments[given + 1];
	}
	// Game_Config::GetLogFileOutput: ORGANIZATION_NAME "-" APPLICATION_NAME ".log"
	const QString name = QStringLiteral("EasyRPG-Player.log");
#ifdef Q_OS_WIN
	const QString local = qEnvironmentVariable("LOCALAPPDATA");
	return local.isEmpty() ? QString() : QDir(local).filePath(name);
#else
	const QString state = qEnvironmentVariable("XDG_STATE_HOME");
	return state.isEmpty() ? QDir::home().filePath(QStringLiteral(".local/state/") + name) : QDir(state).filePath(name);
#endif
}

QString PlayerCrash::exceptionName(quint32 code) {
	switch (code) {
		case 0xC0000005: return QStringLiteral("access violation");
		case 0xC00000FD: return QStringLiteral("stack overflow");
		case 0xC0000409: return QStringLiteral("stack buffer overrun or fail fast");
		case 0xC000001D: return QStringLiteral("illegal instruction");
		case 0xC0000094: return QStringLiteral("integer division by zero");
		case 0xC0000374: return QStringLiteral("heap corruption");
		case 0xC0000135: return QStringLiteral("DLL not found");
		case 0xC0000139: return QStringLiteral("entry point not found");
		case 0xC0000142: return QStringLiteral("DLL initialization failed");
		case 0x80000003: return QStringLiteral("breakpoint");
		case 0xE06D7363: return QStringLiteral("C++ exception not caught");
		default: break;
	}
	// another exception (NTSTATUS of an error)
	return (code & 0xF0000000) == 0xC0000000 ? QStringLiteral("exception") : QString();
}

std::unique_ptr<PlayerCrash> PlayerCrash::find(const Launch& launch, std::optional<quint32> exitCode) {
	std::unique_ptr<PlayerCrash> crash(new PlayerCrash());
	// WindowsUtils::CreateMiniDump of the Player: in its working folder
	crash->m_dump = newestSince(launch.projectDir, {QStringLiteral("Player_*.dmp")}, launch.started);
#ifdef Q_OS_MACOS
	if (crash->m_dump.isEmpty()) {
		crash->m_dump = macCrashReport(launch.player, launch.pid, launch.started);
	}
#endif
	const QString exception = exitCode ? exceptionName(*exitCode) : QString();
	if (crash->m_dump.isEmpty() && exception.isEmpty()) {
		return nullptr;
	}
	crash->m_launch = launch;
	if (!exception.isEmpty()) {
		crash->m_cause = QStringLiteral("exception 0x%1 (%2)")
			.arg(QString::number(*exitCode, 16).toUpper().rightJustified(8, QLatin1Char('0')), exception);
	} else if (crash->m_dump.endsWith(QLatin1String(".dmp"))) {
		crash->m_cause = QStringLiteral("minidump written");
	} else {
		crash->m_cause = QStringLiteral("crash report of macOS");
		// "exception" : {..."type":"EXC_BAD_ACCESS","signal":"SIGSEGV"...}
		QFile report(crash->m_dump);
		if (report.open(QIODevice::ReadOnly)) {
			static const QRegularExpression exception(
				QStringLiteral("\"exception\"\\s*:\\s*\\{[^}]*\"type\"\\s*:\\s*\"([^\"]+)\"[^}]*\"signal\"\\s*:\\s*\"([^\"]+)\""));
			if (const auto match = exception.match(QString::fromUtf8(report.read(256 * 1024))); match.hasMatch()) {
				crash->m_cause = QStringLiteral("%1 (%2)").arg(match.captured(1), match.captured(2));
			}
		}
	}
	crash->m_log = logSince(launch.logPath, launch.logSize);
	// "EasyRPG Player 0.8.1.1 (2025-...) started": the first line of the log of a start
	static const QRegularExpression version(QStringLiteral("EasyRPG Player (.+) started"));
	for (const QString& line : std::as_const(crash->m_log)) {
		if (const auto match = version.match(line); match.hasMatch()) {
			crash->m_playerVersion = match.captured(1).trimmed();
		}
	}
	if (crash->m_log.size() > kLogLines) {
		crash->m_log = crash->m_log.mid(crash->m_log.size() - kLogLines);
	}
	return crash;
}

QString PlayerCrash::dumpPath() const {
	return m_dump;
}

QString PlayerCrash::dumpName() const {
	return QFileInfo(m_dump).fileName();
}

QString PlayerCrash::cause() const {
	return m_cause;
}

QString PlayerCrash::report() const {
	QString text = QStringLiteral("EasyRPG Player crashed during a test play started by EasyRPG Editor %1.\n\n")
		.arg(QCoreApplication::applicationVersion());
	text += QStringLiteral("- Player: %1\n").arg(m_playerVersion.isEmpty()
		? QFileInfo(m_launch.player).fileName() : m_playerVersion);
	text += QStringLiteral("- System: %1 (%2)\n").arg(QSysInfo::prettyProductName(), QSysInfo::currentCpuArchitecture());
	text += QStringLiteral("- Crash: %1\n").arg(m_cause);
	if (!m_dump.isEmpty()) {
		text += QStringLiteral("- Dump: %1\n").arg(dumpName());
	}
	text += QStringLiteral("- Command line: %1\n").arg(withoutHome(m_launch.arguments.join(QLatin1Char(' '))));
	if (!m_log.isEmpty()) {
		text += QStringLiteral("\nThe end of the Player's log:\n```\n%1\n```\n").arg(withoutHome(m_log.join(QLatin1Char('\n'))));
	} else {
		text += QStringLiteral("\nThe Player's log has nothing of this start (logging off, or another log file).\n");
	}
	return text;
}

void PlayerCrash::reveal() const {
	const QString file = m_dump.isEmpty() ? m_launch.projectDir : m_dump;
	if (file.isEmpty()) {
		return;
	}
#if defined(Q_OS_MACOS)
	QProcess::startDetached(QStringLiteral("open"), {QStringLiteral("-R"), file});
#elif defined(Q_OS_WIN)
	QProcess::startDetached(QStringLiteral("explorer.exe"), {QStringLiteral("/select,") + QDir::toNativeSeparators(file)});
#else
	// the file managers show the folder; the file itself needs their D-Bus interface
	QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(file).isDir() ? file : QFileInfo(file).absolutePath()));
#endif
}

void PlayerCrash::copyReport() const {
	QGuiApplication::clipboard()->setText(report());
}

QUrl PlayerCrash::issueUrl() const {
	QString repository = EditorUpdates::repository();
	if (repository.isEmpty()) {
		repository = QString::fromLatin1(kIssueRepository);
	}
	const QString title = QStringLiteral("EasyRPG Player crash in test play: %1").arg(m_cause);
	const QString attach = m_dump.isEmpty() ? QString()
		: QStringLiteral("\nThe crash dump: %1 (in the game folder). Put it in a .zip and drag it here to attach it.\n")
			.arg(dumpName());
	// the log shortened from its start until the address is not too long
	PlayerCrash shortened;
	shortened.m_launch = m_launch;
	shortened.m_dump = m_dump;
	shortened.m_cause = m_cause;
	shortened.m_playerVersion = m_playerVersion;
	shortened.m_log = m_log;
	for (;;) {
		// every character but the unreserved ones encoded: a + of the log is not a space
		const QByteArray encoded = QStringLiteral("https://github.com/%1/issues/new?title=").arg(repository).toUtf8()
			+ QUrl::toPercentEncoding(title) + "&body=" + QUrl::toPercentEncoding(shortened.report() + attach);
		if (encoded.size() <= kIssueUrlLength || shortened.m_log.isEmpty()) {
			return QUrl::fromEncoded(encoded, QUrl::StrictMode);
		}
		shortened.m_log.removeFirst();
	}
}

void PlayerCrash::openIssue() const {
	QDesktopServices::openUrl(issueUrl());
}
