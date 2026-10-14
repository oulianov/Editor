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
#include <functional>

// Settings of the test play
#define PLAYER_PATH_KEY "player_path"
#define TESTPLAY_FULLSCREEN_KEY "testplay_fullscreen"
#define TESTPLAY_SHOW_TITLE_KEY "testplay_show_title"

/**
 * Starts the EasyRPG Player on the open project: test play, from a map
 * position, battle test. What to tell or ask the user (the Player is
 * missing, it could not start) is the interface's: the result says it.
 */
class GameRunner {
public:
	enum class Result {
		Started,
		/** No project open, or the call before the launch said no */
		Cancelled,
		/** No Player program found (findPlayer) */
		PlayerMissing,
		/** The Player program did not start */
		StartFailed
	};

	/**
	 * Starts the Player in test play on the open project, with the Full
	 * Screen and Show Title settings unless use_settings is false. player:
	 * the program, else the one findPlayer finds.
	 */
	static Result launch(const QStringList& options, bool use_settings = true, const QString& player = QString());

	/** The options of a test play from a map position (Start Game Here) */
	static QStringList hereOptions(int map_id, int x, int y);

	/**
	 * The options of a battle test against a monster group. The party and
	 * the background are those saved in the database (System); the RPG
	 * Maker 2003 formation (lcf::rpg::System::BattleFormation, -1: only the
	 * group is given), condition and terrain are given to the Player.
	 */
	static QStringList battleOptions(int troop_id, int formation = -1, int condition = 0, int terrain_id = 1);

	/**
	 * The Player program: the one of the settings, next to the Editor (as
	 * in the release archives), in the PATH, in Applications on macOS, else
	 * the one downloaded (PlayerDownload). Empty when there is none.
	 */
	static QString findPlayer();

	/** The program inside a macOS application bundle, or the path itself; empty if it is not a program */
	static QString executableOf(const QString& path);

	/** Called before the Player starts (to save the maps): false cancels */
	static void setBeforeLaunch(std::function<bool()> callback);

private:
#ifdef Q_OS_MACOS
	/** Brings the Player in front once it runs (macOS gives it no focus) */
	static void activatePlayer(qint64 pid);
#endif
};
