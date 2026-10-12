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

#include "common/rpg_rt_exe.h"

#include <QMap>
#include <QString>
#include <QStringList>
#include <optional>
#include <vector>

class Project;

/**
 * The patches of a game, as EasyRPG Player decides them (Game_ConfigGame,
 * Player::CreateGameObjects): the [Patch] section of the EasyRPG.ini of the
 * game when it has one, else detected from the files of the game (the
 * version of RPG_RT.exe, accord.dll, dynloader.dll...).
 */
namespace GamePatches {
	/** A patch the Editor lets choose: its key in [Patch], its name, what it changes */
	struct Patch {
		const char* key;
		const char* name;
		const char* text;
	};
	const std::vector<Patch>& list();
	/** The explanation of a patch, as rich text */
	QString explain(const Patch& patch);

	struct State {
		/** Chosen in EasyRPG.ini ([Patch]), else detected */
		bool chosen = false;
		/** The value of each patch of the list (0: off; Maniac: its version) */
		QMap<QString, int> values;
		/** The other keys of [Patch] (AntiLagSwitch...): with them, nothing is detected */
		QStringList others;
	};
	State read(const Project& project);
	/** What the files of the game tell, without EasyRPG.ini */
	QMap<QString, int> detect(const Project& project);

	/** Writes the patches in EasyRPG.ini, or (none) removes its [Patch] section to detect them */
	bool write(const Project& project, const std::optional<QMap<QString, int>>& values);

	/** [Game] Engine of EasyRPG.ini: the engine the Player is told (2003v105...), else empty */
	QString engineSetting(const Project& project);
	/** [Game] EnginePath of EasyRPG.ini: the program read instead of RPG_RT.exe */
	QString enginePathSetting(const Project& project);
	/** The program the Player reads to know the engine and the Maniac Patch: empty when none */
	QString engineExe(const Project& project);
	/** What a program is, read once per version of the file */
	RpgRtExe::Info exeInfo(const QString& exe);

	/** Whether the game uses the Maniac Patch: its commands are offered */
	bool usesManiac(const Project& project);
}
