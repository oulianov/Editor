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

#include "battle_screen_widget.h"
#include "ui_battle_screen_widget.h"

#include <QButtonGroup>

#include "common/dbstring.h"
#include "common/lcf_widget_binding.h"
#include "model/project_data.h"

namespace {
QButtonGroup* makeGroup(QWidget* parent, std::initializer_list<QAbstractButton*> buttons) {
	auto* group = new QButtonGroup(parent);
	int id = 0;
	for (auto* button : buttons) {
		group->addButton(button, id++);
	}
	return group;
}

QString commandText(const lcf::rpg::BattleCommand& command) {
	return QString("%1: %2").arg(command.ID, 4, 10, QChar('0')).arg(ToQString(command.name));
}
}

BattleScreenWidget::BattleScreenWidget(ProjectData& project, QWidget *parent) :
	QWidget(parent),
	ui(new Ui::BattleScreenWidget),
	m_project(project)
{
	ui->setupUi(this);

	auto& battle = project.database().battlecommands;
	const bool is2k3 = project.database().system.ldb_id == 2003;
	ui->labelRpg2003Only->setVisible(!is2k3);
	ui->widgetSettings->setEnabled(is2k3);

	auto bindGroup = [this](std::initializer_list<QAbstractButton*> buttons, int32_t& data) {
		auto* group = makeGroup(this, buttons);
		LcfWidgetBinding::connect<int32_t>(this, group);
		LcfWidgetBinding::bind(group, data);
	};
	bindGroup({ui->radioTraditional, ui->radioAlternative, ui->radioGauge}, battle.battle_type);
	bindGroup({ui->radioWindowLarge, ui->radioWindowSmall}, battle.window_size);
	bindGroup({ui->radioPlacementManual, ui->radioPlacementAutomatic}, battle.placement);
	bindGroup({ui->radioRowFront, ui->radioRowBack}, battle.row);
	LcfWidgetBinding::connect<int32_t>(this, ui->checkTransparent);
	LcfWidgetBinding::bind(ui->checkTransparent, battle.transparency);

	m_deathHandler = makeGroup(this, {ui->radioDeathGameOver, ui->radioDeathEvent});
	m_deathHandler->button(battle.death_handler ? 1 : 0)->setChecked(true);
	connect(m_deathHandler, &QButtonGroup::idToggled, this, [this](int id, bool checked) {
		if (checked) {
			// RPG Maker writes both chunks; RPG_RT only reads death_handler
			auto& battle = m_project.database().battlecommands;
			battle.death_handler = battle.death_handler_unused = id == 1;
			updateEnabled();
		}
	});
	ui->comboDeathEvent->makeModel(project);
	LcfWidgetBinding::connect<int32_t>(this, ui->comboDeathEvent);
	LcfWidgetBinding::bind(ui->comboDeathEvent, battle.death_event);

	LcfWidgetBinding::connect<bool>(this, ui->checkDeathTeleport);
	LcfWidgetBinding::bind(ui->checkDeathTeleport, battle.death_teleport);
	connect(ui->checkDeathTeleport, &QCheckBox::toggled, this, &BattleScreenWidget::updateEnabled);
	for (const auto& map : project.treeMap().maps) {
		if (map.ID > 0) {
			ui->comboDeathMap->addItem(QString("%1: %2").arg(map.ID, 4, 10, QChar('0')).arg(ToQString(map.name)), map.ID);
		}
	}
	if (ui->comboDeathMap->findData(battle.death_teleport_id) < 0) {
		ui->comboDeathMap->addItem(tr("Unknown Map (%1)").arg(battle.death_teleport_id), battle.death_teleport_id);
	}
	LcfWidgetBinding::connect<int32_t>(this, ui->comboDeathMap);
	LcfWidgetBinding::bind(ui->comboDeathMap, battle.death_teleport_id);
	for (auto* spin : {ui->spinDeathX, ui->spinDeathY}) {
		LcfWidgetBinding::connect<int32_t>(this, spin);
	}
	LcfWidgetBinding::bind(ui->spinDeathX, battle.death_teleport_x);
	LcfWidgetBinding::bind(ui->spinDeathY, battle.death_teleport_y);
	ui->comboDeathFacing->addItem(tr("Retain Facing"), 0);
	ui->comboDeathFacing->addItem(tr("Up"), 1);
	ui->comboDeathFacing->addItem(tr("Right"), 2);
	ui->comboDeathFacing->addItem(tr("Down"), 3);
	ui->comboDeathFacing->addItem(tr("Left"), 4);
	LcfWidgetBinding::connect<int32_t>(this, ui->comboDeathFacing);
	LcfWidgetBinding::bind(ui->comboDeathFacing, battle.death_teleport_face);

	ui->comboCommandType->addItem(tr("Attack"), lcf::rpg::BattleCommand::Type_attack);
	ui->comboCommandType->addItem(tr("Skill"), lcf::rpg::BattleCommand::Type_skill);
	ui->comboCommandType->addItem(tr("Skill Subset"), lcf::rpg::BattleCommand::Type_subskill);
	ui->comboCommandType->addItem(tr("Defend"), lcf::rpg::BattleCommand::Type_defense);
	ui->comboCommandType->addItem(tr("Item"), lcf::rpg::BattleCommand::Type_item);
	ui->comboCommandType->addItem(tr("Escape"), lcf::rpg::BattleCommand::Type_escape);
	ui->comboCommandType->addItem(tr("Link to Event"), lcf::rpg::BattleCommand::Type_special);
	LcfWidgetBinding::connect(this, ui->lineCommandName);
	LcfWidgetBinding::connect<int32_t>(this, ui->comboCommandType);
	connect(ui->listCommands, &QListWidget::currentRowChanged, this, &BattleScreenWidget::showCommand);
	connect(ui->lineCommandName, &QLineEdit::textChanged, this, [this] {
		const int row = ui->listCommands->currentRow();
		const auto& commands = m_project.database().battlecommands.commands;
		if (row >= 0 && row < static_cast<int>(commands.size())) {
			ui->listCommands->item(row)->setText(commandText(commands[static_cast<size_t>(row)]));
		}
	});
	// Commands are referenced by their ID: only the last one can go, like
	// shrinking the list in RPG Maker
	connect(ui->pushAddCommand, &QPushButton::clicked, this, [this] {
		auto& commands = m_project.database().battlecommands.commands;
		lcf::rpg::BattleCommand command;
		command.ID = static_cast<int>(commands.size()) + 1;
		commands.push_back(command);
		fillCommands(command.ID - 1);
	});
	connect(ui->pushRemoveCommand, &QPushButton::clicked, this, [this] {
		auto& commands = m_project.database().battlecommands.commands;
		if (commands.size() > 1) {
			commands.pop_back();
		}
		fillCommands(static_cast<int>(commands.size()) - 1);
	});
	fillCommands(0);

	updateEnabled();
}

BattleScreenWidget::~BattleScreenWidget()
{
	delete ui;
}

void BattleScreenWidget::fillCommands(int current_row)
{
	const auto& commands = m_project.database().battlecommands.commands;
	{
		SignalBlocker blocker(ui->listCommands);
		ui->listCommands->clear();
		for (const auto& command : commands) {
			ui->listCommands->addItem(commandText(command));
		}
		ui->listCommands->setCurrentRow(current_row);
	}
	// the list can have moved in memory: bind the editors again
	showCommand(ui->listCommands->currentRow());
	ui->pushRemoveCommand->setEnabled(commands.size() > 1);
}

void BattleScreenWidget::showCommand(int row)
{
	auto& commands = m_project.database().battlecommands.commands;
	const bool valid = row >= 0 && row < static_cast<int>(commands.size());
	if (valid) {
		auto& command = commands[static_cast<size_t>(row)];
		LcfWidgetBinding::bind(ui->lineCommandName, command.name);
		LcfWidgetBinding::bind(ui->comboCommandType, command.type);
	} else {
		for (QWidget* editor : std::initializer_list<QWidget*>{ui->lineCommandName, ui->comboCommandType}) {
			editor->setProperty("ee_data", QVariant());
		}
		SignalBlocker blocker({ui->lineCommandName, ui->comboCommandType});
		ui->lineCommandName->clear();
		ui->comboCommandType->setCurrentIndex(-1);
	}
	ui->lineCommandName->setEnabled(valid);
	ui->comboCommandType->setEnabled(valid);
}

void BattleScreenWidget::updateEnabled()
{
	const bool event = ui->radioDeathEvent->isChecked();
	ui->comboDeathEvent->setEnabled(event);
	ui->groupDeathTeleport->setEnabled(event);
	const bool teleport = ui->checkDeathTeleport->isChecked();
	for (QWidget* widget : std::initializer_list<QWidget*>{ui->labelDeathMap, ui->comboDeathMap,
			ui->labelDeathX, ui->spinDeathX, ui->labelDeathY, ui->spinDeathY, ui->labelDeathFacing, ui->comboDeathFacing}) {
		widget->setEnabled(teleport);
	}
}
