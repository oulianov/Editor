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
#include <lcf/rpg/enemyaction.h>
#include "ui/common/rpg_combobox.h"

class ProjectData;
class QCheckBox;
class QComboBox;
class QLabel;
class QRadioButton;
class QSpinBox;

/**
 * Edits one action of a monster's behavior: its precondition, rating,
 * action and switches (TFORMEDMONSTERACT, "Monster Behavior", of RPG Maker).
 */
class EnemyActionDialog : public QDialog {
	Q_OBJECT
public:
	EnemyActionDialog(ProjectData& project, lcf::rpg::EnemyAction& action, QWidget* parent = nullptr);

	void accept() override;

	/** The "Action" column of the behavior list */
	static QString actionText(const ProjectData& project, const lcf::rpg::EnemyAction& action);
	/** The "Precondition" column of the behavior list */
	static QString conditionText(const ProjectData& project, const lcf::rpg::EnemyAction& action);

private:
	/** Shows the parameters of the precondition type */
	void showConditionParams(int type);
	void updateEnabled();

	lcf::rpg::EnemyAction& m_action;

	QComboBox* m_comboCondition;
	SwitchRpgComboBox* m_comboConditionSwitch;
	QLabel* m_labelSwitch;
	QSpinBox* m_spinParam1;
	QLabel* m_labelParam1;
	QSpinBox* m_spinParam2;
	QLabel* m_labelParam2;
	QSpinBox* m_spinRating;

	QRadioButton* m_radioBasic;
	QRadioButton* m_radioSkill;
	QRadioButton* m_radioTransform;
	QComboBox* m_comboBasic;
	SkillRpgComboBox* m_comboSkill;
	EnemyRpgComboBox* m_comboTransform;

	QCheckBox* m_checkSwitchOn;
	SwitchRpgComboBox* m_comboSwitchOn;
	QCheckBox* m_checkSwitchOff;
	SwitchRpgComboBox* m_comboSwitchOff;
};
