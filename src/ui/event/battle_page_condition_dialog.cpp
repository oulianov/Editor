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

#include "battle_page_condition_dialog.h"
#include "common/dbstring.h"
#include "model/project_data.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {
	template<typename T>
	QString nameOf(const std::vector<T>& list, int id) {
		if (id < 1 || id > static_cast<int>(list.size())) {
			return QStringLiteral("<%1?>").arg(id);
		}
		return ToQString(list[id - 1].name);
	}

	QString idName(const QString& name, int id) {
		return QStringLiteral("%1:%2").arg(id, 4, 10, QChar('0')).arg(name);
	}

	/** Monsters are the members of the group, by their 0-based place in it (as the Player reads them) */
	QString memberName(const ProjectData& project, const lcf::rpg::Troop& troop, int index) {
		QString name = index >= 0 && index < static_cast<int>(troop.members.size())
			? nameOf(project.database().enemies, troop.members[index].enemy_id) : QStringLiteral("?");
		return QStringLiteral("%1:%2").arg(index + 1).arg(name);
	}

	/** Every a turns from turn b: the forms of RPG Maker, only the parts that are set */
	QString turnsText(int a, int b) {
		if (a == 0) {
			return QString::number(b);
		} else if (b == 0) {
			return QStringLiteral("%1x").arg(a);
		}
		return QStringLiteral("%1x+%2").arg(a).arg(b);
	}

	template<typename T>
	void selectId(RpgComboBox<T>* combo, int id) {
		combo->setCurrentIndex(combo->comboBox()->findData(id));
	}

	/** The selected entry, or fallback when the combo has none */
	template<typename T>
	int selectedId(RpgComboBox<T>* combo, int fallback) {
		int id = combo->comboBox()->currentData().toInt();
		return id > 0 ? id : fallback;
	}

	/** Selects the item of this value, adding one for a value the list lacks (kept as it is) */
	void selectData(QComboBox* combo, int value, const QString& missing) {
		int index = combo->findData(value);
		if (index < 0) {
			combo->addItem(missing, value);
			index = combo->count() - 1;
		}
		combo->setCurrentIndex(index);
	}

	int selectedData(QComboBox* combo, int fallback) {
		return combo->currentIndex() >= 0 ? combo->currentData().toInt() : fallback;
	}
}

BattlePageConditionDialog::BattlePageConditionDialog(ProjectData& project, const lcf::rpg::Troop& troop,
		lcf::rpg::TroopPageCondition& condition, QWidget* parent) :
	QDialog(parent), m_condition(condition)
{
	setWindowTitle(tr("Define Battle Event Triggers"));
	const auto& database = project.database();
	const bool isRPG2k3 = database.system.ldb_id == 2003;
	const auto& c = condition;

	auto* group = new QGroupBox(tr("Conditions"), this);
	auto* grid = new QGridLayout(group);

	auto check = [&](const QString& text, const QString& name, bool checked) {
		auto* box = new QCheckBox(text, group);
		box->setObjectName(name);
		box->setChecked(checked);
		connect(box, &QCheckBox::toggled, this, &BattlePageConditionDialog::updateEnabled);
		return box;
	};
	auto spin = [&](const QString& name, int min, int max, int value) {
		auto* box = new QSpinBox(group);
		box->setObjectName(name);
		box->setRange(min, max);
		box->setValue(value);
		return box;
	};
	auto label = [&](const QString& text) {
		return new QLabel(text, group);
	};
	// The combos are named on their inner combo box, which set-control of the scripts can set
	auto setup = [&](auto* combo, const QString& name) {
		combo->comboBox()->setObjectName(name);
		combo->makeModel(project);
		return combo;
	};
	auto members = [&](const QString& name, int index) {
		auto* combo = new QComboBox(group);
		combo->setObjectName(name);
		for (int i = 0; i < static_cast<int>(troop.members.size()); ++i) {
			combo->addItem(memberName(project, troop, i), i);
		}
		selectData(combo, index, memberName(project, troop, index));
		return combo;
	};
	// One line per condition: the check box, then its fields
	auto addRow = [&](QCheckBox* box, QList<QWidget*> fields, bool allowed) {
		const int row = grid->rowCount();
		auto* line = new QHBoxLayout();
		for (QWidget* field : fields) {
			line->addWidget(field);
		}
		line->addStretch();
		grid->addWidget(box, row, 0);
		grid->addLayout(line, row, 1);
		m_rows.append({box, fields, allowed});
	};

	m_checkSwitchA = check(tr("Switch"), QStringLiteral("checkSwitchA"), c.flags.switch_a);
	m_comboSwitchA = setup(new SwitchRpgComboBox(group), QStringLiteral("comboSwitchA"));
	selectId(m_comboSwitchA, c.switch_a_id);
	addRow(m_checkSwitchA, {m_comboSwitchA, label(tr("is ON"))}, true);

	m_checkSwitchB = check(tr("Switch"), QStringLiteral("checkSwitchB"), c.flags.switch_b);
	m_comboSwitchB = setup(new SwitchRpgComboBox(group), QStringLiteral("comboSwitchB"));
	selectId(m_comboSwitchB, c.switch_b_id);
	addRow(m_checkSwitchB, {m_comboSwitchB, label(tr("is ON"))}, true);

	// The Player compares with >=
	m_checkVariable = check(tr("Variable"), QStringLiteral("checkVariable"), c.flags.variable);
	m_comboVariable = setup(new VariableRpgComboBox(group), QStringLiteral("comboVariable"));
	selectId(m_comboVariable, c.variable_id);
	const int varMax = isRPG2k3 ? 9999999 : 999999;
	m_spinVariableValue = spin(QStringLiteral("spinVariableValue"), -varMax, varMax, c.variable_value);
	addRow(m_checkVariable, {m_comboVariable, label(tr("is at least")), m_spinVariableValue}, true);

	// Turns a*n + b of the battle (b alone when a is 0)
	m_checkTurn = check(tr("Turns Elapsed"), QStringLiteral("checkTurn"), c.flags.turn);
	m_spinTurnA = spin(QStringLiteral("spinTurnA"), 0, 9999, c.turn_a);
	m_spinTurnB = spin(QStringLiteral("spinTurnB"), 0, 9999, c.turn_b);
	addRow(m_checkTurn, {m_spinTurnA, label(tr("x +")), m_spinTurnB}, true);

	m_checkFatigue = check(tr("Party Exhaustion is between"), QStringLiteral("checkFatigue"), c.flags.fatigue);
	m_spinFatigueMin = spin(QStringLiteral("spinFatigueMin"), 0, 100, c.fatigue_min);
	m_spinFatigueMax = spin(QStringLiteral("spinFatigueMax"), 0, 100, c.fatigue_max);
	auto* fatigueNote = label(tr("* Exhaustion: 100 - 100 x ((HP / Max HP) x 2 + MP / Max MP) / 3"));
	addRow(m_checkFatigue, {m_spinFatigueMin, label(tr("% and")), m_spinFatigueMax, label(tr("%"))}, isRPG2k3);
	grid->addWidget(fatigueNote, grid->rowCount(), 1);
	m_rows.last().fields.append(fatigueNote);

	m_checkEnemyHp = check(tr("Monster"), QStringLiteral("checkEnemyHp"), c.flags.enemy_hp);
	m_comboEnemyHp = members(QStringLiteral("comboEnemyHp"), c.enemy_id);
	m_spinEnemyHpMin = spin(QStringLiteral("spinEnemyHpMin"), 0, 100, c.enemy_hp_min);
	m_spinEnemyHpMax = spin(QStringLiteral("spinEnemyHpMax"), 0, 100, c.enemy_hp_max);
	addRow(m_checkEnemyHp, {m_comboEnemyHp, label(tr("HP is between")), m_spinEnemyHpMin,
		label(tr("% and")), m_spinEnemyHpMax, label(tr("%"))}, true);

	m_checkActorHp = check(tr("Hero"), QStringLiteral("checkActorHp"), c.flags.actor_hp);
	m_comboActorHp = setup(new ActorRpgComboBox(group), QStringLiteral("comboActorHp"));
	selectId(m_comboActorHp, c.actor_id);
	m_spinActorHpMin = spin(QStringLiteral("spinActorHpMin"), 0, 100, c.actor_hp_min);
	m_spinActorHpMax = spin(QStringLiteral("spinActorHpMax"), 0, 100, c.actor_hp_max);
	addRow(m_checkActorHp, {m_comboActorHp, label(tr("HP is between")), m_spinActorHpMin,
		label(tr("% and")), m_spinActorHpMax, label(tr("%"))}, true);

	// RPG Maker 2003: turns of one battler, and the command a hero chose
	m_checkTurnEnemy = check(tr("Monster"), QStringLiteral("checkTurnEnemy"), c.flags.turn_enemy);
	m_comboTurnEnemy = members(QStringLiteral("comboTurnEnemy"), c.turn_enemy_id);
	m_spinTurnEnemyA = spin(QStringLiteral("spinTurnEnemyA"), 0, 9999, c.turn_enemy_a);
	m_spinTurnEnemyB = spin(QStringLiteral("spinTurnEnemyB"), 0, 9999, c.turn_enemy_b);
	addRow(m_checkTurnEnemy, {m_comboTurnEnemy, label(tr("has taken")), m_spinTurnEnemyA,
		label(tr("x +")), m_spinTurnEnemyB, label(tr("turns"))}, isRPG2k3);

	m_checkTurnActor = check(tr("Hero"), QStringLiteral("checkTurnActor"), c.flags.turn_actor);
	m_comboTurnActor = setup(new ActorRpgComboBox(group), QStringLiteral("comboTurnActor"));
	selectId(m_comboTurnActor, c.turn_actor_id);
	m_spinTurnActorA = spin(QStringLiteral("spinTurnActorA"), 0, 9999, c.turn_actor_a);
	m_spinTurnActorB = spin(QStringLiteral("spinTurnActorB"), 0, 9999, c.turn_actor_b);
	addRow(m_checkTurnActor, {m_comboTurnActor, label(tr("has taken")), m_spinTurnActorA,
		label(tr("x +")), m_spinTurnActorB, label(tr("turns"))}, isRPG2k3);

	m_checkCommand = check(tr("Hero"), QStringLiteral("checkCommand"), c.flags.command_actor);
	m_comboCommandActor = setup(new ActorRpgComboBox(group), QStringLiteral("comboCommandActor"));
	selectId(m_comboCommandActor, c.command_actor_id);
	m_comboCommand = new QComboBox(group);
	m_comboCommand->setObjectName(QStringLiteral("comboCommand"));
	for (const auto& command : database.battlecommands.commands) {
		m_comboCommand->addItem(idName(ToQString(command.name), command.ID), command.ID);
	}
	selectData(m_comboCommand, c.command_id, idName(QStringLiteral("?"), c.command_id));
	addRow(m_checkCommand, {m_comboCommandActor, label(tr("uses the")), m_comboCommand,
		label(tr("battle command"))}, isRPG2k3);

	grid->setColumnStretch(1, 1);

	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(buttons, &QDialogButtonBox::accepted, this, &BattlePageConditionDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &BattlePageConditionDialog::reject);

	auto* layout = new QVBoxLayout(this);
	layout->addWidget(group);
	layout->addStretch();
	layout->addWidget(buttons);

	updateEnabled();
}

void BattlePageConditionDialog::updateEnabled() {
	// 2003 conditions stay visible (and keep their data) in 2000 games
	for (const Row& row : m_rows) {
		row.check->setEnabled(row.allowed);
		for (QWidget* field : row.fields) {
			field->setEnabled(row.allowed && row.check->isChecked());
		}
	}
}

void BattlePageConditionDialog::accept() {
	// Written on a copy, the page changes only here
	auto c = m_condition;
	c.flags.switch_a = m_checkSwitchA->isChecked();
	c.switch_a_id = selectedId(m_comboSwitchA, c.switch_a_id);
	c.flags.switch_b = m_checkSwitchB->isChecked();
	c.switch_b_id = selectedId(m_comboSwitchB, c.switch_b_id);
	c.flags.variable = m_checkVariable->isChecked();
	c.variable_id = selectedId(m_comboVariable, c.variable_id);
	c.variable_value = m_spinVariableValue->value();
	c.flags.turn = m_checkTurn->isChecked();
	c.turn_a = m_spinTurnA->value();
	c.turn_b = m_spinTurnB->value();
	c.flags.fatigue = m_checkFatigue->isChecked();
	c.fatigue_min = m_spinFatigueMin->value();
	c.fatigue_max = m_spinFatigueMax->value();
	c.flags.enemy_hp = m_checkEnemyHp->isChecked();
	c.enemy_id = selectedData(m_comboEnemyHp, c.enemy_id);
	c.enemy_hp_min = m_spinEnemyHpMin->value();
	c.enemy_hp_max = m_spinEnemyHpMax->value();
	c.flags.actor_hp = m_checkActorHp->isChecked();
	c.actor_id = selectedId(m_comboActorHp, c.actor_id);
	c.actor_hp_min = m_spinActorHpMin->value();
	c.actor_hp_max = m_spinActorHpMax->value();
	c.flags.turn_enemy = m_checkTurnEnemy->isChecked();
	c.turn_enemy_id = selectedData(m_comboTurnEnemy, c.turn_enemy_id);
	c.turn_enemy_a = m_spinTurnEnemyA->value();
	c.turn_enemy_b = m_spinTurnEnemyB->value();
	c.flags.turn_actor = m_checkTurnActor->isChecked();
	c.turn_actor_id = selectedId(m_comboTurnActor, c.turn_actor_id);
	c.turn_actor_a = m_spinTurnActorA->value();
	c.turn_actor_b = m_spinTurnActorB->value();
	c.flags.command_actor = m_checkCommand->isChecked();
	c.command_actor_id = selectedId(m_comboCommandActor, c.command_actor_id);
	c.command_id = selectedData(m_comboCommand, c.command_id);
	m_condition = c;

	QDialog::accept();
}

QString BattlePageConditionDialog::conditionText(const ProjectData& project, const lcf::rpg::Troop& troop,
		const lcf::rpg::TroopPageCondition& c) {
	const auto& db = project.database();
	auto actor = [&](int id) { return idName(nameOf(db.actors, id), id); };
	QStringList parts;
	if (c.flags.switch_a) {
		parts << tr("Switch [%1] ON").arg(idName(nameOf(db.switches, c.switch_a_id), c.switch_a_id));
	}
	if (c.flags.switch_b) {
		parts << tr("Switch [%1] ON").arg(idName(nameOf(db.switches, c.switch_b_id), c.switch_b_id));
	}
	if (c.flags.variable) {
		parts << tr("Variable [%1] >= %2").arg(idName(nameOf(db.variables, c.variable_id), c.variable_id)).arg(c.variable_value);
	}
	if (c.flags.turn) {
		parts << tr("Turns [%1]").arg(turnsText(c.turn_a, c.turn_b));
	}
	if (c.flags.fatigue) {
		parts << tr("Exhaustion [%1-%2]%").arg(c.fatigue_min).arg(c.fatigue_max);
	}
	if (c.flags.enemy_hp) {
		parts << tr("Monster [%1] HP [%2-%3]%").arg(memberName(project, troop, c.enemy_id)).arg(c.enemy_hp_min).arg(c.enemy_hp_max);
	}
	if (c.flags.actor_hp) {
		parts << tr("Hero [%1] HP [%2-%3]%").arg(actor(c.actor_id)).arg(c.actor_hp_min).arg(c.actor_hp_max);
	}
	if (c.flags.turn_enemy) {
		parts << tr("Monster [%1] Turns [%2]").arg(memberName(project, troop, c.turn_enemy_id), turnsText(c.turn_enemy_a, c.turn_enemy_b));
	}
	if (c.flags.turn_actor) {
		parts << tr("Hero [%1] Turns [%2]").arg(actor(c.turn_actor_id), turnsText(c.turn_actor_a, c.turn_actor_b));
	}
	if (c.flags.command_actor) {
		parts << tr("Hero [%1] uses [%2]").arg(actor(c.command_actor_id), idName(nameOf(db.battlecommands.commands, c.command_id), c.command_id));
	}
	// The Player never runs a page without a trigger
	return parts.isEmpty() ? tr("None (the page never runs)") : parts.join(QStringLiteral(", "));
}
