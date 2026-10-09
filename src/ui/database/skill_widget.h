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
#include <lcf/rpg/skill.h>

class ProjectData;
class BitArrayChecklist;
class RpgSoundLineEdit;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QSpinBox;
template <typename LCF> class RpgComboBox;
namespace lcf::rpg {
	class Animation;
	class Switch;
}

/** The Skills page of the database, like RPG Maker 2003's */
class SkillWidget : public QWidget
{
	Q_OBJECT

public:
	using value_type = lcf::rpg::Skill;

	explicit SkillWidget(ProjectData& project, QWidget *parent = nullptr);

	void setData(lcf::rpg::Skill* skill);

private:
	// shows the fields used by the type of the skill
	void updateType();
	// the effects increase or decrease, inflict or remove, depending on the target
	void updateScope();
	// the MP cost is a number or a percentage of the maximum MP
	void bindMpCost(bool percent);

	ProjectData& m_project;
	lcf::rpg::Skill m_dummy;
	lcf::rpg::Skill* m_current = nullptr;

	QLineEdit* m_name = nullptr;
	QLineEdit* m_description = nullptr;
	QComboBox* m_type = nullptr;
	QSpinBox* m_mpCost = nullptr;
	QButtonGroup* m_mpType = nullptr;

	// Normal skills
	QComboBox* m_scope = nullptr;
	RpgComboBox<lcf::rpg::Animation>* m_animation = nullptr;
	QPushButton* m_battlerAnimation = nullptr;
	// attack influence, intelligence influence and variance: 0 to 10
	struct Rate {
		QSlider* slider;
		QLabel* value;
		int32_t lcf::rpg::Skill::* field;
	};
	std::vector<Rate> m_rates;
	QSpinBox* m_power = nullptr;
	QSpinBox* m_hit = nullptr;
	QGroupBox* m_effects = nullptr;
	QCheckBox* m_affectHp = nullptr;
	QCheckBox* m_affectSp = nullptr;
	QCheckBox* m_affectAttack = nullptr;
	QCheckBox* m_affectDefense = nullptr;
	QCheckBox* m_affectSpirit = nullptr;
	QCheckBox* m_affectAgility = nullptr;
	QCheckBox* m_absorb = nullptr;
	QCheckBox* m_ignoreDefense = nullptr;
	QButtonGroup* m_stateEffect = nullptr;
	BitArrayChecklist* m_states = nullptr;
	BitArrayChecklist* m_attributes = nullptr;
	QCheckBox* m_attributeDefense = nullptr;
	std::vector<QWidget*> m_normalWidgets;

	// Teleport, escape and switch skills
	QGroupBox* m_soundGroup = nullptr;
	RpgSoundLineEdit* m_sound = nullptr;
	QGroupBox* m_switchGroup = nullptr;
	RpgComboBox<lcf::rpg::Switch>* m_switch = nullptr;
	QGroupBox* m_occasionGroup = nullptr;
	QCheckBox* m_occasionField = nullptr;
	QCheckBox* m_occasionBattle = nullptr;

	// RPG Maker 2000 battle messages
	QLineEdit* m_message1 = nullptr;
	QLineEdit* m_message2 = nullptr;
	QComboBox* m_failure = nullptr;
};
