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
#include <lcf/rpg/troop.h>
#include <lcf/rpg/trooppage.h>

class ProjectData;

namespace Ui {
class BattlePageWidget;
}

class BattlePageWidget : public QWidget
{
	Q_OBJECT

public:
	explicit BattlePageWidget(QWidget *parent = nullptr);
	~BattlePageWidget() override;

	lcf::rpg::TroopPage* eventPage() const;
	/** The troop names the monsters of the conditions */
	void setEventPage(ProjectData& project, const lcf::rpg::Troop& troop, lcf::rpg::TroopPage *eventPage);
	/** Shows the conditions again, after the monsters of the group changed */
	void updateTrigger();

private:
	void editTrigger();

	Ui::BattlePageWidget* ui;
	ProjectData* m_project = nullptr;
	const lcf::rpg::Troop* m_troop = nullptr;
	lcf::rpg::TroopPage* m_eventPage = nullptr;
};
