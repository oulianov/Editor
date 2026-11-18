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
#include "game_runner.h"
#include "common/soundfont.h"
#include "core.h"
#include "defines.h"
#include "model/launcher_download.h"
#include "model/project.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <algorithm>
#include "controller/dialog_service.h"
#if defined(Q_OS_WIN)
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#endif

namespace {
std::function<bool()> before_launch;

// The value of --rtp-path: the folders in the order the Player looks in
// them, before its own. Players since 0.7.0 split it at ';', and at ':'
// out of Windows (whose paths have one); older ones skip the option and
// its value, a list or not. A folder whose path has one would be cut in
// two: left out.
QString rtpPathOption(const QStringList& folders)
{
	QStringList kept;
	for (const QString& folder : folders) {
		const QString path = QDir::toNativeSeparators(folder);
#ifdef Q_OS_WIN
		const bool cut = path.contains(QLatin1Char(';'));
#else
		const bool cut = path.contains(QLatin1Char(';')) || path.contains(QLatin1Char(':'));
#endif
		if (cut) {
			qWarning() << "RTP folder left out of the test play, the Player would cut its path:" << path;
			continue;
		}
		kept << path;
	}
	return kept.join(QDir::listSeparator());
}
}

GameRunner::Result GameRunner::launch(const QStringList& options, bool use_settings, const QString& chosen_player)
{
	const auto& project = core().project();
	if (!project) {
		return Result::Cancelled;
	}
	// the Player reads the files: unsaved maps would not be seen
	if (before_launch && !before_launch()) {
		return Result::Cancelled;
	}
	const QString player = chosen_player.isEmpty() ? findPlayer() : chosen_player;
	if (player.isEmpty()) {
		return Result::PlayerMissing;
	}

	const QString project_dir = project->projectDir().absolutePath();
	QStringList args{QStringLiteral("--project-path"), project_dir, QStringLiteral("--test-play")};
	if (use_settings) {
		QSettings settings;
		args << displayOptions(settings.value(TESTPLAY_FULLSCREEN_KEY, false).toBool(),
			settings.value(TESTPLAY_WINDOW_ZOOM_KEY, 0).toInt());
		if (!settings.value(TESTPLAY_SHOW_TITLE_KEY, true).toBool()) {
			args << QStringLiteral("--hide-title");
		}
	}
	// the RTP folders the Editor uses for the game, in its order
	const QString rtp = rtpPathOption(core().rtpDirs());
	if (!rtp.isEmpty() && !options.contains(QStringLiteral("--no-rtp"))) {
		args << QStringLiteral("--rtp-path") << rtp;
	}
	// MIDI music as the Editor plays it (Players since 0.8; older ones skip it)
	if (!options.contains(QStringLiteral("--no-audio")) && !options.contains(QStringLiteral("--soundfont"))) {
		if (const QString soundfont = testPlaySoundfont(); !soundfont.isEmpty()) {
			args << QStringLiteral("--soundfont") << QDir::toNativeSeparators(soundfont);
		}
	}
	args << options;

	// detached (it runs on when the Editor quits), its errors in a file: a
	// Player that stops at once (a library missing, the build of another
	// processor) says why, instead of nothing happening
	const QString errors = QDir::temp().filePath(QStringLiteral("easyrpg-editor-player-%1.log")
		.arg(QCoreApplication::applicationPid()));
	QFile::remove(errors);
	QProcess process;
	process.setProgram(player);
	process.setArguments(args);
	process.setWorkingDirectory(project_dir);
	process.setStandardErrorFile(errors);
	qint64 pid = 0;
	if (!process.startDetached(&pid)) {
		return Result::StartFailed;
	}
	QTimer::singleShot(EarlyExitMs, qApp, [pid, errors] {
		if (isRunning(pid)) {
			return;
		}
		QFile file(errors);
		const QString text = file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()).trimmed() : QString();
		if (text.isEmpty()) {
			return; // closed by the player, or quiet
		}
		// its last lines
		const QStringList lines = text.split(QLatin1Char('\n'));
		const QString last = lines.mid(std::max(0, int(lines.size()) - 8)).join(QLatin1Char('\n'));
		DialogService::instance().warning(QCoreApplication::translate("RunGameDialog", "Test Play"),
			QCoreApplication::translate("RunGameDialog", "EasyRPG Player stopped as soon as it started:\n%1").arg(last));
	});
#ifdef Q_OS_MACOS
	activatePlayer(pid);
#endif
	return Result::Started;
}

bool GameRunner::isRunning(qint64 pid)
{
#if defined(Q_OS_WIN)
	HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
	if (!process) {
		return false;
	}
	DWORD code = 0;
	const bool running = GetExitCodeProcess(process, &code) && code == STILL_ACTIVE;
	CloseHandle(process);
	return running;
#else
	// detached: not a child of the Editor (no zombie), gone once it ended
	return ::kill(static_cast<pid_t>(pid), 0) == 0 || errno == EPERM;
#endif
}

QStringList GameRunner::displayOptions(bool fullscreen, int window_zoom)
{
	if (fullscreen) {
		return {QStringLiteral("--fullscreen")};
	}
	QStringList options{QStringLiteral("--window")};
	// --window-zoom since Player 0.7, older ones skip the options they do not know.
	// A size the Player saved in its settings wins when the game's screen keeps 320x240.
	if (window_zoom > 0 && window_zoom <= MaxWindowZoom) {
		options << QStringLiteral("--window-zoom") << QString::number(window_zoom);
	}
	return options;
}

QString GameRunner::testPlaySoundfont()
{
	const auto& project = core().project();
	if (!project || QSettings().value(TESTPLAY_PLAYER_SOUNDFONT_KEY, false).toBool()) {
		return {};
	}
	const Soundfonts& soundfonts = Soundfonts::instance();
	for (const QString& file : {Soundfonts::gameSoundfont(project->projectDir().absolutePath()), Soundfonts::editorDefault()}) {
		if (!file.isEmpty() && soundfonts.refusal(file).isEmpty()) {
			return QFileInfo(file).absoluteFilePath();
		}
	}
	return {};
}

QStringList GameRunner::hereOptions(int map_id, int x, int y)
{
	// Straight into the map, without the title screen
	return {QStringLiteral("--new-game"),
		QStringLiteral("--start-map-id"), QString::number(map_id),
		QStringLiteral("--start-position"), QString::number(x), QString::number(y)};
}

QStringList GameRunner::battleOptions(int troop_id, int formation, int condition, int terrain_id)
{
	QStringList options{QStringLiteral("--battle-test"), QString::number(troop_id)};
	if (formation >= 0) {
		// the four values of RPG_RT's BattleTest: troop, formation, condition, terrain
		options << QString::number(formation) << QString::number(condition) << QString::number(terrain_id);
	}
	return options;
}

void GameRunner::setBeforeLaunch(std::function<bool()> callback)
{
	before_launch = std::move(callback);
}

QString GameRunner::executableOf(const QString& path)
{
	QFileInfo info(QDir::cleanPath(path));
	if (info.isDir() && info.suffix().compare(QStringLiteral("app"), Qt::CaseInsensitive) == 0) {
		QDir macos(info.absoluteFilePath() + QStringLiteral("/Contents/MacOS"));
		const QStringList files = macos.entryList(QDir::Files | QDir::Executable);
		if (files.contains(info.completeBaseName())) {
			return macos.filePath(info.completeBaseName());
		}
		return files.isEmpty() ? QString() : macos.filePath(files.first());
	}
	return info.isFile() && info.isExecutable() ? info.absoluteFilePath() : QString();
}

QString GameRunner::findPlayer()
{
	QStringList candidates;
	candidates << QSettings().value(PLAYER_PATH_KEY).toString();
	// next to the Editor, as in the release archives
	const QString app_dir = QCoreApplication::applicationDirPath();
	candidates << app_dir + "/" + PLAYER << app_dir + "/EasyRPG Player";
#ifdef Q_OS_MACOS
	// the Editor's bundle next to the Player's bundle
	candidates << QDir::cleanPath(app_dir + "/../../../EasyRPG Player.app");
#endif
	candidates << QStandardPaths::findExecutable(PLAYER);
#ifdef Q_OS_MACOS
	candidates << QStringLiteral("/Applications/EasyRPG Player.app")
		<< QDir::homePath() + "/Applications/EasyRPG Player.app";
#endif
	// at last the one downloaded for this computer (Settings, Run Game)
	candidates << Launchers::downloads(Launchers::hostPlatform());
	for (const QString& candidate : candidates) {
		if (candidate.isEmpty()) {
			continue;
		}
		QString program = executableOf(candidate);
		// one built for another processor (the x86_64 Player on ARM) does not run
		if (!program.isEmpty() && Launchers::runsHere(program)) {
			return program;
		}
	}
	return QString();
}
