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

#include "game_setup.h"
#include "core.h"
#include "defines.h"
#include "model/game_patches.h"
#include "model/project.h"
#include "model/project_watcher.h"
#include "common/rpg_rt_exe.h"

GameSetup& GameSetup::instance() {
	static GameSetup setup;
	return setup;
}

GameSetup::GameSetup() {
	// writers often replace a file in several steps: wait until it settles
	m_debounce.setSingleShot(true);
	m_debounce.setInterval(250);
	connect(&m_debounce, &QTimer::timeout, this, &GameSetup::reload);
	connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_debounce.start(); });
	connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_debounce.start(); });
	// the patches written (Project Settings, the Maniac Patch's conversion)
	connect(&GamePatchesWatch::instance(), &GamePatchesWatch::written, this, &GameSetup::reload);
	// a project opened or closed: the watcher is told of each
	ProjectWatcher& watcher = core().watcher();
	connect(&watcher, &ProjectWatcher::projectSet, this, &GameSetup::reload);
	// the database read again from disk: once the project has read it
	connect(&watcher, &ProjectWatcher::databaseChanged, this, &GameSetup::reload, Qt::QueuedConnection);
}

const GameSetup::State& GameSetup::state() const {
	// another project (or none) since they were read: read now, so that
	// what asks gets the project's
	const auto& project = core().project();
	const bool same = project ? m_project.lock() == project : !m_state.open;
	if (!m_read || !same) {
		const_cast<GameSetup*>(this)->update(true);
	}
	return m_state;
}

bool GameSetup::isOpen() const {
	return state().open;
}

bool GameSetup::rpg2k3() const {
	return state().rpg2k3;
}

int GameSetup::engine() const {
	return rpg2k3() ? 2003 : 2000;
}

bool GameSetup::commands2k3() const {
	return state().commands2k3;
}

bool GameSetup::commands2k3e() const {
	return state().commands2k3e;
}

bool GameSetup::maniac() const {
	return state().maniac;
}

bool GameSetup::easyrpg() const {
	return state().easyrpg;
}

bool GameSetup::battle2k() const {
	return state().battle2k;
}

int GameSetup::alternativeExp() const {
	return state().alternativeExp;
}

QString GameSetup::playerEngine() const {
	return state().playerEngine;
}

QStringList GameSetup::playerOnly() const {
	return state().playerOnly;
}

QVariantMap GameSetup::patches() const {
	QVariantMap patches;
	const State& now = state();
	for (auto it = now.patches.cbegin(); it != now.patches.cend(); ++it) {
		patches.insert(it.key(), it.value());
	}
	return patches;
}

QStringList GameSetup::runsWith() const {
	return state().runsWith;
}

QStringList GameSetup::playerOnlyOf(bool rpg2k3, bool commands2k3Patch, bool battle2k, int alternativeExp,
		bool easyrpg, bool engineSet) {
	QStringList only;
	if (!rpg2k3 && commands2k3Patch) {
		only << QStringLiteral("commands2k3");
	}
	if (rpg2k3 && battle2k) {
		only << QStringLiteral("battle2k");
	}
	if (alternativeExp != 0) {
		only << QStringLiteral("alternativeExp");
	}
	if (easyrpg) {
		only << QStringLiteral("easyrpg");
	}
	if (engineSet) {
		only << QStringLiteral("engine");
	}
	return only;
}

QStringList GameSetup::runsWithOf(bool rpg2k3, bool maniac, const QStringList& playerOnly) {
	QStringList programs;
	// what RPG_RT.exe would play otherwise: not the engine told to the Player
	// ([Game] Engine), which RPG_RT.exe ignores and still runs the game
	QStringList changes = playerOnly;
	changes.removeAll(QStringLiteral("engine"));
	// the RPG_RT.exe of the engine, not the Maniac Patch's
	if (!maniac && changes.isEmpty()) {
		programs << QStringLiteral("rpgrt");
	}
	// the Maniac Patch's is RPG Maker 2003's
	if ((rpg2k3 || maniac) && changes.isEmpty()) {
		programs << QStringLiteral("maniac");
	}
	programs << QStringLiteral("easyrpg");
	return programs;
}

QVariantMap GameSetup::values() const {
	const State& now = state();
	return {
		{QStringLiteral("open"), now.open},
		{QStringLiteral("rpg2k3"), now.rpg2k3},
		{QStringLiteral("engine"), now.rpg2k3 ? 2003 : 2000},
		{QStringLiteral("commands2k3"), now.commands2k3},
		{QStringLiteral("commands2k3e"), now.commands2k3e},
		{QStringLiteral("maniac"), now.maniac},
		{QStringLiteral("easyrpg"), now.easyrpg},
		{QStringLiteral("battle2k"), now.battle2k},
		{QStringLiteral("alternativeExp"), now.alternativeExp},
		{QStringLiteral("playerEngine"), now.playerEngine.isEmpty() ? QStringLiteral("detect") : now.playerEngine},
		{QStringLiteral("playerOnly"), now.playerOnly},
		{QStringLiteral("patches"), patches()},
		{QStringLiteral("runsWith"), now.runsWith},
	};
}

void GameSetup::reload() {
	update(false);
}

void GameSetup::update(bool queued) {
	const std::shared_ptr<Project> project = core().project();
	m_project = project;
	m_read = true;
	State next;
	if (project) {
		const auto& system = project->database().system;
		const auto patches = GamePatches::read(*project).values;
		next.open = true;
		next.rpg2k3 = system.ldb_id == 2003;
		// Player::IsRPG2k3Commands
		next.commands2k3 = next.rpg2k3 || patches.value(QStringLiteral("RPG2k3Commands")) > 0;
		next.maniac = patches.value(QStringLiteral("Maniac")) > 0;
		next.easyrpg = patches.value(QStringLiteral("EasyRPG")) > 0;
		next.battle2k = !next.rpg2k3 || system.easyrpg_use_rpg2k_battle_system;
		next.alternativeExp = system.easyrpg_alternative_exp;
		const int engineSet = GamePatches::engineSet(*project);
		next.playerEngine = RpgRtExe::settingOf(engineSet);
		// Player::IsRPG2k3ECommands: the engine told, else the one it finds
		const int engine = engineSet != RpgRtExe::EngineNone ? engineSet
			: GamePatches::detectEngine(*project, system.ldb_id, project->database().version);
		const int english2k3 = RpgRtExe::EngineRpg2k3 | RpgRtExe::EngineEnglish;
		next.commands2k3e = (engine & english2k3) == english2k3 || patches.value(QStringLiteral("RPG2k3Commands")) > 0
			|| next.maniac;
		next.patches = patches;
		// what RPG_RT.exe ignores, and what runs the game then
		next.playerOnly = playerOnlyOf(next.rpg2k3, next.commands2k3, next.battle2k, next.alternativeExp, next.easyrpg,
			!next.playerEngine.isEmpty());
		next.runsWith = runsWithOf(next.rpg2k3, next.maniac, next.playerOnly);
	}
	watch(project.get());
	const State before = m_state;
	m_state = next;
	if (queued) {
		QMetaObject::invokeMethod(this, [this, before, next] { emitChanges(before, next); }, Qt::QueuedConnection);
	} else {
		emitChanges(before, next);
	}
}

void GameSetup::emitChanges(const State& before, const State& after) {
	bool any = false;
	auto tell = [&any](bool changed, auto signal) {
		if (changed) {
			any = true;
			signal();
		}
	};
	tell(before.open != after.open, [this] { emit openChanged(); });
	tell(before.rpg2k3 != after.rpg2k3, [this] { emit rpg2k3Changed(); });
	tell(before.commands2k3 != after.commands2k3, [this] { emit commands2k3Changed(); });
	tell(before.commands2k3e != after.commands2k3e, [this] { emit commands2k3eChanged(); });
	tell(before.maniac != after.maniac, [this] { emit maniacChanged(); });
	tell(before.easyrpg != after.easyrpg, [this] { emit easyrpgChanged(); });
	tell(before.battle2k != after.battle2k, [this] { emit battle2kChanged(); });
	tell(before.alternativeExp != after.alternativeExp, [this] { emit alternativeExpChanged(); });
	tell(before.playerEngine != after.playerEngine, [this] { emit playerEngineChanged(); });
	tell(before.playerOnly != after.playerOnly, [this] { emit playerOnlyChanged(); });
	tell(before.patches != after.patches, [this] { emit patchesChanged(); });
	tell(before.runsWith != after.runsWith, [this] { emit runsWithChanged(); });
	if (any) {
		emit changed();
	}
}

void GameSetup::watch(const Project* project) {
	QString dir;
	QStringList files;
	if (project) {
		dir = project->projectDir().absolutePath();
		for (const QString& name : {QStringLiteral("EasyRPG.ini"), QStringLiteral(RM_INI)}) {
			const QString file = project->findFile(name);
			if (!file.isEmpty()) {
				files << file;
			}
		}
		const QString exe = GamePatches::engineExe(*project);
		if (!exe.isEmpty()) {
			files << exe;
		}
	}
	// the folder: the files added or removed (EasyRPG.ini, accord.dll...)
	const QStringList dirs = m_watcher.directories();
	if (dirs != (dir.isEmpty() ? QStringList() : QStringList{dir})) {
		if (!dirs.isEmpty()) {
			m_watcher.removePaths(dirs);
		}
		if (!dir.isEmpty()) {
			m_watcher.addPath(dir);
		}
	}
	// the files: their changes; one replaced by a rename is no longer
	// watched, so all are watched again
	if (!m_watcher.files().isEmpty()) {
		m_watcher.removePaths(m_watcher.files());
	}
	if (!files.isEmpty()) {
		m_watcher.addPaths(files);
	}
}
