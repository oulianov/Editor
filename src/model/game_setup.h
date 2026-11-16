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

#include <QFileSystemWatcher>
#include <QMap>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>
#include <memory>

class Project;

/**
 * How EasyRPG Player runs the game open: the format of its database (RPG
 * Maker 2000 or 2003), its patches (GamePatches: its EasyRPG.ini, else what
 * its files tell) and the options of its database that change the rules.
 * One for the Editor (instance()), the singleton Ez.Game in QML: the editors
 * read it, and follow its signals while they are open.
 *
 * It reads them again when a project opens or closes, when the patches are
 * written (GamePatchesWatch), when the project's database changes (applied
 * by the database window, read again from disk), and when the files of the
 * game that tell them change on disk (EasyRPG.ini, RPG_RT.ini, the engine's
 * program, accord.dll...). A signal is emitted only for a value that changed.
 * Without a game, all are false.
 */
class GameSetup : public QObject
{
	Q_OBJECT
	/** A game is open */
	Q_PROPERTY(bool open READ isOpen NOTIFY openChanged)
	/** The database is RPG Maker 2003's (ldb_id 2003), else RPG Maker 2000's */
	Q_PROPERTY(bool rpg2k3 READ rpg2k3 NOTIFY rpg2k3Changed)
	/** The engine of the database: 2003 or 2000 (its RTP) */
	Q_PROPERTY(int engine READ engine NOTIFY rpg2k3Changed)
	/**
	 * RPG Maker 2003's event commands and their options run: a 2003 game, or
	 * a 2000 game with the patch RPG2k3Commands (Player::IsRPG2k3Commands):
	 * the battle commands of 2003, the facing of a teleport, the second
	 * timer, the conditions of 2003 of the pages...
	 */
	Q_PROPERTY(bool commands2k3 READ commands2k3 NOTIFY commands2k3Changed)
	/** The game's Maniac Patch (GamePatches::hasManiac): its commands and fields are offered */
	Q_PROPERTY(bool maniac READ maniac NOTIFY maniacChanged)
	/** The game's EasyRPG Extensions (GamePatches::hasEasyRpg): EasyRPG Player's own commands */
	Q_PROPERTY(bool easyrpg READ easyrpg NOTIFY easyrpgChanged)
	/** The battles are RPG Maker 2000's: a 2000 game, or system.easyrpg_use_rpg2k_battle_system */
	Q_PROPERTY(bool battle2k READ battle2k NOTIFY battle2kChanged)
	/** system.easyrpg_alternative_exp: the experience curve (0: the engine's, 1: RPG Maker 2000's, 2: 2003's) */
	Q_PROPERTY(int alternativeExp READ alternativeExp NOTIFY alternativeExpChanged)
	/**
	 * [Game] Engine of EasyRPG.ini as the Player reads it, the engine it is
	 * told to run the game as ("rpg2k3e"...); empty when it finds it itself
	 * (none, or a value it does not know)
	 */
	Q_PROPERTY(QString playerEngine READ playerEngine NOTIFY playerEngineChanged)
	/**
	 * The choices of the game that only EasyRPG Player runs, RPG_RT.exe
	 * ignoring them: "commands2k3" (a 2000 game with RPG2k3Commands),
	 * "battle2k" (a 2003 game with the battles of 2000), "alternativeExp"
	 * (another experience curve), "easyrpg" (the EasyRPG Extensions),
	 * "engine" ([Game] Engine set), in this order
	 */
	Q_PROPERTY(QStringList playerOnly READ playerOnly NOTIFY playerOnlyChanged)
	/**
	 * The patches of the game: each key of GamePatches::list() with its
	 * value as the Player takes it (its [Patch], else detected; 0 off, the
	 * Maniac Patch its version)
	 */
	Q_PROPERTY(QVariantMap patches READ patches NOTIFY patchesChanged)
	/**
	 * The programs the game can run with as it is set up, among "rpgrt"
	 * (RPG_RT.exe of its engine), "maniac" (the RPG_RT.exe of the Maniac
	 * Patch, RPG Maker 2003's) and "easyrpg" (EasyRPG Player, always), in
	 * this order (runsWithOf)
	 */
	Q_PROPERTY(QStringList runsWith READ runsWith NOTIFY runsWithChanged)

public:
	static GameSetup& instance();

	bool isOpen() const;
	bool rpg2k3() const;
	int engine() const;
	bool commands2k3() const;
	bool maniac() const;
	bool easyrpg() const;
	bool battle2k() const;
	int alternativeExp() const;
	QString playerEngine() const;
	QStringList playerOnly() const;
	QVariantMap patches() const;
	QStringList runsWith() const;

	/** The choices only EasyRPG Player runs (playerOnly) of a setup: a 2000
	 *  game with RPG2k3Commands, a 2003 game with the battles of 2000,
	 *  another experience curve, the EasyRPG Extensions, an engine told */
	static QStringList playerOnlyOf(bool rpg2k3, bool commands2k3Patch, bool battle2k, int alternativeExp,
		bool easyrpg, bool engineSet);
	/** The programs a setup runs with (runsWith): EasyRPG Player always;
	 *  nothing only it runs (but the engine told to it, which RPG_RT.exe
	 *  ignores), the Maniac Patch's RPG_RT.exe in a 2003 game or with the
	 *  patch, the engine's RPG_RT.exe without the patch */
	static QStringList runsWithOf(bool rpg2k3, bool maniac, const QStringList& playerOnly);

	/** The values by their property names, as the automation logs them */
	QVariantMap values() const;

	/** Reads them all again now (the signals of those that changed) */
	void reload();

signals:
	void openChanged();
	void rpg2k3Changed();
	void commands2k3Changed();
	void maniacChanged();
	void easyrpgChanged();
	void battle2kChanged();
	void alternativeExpChanged();
	void playerEngineChanged();
	void playerOnlyChanged();
	void patchesChanged();
	void runsWithChanged();
	/** After the signals of the values that changed */
	void changed();

private:
	GameSetup();

	struct State {
		bool open = false;
		bool rpg2k3 = false;
		bool commands2k3 = false;
		bool maniac = false;
		bool easyrpg = false;
		bool battle2k = false;
		int alternativeExp = 0;
		QString playerEngine;
		QStringList playerOnly;
		QMap<QString, int> patches;
		QStringList runsWith;
	};
	/** The values, read again first when another project is open (the
	 *  signals then come once back in the event loop) */
	const State& state() const;
	/** Reads the values; their signals now, or once back in the event loop */
	void update(bool queued);
	void emitChanges(const State& before, const State& after);
	/** The files of the game watched for its setup */
	void watch(const Project* project);

	State m_state;
	/** The project the values are of */
	std::weak_ptr<Project> m_project;
	bool m_read = false;
	QFileSystemWatcher m_watcher;
	QTimer m_debounce;
};
