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

#include "scene_widgets.h"
#include "event_branch_helpers.h"
#include "common/dbstring.h"
#include "defines.h"
#include "model/event_command_defaults.h"
#include "model/event_command_list.h"
#include "model/project_data.h"
#include "ui/common/rpg_combobox.h"
#include "ui/common/rpg_spinbox.h"
#include "ui/picker/picker_backdrop_widget.h"
#include "ui/picker/picker_dialog.h"
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QListWidget>
#include <QPoint>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <optional>

using Cmd = lcf::rpg::EventCommand::Code;

namespace {
/** The value a choice of radioField() writes: a number or a database entry */
std::optional<int> fieldValue(QWidget* field) {
	if (auto* spin = qobject_cast<QSpinBox*>(field)) {
		return spin->value();
	}
	if (auto* combo = qobject_cast<RpgComboBoxBase*>(field); combo && combo->comboBox()->currentIndex() >= 0) {
		return combo->comboBox()->currentData().toInt();
	}
	return std::nullopt;
}

bool isRpg2k3(ProjectData& project) {
	return project.database().system.ldb_id == 2003;
}
}

SceneCommandWidget::SceneCommandWidget(ProjectData& project, QWidget* parent) :
	EventCommandBaseWidget(project, parent)
{}

int SceneCommandWidget::parameter(int index, int fallback) const {
	return index < static_cast<int>(m_cmd->parameters.size()) ? m_cmd->parameters[index] : fallback;
}

void SceneCommandWidget::setParameter(int index, int value) {
	if (index >= static_cast<int>(m_cmd->parameters.size())) {
		std::vector<int32_t> parameters(m_cmd->parameters.begin(), m_cmd->parameters.end());
		parameters.resize(index + 1);
		m_cmd->parameters = lcf::DBArray<int32_t>(parameters.begin(), parameters.end());
	}
	m_cmd->parameters[index] = value;
}

QFormLayout* SceneCommandWidget::section(QBoxLayout* layout, const QString& title) {
	auto* group = new QGroupBox(title, this);
	auto* form = new QFormLayout(group);
	form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	layout->addWidget(group);
	return form;
}

QComboBox* SceneCommandWidget::enumField(const QString& name, int index, const QStringList& choices, int first) {
	auto* field = new QComboBox(this);
	field->setObjectName(name);
	for (int i = 0; i < choices.size(); ++i) {
		field->addItem(choices[i], first + i);
	}
	// A missing optional parameter shows the first choice
	bindCombo(field, index, first);
	return field;
}

void SceneCommandWidget::bindCombo(QComboBox* field, int index, int fallback) {
	const int value = parameter(index, fallback);
	int row = field->findData(value);
	if (row < 0) {
		field->addItem(tr("Unknown (%1)").arg(value), value);
		row = field->count() - 1;
	}
	field->setCurrentIndex(row);
	connect(field, &QComboBox::currentIndexChanged, this, [this, field, index](int row) {
		if (row >= 0) {
			setParameter(index, field->itemData(row).toInt());
		}
	});
}

QButtonGroup* SceneCommandWidget::radioField(QFormLayout* form, const QString& name, int index,
	const QList<QPair<QString, QWidget*>>& choices, int value_index)
{
	auto* group = new QButtonGroup(this);
	for (int i = 0; i < choices.size(); ++i) {
		auto* button = new QRadioButton(choices[i].first, this);
		button->setObjectName(name + QString::number(i));
		group->addButton(button, i);
		if (choices[i].second) {
			form->addRow(button, choices[i].second);
		} else {
			form->addRow(button);
		}
	}
	// An unknown value checks no button and stays until a choice is made
	if (auto* button = group->button(parameter(index))) {
		button->setChecked(true);
	}
	auto enable_fields = [group, choices] {
		for (int i = 0; i < choices.size(); ++i) {
			if (choices[i].second) {
				choices[i].second->setEnabled(group->button(i)->isChecked());
			}
		}
	};
	enable_fields();
	connect(group, &QButtonGroup::idToggled, this, [=, this](int id, bool checked) {
		if (!checked) {
			return;
		}
		enable_fields();
		setParameter(index, id);
		if (value_index >= 0) {
			if (auto value = fieldValue(choices[id].second)) {
				setParameter(value_index, *value);
			}
		}
	});
	return group;
}

RpgSpinBox* SceneCommandWidget::spinField(const QString& name, int index, int minimum, int maximum) {
	auto* field = new RpgSpinBox(this);
	field->setObjectName(name);
	const int value = parameter(index);
	field->setRange(std::min(minimum, value), std::max(maximum, value));
	field->setValue(value);
	connect(field, &QSpinBox::valueChanged, this, [this, index](int value) { setParameter(index, value); });
	return field;
}

QCheckBox* SceneCommandWidget::checkField(const QString& text, const QString& name, int index, bool inverted) {
	auto* field = new QCheckBox(text, this);
	field->setObjectName(name);
	field->setChecked((parameter(index) != 0) != inverted);
	connect(field, &QCheckBox::toggled, this, [this, index, inverted](bool checked) {
		setParameter(index, checked != inverted ? 1 : 0);
	});
	return field;
}

template <typename T>
T* SceneCommandWidget::databaseField(const QString& name, int index) {
	auto* field = new T(this);
	field->makeModel(m_project);
	auto* combo = field->comboBox();
	combo->setObjectName(name);
	combo->setCurrentIndex(combo->findData(parameter(index)));
	connect(combo, &QComboBox::currentIndexChanged, this, [this, combo, index](int row) {
		if (row >= 0) {
			setParameter(index, combo->itemData(row).toInt());
		}
	});
	return field;
}

QComboBox* SceneCommandWidget::monsterField(const QString& name, int index) {
	QStringList monsters;
	for (int i = 0; i < 8; ++i) {
		monsters << tr("Monster %1").arg(i + 1);
	}
	auto* field = enumField(name, index, monsters);
	field->setToolTip(tr("Position in the monster group"));
	return field;
}

QWidget* SceneCommandWidget::backdropField(const QString& name) {
	auto* row = new QWidget(this);
	auto* layout = new QHBoxLayout(row);
	layout->setContentsMargins(0, 0, 0, 0);
	auto* file = new QLineEdit(ToQString(m_cmd->string), row);
	file->setObjectName(name);
	auto* browse = new QToolButton(row);
	browse->setObjectName(name + QStringLiteral("Picker"));
	browse->setText(QStringLiteral("..."));
	browse->setToolTip(tr("Choose a battle background"));
	layout->addWidget(file, 1);
	layout->addWidget(browse);
	connect(file, &QLineEdit::textEdited, this, [this](const QString& text) {
		m_cmd->string = lcf::DBString(ToDBString(text));
	});
	connect(browse, &QToolButton::clicked, this, [this, file] {
		PickerDialog picker(m_project, FileFinder::FileType::Image, new PickerBackdropWidget(), this);
		picker.setDirectoryAndFile(QStringLiteral(BACKDROP), file->text());
		connect(&picker, &PickerDialog::fileSelected, this, [this, file](const QString& name) {
			file->setText(name);
			m_cmd->string = lcf::DBString(ToDBString(name));
		});
		picker.exec();
	});
	return row;
}

void SceneCommandWidget::monsterAmountFields(bool hp) {
	const QString prefix = hp ? QStringLiteral("monsterHP") : QStringLiteral("monsterMP");
	auto* layout = new QVBoxLayout(this);
	section(layout, tr("Monster"))->addRow(monsterField(prefix + QStringLiteral("Target"), 0));
	auto* operation = radioField(section(layout, tr("Operation")), prefix + QStringLiteral("Operation"), 1,
		{{hp ? tr("Increase HP") : tr("Increase MP"), nullptr}, {hp ? tr("Decrease HP") : tr("Decrease MP"), nullptr}});

	QList<QPair<QString, QWidget*>> amounts = {
		{tr("Specific Amount"), spinField(prefix + QStringLiteral("Amount"), 3, 0, 9999)},
		{tr("Amount Stored in"), databaseField<VariableRpgComboBox>(prefix + QStringLiteral("Variable"), 3)}};
	if (hp) {
		auto* percent = spinField(QStringLiteral("monsterHPPercent"), 3, 0, 100);
		percent->setSuffix(tr(" % of Current HP"));
		amounts.append({tr("Amount Equal to"), percent});
	}
	radioField(section(layout, tr("Amount")), prefix + QStringLiteral("AmountKind"), 2, amounts, 3);

	if (hp) {
		// Only a decrease can kill
		auto* lethal = checkField(tr("HP Reduction can Kill Target"), QStringLiteral("monsterHPAllowDeath"), 4);
		section(layout, tr("Option"))->addRow(lethal);
		lethal->setEnabled(parameter(1) != 0);
		connect(operation, &QButtonGroup::idToggled, lethal, [lethal](int id, bool checked) {
			if (checked) {
				lethal->setEnabled(id != 0);
			}
		});
	}
	layout->addStretch();
}

// Commands with handler branches

SceneBlockWidget::SceneBlockWidget(ProjectData& project, Cmd closing, QWidget* parent) :
	SceneCommandWidget(project, parent), m_closing(closing)
{}

void SceneBlockWidget::setData(EventCommandList* commands) {
	m_commands = commands;
	m_edited = commands->command();
	setData(&m_edited);
}

bool SceneBlockWidget::apply() {
	const auto wanted = handlers(m_edited);
	if (wanted == handlers(m_commands->command())) {
		m_commands->command() = m_edited;
		return true;
	}
	std::vector<lcf::rpg::EventCommand> markers;
	for (auto code : wanted) {
		markers.push_back(EventCommandDefaults::make(code, m_edited.indent).front());
	}
	return EventBranchHelpers::apply(*m_commands, m_edited, m_closing, markers, this, false);
}

// Enemy Encounter

EnemyEncounterWidget::EnemyEncounterWidget(ProjectData& project, QWidget* parent) :
	SceneBlockWidget(project, Cmd::EndBattle, parent)
{
	setWindowTitle(tr("Enemy Encounter"));
}

std::vector<Cmd> EnemyEncounterWidget::handlers(const lcf::rpg::EventCommand& cmd) const {
	const bool escape = cmd.parameters.size() > 3 && cmd.parameters[3] == 2;
	const bool defeat = cmd.parameters.size() > 4 && cmd.parameters[4] == 1;
	std::vector<Cmd> result;
	if (escape || defeat) {
		result.push_back(Cmd::VictoryHandler);
	}
	if (escape) {
		result.push_back(Cmd::EscapeHandler);
	}
	if (defeat) {
		result.push_back(Cmd::DefeatHandler);
	}
	return result;
}

void EnemyEncounterWidget::setData(lcf::rpg::EventCommand* cmd) {
	m_cmd = cmd;
	const bool rpg2k3 = isRpg2k3(m_project);
	auto* columns = new QHBoxLayout(this);
	auto* left = new QVBoxLayout();
	auto* right = new QVBoxLayout();
	columns->addLayout(left, 1);
	columns->addLayout(right, 1);

	radioField(section(left, tr("Monster Group")), QStringLiteral("encounterTroopKind"), 0, {
		{tr("Specific Monster Group"), databaseField<TroopRpgComboBox>(QStringLiteral("encounterTroop"), 1)},
		{tr("Monster Group with ID Stored in"), databaseField<VariableRpgComboBox>(QStringLiteral("encounterTroopVariable"), 1)}}, 1);

	auto* background_form = section(left, tr("Battle Background"));
	auto* background = radioField(background_form, QStringLiteral("encounterBackground"), 2, {
		{tr("Use Background Defined in Map or Terrain Settings"), nullptr},
		{tr("Specific Background"), backdropField(QStringLiteral("encounterBackgroundFile"))},
		{tr("Use Background Associated with"), databaseField<TerrainRpgComboBox>(QStringLiteral("encounterTerrain"), 8)}});
	// Values 1 and 2: 0 (the terrain's formation) is not offered by RPG Maker
	auto* formation = enumField(QStringLiteral("encounterFormation"), 7, {tr("Loose Formation"), tr("Tight Formation")}, 1);
	background_form->addRow(tr("Formation"), formation);
	// The terrain background and the formation are RPG Maker 2003 options
	background->button(2)->setEnabled(rpg2k3 || background->checkedId() == 2);
	formation->setEnabled(rpg2k3 && background->checkedId() == 1);
	connect(background, &QButtonGroup::idToggled, this, [this, formation, rpg2k3](int id, bool checked) {
		if (checked) {
			formation->setEnabled(rpg2k3 && id == 1);
			// RPG Maker 2003 reads the formation of a specific background
			if (rpg2k3 && id == 1 && m_cmd->parameters.size() <= 7) {
				setParameter(7, formation->currentData().toInt());
			}
		}
	});

	section(left, tr("Option"))->addRow(checkField(tr("Begin Battle with First Strike without Affecting Enemy Disposition"),
		QStringLiteral("encounterFirstStrike"), 5));
	left->addStretch();

	radioField(section(right, tr("Escape Handler")), QStringLiteral("encounterEscape"), 3,
		{{tr("Disallow Escape"), nullptr}, {tr("End Event Processing"), nullptr}, {tr("Execute Custom Handler"), nullptr}});
	radioField(section(right, tr("Defeat Handler")), QStringLiteral("encounterDefeat"), 4,
		{{tr("Game Over"), nullptr}, {tr("Execute Custom Handler"), nullptr}});
	auto* conditions = section(right, tr("Special Battle Conditions"));
	radioField(conditions, QStringLiteral("encounterCondition"), 6,
		{{tr("No Special Conditions"), nullptr}, {tr("Initiative"), nullptr}, {tr("Back Attack"), nullptr},
		{tr("Surround Attack"), nullptr}, {tr("Pincers Attack"), nullptr}});
	conditions->parentWidget()->setEnabled(rpg2k3);
	right->addStretch();
}

// Open Shop

OpenShopWidget::OpenShopWidget(ProjectData& project, QWidget* parent) :
	SceneBlockWidget(project, Cmd::EndShop, parent)
{
	setWindowTitle(tr("Open Shop Window"));
}

std::vector<Cmd> OpenShopWidget::handlers(const lcf::rpg::EventCommand& cmd) const {
	if (cmd.parameters.size() > 2 && cmd.parameters[2] != 0) {
		return {Cmd::Transaction, Cmd::NoTransaction};
	}
	return {};
}

void OpenShopWidget::setData(lcf::rpg::EventCommand* cmd) {
	m_cmd = cmd;
	auto* columns = new QHBoxLayout(this);
	auto* left = new QVBoxLayout();
	columns->addLayout(left);
	auto* transactions = radioField(section(left, tr("Transactions Available")), QStringLiteral("shopTransactions"), 0,
		{{tr("Buy and Sell"), nullptr}, {tr("Buy Only"), nullptr}, {tr("Sell Only"), nullptr}});
	section(left, tr("Shop Message Set"))->addRow(enumField(QStringLiteral("shopMessageSet"), 1, {tr("A"), tr("B"), tr("C")}));
	section(left, tr("Option"))->addRow(checkField(tr("Define Custom Handlers for \"Transaction Made\" and \"Transaction not Made\""),
		QStringLiteral("shopHandlers"), 2));
	left->addStretch();

	// Every parameter after the fourth is an item for sale
	auto* goods = new QGroupBox(tr("Goods for Sale"), this);
	auto* goods_layout = new QVBoxLayout(goods);
	columns->addWidget(goods, 1);
	auto* items = new QListWidget(goods);
	items->setObjectName(QStringLiteral("shopGoods"));
	const auto& database_items = m_project.database().items;
	for (int i = 4; i < static_cast<int>(cmd->parameters.size()); ++i) {
		const int id = cmd->parameters[i];
		auto found = std::find_if(database_items.begin(), database_items.end(), [id](const auto& item) { return item.ID == id; });
		const QString name = found != database_items.end() ? ToQString(found->name) : tr("Unknown");
		auto* entry = new QListWidgetItem(QStringLiteral("%1: %2").arg(id, 4, 10, QChar('0')).arg(name), items);
		entry->setData(Qt::UserRole, id);
	}
	goods_layout->addWidget(items);
	auto* selection = new ItemRpgComboBox(goods);
	selection->makeModel(m_project);
	selection->comboBox()->setObjectName(QStringLiteral("shopItem"));
	goods_layout->addWidget(selection);
	auto* buttons = new QHBoxLayout();
	auto* add = new QPushButton(tr("Add"), goods);
	add->setObjectName(QStringLiteral("shopAddItem"));
	auto* remove = new QPushButton(tr("Remove"), goods);
	remove->setObjectName(QStringLiteral("shopRemoveItem"));
	buttons->addWidget(add);
	buttons->addWidget(remove);
	goods_layout->addLayout(buttons);

	auto store_goods = [this, items] {
		std::vector<int32_t> parameters(m_cmd->parameters.begin(), m_cmd->parameters.end());
		parameters.resize(4);
		for (int i = 0; i < items->count(); ++i) {
			parameters.push_back(items->item(i)->data(Qt::UserRole).toInt());
		}
		m_cmd->parameters = lcf::DBArray<int32_t>(parameters.begin(), parameters.end());
	};
	connect(add, &QPushButton::clicked, this, [selection, items, store_goods] {
		auto* combo = selection->comboBox();
		if (combo->currentIndex() < 0) {
			return;
		}
		// An item is sold once, like in RPG Maker
		const int id = combo->currentData().toInt();
		for (int i = 0; i < items->count(); ++i) {
			if (items->item(i)->data(Qt::UserRole).toInt() == id) {
				items->setCurrentRow(i);
				return;
			}
		}
		auto* entry = new QListWidgetItem(combo->currentText(), items);
		entry->setData(Qt::UserRole, id);
		items->setCurrentItem(entry);
		store_goods();
	});
	connect(remove, &QPushButton::clicked, this, [items, store_goods] {
		if (items->currentRow() >= 0) {
			delete items->takeItem(items->currentRow());
			store_goods();
		}
	});
	remove->setEnabled(false);
	connect(items, &QListWidget::currentRowChanged, remove, [remove](int row) { remove->setEnabled(row >= 0); });

	// A shop that only buys sells nothing
	goods->setEnabled(transactions->checkedId() != 2);
	connect(transactions, &QButtonGroup::idToggled, goods, [goods](int id, bool checked) {
		if (checked) {
			goods->setEnabled(id != 2);
		}
	});
}

// Show Inn

ShowInnWidget::ShowInnWidget(ProjectData& project, QWidget* parent) :
	SceneBlockWidget(project, Cmd::EndInn, parent)
{
	setWindowTitle(tr("Show Inn Message"));
}

std::vector<Cmd> ShowInnWidget::handlers(const lcf::rpg::EventCommand& cmd) const {
	if (cmd.parameters.size() > 2 && cmd.parameters[2] != 0) {
		return {Cmd::Stay, Cmd::NoStay};
	}
	return {};
}

void ShowInnWidget::setData(lcf::rpg::EventCommand* cmd) {
	m_cmd = cmd;
	auto* layout = new QVBoxLayout(this);
	section(layout, tr("Inn Message Set"))->addRow(enumField(QStringLiteral("innMessageSet"), 0, {tr("A"), tr("B")}));
	section(layout, tr("Price"))->addRow(spinField(QStringLiteral("innPrice"), 1, 0, 999999));
	section(layout, tr("Option"))->addRow(checkField(tr("Use Custom Inn Handler"), QStringLiteral("innHandlers"), 2));
	layout->addStretch();
}

// Battle commands

ChangeMonsterHPWidget::ChangeMonsterHPWidget(ProjectData& project, QWidget* parent) :
	SceneCommandWidget(project, parent)
{
	setWindowTitle(tr("Change Monster HP"));
}

void ChangeMonsterHPWidget::setData(lcf::rpg::EventCommand* cmd) {
	m_cmd = cmd;
	monsterAmountFields(true);
}

ChangeMonsterMPWidget::ChangeMonsterMPWidget(ProjectData& project, QWidget* parent) :
	SceneCommandWidget(project, parent)
{
	setWindowTitle(tr("Change Monster MP"));
}

void ChangeMonsterMPWidget::setData(lcf::rpg::EventCommand* cmd) {
	m_cmd = cmd;
	monsterAmountFields(false);
}

ChangeMonsterConditionWidget::ChangeMonsterConditionWidget(ProjectData& project, QWidget* parent) :
	SceneCommandWidget(project, parent)
{
	setWindowTitle(tr("Change Monster Condition"));
}

void ChangeMonsterConditionWidget::setData(lcf::rpg::EventCommand* cmd) {
	m_cmd = cmd;
	auto* layout = new QVBoxLayout(this);
	section(layout, tr("Monster"))->addRow(monsterField(QStringLiteral("monsterConditionTarget"), 0));
	radioField(section(layout, tr("Operation")), QStringLiteral("monsterConditionOperation"), 1,
		{{tr("Add Condition"), nullptr}, {tr("Remove Condition"), nullptr}});
	section(layout, tr("Condition"))->addRow(databaseField<StateRpgComboBox>(QStringLiteral("monsterConditionState"), 2));
	layout->addStretch();
}

ShowHiddenMonsterWidget::ShowHiddenMonsterWidget(ProjectData& project, QWidget* parent) :
	SceneCommandWidget(project, parent)
{
	setWindowTitle(tr("Show Hidden Monster"));
}

void ShowHiddenMonsterWidget::setData(lcf::rpg::EventCommand* cmd) {
	m_cmd = cmd;
	auto* layout = new QVBoxLayout(this);
	section(layout, tr("Monster to Show"))->addRow(monsterField(QStringLiteral("hiddenMonsterTarget"), 0));
	layout->addStretch();
}

ChangeBattleBGWidget::ChangeBattleBGWidget(ProjectData& project, QWidget* parent) :
	SceneCommandWidget(project, parent)
{
	setWindowTitle(tr("Change Battle Background"));
}

void ChangeBattleBGWidget::setData(lcf::rpg::EventCommand* cmd) {
	m_cmd = cmd;
	auto* layout = new QVBoxLayout(this);
	section(layout, tr("Battle Background"))->addRow(backdropField(QStringLiteral("battleBackground")));
	layout->addStretch();
}

ShowBattleAnimation_BWidget::ShowBattleAnimation_BWidget(ProjectData& project, QWidget* parent) :
	SceneCommandWidget(project, parent)
{
	setWindowTitle(tr("Show Battle Animation"));
}

void ShowBattleAnimation_BWidget::setData(lcf::rpg::EventCommand* cmd) {
	m_cmd = cmd;
	auto* layout = new QVBoxLayout(this);
	section(layout, tr("Animation"))->addRow(databaseField<AnimationRpgComboBox>(QStringLiteral("battleAnimation"), 0));

	// Item data: (target, side). Monsters count from 0, party members from 1;
	// the party side (parameter 3) is an RPG Maker 2003 addition.
	auto* target = new QComboBox(this);
	target->setObjectName(QStringLiteral("battleAnimationTarget"));
	target->addItem(tr("All Enemies"), QPoint(-1, 0));
	for (int i = 0; i < 8; ++i) {
		target->addItem(tr("Monster %1").arg(i + 1), QPoint(i, 0));
	}
	if (isRpg2k3(m_project)) {
		target->addItem(tr("Entire Party"), QPoint(-1, 1));
		for (int i = 1; i <= 4; ++i) {
			target->addItem(tr("Party Member %1").arg(i), QPoint(i, 1));
		}
	}
	const QPoint current(parameter(1), parameter(3));
	int row = target->findData(current);
	if (row < 0) {
		target->addItem(tr("Unknown (%1)").arg(current.x()), current);
		row = target->count() - 1;
	}
	target->setCurrentIndex(row);
	connect(target, &QComboBox::currentIndexChanged, this, [this, target](int row) {
		if (row < 0) {
			return;
		}
		const QPoint selected = target->itemData(row).toPoint();
		setParameter(1, selected.x());
		if (selected.y() != 0 || m_cmd->parameters.size() > 3) {
			setParameter(3, selected.y());
		}
	});
	section(layout, tr("Target"))->addRow(target);
	section(layout, tr("Option"))->addRow(checkField(tr("Halt Processing Until Animation Ends"), QStringLiteral("battleAnimationWait"), 2));
	layout->addStretch();
}

// RPG Maker 2003 battle commands

CallCommonEventWidget::CallCommonEventWidget(ProjectData& project, QWidget* parent) :
	SceneCommandWidget(project, parent)
{
	setWindowTitle(tr("Call Common Event"));
}

void CallCommonEventWidget::setData(lcf::rpg::EventCommand* cmd) {
	m_cmd = cmd;
	auto* layout = new QVBoxLayout(this);
	section(layout, tr("Event to Call"))->addRow(databaseField<CommonEventRpgComboBox>(QStringLiteral("battleCommonEvent"), 0));
	layout->addStretch();
	setEnabled(isRpg2k3(m_project));
}

ForceFleeWidget::ForceFleeWidget(ProjectData& project, QWidget* parent) :
	SceneCommandWidget(project, parent)
{
	setWindowTitle(tr("Force Flee"));
}

void ForceFleeWidget::setData(lcf::rpg::EventCommand* cmd) {
	m_cmd = cmd;
	auto* layout = new QVBoxLayout(this);
	radioField(section(layout, tr("Target")), QStringLiteral("fleeTarget"), 0,
		{{tr("Party Flees"), nullptr}, {tr("All Monsters Flee"), nullptr},
		{tr("Only this Monster Flees"), monsterField(QStringLiteral("fleeMonster"), 1)}});
	// Checked, a surrounded target (the party: in a pincers attack) does not flee
	section(layout, tr("Option"))->addRow(checkField(tr("Ignore if Target is Surrounded"),
		QStringLiteral("fleeCheckSurrounded"), 2, true));
	layout->addStretch();
	setEnabled(isRpg2k3(m_project));
}

EnableComboWidget::EnableComboWidget(ProjectData& project, QWidget* parent) :
	SceneCommandWidget(project, parent)
{
	setWindowTitle(tr("Enable Combo"));
}

void EnableComboWidget::setData(lcf::rpg::EventCommand* cmd) {
	m_cmd = cmd;
	auto* layout = new QVBoxLayout(this);
	auto* form = section(layout, tr("Parameters"));
	auto* command = new SearchComboBox(this);
	command->setObjectName(QStringLiteral("comboBattleCommand"));
	for (const auto& entry : m_project.database().battlecommands.commands) {
		command->addItem(QStringLiteral("%1: %2").arg(entry.ID, 4, 10, QChar('0')).arg(ToQString(entry.name)), entry.ID);
	}
	bindCombo(command, 1);
	form->addRow(tr("Target Hero"), databaseField<ActorRpgComboBox>(QStringLiteral("comboHero"), 0));
	form->addRow(tr("Target Battle Command"), command);
	form->addRow(tr("Number of Repetitions"), spinField(QStringLiteral("comboRepetitions"), 2, 1, 99));
	layout->addStretch();
	setEnabled(isRpg2k3(m_project));
}
