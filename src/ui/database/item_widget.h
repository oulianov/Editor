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
#include <vector>
#include <lcf/rpg/item.h>

class ProjectData;
class BitArrayChecklist;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
template <typename LCF> class RpgComboBox;
namespace lcf::rpg {
	class Animation;
	class Skill;
	class Switch;
}

/** The Items page of the database, like RPG Maker 2003's */
class ItemWidget : public QWidget
{
	Q_OBJECT

public:
	using value_type = lcf::rpg::Item;

	explicit ItemWidget(ProjectData& project, QWidget *parent = nullptr);

	void setData(lcf::rpg::Item* item);

private:
	// shows the fields used by the type of the item
	void updateType();
	// the heroes or the classes who can use the item (RPG Maker 2003 setting)
	void bindUsers();

	ProjectData& m_project;
	lcf::rpg::Item m_dummy;
	lcf::rpg::Item* m_current = nullptr;

	QLineEdit* m_name = nullptr;
	QLineEdit* m_description = nullptr;
	QComboBox* m_type = nullptr;
	QLabel* m_usesLabel = nullptr;
	QComboBox* m_uses = nullptr;
	// the numbers and flags, bound by setData
	std::vector<std::pair<QSpinBox*, int32_t lcf::rpg::Item::*>> m_spins;
	std::vector<std::pair<QCheckBox*, bool lcf::rpg::Item::*>> m_checks;

	// Weapons
	std::vector<QWidget*> m_weaponWidgets;
	QButtonGroup* m_hands = nullptr;
	RpgComboBox<lcf::rpg::Animation>* m_animation = nullptr;
	QPushButton* m_battlerAnimation = nullptr;
	QGroupBox* m_armorOptions = nullptr;

	// Equipment
	QGroupBox* m_statGroup = nullptr;
	QGroupBox* m_attributeGroup = nullptr;
	BitArrayChecklist* m_attributes = nullptr;
	QGroupBox* m_conditionGroup = nullptr;
	QWidget* m_stateEffectRow = nullptr;
	QButtonGroup* m_stateEffect = nullptr;
	QLabel* m_stateChanceLabel = nullptr;
	BitArrayChecklist* m_states = nullptr;

	// Medicine
	std::vector<QWidget*> m_medicineWidgets;
	QButtonGroup* m_entireParty = nullptr;

	// Seeds
	QGroupBox* m_seedGroup = nullptr;

	// Skill books, skill scrolls and equipment invoking a skill
	QGroupBox* m_skillGroup = nullptr;
	QCheckBox* m_useSkill = nullptr;
	RpgComboBox<lcf::rpg::Skill>* m_skill = nullptr;
	QWidget* m_messageRow = nullptr;
	QButtonGroup* m_message = nullptr;

	// Switches
	QGroupBox* m_switchGroup = nullptr;
	RpgComboBox<lcf::rpg::Switch>* m_switch = nullptr;
	QGroupBox* m_occasionGroup = nullptr;

	// Who can use it
	QGroupBox* m_userGroup = nullptr;
	QButtonGroup* m_userType = nullptr;
	BitArrayChecklist* m_users = nullptr;
};
