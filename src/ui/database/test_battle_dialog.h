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

class ProjectData;
class QButtonGroup;
class QComboBox;
class QGroupBox;
class TestBattlePartyWidget;

/**
 * RPG Maker's Test Battle window of a monster group: the party, the
 * formation and the special condition of the battle, kept in the database
 * (System), then "Start Test" runs the EasyRPG Player in a battle test.
 */
class TestBattleDialog : public QDialog
{
	Q_OBJECT

public:
	TestBattleDialog(ProjectData& project, int troop_id, QWidget *parent = nullptr);

	void accept() override;

private:
	/** Writes the settings to the database */
	void save();
	void startTest();
	void updateEnabled();

	ProjectData& m_project;
	int m_troopId;
	TestBattlePartyWidget* m_party = nullptr;
	QGroupBox* m_formationGroup = nullptr;
	QGroupBox* m_conditionGroup = nullptr;
	QButtonGroup* m_formation = nullptr;
	QButtonGroup* m_condition = nullptr;
	QComboBox* m_terrain = nullptr;
};
