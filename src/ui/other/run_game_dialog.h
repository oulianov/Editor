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

#include <QDialog>
#include <functional>

class ProjectData;
class QGroupBox;
class TestBattlePartyWidget;

namespace Ui {
class RunGameDialog;
}

// Settings of the test play
#define PLAYER_PATH_KEY "player_path"
#define TESTPLAY_FULLSCREEN_KEY "testplay_fullscreen"
#define TESTPLAY_SHOW_TITLE_KEY "testplay_show_title"

/**
 * Starts the EasyRPG Player on the open project. The dialog itself is the
 * "Test Play Options": play mode, start position, party, battle test.
 */
class RunGameDialog : public QDialog
{
	Q_OBJECT

public:

	explicit RunGameDialog(ProjectData& project, QWidget *parent = nullptr);
	~RunGameDialog();

	/** Test play from a map position (Start Game Here) */
	void runHere(int map_id, int x, int y);

	/**
	 * Battle test against a monster group. The party and the background are
	 * those saved in the database (System); the RPG Maker 2003 formation
	 * (lcf::rpg::System::BattleFormation, -1: only the group is given),
	 * condition and terrain are given to the Player (--battle-test).
	 */
	static bool runBattle(int troop_id, int formation = -1, int condition = 0, int terrain_id = 1);

	/**
	 * Starts the Player in test play on the open project, with the Full
	 * Screen and Show Title settings unless the options decide them.
	 */
	static bool launch(QWidget* parent, const QStringList& options, bool use_settings = true);

	/** Asks for the Player program and remembers it, empty when cancelled */
	static QString choosePlayer(QWidget* parent);

	/** Called before the Player starts (to save the maps): false cancels */
	static void setBeforeLaunch(std::function<bool()> callback);

public slots:
	void accept() override;

private slots:
	void on_comboMode_currentIndexChanged(int index);

private:
	void UpdateModels();
	static QString findPlayer();
#ifdef Q_OS_MACOS
	static void activatePlayer(qint64 pid);
#endif

	Ui::RunGameDialog *ui;
	ProjectData& m_project;
	/** The party of the battle test, shown instead of the initial party */
	QGroupBox* m_groupBattleParty = nullptr;
	TestBattlePartyWidget* m_battleParty = nullptr;
};
