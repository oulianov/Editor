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
#include <QList>
#include <lcf/rpg/troop.h>
#include <lcf/rpg/trooppagecondition.h>
#include "ui/common/rpg_combobox.h"

class ProjectData;
class QCheckBox;
class QComboBox;
class QSpinBox;

/**
 * Edits the trigger conditions of a battle event page
 * (TFORMEDMONGROUPBATTLEEVENTTRIGGER, "Define Battle Event Triggers", of RPG Maker).
 */
class BattlePageConditionDialog : public QDialog {
	Q_OBJECT
public:
	BattlePageConditionDialog(ProjectData& project, const lcf::rpg::Troop& troop,
		lcf::rpg::TroopPageCondition& condition, QWidget* parent = nullptr);

	void accept() override;

	/** The "Trigger" line of a battle page: "Switch [0001:xxx] ON, Turns [3x+1]" */
	static QString conditionText(const ProjectData& project, const lcf::rpg::Troop& troop,
		const lcf::rpg::TroopPageCondition& condition);

private:
	/** A condition: its check box and the fields it enables */
	struct Row {
		QCheckBox* check;
		QList<QWidget*> fields;
		bool allowed;
	};

	void updateEnabled();

	lcf::rpg::TroopPageCondition& m_condition;
	QList<Row> m_rows;

	QCheckBox* m_checkSwitchA;
	SwitchRpgComboBox* m_comboSwitchA;
	QCheckBox* m_checkSwitchB;
	SwitchRpgComboBox* m_comboSwitchB;
	QCheckBox* m_checkVariable;
	VariableRpgComboBox* m_comboVariable;
	QSpinBox* m_spinVariableValue;
	QCheckBox* m_checkTurn;
	QSpinBox* m_spinTurnA;
	QSpinBox* m_spinTurnB;
	QCheckBox* m_checkFatigue;
	QSpinBox* m_spinFatigueMin;
	QSpinBox* m_spinFatigueMax;
	QCheckBox* m_checkEnemyHp;
	QComboBox* m_comboEnemyHp;
	QSpinBox* m_spinEnemyHpMin;
	QSpinBox* m_spinEnemyHpMax;
	QCheckBox* m_checkActorHp;
	ActorRpgComboBox* m_comboActorHp;
	QSpinBox* m_spinActorHpMin;
	QSpinBox* m_spinActorHpMax;
	QCheckBox* m_checkTurnEnemy;
	QComboBox* m_comboTurnEnemy;
	QSpinBox* m_spinTurnEnemyA;
	QSpinBox* m_spinTurnEnemyB;
	QCheckBox* m_checkTurnActor;
	ActorRpgComboBox* m_comboTurnActor;
	QSpinBox* m_spinTurnActorA;
	QSpinBox* m_spinTurnActorB;
	QCheckBox* m_checkCommand;
	ActorRpgComboBox* m_comboCommandActor;
	QComboBox* m_comboCommand;
};
