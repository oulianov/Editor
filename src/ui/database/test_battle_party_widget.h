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
#include <array>
#include <lcf/rpg/testbattler.h>

class ProjectData;
class QButtonGroup;
class QComboBox;
class QLabel;
class QSpinBox;
class QTabWidget;

/**
 * The party of the battle tests (lcf::rpg::System::battletest_data), as in
 * RPG Maker's Test Battle window: up to 4 heroes, each with a level and
 * equipment, and the parameters they give. The EasyRPG Player reads it from
 * the saved database (Game_Party::SetupBattleTest).
 */
class TestBattlePartyWidget : public QWidget
{
	Q_OBJECT

public:
	explicit TestBattlePartyWidget(ProjectData& project, QWidget *parent = nullptr);

	/** Shows the party of the database */
	void load();
	/** Writes the party to the database */
	void save();

private:
	static constexpr int kMembers = 4;
	enum Slot { Weapon, Shield, Armor, Helmet, Accessory, SlotCount };
	enum Parameter { MaxHp, MaxMp, Attack, Defense, Intellect, Agility, ParameterCount };

	/** The controls of one member's tab */
	struct Member {
		QComboBox* hero = nullptr;
		QSpinBox* level = nullptr;
		QLabel* shieldLabel = nullptr;
		std::array<QComboBox*, SlotCount> equipment{};
		std::array<QLabel*, ParameterCount> parameters{};
	};

	QWidget* makeMemberPage(int index);
	/** The level and equipment the hero starts the game with */
	void initialStats(int index);
	/** Shows the data of a member in its tab */
	void showMember(int index);
	void updateParameters(int index);
	void updateEnabled();
	int32_t& slotData(lcf::rpg::TestBattler& battler, int slot);

	ProjectData& m_project;
	std::array<lcf::rpg::TestBattler, kMembers> m_battlers;
	std::array<Member, kMembers> m_members;
	QButtonGroup* m_size = nullptr;
	QTabWidget* m_tabs = nullptr;
};
