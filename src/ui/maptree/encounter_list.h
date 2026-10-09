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

#include <QObject>
#include <vector>
#include <lcf/rpg/encounter.h>

class ProjectData;
class QAction;
class QTableWidget;

/**
 * The "Enemy Encounters" list of the map and area properties: a list of
 * monster groups, edited in the "Define Encounter" dialog (double-click or
 * Enter), with Cut, Copy, Paste and Delete like RPG Maker.
 */
class EncounterList : public QObject
{
	Q_OBJECT

public:
	EncounterList(ProjectData& project, QTableWidget* table, QObject* parent = nullptr);

	void setEncounters(const std::vector<lcf::rpg::Encounter>& encounters);
	std::vector<lcf::rpg::Encounter> encounters() const;

	/** Asks for a monster group, 0 when cancelled */
	static int defineEncounter(ProjectData& project, int troop_id, QWidget* parent);

public slots:
	void add();
	void edit();
	void cut();
	void copy();
	void paste();
	void remove();

private:
	void insertRow(int row, int troop_id);
	QList<int> selectedRows() const;

	ProjectData& m_project;
	QTableWidget* m_table;
	QAction* m_paste;
};
