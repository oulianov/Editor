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
#include "core.h"
#include "defines.h"
#include "model/launcher_download.h"
#include "model/project.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>

namespace {
std::function<bool()> before_launch;
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
		args << (settings.value(TESTPLAY_FULLSCREEN_KEY, false).toBool() ? QStringLiteral("--fullscreen") : QStringLiteral("--window"));
		if (!settings.value(TESTPLAY_SHOW_TITLE_KEY, true).toBool()) {
			args << QStringLiteral("--hide-title");
		}
	}
	// the RTP the Editor uses
	const QString rtp = QSettings().value(RTP_KEY).toString();
	if (!rtp.isEmpty() && QFileInfo(rtp).isDir() && !options.contains(QStringLiteral("--no-rtp"))) {
		args << QStringLiteral("--rtp-path") << rtp;
	}
	args << options;

	qint64 pid = 0;
	if (!QProcess::startDetached(player, args, project_dir, &pid)) {
		return Result::StartFailed;
	}
#ifdef Q_OS_MACOS
	activatePlayer(pid);
#endif
	return Result::Started;
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
		if (!program.isEmpty()) {
			return program;
		}
	}
	return QString();
}
