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

#include <QWidget>

class ProjectData;
class QButtonGroup;

namespace Ui {
class BattleScreenWidget;
}

// The battle layout and the battle commands of RPG Maker 2003
// (lcf::rpg::BattleCommands), edited in the database copy of the dialog
class BattleScreenWidget : public QWidget
{
	Q_OBJECT

public:
	explicit BattleScreenWidget(ProjectData& project, QWidget *parent = nullptr);
	~BattleScreenWidget() override;

private:
	void fillCommands(int current_row);
	void showCommand(int row);
	void updateEnabled();

	Ui::BattleScreenWidget *ui;
	QButtonGroup* m_deathHandler = nullptr;
	ProjectData& m_project;
};
