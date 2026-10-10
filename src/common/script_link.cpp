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


#include "script_link.h"
#include "core.h"
#include "common/editor_settings.h"
#include "model/game_export.h"
#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>

namespace {
/** The rpgsync of the game's scripts (their .venv), else the one of the PATH */
QString rpgsync(const QDir& game) {
#ifdef Q_OS_WIN
	const QString venv = game.filePath(QStringLiteral("Scripts/.venv/Scripts/rpgsync.exe"));
#else
	const QString venv = game.filePath(QStringLiteral("Scripts/.venv/bin/rpgsync"));
#endif
	return QFileInfo(venv).isExecutable() ? venv : QStandardPaths::findExecutable(QStringLiteral("rpgsync"));
}
}

bool ScriptLink::available() {
	return core().project() && GameExporter::usesRpgsync(core().project()->projectDir());
}

void ScriptLink::open(QWidget* parent, const QStringList& location, int command) {
	if (!available() || location.isEmpty()) {
		return;
	}
	const QDir game = core().project()->projectDir();
	const QString program = rpgsync(game);
	const QString title = QApplication::translate("ScriptLink", "Open in Script Editor");
	if (program.isEmpty()) {
		QMessageBox::warning(parent, title, QApplication::translate("ScriptLink",
			"rpgsync is not installed for this game: run rpgsync in its folder first."));
		return;
	}
	QStringList args{QStringLiteral("locate"), game.absolutePath()};
	args << location;
	if (command >= 0) {
		args << QStringLiteral("--command") << QString::number(command);
	}
	// rpgsync compiles the script: a moment for a large one, the Editor waits
	auto* process = new QProcess(parent);
	QObject::connect(process, &QProcess::finished, parent, [process, parent, title](int code, QProcess::ExitStatus) {
		process->deleteLater();
		QApplication::restoreOverrideCursor();
		const QString out = QString::fromUtf8(process->readAllStandardOutput()).trimmed();
		const int colon = out.lastIndexOf(QLatin1Char(':'));
		bool ok = false;
		const int line = colon > 0 ? out.mid(colon + 1).toInt(&ok) : 0;
		if (code != 0 || !ok) {
			QString error = QString::fromUtf8(process->readAllStandardError()).trimmed();
			error.remove(QStringLiteral("❌ "));
			QMessageBox::warning(parent, title, error.isEmpty()
				? QApplication::translate("ScriptLink", "rpgsync did not find the script line.") : error);
			return;
		}
		if (!openAt(out.left(colon), line)) {
			QMessageBox::warning(parent, title, QApplication::translate("ScriptLink",
				"The script editor of the settings cannot open %1.").arg(QDir::toNativeSeparators(out.left(colon))));
		}
	});
	QObject::connect(process, &QProcess::errorOccurred, parent, [process, parent, title](QProcess::ProcessError error) {
		if (error != QProcess::FailedToStart) {
			return;
		}
		process->deleteLater();
		QApplication::restoreOverrideCursor();
		QMessageBox::warning(parent, title, QApplication::translate("ScriptLink", "rpgsync cannot be started."));
	});
	QApplication::setOverrideCursor(Qt::BusyCursor);
	// no colors nor emojis in what is read back
	QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
	env.insert(QStringLiteral("NO_COLOR"), QStringLiteral("1"));
	env.insert(QStringLiteral("RPGSYNC_ASCII"), QStringLiteral("1"));
	process->setProcessEnvironment(env);
	process->start(program, args);
}

bool ScriptLink::openAt(const QString& file, int line) {
	const QString editor = EditorSettings::instance().scriptEditor().trimmed();
	if (editor.isEmpty()) {
		// the program of the system for .py files, at the start of the file
		return QDesktopServices::openUrl(QUrl::fromLocalFile(file));
	}
	auto fill = [&](QString text, const QString& path) {
		return text.replace(QStringLiteral("{file}"), path).replace(QStringLiteral("{line}"), QString::number(line));
	};
	if (editor.contains(QStringLiteral("://"))) {
		// an editor's URL (vscode://file/{file}:{line}): no PATH needed
		return QDesktopServices::openUrl(QUrl(fill(editor, QDir::fromNativeSeparators(file))));
	}
	// a command (code -g {file}:{line}): each word filled in, the file one word
	QStringList words = QProcess::splitCommand(editor);
	if (words.isEmpty()) {
		return false;
	}
	for (QString& word : words) {
		word = fill(word, file);
	}
	const QString program = words.takeFirst();
	return QProcess::startDetached(program, words);
}
