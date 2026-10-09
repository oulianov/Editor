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

#include "variable_operations_widget.h"
#include "common/dbstring.h"
#include "core.h"
#include "model/maniac_expression.h"
#include "model/project_data.h"
#include "ui/common/rpg_combobox.h"
#include "ui/common/rpg_spinbox.h"
#include <QButtonGroup>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QVBoxLayout>
#include <algorithm>
#include <limits>

namespace {
constexpr int expressionOperand = 21;
// the operands use the parameters up to 10 (the ternary operator)
constexpr int operandEnd = 11;

std::vector<int32_t> defaultExpression(const char* text) {
	std::vector<int32_t> words;
	size_t error = 0;
	ManiacExpression::encode(text, words, error);
	return words;
}

QComboBox* listCombo(QWidget* parent, const QString& name, const QList<std::pair<int, QString>>& items) {
	auto* combo = new QComboBox(parent);
	combo->setObjectName(name);
	for (const auto& item : items) {
		combo->addItem(item.second, item.first);
	}
	return combo;
}
}

VariableOperationsWidget::VariableOperationsWidget(ProjectData& project, QWidget *parent) :
	ManiacCommandWidget(project, parent)
{
	setWindowTitle(tr("Control Variables"));
}

QRadioButton* VariableOperationsWidget::addRadio(QButtonGroup* group, QGridLayout* grid, int id, const QString& text,
	const QString& name, int row, int column) {
	auto* radio = new QRadioButton(text, this);
	radio->setObjectName(name);
	group->addButton(radio, id);
	grid->addWidget(radio, row, column);
	return radio;
}

void VariableOperationsWidget::addRow(QHash<int, QList<QWidget*>>& rows, QGridLayout* grid, int id, int row,
	QList<QWidget*> widgets) {
	if (widgets.size() == 1) {
		grid->addWidget(widgets.front(), row, 1);
	} else {
		auto* box = new QHBoxLayout();
		for (auto* widget : widgets) {
			box->addWidget(widget, widget->inherits("QLabel") ? 0 : 1);
		}
		grid->addLayout(box, row, 1);
	}
	rows[id].append(widgets);
}

QWidget* VariableOperationsWidget::idField(const QString& name, int index, QWidget* constant, QComboBox* combo,
	bool database, int modeIndex, const std::vector<Mode>& modes) {
	const int value = parameter(index);
	// the page not shown starts from its first choice: what it shows is what it writes
	bool fromVariableAtLoad = false;
	for (const auto& mode : modes) {
		if (m_showManiac && modeIndex >= 0 && mode.mode == parameter(modeIndex)) {
			fromVariableAtLoad = mode.kind == Kind::Variable;
		}
	}
	if (fromVariableAtLoad) {
		combo->setCurrentIndex(0);
	} else if (database) {
		combo->setCurrentIndex(value - 1);
	} else {
		select(combo, value);
	}
	auto constantValue = [=] {
		return database ? combo->currentIndex() + 1 : combo->currentData().toInt();
	};
	if (!m_showManiac || modeIndex < 0) {
		connect(combo, &QComboBox::currentIndexChanged, this, [=, this](int row) {
			if (row >= 0) {
				setParameter(index, constantValue());
			}
		});
		onCommit(constant, [=, this] {
			if (combo->currentIndex() >= 0) {
				setParameter(index, constantValue());
			}
		});
		return constant;
	}

	// Maniac Patch: the id in a variable (or a variable pointer)
	auto* row = new QWidget(this);
	auto* layout = new QHBoxLayout(row);
	layout->setContentsMargins(0, 0, 0, 0);
	auto* mode = modeCombo(name + QStringLiteral("Mode"), modeIndex, -1, modes);
	mode->setEnabled(m_maniac);
	auto* pages = new QStackedWidget(row);
	auto* variables = new VariableRpgComboBox(pages);
	variables->makeModel(m_project);
	variables->comboBox()->setObjectName(name + QStringLiteral("Variable"));
	variables->setCurrentIndex(fromVariableAtLoad ? value - 1 : 0);
	pages->addWidget(constant);
	pages->addWidget(variables);
	auto fromVariable = [mode] {
		return kindOf(mode) == Kind::Variable;
	};
	pages->setCurrentIndex(fromVariable() ? 1 : 0);
	auto write = [=, this] {
		if (fromVariable()) {
			if (variables->currentIndex() >= 0) {
				setParameter(index, variables->currentIndex() + 1);
			}
		} else if (combo->currentIndex() >= 0) {
			setParameter(index, constantValue());
		}
	};
	connect(mode, &QComboBox::currentIndexChanged, this, [=] {
		pages->setCurrentIndex(fromVariable() ? 1 : 0);
		write();
	});
	connect(combo, &QComboBox::currentIndexChanged, this, [=] { if (!fromVariable()) write(); });
	connect(variables->comboBox(), &QComboBox::currentIndexChanged, this, [=] { if (fromVariable()) write(); });
	layout->addWidget(mode);
	layout->addWidget(pages, 1);
	onCommit(row, [=, this] {
		commit(mode);
		write();
	});
	return row;
}

void VariableOperationsWidget::build(QBoxLayout* layout) {
	m_maniac = maniac();
	const bool rpg2003 = m_project.database().system.ldb_id == 2003;
	const int target = parameter(0);
	const int operation = parameter(3);
	const int operand = parameter(4);
	const int size = static_cast<int>(m_cmd->parameters.size());
	// a command written with Maniac Patch shows its options in any game
	m_showManiac = m_maniac || target >= 3 || operation >= 6 || operand >= 9;

	auto words = [&](int from, int count) {
		std::vector<int32_t> result;
		for (int i = std::max(from, 0); i < std::min(from + count, size); ++i) {
			result.push_back(m_cmd->parameters[i]);
		}
		return result;
	};
	if (target == 4 && parameter(1) >= 0 && parameter(1) < size) {
		m_targetAt = parameter(1);
		m_targetWords = words(m_targetAt + 1, parameter(m_targetAt));
	}
	if (operand == expressionOperand) {
		m_operandWords = words(6, parameter(5));
	}
	// what a new expression starts from: V[1] for the target, 0 for the value
	if (m_targetWords.empty()) {
		m_targetWords = defaultExpression("1");
	}
	if (m_operandWords.empty()) {
		m_operandWords = defaultExpression("0");
	}

	// the rows of the other targets and operands show their defaults, not the
	// values of this one (an actor ID is not an item ID): what they show is what they write
	auto* stored = m_cmd;
	lcf::rpg::EventCommand defaults;
	auto source = [&](bool selected, std::initializer_list<std::pair<int, int>> values = {}) {
		std::vector<int32_t> parameters(operandEnd, 0);
		for (const auto& value : values) {
			parameters[value.first] = value.second;
		}
		defaults.parameters = lcf::DBArray<int32_t>(parameters.begin(), parameters.end());
		m_cmd = selected ? stored : &defaults;
	};

	// Variable to change
	auto* targetBox = new QGroupBox(tr("Variable to change"), this);
	auto* targetGrid = new QGridLayout(targetBox);
	m_target = new QButtonGroup(this);
	source(target == 0, {{1, 1}});
	addRadio(m_target, targetGrid, 0, tr("Specific"), QStringLiteral("targetSingle"), 0);
	addRow(m_targetRows, targetGrid, 0, 0, {variable(QStringLiteral("targetVariable"), 1)});
	source(target == 1, {{1, 1}, {2, 1}});
	addRadio(m_target, targetGrid, 1, tr("Range"), QStringLiteral("targetRange"), 1);
	addRow(m_targetRows, targetGrid, 1, 1, {number(QStringLiteral("targetFirst"), 1, 1, 9999),
		new QLabel(QStringLiteral("-"), this), number(QStringLiteral("targetLast"), 2, 1, 9999)});
	source(target == 2, {{1, 1}});
	addRadio(m_target, targetGrid, 2, tr("By Variable"), QStringLiteral("targetPointer"), 2);
	addRow(m_targetRows, targetGrid, 2, 2, {variable(QStringLiteral("targetPointerVariable"), 1)});
	if (m_showManiac) {
		source(target == 3, {{1, 1}, {2, 1}});
		addRadio(m_target, targetGrid, 3, tr("Range by Variables"), QStringLiteral("targetPointerRange"), 3)->setEnabled(m_maniac);
		addRow(m_targetRows, targetGrid, 3, 3, {variable(QStringLiteral("targetFirstPointer"), 1),
			new QLabel(QStringLiteral("-"), this), variable(QStringLiteral("targetLastPointer"), 2)});
		addRadio(m_target, targetGrid, 4, tr("Expression"), QStringLiteral("targetExpressionRadio"), 4)->setEnabled(m_maniac);
		addRow(m_targetRows, targetGrid, 4, 4, {expressionField(QStringLiteral("targetExpression"), m_targetWords,
			[this](const std::vector<int32_t>& value) {
				m_targetWords = value;
				layoutExpressions();
			})});
	}
	m_cmd = stored;
	targetGrid->setColumnStretch(1, 1);
	layout->addWidget(targetBox);

	ManiacCommandWidget::Items heroFields = {{0, tr("Level")}, {1, tr("Experience")}, {2, tr("HP")}, {3, tr("MP")},
		{4, tr("Maximum HP")}, {5, tr("Maximum MP")}, {6, tr("Attack")}, {7, tr("Defense")}, {8, tr("Intelligence")},
		{9, tr("Agility")}, {10, tr("Weapon number")}, {11, tr("Shield number")}, {12, tr("Armor number")},
		{13, tr("Helmet number")}, {14, tr("Accessory number")}};
	ManiacCommandWidget::Items characters = {{10001, tr("Hero")}, {10002, tr("Boat")}, {10003, tr("Ship")},
		{10004, tr("Airship")}, {10005, tr("This Event")}};
	if (auto* events = core().currentMapEvents()) {
		for (auto it = events->cbegin(); it != events->cend(); ++it) {
			characters << std::pair(it.key(), QStringLiteral("%1: %2").arg(it.key(), 4, 10, QChar('0')).arg(ToQString(it.value()->name)));
		}
	}
	ManiacCommandWidget::Items spriteFields = {{0, tr("Map ID")}, {1, tr("X coordinate")}, {2, tr("Y coordinate")},
		{3, tr("Facing")}, {4, tr("Screen-relative X")}, {5, tr("Screen-relative Y")}};
	ManiacCommandWidget::Items others = {{0, tr("Money")}, {1, tr("Timer 1 seconds")}, {2, tr("Size of party")},
		{3, tr("Number of saves")}, {4, tr("Number of battles")}, {5, tr("Number of victories")},
		{6, tr("Number of defeats")}, {7, tr("Number of escapes")}, {8, tr("MIDI ticks")}, {9, tr("Timer 2 seconds")}};
	// RPG Maker 2003: the monsters of the battle (troop slots from 0)
	ManiacCommandWidget::Items monsters;
	for (int i = 0; i < 8; ++i) {
		monsters << std::pair(i, tr("Monster %1").arg(i + 1));
	}
	ManiacCommandWidget::Items monsterFields = {{0, tr("HP")}, {1, tr("MP")}, {2, tr("Maximum HP")}, {3, tr("Maximum MP")},
		{4, tr("Attack")}, {5, tr("Defense")}, {6, tr("Intelligence")}, {7, tr("Agility")}};
	if (m_maniac) {
		heroFields << std::pair(15, tr("ID")) << std::pair(16, tr("ATB gauge")) << std::pair(17, tr("Experience to next level"));
		spriteFields << std::pair(6, tr("Event ID"));
		others << std::pair(10, tr("Date (YYMMDD)")) << std::pair(11, tr("Time (HHMMSS)")) << std::pair(12, tr("Frames"))
			<< std::pair(13, tr("Maniac Patch version"));
		monsterFields << std::pair(8, tr("ID")) << std::pair(9, tr("ATB gauge"));
	}
	const auto modes = numberModes();

	if (!m_showManiac) {
		// RPG Maker's window: radio buttons, the controls of the checked one enabled
		auto* operationBox = new QGroupBox(tr("Operation"), this);
		auto* operationGrid = new QGridLayout(operationBox);
		// Native macOS radio buttons extend beyond their layout-item margins.
		operationGrid->setVerticalSpacing(12);
		m_operation = new QButtonGroup(this);
		addRadio(m_operation, operationGrid, 0, tr("Set e&qual"), QStringLiteral("operationSet"), 0, 0);
		addRadio(m_operation, operationGrid, 2, tr("Subtract"), QStringLiteral("operationSubtract"), 0, 1);
		addRadio(m_operation, operationGrid, 4, tr("Divide"), QStringLiteral("operationDivide"), 0, 2);
		addRadio(m_operation, operationGrid, 1, tr("Add"), QStringLiteral("operationAdd"), 1, 0);
		addRadio(m_operation, operationGrid, 3, tr("&Multiply"), QStringLiteral("operationMultiply"), 1, 1);
		addRadio(m_operation, operationGrid, 5, tr("Modulus"), QStringLiteral("operationModulus"), 1, 2);
		layout->addWidget(operationBox);

		auto* operandBox = new QGroupBox(tr("Operand"), this);
		auto* operandGrid = new QGridLayout(operandBox);
		m_operand = new QButtonGroup(this);
		const int largest = 9999999;
		int row = 0;
		source(operand == 0);
		addRadio(m_operand, operandGrid, 0, tr("Number"), QStringLiteral("operandNumberRadio"), row);
		addRow(m_operandRows, operandGrid, 0, row++, {number(QStringLiteral("operandNumber"), 5, -largest, largest)});
		source(operand == 1, {{5, 1}});
		addRadio(m_operand, operandGrid, 1, tr("Va&lue of variable"), QStringLiteral("operandVariableRadio"), row);
		addRow(m_operandRows, operandGrid, 1, row++, {variable(QStringLiteral("operandVariable"), 5)});
		source(operand == 2, {{5, 1}});
		addRadio(m_operand, operandGrid, 2, tr("Value stored in inde&x"), QStringLiteral("operandPointerRadio"), row);
		addRow(m_operandRows, operandGrid, 2, row++, {variable(QStringLiteral("operandPointer"), 5)});
		source(operand == 3);
		addRadio(m_operand, operandGrid, 3, tr("Random number between"), QStringLiteral("operandRandomRadio"), row);
		addRow(m_operandRows, operandGrid, 3, row++, {number(QStringLiteral("operandRandomFrom"), 5, -largest, largest),
			new QLabel(QStringLiteral("-"), this), number(QStringLiteral("operandRandomTo"), 6, -largest, largest)});
		source(operand == 4, {{5, 1}});
		addRadio(m_operand, operandGrid, 4, tr("Item"), QStringLiteral("operandItemRadio"), row);
		auto* item = new ItemRpgComboBox(this);
		item->makeModel(m_project);
		item->comboBox()->setObjectName(QStringLiteral("operandItem"));
		addRow(m_operandRows, operandGrid, 4, row++, {idField(QStringLiteral("operandItem"), 5, item, item->comboBox(), true, -1, modes),
			choice(QStringLiteral("operandItemCount"), 6, {{0, tr("Number held")}, {1, tr("Number equipped")}})});
		source(operand == 5, {{5, 1}});
		addRadio(m_operand, operandGrid, 5, tr("Hero"), QStringLiteral("operandHeroRadio"), row);
		auto* hero = new ActorRpgComboBox(this);
		hero->makeModel(m_project);
		hero->comboBox()->setObjectName(QStringLiteral("operandHero"));
		addRow(m_operandRows, operandGrid, 5, row++, {idField(QStringLiteral("operandHero"), 5, hero, hero->comboBox(), true, -1, modes),
			choice(QStringLiteral("operandHeroField"), 6, heroFields)});
		source(operand == 6, {{5, 10001}});
		addRadio(m_operand, operandGrid, 6, tr("Sprite"), QStringLiteral("operandSpriteRadio"), row);
		auto* sprite = listCombo(this, QStringLiteral("operandSprite"), characters);
		addRow(m_operandRows, operandGrid, 6, row++, {idField(QStringLiteral("operandSprite"), 5, sprite, sprite, false, -1, modes),
			choice(QStringLiteral("operandSpriteField"), 6, spriteFields)});
		source(operand == 7);
		addRadio(m_operand, operandGrid, 7, tr("Other"), QStringLiteral("operandOtherRadio"), row);
		addRow(m_operandRows, operandGrid, 7, row++, {choice(QStringLiteral("operandOther"), 5, others)});
		source(operand == 8);
		addRadio(m_operand, operandGrid, 8, tr("Monster"), QStringLiteral("operandMonsterRadio"), row)->setEnabled(rpg2003);
		auto* monster = listCombo(this, QStringLiteral("operandMonster"), monsters);
		addRow(m_operandRows, operandGrid, 8, row++, {idField(QStringLiteral("operandMonster"), 5, monster, monster, false, -1, modes),
			choice(QStringLiteral("operandMonsterField"), 6, monsterFields)});
		m_cmd = stored;
		operandGrid->setColumnStretch(1, 1);
		layout->addWidget(operandBox);

		// check the stored choices before listening: loading writes nothing
		if (auto* button = m_operation->button(operation)) {
			button->setChecked(true);
		}
		if (auto* button = m_operand->button(operand)) {
			button->setChecked(true);
		}
		connect(m_operation, &QButtonGroup::idToggled, this, [this](int id, bool checked) {
			if (checked) {
				setParameter(3, id);
			}
		});
		connect(m_operand, &QButtonGroup::idToggled, this, [this](int id, bool checked) {
			if (!checked) {
				return;
			}
			setParameter(4, id);
			layoutExpressions();
			for (auto* widget : m_operandRows.value(id)) {
				commit(widget);
			}
			update();
		});
	} else {
		// Maniac Patch: too many choices for radio buttons; the rows of the
		// operand chosen are shown, and written when they appear
		auto* operationForm = section(layout, tr("Operation"));
		auto* operationCombo = choice(QStringLiteral("operation"), 3, {{0, tr("Set equal")}, {1, tr("Add")},
			{2, tr("Subtract")}, {3, tr("Multiply")}, {4, tr("Divide")}, {5, tr("Modulus")}, {6, tr("Bitwise OR")},
			{7, tr("Bitwise AND")}, {8, tr("Bitwise XOR")}, {9, tr("Shift left")}, {10, tr("Shift right")}});
		operationForm->addRow(tr("Operation"), operationCombo);

		auto* form = section(layout, tr("Operand"));
		auto* operandCombo = choice(QStringLiteral("operandType"), 4, {{0, tr("Number")}, {1, tr("Value of variable")},
			{2, tr("Value stored in index")}, {3, tr("Random number")}, {4, tr("Item")}, {5, tr("Hero")}, {6, tr("Sprite")},
			{7, tr("Other")}, {8, tr("Monster")}, {9, tr("Party member")}, {10, tr("Switch (0 or 1)")}, {11, tr("Power")},
			{12, tr("Square root")}, {13, tr("Sine")}, {14, tr("Cosine")}, {15, tr("Arc tangent (atan2)")},
			{16, tr("Minimum")}, {17, tr("Maximum")}, {18, tr("Absolute value")}, {19, tr("Binary operation")},
			{20, tr("Comparison (ternary)")}, {expressionOperand, tr("Expression")}, {22, tr("Linear interpolation")},
			{23, tr("Sum of variables")}, {24, tr("Minimum of variables")}, {25, tr("Maximum of variables")}});
		form->addRow(tr("Operand"), operandCombo);
		if (!m_maniac) {
			// a Maniac Patch command in another game: shown, its Maniac values kept
			for (auto* combo : {operationCombo, operandCombo}) {
				auto* model = qobject_cast<QStandardItemModel*>(combo->model());
				for (int i = 0; model && i < combo->count(); ++i) {
					const int value = combo->itemData(i).toInt();
					if ((combo == operationCombo && value >= 6) || (combo == operandCombo && value >= 9)) {
						model->item(i)->setEnabled(false);
					}
				}
			}
		}
		// before the rows: the target expression moves out of the way of the new operand
		connect(operandCombo, &QComboBox::currentIndexChanged, this, [this] { layoutExpressions(); });

		const int largest = std::numeric_limits<int>::max();
		auto row = [&](const QString& label, QWidget* field, const QList<int>& operands) {
			form->addRow(label, field);
			showWhen(form, field, operandCombo, operands);
			field->setEnabled(m_maniac || operands.front() < 9);
		};
		auto argument = [&](const QString& name, int modeIndex, int slot, int index) {
			return value(name, modeIndex, slot, index, -largest, largest, modes);
		};
		auto id = [&](const QString& name, QWidget* constant, QComboBox* combo, bool database, int modeIndex,
			const std::vector<Mode>& idModes) {
			return idField(name, 5, constant, combo, database, modeIndex, idModes);
		};
		source(operand == 0);
		row(tr("Value"), number(QStringLiteral("operandNumber"), 5, -largest, largest), {0});
		source(operand == 1, {{5, 1}});
		row(tr("Variable"), variable(QStringLiteral("operandVariable"), 5), {1});
		source(operand == 2, {{5, 1}});
		row(tr("Variable"), variable(QStringLiteral("operandPointer"), 5), {2});
		// Maniac Patch: each bound can be a variable (modes in p7)
		source(operand == 3);
		row(tr("From"), argument(QStringLiteral("operandRandomFrom"), 7, 0, 5), {3});
		row(tr("To"), argument(QStringLiteral("operandRandomTo"), 7, 1, 6), {3});
		source(operand == 4, {{5, 1}});
		auto* item = new ItemRpgComboBox(this);
		item->makeModel(m_project);
		item->comboBox()->setObjectName(QStringLiteral("operandItem"));
		row(tr("Item"), id(QStringLiteral("operandItem"), item, item->comboBox(), true, 7, modes), {4});
		row(tr("Count"), choice(QStringLiteral("operandItemCount"), 6, {{0, tr("Number held")}, {1, tr("Number equipped")}}), {4});
		source(operand == 5, {{5, 1}});
		auto* hero = new ActorRpgComboBox(this);
		hero->makeModel(m_project);
		hero->comboBox()->setObjectName(QStringLiteral("operandHero"));
		row(tr("Hero"), id(QStringLiteral("operandHero"), hero, hero->comboBox(), true, 7, modes), {5});
		row(tr("Value"), choice(QStringLiteral("operandHeroField"), 6, heroFields), {5});
		source(operand == 6, {{5, 10001}});
		auto* sprite = listCombo(this, QStringLiteral("operandSprite"), characters);
		row(tr("Sprite"), id(QStringLiteral("operandSprite"), sprite, sprite, false, 7, modes), {6});
		row(tr("Value"), choice(QStringLiteral("operandSpriteField"), 6, spriteFields), {6});
		source(operand == 7);
		row(tr("Value"), choice(QStringLiteral("operandOther"), 5, others), {7});
		source(operand == 8);
		auto* monster = listCombo(this, QStringLiteral("operandMonster"), monsters);
		row(tr("Monster"), id(QStringLiteral("operandMonster"), monster, monster, false, 7, modes), {8});
		row(tr("Value"), choice(QStringLiteral("operandMonsterField"), 6, monsterFields), {8});
		source(operand == 9);
		ManiacCommandWidget::Items members;
		for (int i = 0; i < 4; ++i) {
			members << std::pair(i, tr("Member %1").arg(i + 1));
		}
		auto* member = listCombo(this, QStringLiteral("operandParty"), members);
		row(tr("Party member"), id(QStringLiteral("operandParty"), member, member, false, 7, modes), {9});
		row(tr("Value"), choice(QStringLiteral("operandPartyField"), 6, heroFields), {9});
		source(operand == 10, {{5, 1}});
		auto* switches = new SwitchRpgComboBox(this);
		switches->makeModel(m_project);
		switches->comboBox()->setObjectName(QStringLiteral("operandSwitch"));
		row(tr("Switch"), id(QStringLiteral("operandSwitch"), switches, switches->comboBox(), true, 6,
			{{0, tr("Switch"), Kind::Number}, {1, tr("Switch of variable"), Kind::Variable}}), {10});

		// the functions, their arguments as EasyRPG Player reads them
		source(operand == 11);
		row(tr("Base"), argument(QStringLiteral("argumentBase"), 7, 0, 5), {11});
		row(tr("Exponent"), argument(QStringLiteral("argumentExponent"), 7, 1, 6), {11});
		source(operand == 12);
		row(tr("Value"), argument(QStringLiteral("argumentSquare"), 7, 0, 5), {12});
		row(tr("Multiplier"), number(QStringLiteral("argumentSquareMultiplier"), 6, -largest, largest), {12});
		source(operand == 13 || operand == 14);
		row(tr("Angle"), argument(QStringLiteral("argumentAngle"), 7, 0, 5), {13, 14});
		row(tr("Angle divisor"), argument(QStringLiteral("argumentAngleDivisor"), 7, 1, 8), {13, 14});
		row(tr("Multiplier"), number(QStringLiteral("argumentAngleMultiplier"), 6, -largest, largest), {13, 14});
		source(operand == 15);
		row(tr("Y"), argument(QStringLiteral("argumentY"), 8, 0, 5), {15});
		row(tr("X"), argument(QStringLiteral("argumentX"), 8, 1, 6), {15});
		row(tr("Multiplier"), number(QStringLiteral("argumentAtanMultiplier"), 7, -largest, largest), {15});
		source(operand == 16 || operand == 17);
		row(tr("A"), argument(QStringLiteral("argumentA"), 7, 0, 5), {16, 17});
		row(tr("B"), argument(QStringLiteral("argumentB"), 7, 1, 6), {16, 17});
		source(operand == 18);
		row(tr("Value"), argument(QStringLiteral("argumentAbsolute"), 6, 0, 5), {18});
		source(operand == 19, {{5, 1}});
		row(tr("A"), argument(QStringLiteral("argumentBinaryA"), 8, 0, 6), {19});
		row(tr("Operation"), choice(QStringLiteral("argumentBinaryOperation"), 5, {{1, QStringLiteral("+")},
			{2, QStringLiteral("-")}, {3, QStringLiteral("*")}, {4, QStringLiteral("/")}, {5, QStringLiteral("%")},
			{6, tr("OR")}, {7, tr("AND")}, {8, tr("XOR")}, {9, tr("Shift left")}, {10, tr("Shift right")}}), {19});
		row(tr("B"), argument(QStringLiteral("argumentBinaryB"), 8, 1, 7), {19});
		source(operand == 20);
		row(tr("A"), argument(QStringLiteral("argumentTernaryA"), 10, 0, 6), {20});
		row(tr("Comparison"), choice(QStringLiteral("argumentTernaryComparison"), 5, {{0, QStringLiteral("==")},
			{1, QStringLiteral(">=")}, {2, QStringLiteral("<=")}, {3, QStringLiteral(">")}, {4, QStringLiteral("<")},
			{5, QStringLiteral("!=")}}), {20});
		row(tr("B"), argument(QStringLiteral("argumentTernaryB"), 10, 1, 7), {20});
		row(tr("If true"), argument(QStringLiteral("argumentTernaryTrue"), 10, 2, 8), {20});
		row(tr("If false"), argument(QStringLiteral("argumentTernaryFalse"), 10, 3, 9), {20});
		row(tr("Expression"), expressionField(QStringLiteral("operandExpression"), m_operandWords,
			[this](const std::vector<int32_t>& value) {
				m_operandWords = value;
				layoutExpressions();
			}), {expressionOperand});
		source(operand == 22);
		row(tr("From"), argument(QStringLiteral("argumentLerpFrom"), 5, 0, 6), {22});
		row(tr("To"), argument(QStringLiteral("argumentLerpTo"), 5, 1, 7), {22});
		row(tr("Numerator"), argument(QStringLiteral("argumentLerpNumerator"), 5, 2, 8), {22});
		row(tr("Denominator"), argument(QStringLiteral("argumentLerpDenominator"), 5, 3, 9), {22});
		// a number is the ID of the first variable
		source(operand >= 23 && operand <= 25);
		row(tr("First variable ID"), argument(QStringLiteral("argumentArrayFirst"), 5, 0, 6), {23, 24, 25});
		row(tr("Count"), argument(QStringLiteral("argumentArrayCount"), 5, 1, 7), {23, 24, 25});
		m_cmd = stored;
		if (!m_maniac) {
			// the modes (number, variable...) are Maniac Patch values
			for (auto* combo : findChildren<QComboBox*>()) {
				if (combo->objectName().endsWith(QStringLiteral("Mode"))) {
					combo->setEnabled(false);
				}
			}
		}
	}

	if (auto* button = m_target->button(target)) {
		button->setChecked(true);
	}
	update();
	connect(m_target, &QButtonGroup::idToggled, this, [this](int id, bool checked) {
		if (!checked) {
			return;
		}
		setParameter(0, id);
		for (auto* widget : m_targetRows.value(id)) {
			commit(widget);
		}
		layoutExpressions();
		update();
	});
}

void VariableOperationsWidget::update() {
	auto enable = [this](const QHash<int, QList<QWidget*>>& rows, QButtonGroup* group, const QList<int>& patched) {
		for (auto it = rows.cbegin(); it != rows.cend(); ++it) {
			const bool enabled = it.key() == group->checkedId() && (m_maniac || !patched.contains(it.key()));
			for (auto* widget : it.value()) {
				widget->setEnabled(enabled);
			}
		}
	};
	enable(m_targetRows, m_target, {3, 4});
	if (m_operand) {
		enable(m_operandRows, m_operand, {});
	}
}

void VariableOperationsWidget::layoutExpressions() {
	const bool targetExpression = parameter(0) == 4;
	const bool operandExpression = parameter(4) == expressionOperand;
	if (!targetExpression && !operandExpression) {
		return;
	}
	std::vector<int32_t> parameters(m_cmd->parameters.begin(), m_cmd->parameters.end());
	size_t end = parameters.size();
	if (operandExpression) {
		parameters.resize(6);
		parameters[5] = static_cast<int32_t>(m_operandWords.size());
		parameters.insert(parameters.end(), m_operandWords.begin(), m_operandWords.end());
		end = parameters.size();
	} else if (targetExpression) {
		// after the parameters of any operand, where it already is when it is there
		end = m_targetAt >= operandEnd ? m_targetAt : operandEnd;
		if (m_targetAt >= 0 && m_targetAt < operandEnd) {
			// the old place of the expression: the next operand writes there
			std::fill(parameters.begin() + std::min<size_t>(m_targetAt, parameters.size()),
				parameters.begin() + std::min<size_t>(operandEnd, parameters.size()), 0);
		}
	}
	if (targetExpression) {
		parameters.resize(end);
		parameters[1] = static_cast<int32_t>(end);
		parameters.push_back(static_cast<int32_t>(m_targetWords.size()));
		parameters.insert(parameters.end(), m_targetWords.begin(), m_targetWords.end());
		m_targetAt = static_cast<int>(end);
	}
	m_cmd->parameters = lcf::DBArray<int32_t>(parameters.begin(), parameters.end());
}
