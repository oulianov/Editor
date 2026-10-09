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

#include "flow_widgets.h"
#include "event_branch_helpers.h"
#include "common/dbstring.h"
#include "core.h"
#include "model/event_command_defaults.h"
#include "model/event_command_list.h"
#include "model/maniac_expression.h"
#include "model/project_data.h"
#include "ui/common/rpg_combobox.h"
#include "ui/common/rpg_spinbox.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QVBoxLayout>
#include <algorithm>
#include <limits>

namespace {
int parameter(const lcf::rpg::EventCommand& command, int index) {
	return index < static_cast<int>(command.parameters.size()) ? command.parameters[index] : 0;
}

void setParameter(lcf::rpg::EventCommand& command, int index, int value) {
	if (index >= static_cast<int>(command.parameters.size())) {
		std::vector<int32_t> parameters(command.parameters.begin(), command.parameters.end());
		parameters.resize(index + 1);
		command.parameters = lcf::DBArray<int32_t>(parameters.begin(), parameters.end());
	}
	command.parameters[index] = value;
}

void selectData(QComboBox* combo, int value) {
	int index = combo->findData(value);
	if (index < 0) {
		combo->addItem(QObject::tr("Unknown (%1)").arg(value), value);
		index = combo->count() - 1;
	}
	combo->setCurrentIndex(index);
}

void disableItem(QComboBox* combo, int value) {
	if (auto* model = qobject_cast<QStandardItemModel*>(combo->model())) {
		if (auto* item = model->item(combo->findData(value))) {
			item->setEnabled(false);
		}
	}
}

RpgSpinBox* number(QWidget* parent, const QString& name, int minimum, int maximum, int value) {
	auto* spin = new RpgSpinBox(parent);
	spin->setObjectName(name);
	spin->setRange(std::min(minimum, value), std::max(maximum, value));
	spin->setValue(value);
	return spin;
}

QComboBox* options(QWidget* parent, const QString& name, const QStringList& labels, int value) {
	auto* combo = new QComboBox(parent);
	combo->setObjectName(name);
	for (int i = 0; i < labels.size(); ++i) {
		combo->addItem(labels[i], i);
	}
	selectData(combo, value);
	return combo;
}
}

LabelWidget::LabelWidget(ProjectData& project, QWidget* parent) :
	EventCommandBaseWidget(project, parent)
{}

void LabelWidget::setData(lcf::rpg::EventCommand* command) {
	m_cmd = command;
	setWindowTitle(command->code == static_cast<int>(lcf::rpg::EventCommand::Code::Label)
		? tr("Label") : tr("Jump to Label"));
	auto* layout = new QFormLayout(this);
	auto* label = number(this, QStringLiteral("labelNumber"), 1, 9999, parameter(*command, 0));
	layout->addRow(tr("Label"), label);
	connect(label, &QSpinBox::valueChanged, this, [command](int value) { setParameter(*command, 0, value); });
}

KeyInputProcWidget::KeyInputProcWidget(ProjectData& project, QWidget* parent) :
	EventCommandBaseWidget(project, parent)
{
	setWindowTitle(tr("Key Input Processing"));
}

void KeyInputProcWidget::setData(lcf::rpg::EventCommand* command) {
	m_cmd = command;
	m_rpg2003 = m_project.database().system.ldb_id == 2003;
	m_modern2000 = command->parameters.size() == 10 || (!m_rpg2003 && command->parameters.size() >= 6);
	m_modern2003 = m_rpg2003 && command->parameters.size() > 10;
	auto* layout = new QVBoxLayout(this);
	auto* destination = new QGroupBox(tr("Store Key Code in"), this);
	auto* destinationLayout = new QFormLayout(destination);
	auto* result = new VariableRpgComboBox(destination);
	result->makeModel(m_project);
	result->setObjectName(QStringLiteral("keyResultVariable"));
	result->comboBox()->setObjectName(QStringLiteral("keyResultVariableCombo"));
	result->setCurrentIndex(parameter(*command, 0) - 1);
	m_result = result;
	destinationLayout->addRow(tr("Variable"), result);
	layout->addWidget(destination);

	auto* settings = new QGroupBox(tr("Option"), this);
	auto* settingsLayout = new QFormLayout(settings);
	m_wait = new QCheckBox(tr("Wait Until Key Pressed"), settings);
	m_wait->setObjectName(QStringLiteral("keyWait"));
	m_wait->setChecked(parameter(*command, 1) != 0);
	settingsLayout->addRow(m_wait);
	m_timed = new QCheckBox(tr("Store Amount of Time it Takes to Press Key"), settings);
	m_timed->setObjectName(QStringLiteral("keyStoreTime"));
	m_timed->setChecked(m_rpg2003 && !m_modern2000 && parameter(*command, 8) != 0);
	m_timed->setEnabled(m_rpg2003 && m_wait->isChecked());
	auto* time = new VariableRpgComboBox(settings);
	time->makeModel(m_project);
	time->setObjectName(QStringLiteral("keyTimeVariable"));
	time->comboBox()->setObjectName(QStringLiteral("keyTimeVariableCombo"));
	time->setCurrentIndex(m_rpg2003 && !m_modern2000 ? parameter(*command, 7) - 1 : 0);
	time->setEnabled(m_rpg2003 && m_wait->isChecked() && m_timed->isChecked());
	m_timeVariable = time;
	settingsLayout->addRow(m_timed, time);
	layout->addWidget(settings);

	auto* keys = new QGroupBox(tr("Keys to Process"), this);
	auto* keysLayout = new QGridLayout(keys);
	const QStringList labels{tr("Down Key (1)"), tr("Left Key (2)"), tr("Right Key (3)"), tr("Up Key (4)"),
		tr("Decision Key (5)"), tr("Cancel Key (6)"), tr("Shift Key (7)"),
		tr("Number Keys 0-9 (10-19)"), tr("+,-,*,/,. Keys (20,21,22,23,24)")};
	const QStringList names{QStringLiteral("keyDown"), QStringLiteral("keyLeft"), QStringLiteral("keyRight"),
		QStringLiteral("keyUp"), QStringLiteral("keyDecision"), QStringLiteral("keyCancel"),
		QStringLiteral("keyShift"), QStringLiteral("keyNumbers"), QStringLiteral("keyOperators")};
	for (int i = 0; i < static_cast<int>(m_keys.size()); ++i) {
		auto* check = new QCheckBox(labels[i], keys);
		check->setObjectName(names[i]);
		int index = -1;
		if (i < 4) {
			index = m_modern2003 ? i + 10 : m_modern2000 ? i + 6 : 2;
		} else if (i < 6) {
			index = i - 1;
		} else if (i == 6) {
			index = m_modern2003 ? 9 : m_modern2000 ? 5 : -1;
		} else if (m_rpg2003 && !m_modern2000) {
			index = i - 2;
		}
		const int value = index < 0 ? 0 : parameter(*command, index);
		const bool packed = m_rpg2003 && (i == 4 || i == 5 || (m_modern2003 && (i == 0 || i == 3 || i == 6)));
		check->setChecked(packed ? (value & 1) != 0 : value != 0);
		check->setEnabled(i < 7 || m_rpg2003);
		keysLayout->addWidget(check, i / 2, i % 2);
		m_keys[i] = check;
		connect(check, &QCheckBox::toggled, this, [this, i](bool checked) { setKey(i, checked); });
	}
	layout->addWidget(keys);
	connect(result->comboBox(), &QComboBox::currentIndexChanged, this, [this](int index) {
		setParameter(*m_cmd, 0, index + 1);
	});
	connect(m_wait, &QCheckBox::toggled, this, [this](bool checked) {
		setParameter(*m_cmd, 1, checked ? 1 : 0);
		m_timed->setEnabled(m_rpg2003 && checked);
		m_timeVariable->setEnabled(m_rpg2003 && checked && m_timed->isChecked());
	});
	connect(m_timed, &QCheckBox::toggled, this, [this](bool checked) {
		modernize();
		setParameter(*m_cmd, 8, checked ? 1 : 0);
		m_timeVariable->setEnabled(m_wait->isChecked() && checked);
	});
	connect(time->comboBox(), &QComboBox::currentIndexChanged, this, [this](int index) {
		modernize();
		setParameter(*m_cmd, 7, index + 1);
	});
}

void KeyInputProcWidget::modernize() {
	if (m_modern2003 || (!m_rpg2003 && m_modern2000)) {
		return;
	}
	// Older layouts share parameter positions. Migrate their meaning only
	// when the user chooses an option that needs the modern layout.
	if (m_rpg2003) {
		setParameter(*m_cmd, 13, 0);
		for (int i = 0; i < 4; ++i) {
			setParameter(*m_cmd, i + 10, m_keys[i]->isChecked() ? 1 : 0);
		}
		setParameter(*m_cmd, 9, m_keys[6]->isChecked() ? 1 : 0);
		setParameter(*m_cmd, 5, m_keys[7]->isChecked() ? 1 : 0);
		setParameter(*m_cmd, 6, m_keys[8]->isChecked() ? 1 : 0);
		setParameter(*m_cmd, 7, m_timeVariable->comboBox()->currentIndex() + 1);
		setParameter(*m_cmd, 8, m_timed->isChecked() ? 1 : 0);
		m_modern2003 = true;
		m_modern2000 = false;
	} else {
		setParameter(*m_cmd, 9, 0);
		for (int i = 0; i < 4; ++i) {
			setParameter(*m_cmd, i + 6, m_keys[i]->isChecked() ? 1 : 0);
		}
		setParameter(*m_cmd, 5, m_keys[6]->isChecked() ? 1 : 0);
		m_modern2000 = true;
	}
}

void KeyInputProcWidget::setKey(int key, bool checked) {
	if (key < 4 || key >= 6) {
		modernize();
	}
	const int index = key < 4 ? key + (m_modern2003 ? 10 : 6)
		: key < 6 ? key - 1 : key == 6 ? (m_modern2003 ? 9 : 5) : key - 2;
	const bool packed = m_rpg2003 && (key == 4 || key == 5 || (m_modern2003 && (key == 0 || key == 3 || key == 6)));
	const int value = packed ? (parameter(*m_cmd, index) & ~1) | (checked ? 1 : 0) : checked ? 1 : 0;
	setParameter(*m_cmd, index, value);
}

ConditionalBranchWidget::ConditionalBranchWidget(ProjectData& project, QWidget* parent) :
	ConditionalBranchWidget(project, parent, false)
{}

ConditionalBranchBattleWidget::ConditionalBranchBattleWidget(ProjectData& project, QWidget* parent) :
	ConditionalBranchWidget(project, parent, true)
{}

ConditionalBranchWidget::ConditionalBranchWidget(ProjectData& project, QWidget* parent, bool battle) :
	ManiacCommandWidget(project, parent), m_battle(battle)
{
	setWindowTitle(battle ? tr("Conditional Branch (battle)") : tr("Conditional Branch"));
}

void ConditionalBranchWidget::setData(EventCommandList* commands) {
	m_commands = commands;
	m_edited = commands->command();
	m_cmd = &m_edited;
	m_loading = true;
	const bool rpg2003 = m_project.database().system.ldb_id == 2003;
	const int kind = ::parameter(m_edited, 0);
	// Maniac Patch conditions: offered in its games, shown (disabled) in others
	const bool maniacProject = !m_battle && maniac();
	const bool showManiac = maniacProject || (!m_battle && kind >= 12 && kind <= 16);
	const int first = ::parameter(m_edited, 1);
	const int second = ::parameter(m_edited, 2);
	const int third = ::parameter(m_edited, 3);
	auto* layout = new QVBoxLayout(this);
	auto* condition = new QGroupBox(tr("Condition"), this);
	auto* conditionLayout = new QVBoxLayout(condition);
	m_kind = new QComboBox(condition);
	m_kind->setObjectName(QStringLiteral("conditionType"));
	m_kind->addItem(tr("Switch"), 0);
	m_kind->addItem(tr("Variable"), 1);
	if (m_battle) {
		m_kind->addItem(tr("Hero is able to act"), 2);
		m_kind->addItem(tr("Monster is able to act"), 3);
		m_kind->addItem(tr("Monster is the current target"), 4);
		m_kind->addItem(tr("Hero uses the battle command"), 5);
		if (!rpg2003) {
			disableItem(m_kind, 4);
			disableItem(m_kind, 5);
		}
	} else {
		m_kind->addItem(tr("Timer 1"), 2);
		m_kind->addItem(tr("Money"), 3);
		m_kind->addItem(tr("Item"), 4);
		m_kind->addItem(tr("Hero"), 5);
		m_kind->addItem(tr("Sprite direction"), 6);
		m_kind->addItem(tr("Party is Riding"), 7);
		m_kind->addItem(tr("Event was Started Using Action Key"), 8);
		m_kind->addItem(tr("BGM has Played Through Once"), 9);
		m_kind->addItem(tr("Timer 2"), 10);
		m_kind->addItem(tr("Other (RPG Maker 2003)"), 11);
		if (!rpg2003) {
			disableItem(m_kind, 10);
			disableItem(m_kind, 11);
		}
		if (showManiac) {
			m_kind->addItem(tr("Other (Maniac Patch)"), 12);
			m_kind->addItem(tr("Switch of Variable"), 13);
			m_kind->addItem(tr("Variable of Variable"), 14);
			m_kind->addItem(tr("String"), 15);
			m_kind->addItem(tr("Expression"), 16);
			for (int type = 12; type <= 16 && !maniacProject; ++type) {
				disableItem(m_kind, type);
			}
		}
	}
	selectData(m_kind, kind);
	conditionLayout->addWidget(m_kind);
	m_pages = new QStackedWidget(condition);
	conditionLayout->addWidget(m_pages);
	layout->addWidget(condition);
	const auto addPage = [this]() {
		auto* page = new QWidget(m_pages);
		m_pages->addWidget(page);
		return new QFormLayout(page);
	};
	const auto registerDatabase = [this](RpgComboBoxBase* combo, const QString& name, int id) {
		combo->setObjectName(name);
		combo->comboBox()->setObjectName(name + QStringLiteral("Combo"));
		combo->comboBox()->setCurrentIndex(id - 1);
		connect(combo->comboBox(), &QComboBox::currentIndexChanged, this, &ConditionalBranchWidget::store);
	};

	auto* switchForm = addPage();
	auto* switchCombo = new SwitchRpgComboBox(switchForm->parentWidget());
	switchCombo->makeModel(m_project);
	registerDatabase(switchCombo, QStringLiteral("conditionSwitch"), first);
	m_switch = switchCombo;
	m_switchState = options(switchForm->parentWidget(), QStringLiteral("conditionSwitchState"), {tr("ON"), tr("OFF")}, second);
	switchForm->addRow(tr("Switch"), switchCombo);
	switchForm->addRow(tr("is"), m_switchState);

	auto* variableForm = addPage();
	auto* variableCombo = new VariableRpgComboBox(variableForm->parentWidget());
	variableCombo->makeModel(m_project);
	registerDatabase(variableCombo, QStringLiteral("conditionVariable"), first);
	m_variable = variableCombo;
	m_comparison = options(variableForm->parentWidget(), QStringLiteral("conditionComparison"),
		{tr("Equal to"), tr("Greater than or Equal to"), tr("Less than or Equal to"),
		 tr("Greater than"), tr("Less than"), tr("Not Equal to")}, ::parameter(m_edited, 4));
	m_operandKind = options(variableForm->parentWidget(), QStringLiteral("conditionOperandType"), {tr("Number"), tr("Variable Reference")}, second);
	if (maniacProject) {
		// EasyRPG Player reads the value with ValueOrVariable: V[V[x]] too
		const int pointer = m_operandKind->findData(2);
		if (pointer < 0) {
			m_operandKind->insertItem(2, tr("Variable Pointer"), 2);
		} else {
			m_operandKind->setItemText(pointer, tr("Variable Pointer"));
		}
	}
	m_operandNumber = ::number(variableForm->parentWidget(), QStringLiteral("conditionOperandNumber"), -9999999, 9999999, third);
	auto* operandCombo = new VariableRpgComboBox(variableForm->parentWidget());
	operandCombo->makeModel(m_project);
	registerDatabase(operandCombo, QStringLiteral("conditionOperandVariable"), third);
	m_operandVariable = operandCombo;
	variableForm->addRow(tr("Variable"), variableCombo);
	variableForm->addRow(tr("Comparison"), m_comparison);
	variableForm->addRow(tr("Compare with"), m_operandKind);
	variableForm->addRow(tr("Number"), m_operandNumber);
	variableForm->addRow(tr("Variable Reference"), operandCombo);
	m_operandNumber->setEnabled(second == 0);
	operandCombo->setEnabled(second != 0);
	connect(m_operandKind, &QComboBox::currentIndexChanged, this, [this]() {
		m_operandNumber->setEnabled(m_operandKind->currentData().toInt() == 0);
		m_operandVariable->setEnabled(m_operandKind->currentData().toInt() != 0);
	});

	if (m_battle) {
		auto* heroForm = addPage();
		auto* hero = new ActorRpgComboBox(heroForm->parentWidget());
		hero->makeModel(m_project);
		registerDatabase(hero, QStringLiteral("conditionHero"), first);
		m_hero = hero;
		heroForm->addRow(tr("Hero is able to act"), hero);
		auto* monsterForm = addPage();
		m_monster = new QComboBox(monsterForm->parentWidget());
		m_monster->setObjectName(QStringLiteral("conditionMonster"));
		for (int i = 0; i < 8; ++i) {
			m_monster->addItem(tr("Monster %1").arg(i + 1), i);
		}
		selectData(m_monster, first);
		monsterForm->addRow(tr("Monster"), m_monster);
		// Both monster tests use the troop slot, whose first index is zero.
		auto* targetForm = addPage();
		auto* target = new QComboBox(targetForm->parentWidget());
		target->setObjectName(QStringLiteral("conditionTargetMonster"));
		for (int i = 0; i < 8; ++i) {
			target->addItem(tr("Monster %1").arg(i + 1), i);
		}
		selectData(target, first);
		target->setEnabled(rpg2003);
		targetForm->addRow(tr("Monster is the current target"), target);
		connect(target, &QComboBox::currentIndexChanged, this, [this, target]() {
			if (!m_loading && m_kind->currentData().toInt() == 4) {
				::setParameter(m_edited, 1, target->currentData().toInt());
			}
		});
		auto* commandForm = addPage();
		auto* commandHero = new ActorRpgComboBox(commandForm->parentWidget());
		commandHero->makeModel(m_project);
		registerDatabase(commandHero, QStringLiteral("conditionCommandHero"), first);
		m_commandHero = commandHero;
		m_battleCommand = new QComboBox(commandForm->parentWidget());
		m_battleCommand->setObjectName(QStringLiteral("conditionBattleCommand"));
		for (const auto& command : m_project.database().battlecommands.commands) {
			m_battleCommand->addItem(QStringLiteral("%1: %2").arg(command.ID, 4, 10, QChar('0')).arg(ToQString(command.name)), command.ID);
		}
		selectData(m_battleCommand, second);
		commandHero->setEnabled(rpg2003);
		m_battleCommand->setEnabled(rpg2003);
		commandForm->addRow(tr("Hero"), commandHero);
		commandForm->addRow(tr("Battle command"), m_battleCommand);
	} else {
		auto* timerForm = addPage();
		m_minutes = ::number(timerForm->parentWidget(), QStringLiteral("conditionTimerMinutes"), 0, 9999, std::max(0, first) / 60);
		m_seconds = ::number(timerForm->parentWidget(), QStringLiteral("conditionTimerSeconds"), 0, 59, std::max(0, first) % 60);
		m_timerComparison = options(timerForm->parentWidget(), QStringLiteral("conditionTimerComparison"), {tr("or more"), tr("or less")}, second);
		timerForm->addRow(tr("Minutes"), m_minutes);
		timerForm->addRow(tr("Seconds"), m_seconds);
		timerForm->addRow(tr("Comparison"), m_timerComparison);
		auto* moneyForm = addPage();
		m_money = ::number(moneyForm->parentWidget(), QStringLiteral("conditionMoney"), 0, 9999999, first);
		m_moneyComparison = options(moneyForm->parentWidget(), QStringLiteral("conditionMoneyComparison"), {tr("at least"), tr("at most")}, second);
		moneyForm->addRow(tr("Money"), m_money);
		moneyForm->addRow(tr("Comparison"), m_moneyComparison);
		auto* itemForm = addPage();
		auto* item = new ItemRpgComboBox(itemForm->parentWidget());
		item->makeModel(m_project);
		registerDatabase(item, QStringLiteral("conditionItem"), first);
		m_item = item;
		m_possession = options(itemForm->parentWidget(), QStringLiteral("conditionPossession"), {tr("Party Possesses"), tr("Party Does Not Possess")}, second);
		itemForm->addRow(tr("Item"), item);
		itemForm->addRow(tr("Condition"), m_possession);
		if (maniacProject || (kind == 4 && ::parameter(m_edited, 3) != 0)) {
			m_itemMode = addIdMode(itemForm, QStringLiteral("conditionItem"), 3, item, m_itemVariable, kind == 4);
			m_itemMode->setEnabled(maniacProject);
			m_itemVariable->setEnabled(maniacProject);
		}
		auto* heroForm = addPage();
		auto* hero = new ActorRpgComboBox(heroForm->parentWidget());
		hero->makeModel(m_project);
		registerDatabase(hero, QStringLiteral("conditionHero"), first);
		m_hero = hero;
		m_heroTest = options(heroForm->parentWidget(), QStringLiteral("conditionHeroTest"),
			{tr("In the Party"), tr("Name"), tr("Level at least"), tr("HP at least"), tr("Skill learned"), tr("Item equipped"), tr("Condition")}, second);
		heroForm->addRow(tr("Hero"), hero);
		heroForm->addRow(tr("Condition"), m_heroTest);
		if (maniacProject || (kind == 5 && ::parameter(m_edited, 4) != 0)) {
			m_heroMode = addIdMode(heroForm, QStringLiteral("conditionHero"), 4, hero, m_heroVariable, kind == 5);
			m_heroMode->setEnabled(maniacProject);
			m_heroVariable->setEnabled(maniacProject);
		}
		m_heroPages = new QStackedWidget(heroForm->parentWidget());
		heroForm->addRow(m_heroPages);
		const auto addHeroPage = [this]() {
			auto* page = new QWidget(m_heroPages);
			m_heroPages->addWidget(page);
			return new QFormLayout(page);
		};
		addHeroPage()->addRow(new QLabel(tr("Hero is in the party"), m_heroPages));
		auto* nameForm = addHeroPage();
		m_name = new QLineEdit(ToQString(m_edited.string), nameForm->parentWidget());
		m_name->setObjectName(QStringLiteral("conditionHeroName"));
		nameForm->addRow(tr("Name"), m_name);
		auto* levelForm = addHeroPage();
		m_level = ::number(levelForm->parentWidget(), QStringLiteral("conditionHeroLevel"), 1, 99, third);
		levelForm->addRow(tr("Level at least"), m_level);
		auto* hpForm = addHeroPage();
		m_hp = ::number(hpForm->parentWidget(), QStringLiteral("conditionHeroHP"), 0, 9999999, third);
		hpForm->addRow(tr("HP at least"), m_hp);
		auto* skillForm = addHeroPage();
		auto* skill = new SkillRpgComboBox(skillForm->parentWidget());
		skill->makeModel(m_project);
		registerDatabase(skill, QStringLiteral("conditionHeroSkill"), third);
		m_skill = skill;
		skillForm->addRow(tr("Skill learned"), skill);
		auto* equipmentForm = addHeroPage();
		auto* equipment = new ItemRpgComboBox(equipmentForm->parentWidget());
		equipment->makeModel(m_project);
		registerDatabase(equipment, QStringLiteral("conditionHeroEquipment"), third);
		m_equipment = equipment;
		equipmentForm->addRow(tr("Item equipped"), equipment);
		auto* stateForm = addHeroPage();
		auto* state = new StateRpgComboBox(stateForm->parentWidget());
		state->makeModel(m_project);
		registerDatabase(state, QStringLiteral("conditionHeroCondition"), third);
		m_condition = state;
		stateForm->addRow(tr("Condition"), state);
		m_heroPages->setCurrentIndex(std::clamp(second, 0, 6));
		connect(m_heroTest, &QComboBox::currentIndexChanged, this, [this]() {
			m_heroPages->setCurrentIndex(std::clamp(m_heroTest->currentData().toInt(), 0, 6));
		});
		auto* characterForm = addPage();
		m_character = new QComboBox(characterForm->parentWidget());
		m_character->setObjectName(QStringLiteral("conditionCharacter"));
		const QStringList characters{tr("Hero"), tr("Boat"), tr("Ship"), tr("Airship"), tr("This Event")};
		for (int i = 0; i < characters.size(); ++i) {
			m_character->addItem(characters[i], 10001 + i);
		}
		if (auto* events = core().currentMapEvents()) {
			for (auto it = events->cbegin(); it != events->cend(); ++it) {
				m_character->addItem(QStringLiteral("%1: %2").arg(it.key(), 4, 10, QChar('0')).arg(ToQString(it.value()->name)), it.key());
			}
		}
		selectData(m_character, first);
		m_direction = options(characterForm->parentWidget(), QStringLiteral("conditionDirection"), {tr("Up"), tr("Right"), tr("Down"), tr("Left")}, second);
		characterForm->addRow(tr("Sprite"), m_character);
		characterForm->addRow(tr("is Facing"), m_direction);
		if (maniacProject || (kind == 6 && (::parameter(m_edited, 3) != 0 || ::parameter(m_edited, 4) != 0))) {
			m_characterMode = addIdMode(characterForm, QStringLiteral("conditionCharacter"), 3, m_character, m_characterVariable, kind == 6);
			m_characterMode->setEnabled(maniacProject);
			m_characterVariable->setEnabled(maniacProject);
			// p4 = 1: only whether the event exists
			auto* stored = m_cmd;
			lcf::rpg::EventCommand neutral;
			m_cmd = kind == 6 ? stored : &neutral;
			auto* exists = boolean(QStringLiteral("conditionCharacterExists"), tr("Only check that the event exists"), 4);
			m_cmd = stored;
			exists->setEnabled(maniacProject);
			characterForm->addRow(exists);
			m_direction->setEnabled(!exists->isChecked());
			connect(exists, &QCheckBox::toggled, m_direction, [this](bool checked) { m_direction->setEnabled(!checked); });
		}
		auto* vehicleForm = addPage();
		m_vehicle = options(vehicleForm->parentWidget(), QStringLiteral("conditionVehicle"), {tr("Boat"), tr("Ship"), tr("Airship")}, first);
		vehicleForm->addRow(tr("Party is Riding"), m_vehicle);
		addPage()->addRow(new QLabel(tr("Event was Started Using Action Key"), m_pages));
		addPage()->addRow(new QLabel(tr("BGM has Played Through Once"), m_pages));
		// Timer 2 shares the same seconds/comparison encoding as Timer 1.
		addPage()->addRow(new QLabel(tr("Set Timer 2 using the timer controls."), m_pages));
		auto* otherForm = addPage();
		m_other = options(otherForm->parentWidget(), QStringLiteral("conditionOther"),
			{tr("A save file exists"), tr("Test Play mode is active"), tr("ATB Wait mode is active"), tr("Fullscreen is active")}, first);
		m_other->setEnabled(rpg2003);
		otherForm->addRow(tr("Condition"), m_other);
		if (showManiac) {
			addManiacPages(maniacProject);
		}
	}
	// Patched condition types remain unchanged until a native condition is selected.
	if (kind >= m_pages->count() || kind < 0) {
		addPage()->addRow(new QLabel(tr("This condition is provided by an engine extension."), m_pages));
	}
	const auto displayPage = [this]() {
		const int type = m_kind->currentData().toInt();
		m_pages->setCurrentIndex(!m_battle && type == 10 ? 2 : std::clamp(type, 0, m_pages->count() - 1));
		if (!m_battle && (type == 2 || type == 10)) {
			m_pages->widget(2)->setEnabled(type == 2 || m_project.database().system.ldb_id == 2003);
		}
	};
	displayPage();

	auto* settings = new QGroupBox(tr("Option"), this);
	auto* settingsLayout = new QVBoxLayout(settings);
	m_else = new QCheckBox(tr("Execute Custom Handler if Condition Not Met"), settings);
	m_else->setObjectName(QStringLiteral("conditionElse"));
	using Cmd = lcf::rpg::EventCommand::Code;
	const auto [start, end] = EventCommandDefaults::range(commands->commands(), commands->index());
	const int else_code = static_cast<int>(m_battle ? Cmd::ElseBranch_B : Cmd::ElseBranch);
	const bool has_else = std::any_of(commands->commands().begin() + start + 1, commands->commands().begin() + end,
		[&](const auto& command) { return command.indent == m_edited.indent && command.code == else_code; });
	m_else->setChecked(has_else);
	settingsLayout->addWidget(m_else);
	layout->addWidget(settings);
	connect(m_kind, &QComboBox::currentIndexChanged, this, displayPage);
	for (auto* combo : findChildren<QComboBox*>()) {
		connect(combo, &QComboBox::currentIndexChanged, this, &ConditionalBranchWidget::store);
	}
	for (auto* spin : findChildren<RpgSpinBox*>()) {
		connect(spin, &QSpinBox::valueChanged, this, &ConditionalBranchWidget::store);
	}
	if (m_name) {
		connect(m_name, &QLineEdit::textEdited, this, [this](const QString& text) {
			if (!m_loading) {
				m_edited.string = lcf::DBString(ToDBString(text));
			}
		});
	}
	connect(m_else, &QCheckBox::toggled, this, [this](bool checked) {
		::setParameter(m_edited, 5, checked ? 1 : 0);
	});
	m_loading = false;
}

void ConditionalBranchWidget::store() {
	if (m_loading) {
		return;
	}
	const int kind = m_kind->currentData().toInt();
	::setParameter(m_edited, 0, kind);
	switch (kind) {
	case 0:
		::setParameter(m_edited, 1, m_switch->comboBox()->currentIndex() + 1);
		::setParameter(m_edited, 2, m_switchState->currentData().toInt());
		break;
	case 1:
		::setParameter(m_edited, 1, m_variable->comboBox()->currentIndex() + 1);
		::setParameter(m_edited, 2, m_operandKind->currentData().toInt());
		::setParameter(m_edited, 3, m_operandKind->currentData().toInt() == 0
			? m_operandNumber->value() : m_operandVariable->comboBox()->currentIndex() + 1);
		::setParameter(m_edited, 4, m_comparison->currentData().toInt());
		break;
	case 2:
	case 10:
		if (m_battle) {
			::setParameter(m_edited, 1, m_hero->comboBox()->currentIndex() + 1);
		} else {
			::setParameter(m_edited, 1, m_minutes->value() * 60 + m_seconds->value());
			::setParameter(m_edited, 2, m_timerComparison->currentData().toInt());
		}
		break;
	case 3:
		if (m_battle) {
			::setParameter(m_edited, 1, m_monster->currentData().toInt());
		} else {
			::setParameter(m_edited, 1, m_money->value());
			::setParameter(m_edited, 2, m_moneyComparison->currentData().toInt());
		}
		break;
	case 4:
		if (m_battle) {
			::setParameter(m_edited, 1, findChild<QComboBox*>(QStringLiteral("conditionTargetMonster"))->currentData().toInt());
		} else {
			::setParameter(m_edited, 1, (fromVariable(m_itemMode) ? m_itemVariable : m_item)->comboBox()->currentIndex() + 1);
			::setParameter(m_edited, 2, m_possession->currentData().toInt());
		}
		break;
	case 5:
		if (m_battle) {
			::setParameter(m_edited, 1, m_commandHero->comboBox()->currentIndex() + 1);
			::setParameter(m_edited, 2, m_battleCommand->currentData().toInt());
		} else {
			::setParameter(m_edited, 1, (fromVariable(m_heroMode) ? m_heroVariable : m_hero)->comboBox()->currentIndex() + 1);
			::setParameter(m_edited, 2, m_heroTest->currentData().toInt());
			switch (m_heroTest->currentData().toInt()) {
			case 1: m_edited.string = lcf::DBString(ToDBString(m_name->text())); break;
			case 2: ::setParameter(m_edited, 3, m_level->value()); break;
			case 3: ::setParameter(m_edited, 3, m_hp->value()); break;
			case 4: ::setParameter(m_edited, 3, m_skill->comboBox()->currentIndex() + 1); break;
			case 5: ::setParameter(m_edited, 3, m_equipment->comboBox()->currentIndex() + 1); break;
			case 6: ::setParameter(m_edited, 3, m_condition->comboBox()->currentIndex() + 1); break;
			default: break;
			}
		}
		break;
	case 6:
		if (!m_battle) {
			::setParameter(m_edited, 1, fromVariable(m_characterMode) ? m_characterVariable->comboBox()->currentIndex() + 1
				: m_character->currentData().toInt());
			::setParameter(m_edited, 2, m_direction->currentData().toInt());
		}
		break;
	case 7:
		if (!m_battle) {
			::setParameter(m_edited, 1, m_vehicle->currentData().toInt());
		}
		break;
	case 11:
		if (!m_battle) {
			::setParameter(m_edited, 1, m_other->currentData().toInt());
		}
		break;
	case 12:
	case 13:
	case 14:
	case 15:
	case 16:
		// what the page shows is what is stored
		for (auto* widget : m_maniacRows.value(kind)) {
			commit(widget);
		}
		break;
	default: break;
	}
}

bool ConditionalBranchWidget::fromVariable(QComboBox* mode) {
	return mode && kindOf(mode) == Kind::Variable;
}

QComboBox* ConditionalBranchWidget::addIdMode(QFormLayout* form, const QString& name, int modeIndex, QWidget* constant,
	RpgComboBoxBase*& variable, bool stored) {
	auto* command = m_cmd;
	lcf::rpg::EventCommand neutral;
	m_cmd = stored ? command : &neutral;
	auto* mode = modeCombo(name + QStringLiteral("Mode"), modeIndex, -1, numberModes());
	auto* variables = new VariableRpgComboBox(form->parentWidget());
	variables->makeModel(m_project);
	variables->setObjectName(name + QStringLiteral("Variable"));
	variables->comboBox()->setObjectName(name + QStringLiteral("VariableCombo"));
	variables->setCurrentIndex(fromVariable(mode) ? ::parameter(m_edited, 1) - 1 : 0);
	m_cmd = command;
	variable = variables;
	int row = 0;
	QFormLayout::ItemRole role;
	form->getWidgetPosition(constant, &row, &role);
	form->insertRow(row, tr("Given as"), mode);
	form->insertRow(row + 2, tr("Variable"), variables);
	// the id is a number of the list or a variable
	auto show = [=, this] {
		form->setRowVisible(constant, !fromVariable(mode));
		form->setRowVisible(variables, fromVariable(mode));
	};
	show();
	connect(mode, &QComboBox::currentIndexChanged, this, show);
	return mode;
}

void ConditionalBranchWidget::addManiacPages(bool enabled) {
	const int kind = ::parameter(m_edited, 0);
	auto* stored = m_cmd;
	// the pages of the other conditions start from neutral values
	lcf::rpg::EventCommand neutral;
	auto page = [&](int type, std::initializer_list<int32_t> values) {
		neutral.parameters = lcf::DBArray<int32_t>(values.begin(), values.end());
		m_cmd = kind == type ? stored : &neutral;
		auto* widget = new QWidget(m_pages);
		widget->setEnabled(enabled);
		m_pages->addWidget(widget);
		return new QFormLayout(widget);
	};
	auto row = [&](QFormLayout* form, int type, const QString& label, QWidget* field) {
		if (label.isEmpty()) {
			form->addRow(field);
		} else {
			form->addRow(label, field);
		}
		m_maniacRows[type].append(field);
	};
	const Items onOff = {{0, tr("ON")}, {1, tr("OFF")}};

	auto* other = page(12, {12, 0, 0, 0, 0, 0});
	row(other, 12, tr("Condition"), choice(QStringLiteral("conditionManiacOther"), 1, {{0, tr("A save was loaded this frame")},
		{1, tr("A joypad is used")}, {2, tr("The window has the focus")}, {3, tr("Files can be written")}}));

	auto* switches = page(13, {13, 1, 0, 0, 0, 0});
	row(switches, 13, tr("Switch of variable"), variable(QStringLiteral("conditionSwitchPointer"), 1));
	row(switches, 13, tr("is"), choice(QStringLiteral("conditionSwitchPointerState"), 2, onOff));

	auto* variables = page(14, {14, 1, 0, 0, 0, 0});
	row(variables, 14, tr("Variable of variable"), variable(QStringLiteral("conditionVariablePointer"), 1));
	row(variables, 14, tr("Comparison"), choice(QStringLiteral("conditionVariablePointerComparison"), 4, {{0, tr("Equal to")},
		{1, tr("Greater than or Equal to")}, {2, tr("Less than or Equal to")}, {3, tr("Greater than")},
		{4, tr("Less than")}, {5, tr("Not Equal to")}}));
	row(variables, 14, tr("Compare with"), value(QStringLiteral("conditionVariablePointerOperand"), 2, -1, 3,
		std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), numberModes()));

	// the left string is T[x] or T[V[x]] (mode + 1 for the Player), the right one also a text
	auto* strings = page(15, {15, 0, 1, 1, 0, 0});
	auto* leftMode = modeComboAt(QStringLiteral("conditionStringLeftMode"), 1, 0, 4,
		{{0, tr("String"), Kind::StringId}, {1, tr("String pointer"), Kind::Variable}});
	auto* left = new QWidget(this);
	auto* leftLayout = new QHBoxLayout(left);
	leftLayout->setContentsMargins(0, 0, 0, 0);
	auto* leftValue = valueField(QStringLiteral("conditionStringLeft"), leftMode, 2, 0, 99999);
	leftLayout->addWidget(leftMode);
	leftLayout->addWidget(leftValue, 1);
	onCommit(left, [=, this] {
		commit(leftMode);
		commit(leftValue);
	});
	auto* rightMode = modeComboAt(QStringLiteral("conditionStringRightMode"), 1, 4, 4, stringModes());
	auto* text = textField(QStringLiteral("conditionStringText"));
	auto* right = new QWidget(this);
	auto* rightLayout = new QHBoxLayout(right);
	rightLayout->setContentsMargins(0, 0, 0, 0);
	auto* rightValue = valueField(QStringLiteral("conditionStringRight"), rightMode, 3, 0, 99999, text);
	rightLayout->addWidget(rightMode);
	rightLayout->addWidget(rightValue, 1);
	onCommit(right, [=, this] {
		commit(rightMode);
		commit(rightValue);
		if (kindOf(rightMode) == Kind::Text) {
			m_edited.string = lcf::DBString(ToDBString(text->text()));
		}
	});
	row(strings, 15, tr("String"), left);
	row(strings, 15, tr("Comparison"), choice(QStringLiteral("conditionStringComparison"), 4, {{0, tr("Equal to")},
		{1, tr("Not Equal to")}, {2, tr("Contains")}, {3, tr("Does not contain")}}, 0, 2));
	row(strings, 15, tr("Compare with"), right);
	row(strings, 15, QString(), flag(QStringLiteral("conditionStringIgnoreCase"), tr("Ignore case"), 4, 8));

	// the expression is all the parameters after the "else" flag (p5)
	auto* expression = page(16, {16, 0, 0, 0, 0, 0});
	std::vector<int32_t> words(m_cmd->parameters.begin() + std::min<size_t>(6, m_cmd->parameters.size()), m_cmd->parameters.end());
	if (words.empty()) {
		size_t error = 0;
		ManiacExpression::encode("S[1]", words, error);
	}
	row(expression, 16, tr("Expression"), expressionField(QStringLiteral("conditionExpression"), words,
		[this](const std::vector<int32_t>& value) {
			std::vector<int32_t> parameters(m_edited.parameters.begin(), m_edited.parameters.end());
			parameters.resize(6);
			parameters.insert(parameters.end(), value.begin(), value.end());
			m_edited.parameters = lcf::DBArray<int32_t>(parameters.begin(), parameters.end());
		}));
	m_cmd = stored;
}

bool ConditionalBranchWidget::apply() {
	using Cmd = lcf::rpg::EventCommand::Code;
	const Cmd else_code = m_battle ? Cmd::ElseBranch_B : Cmd::ElseBranch;
	const Cmd closing_code = m_battle ? Cmd::EndBranch_B : Cmd::EndBranch;
	std::vector<lcf::rpg::EventCommand> markers;
	if (m_else->isChecked()) {
		const auto [start, end] = EventCommandDefaults::range(m_commands->commands(), m_commands->index());
		auto existing = std::find_if(m_commands->commands().begin() + start + 1, m_commands->commands().begin() + end,
			[&](const auto& command) { return command.indent == m_edited.indent && command.code == static_cast<int>(else_code); });
		markers.push_back(existing == m_commands->commands().begin() + end
			? EventCommandDefaults::make(else_code, m_edited.indent).front() : *existing);
	}
	return EventBranchHelpers::apply(*m_commands, m_edited, closing_code, markers, this, true);
}
