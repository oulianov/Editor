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

#include "test_battle_dialog.h"
#include "test_battle_party_widget.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>
#include <algorithm>

#include "common/dbstring.h"
#include "model/project_data.h"
#include "ui/other/run_game_dialog.h"

TestBattleDialog::TestBattleDialog(ProjectData& project, int troop_id, QWidget *parent) :
	QDialog(parent),
	m_project(project),
	m_troopId(troop_id)
{
	setObjectName(QStringLiteral("TestBattleDialog"));
	setWindowTitle(tr("Test Battle"));
	const auto& database = project.database();
	const auto& system = database.system;

	auto* layout = new QVBoxLayout(this);
	auto* columns = new QHBoxLayout();
	m_party = new TestBattlePartyWidget(project, this);
	columns->addWidget(m_party, 1);

	auto* right = new QVBoxLayout();
	m_formationGroup = new QGroupBox(tr("Party Formation"), this);
	m_formationGroup->setObjectName(QStringLiteral("groupFormation"));
	auto* formation_layout = new QVBoxLayout(m_formationGroup);
	m_formation = new QButtonGroup(this);
	m_formation->setObjectName(QStringLiteral("formation"));
	const std::vector<std::pair<int, QString>> formations = {
		{lcf::rpg::System::BattleFormation_loose, tr("Use a Loose Formation")},
		{lcf::rpg::System::BattleFormation_tight, tr("Use a Tight Formation")},
		{lcf::rpg::System::BattleFormation_terrain, tr("Use the following Terrain:")},
	};
	for (const auto& [id, text] : formations) {
		auto* radio = new QRadioButton(text, m_formationGroup);
		m_formation->addButton(radio, id);
		formation_layout->addWidget(radio);
	}
	m_terrain = new QComboBox(m_formationGroup);
	m_terrain->setObjectName(QStringLiteral("comboTerrain"));
	m_terrain->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
	m_terrain->setMinimumContentsLength(14);
	for (const auto& terrain : database.terrains) {
		m_terrain->addItem(QStringLiteral("%1: %2").arg(terrain.ID, 4, 10, QChar('0')).arg(ToQString(terrain.name)), terrain.ID);
	}
	formation_layout->addWidget(m_terrain);
	right->addWidget(m_formationGroup);

	m_conditionGroup = new QGroupBox(tr("Special Battle Condition"), this);
	m_conditionGroup->setObjectName(QStringLiteral("groupCondition"));
	auto* condition_layout = new QVBoxLayout(m_conditionGroup);
	m_condition = new QButtonGroup(this);
	m_condition->setObjectName(QStringLiteral("condition"));
	const std::vector<std::pair<int, QString>> conditions = {
		{lcf::rpg::System::BattleCondition_none, tr("No Special Conditions")},
		{lcf::rpg::System::BattleCondition_back, tr("Back Attack")},
		{lcf::rpg::System::BattleCondition_surround, tr("Surround Attack")},
		{lcf::rpg::System::BattleCondition_pincers, tr("Pincers Attack")},
		{lcf::rpg::System::BattleCondition_initiative, tr("Initiative")},
	};
	for (const auto& [id, text] : conditions) {
		auto* radio = new QRadioButton(text, m_conditionGroup);
		m_condition->addButton(radio, id);
		condition_layout->addWidget(radio);
	}
	right->addWidget(m_conditionGroup);

	auto* start = new QPushButton(tr("Start Test"), this);
	start->setObjectName(QStringLiteral("pushStartTest"));
	start->setToolTip(tr("Saves the database, then starts the EasyRPG Player in a battle against this monster group"));
	right->addWidget(start);
	right->addStretch(1);
	columns->addLayout(right);
	layout->addLayout(columns, 1);

	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	buttons->setObjectName(QStringLiteral("buttonBoxTestBattle"));
	connect(buttons, &QDialogButtonBox::accepted, this, &TestBattleDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &TestBattleDialog::reject);
	connect(start, &QPushButton::clicked, this, &TestBattleDialog::startTest);
	connect(m_formation, &QButtonGroup::idClicked, this, &TestBattleDialog::updateEnabled);
	layout->addWidget(buttons);

	// the settings of the last test
	auto* formation = m_formation->button(system.battletest_formation);
	(formation ? formation : m_formation->button(lcf::rpg::System::BattleFormation_loose))->setChecked(true);
	auto* condition = m_condition->button(system.battletest_condition);
	(condition ? condition : m_condition->button(lcf::rpg::System::BattleCondition_none))->setChecked(true);
	m_terrain->setCurrentIndex(std::max(0, m_terrain->findData(system.battletest_terrain)));
	updateEnabled();
}

void TestBattleDialog::updateEnabled() {
	const auto& database = m_project.database();
	if (database.system.ldb_id != 2003) {
		// RPG Maker 2000 battles have neither: the Player only takes the group
		for (auto* group : {m_formationGroup, m_conditionGroup}) {
			group->setEnabled(false);
			group->setToolTip(tr("Only in RPG Maker 2003 games"));
		}
		return;
	}
	// The formation places whoever is placed automatically: the heroes
	// (Battle Layout) or the monsters (the group's alignment)
	const bool manual_heroes = database.battlecommands.placement == lcf::rpg::BattleCommands::Placement_manual;
	const auto* troop = m_troopId >= 1 && m_troopId <= static_cast<int>(database.troops.size())
		? &database.troops[static_cast<size_t>(m_troopId) - 1] : nullptr;
	const bool manual_monsters = troop && !troop->auto_alignment;
	m_formationGroup->setEnabled(!(manual_heroes && manual_monsters));
	m_formationGroup->setToolTip(m_formationGroup->isEnabled() ? QString()
		: tr("The heroes and the monsters are placed by hand (Battle Layout, Alignment of the group)"));
	m_terrain->setEnabled(m_formation->checkedId() == lcf::rpg::System::BattleFormation_terrain);

	// Like RPG Maker: no surround or pincers attack when the heroes are placed by hand
	for (int id : {lcf::rpg::System::BattleCondition_surround, lcf::rpg::System::BattleCondition_pincers}) {
		auto* radio = m_condition->button(id);
		radio->setEnabled(!manual_heroes);
		radio->setToolTip(manual_heroes ? tr("Not with the manual placement of the Battle Layout") : QString());
		if (manual_heroes && radio->isChecked()) {
			m_condition->button(lcf::rpg::System::BattleCondition_none)->setChecked(true);
		}
	}
}

void TestBattleDialog::save() {
	auto& system = m_project.database().system;
	m_party->save();
	system.battletest_formation = m_formation->checkedId();
	system.battletest_condition = m_condition->checkedId();
	system.battletest_terrain = m_terrain->currentData().toInt();
}

void TestBattleDialog::accept() {
	save();
	QDialog::accept();
}

void TestBattleDialog::startTest() {
	save();
	// The Player reads the files: apply the database first, as RPG Maker
	// saves it before a test battle
	QWidget* database = parentWidget() ? parentWidget()->window() : nullptr;
	auto* box = database ? database->findChild<QDialogButtonBox*>(QStringLiteral("buttonBox"), Qt::FindDirectChildrenOnly) : nullptr;
	if (QPushButton* apply = box ? box->button(QDialogButtonBox::Apply) : nullptr) {
		apply->click();
	}

	const bool rpg2k3 = m_project.database().system.ldb_id == 2003;
	bool started;
	if (rpg2k3) {
		started = RunGameDialog::runBattle(m_troopId, m_formation->checkedId(), m_condition->checkedId(),
			std::max(1, m_terrain->currentData().toInt()));
	} else {
		started = RunGameDialog::runBattle(m_troopId);
	}
	// RPG Maker closes the window when the battle ends
	if (started) {
		QDialog::accept();
	}
}
