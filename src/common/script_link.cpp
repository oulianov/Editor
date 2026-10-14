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
#include <QCoreApplication>
#include <QCursor>
#include <QGuiApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>
#include <lcf/rpg/treemap.h>

namespace {
/**
 * The rpgsync of the game's scripts (the Python of their .venv), else the one
 * of the PATH: the program and its first arguments. Its own launcher is not
 * run: a game folder copied or renamed keeps it pointing at the old folder
 * (rpgsync then writes the launchers again for the new one)
 */
QStringList rpgsync(const QDir& game) {
#ifdef Q_OS_WIN
	const QString python = game.filePath(QStringLiteral("Scripts/.venv/Scripts/python.exe"));
#else
	const QString python = game.filePath(QStringLiteral("Scripts/.venv/bin/python"));
#endif
	if (QFileInfo(python).isExecutable()) {
		return {python, QStringLiteral("-m"), QStringLiteral("rpgsync")};
	}
	const QString program = QStandardPaths::findExecutable(QStringLiteral("rpgsync"));
	return program.isEmpty() ? QStringList() : QStringList{program};
}
}

bool ScriptLink::available() {
	return core().project() && GameExporter::usesRpgsync(core().project()->projectDir());
}

QString ScriptLink::unavailableReason() {
	if (!available()) {
		return QCoreApplication::translate("ScriptLink", "Set up rpgsync for this game to open its events as Python scripts: "
			"install rpgsync (github.com/oulianov/rpgsync), then run rpgsync in the game folder.");
	}
	if (rpgsync(core().project()->projectDir()).isEmpty()) {
		return QCoreApplication::translate("ScriptLink", "rpgsync is not installed: install it "
			"(github.com/oulianov/rpgsync), or run rpgsync in the game folder to create the scripts' .venv.");
	}
	return {};
}

QString ScriptLink::title() {
	return QCoreApplication::translate("ScriptLink", "Open in Script Editor");
}

void ScriptLink::open(QObject* context, const QStringList& location, int command,
		std::function<void(const QString& error)> failed) {
	if (!available() || location.isEmpty()) {
		return;
	}
	const QDir game = core().project()->projectDir();
	QStringList args = rpgsync(game);
	if (args.isEmpty()) {
		failed(QCoreApplication::translate("ScriptLink",
			"rpgsync is not installed for this game: run rpgsync in its folder first."));
		return;
	}
	const QString program = args.takeFirst();
	args << QStringLiteral("locate") << game.absolutePath() << location;
	if (command >= 0) {
		args << QStringLiteral("--command") << QString::number(command);
	}
	// rpgsync compiles the script: a moment for a large one, the Editor waits
	auto* process = new QProcess(context);
	QObject::connect(process, &QProcess::finished, context, [process, failed](int code, QProcess::ExitStatus) {
		process->deleteLater();
		QGuiApplication::restoreOverrideCursor();
		const QString out = QString::fromUtf8(process->readAllStandardOutput()).trimmed();
		const int colon = out.lastIndexOf(QLatin1Char(':'));
		bool ok = false;
		const int line = colon > 0 ? out.mid(colon + 1).toInt(&ok) : 0;
		if (code != 0 || !ok) {
			QString error = QString::fromUtf8(process->readAllStandardError()).trimmed();
			error.remove(QStringLiteral("❌ "));
			failed(error.isEmpty()
				? QCoreApplication::translate("ScriptLink", "rpgsync did not find the script line.") : error);
			return;
		}
		if (!openAt(out.left(colon), line)) {
			failed(QCoreApplication::translate("ScriptLink",
				"The script editor of the settings cannot open %1.").arg(QDir::toNativeSeparators(out.left(colon))));
		}
	});
	QObject::connect(process, &QProcess::errorOccurred, context, [process, failed](QProcess::ProcessError error) {
		if (error != QProcess::FailedToStart) {
			return;
		}
		process->deleteLater();
		QGuiApplication::restoreOverrideCursor();
		failed(QCoreApplication::translate("ScriptLink", "rpgsync cannot be started."));
	});
	QGuiApplication::setOverrideCursor(Qt::BusyCursor);
	// no colors nor emojis in what is read back
	QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
	env.insert(QStringLiteral("NO_COLOR"), QStringLiteral("1"));
	env.insert(QStringLiteral("RPGSYNC_ASCII"), QStringLiteral("1"));
	process->setProcessEnvironment(env);
	process->start(program, args);
}

ScriptLink::MapTreeLocations ScriptLink::mapTreeLocations(int id, int type) {
	const QString node = QString::number(id);
	switch (type) {
	case lcf::rpg::TreeMap::MapType_map:
		return {{QStringLiteral("--map"), node}, {QStringLiteral("--map-tree"), node}};
	case lcf::rpg::TreeMap::MapType_area:
		return {{}, {QStringLiteral("--map-tree"), node}};
	case lcf::rpg::TreeMap::MapType_root:
		// rpgsync opens the tree at its start positions
		return {{}, {QStringLiteral("--map-tree"), QStringLiteral("0")}};
	default:
		return {};
	}
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
