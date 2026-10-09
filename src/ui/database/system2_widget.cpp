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

#include "system2_widget.h"
#include <array>
#include <QCheckBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>
#include <lcf/rpg/database.h>
#include "defines.h"
#include "common/lcf_widget_binding.h"
#include "model/project_data.h"
#include "ui/common/rpg_combobox.h"
#include "system_fields.h"

System2Widget::System2Widget(ProjectData& project, QWidget *parent) :
	QWidget(parent)
{
	auto& database = project.database();
	auto& sys = database.system;
	const bool rpg2k3 = sys.ldb_id == 2003;

	auto* page = new QWidget();
	auto* layout = new QVBoxLayout(page);
	auto* top = new QHBoxLayout();
	layout->addLayout(top);

	// RPG Maker 2003: the commands of the main menu, in order, then "Quit Game"
	auto* menu = new QGroupBox(tr("Menu Commands"), page);
	auto* menu_grid = new QGridLayout(menu);
	const std::vector<std::pair<int, QString>> commands = {
		{1, tr("Item")}, {2, tr("Skill")}, {3, tr("Equipment")}, {4, tr("Save")},
		{5, tr("Status")}, {6, tr("Row")}, {7, tr("Order")}, {8, tr("Wait")}
	};
	std::vector<QComboBox*> menu_combos;
	for (int i = 0; i < 8; ++i) {
		auto* combo = new QComboBox(menu);
		combo->addItem(tr("<none>"), 0);
		for (const auto& [id, name] : commands) {
			combo->addItem(name, id);
		}
		// read down the first column, then the second
		menu_grid->addWidget(combo, i % 4, i / 4);
		menu_combos.push_back(combo);
	}
	menu_grid->addWidget(new QLabel(tr("Quit Game"), menu), 4, 1);
	SystemFields::bindIdList(this, menu_combos, sys.menu_commands);
	top->addWidget(menu, 1);

	// RPG Maker 2003: frame, battle graphics and title
	auto* graphics = new QGroupBox(tr("Graphics"), page);
	auto* graphics_form = new QFormLayout(graphics);
	graphics_form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	auto* use_frame = new QCheckBox(tr("Use Frame"), graphics);
	graphics_form->addRow(use_frame);
	graphics_form->addRow(tr("Frame"), SystemFields::imageField(project, FRAME, sys.frame_name, graphics));
	graphics_form->addRow(tr("Battle Graphics"), SystemFields::imageField(project, SYSTEM2, sys.system2_name, graphics));
	auto* invert = new QCheckBox(tr("Reverse Battle Animations when Attacked from Behind"), graphics);
	graphics_form->addRow(invert);
	// RPG Maker 2003 1.11: without title screen, the game starts on the first map
	auto* show_title = new QCheckBox(tr("Show Title Screen"), graphics);
	graphics_form->addRow(show_title);
	for (auto [check, value] : std::initializer_list<std::pair<QCheckBox*, bool*>>{
			{use_frame, &sys.show_frame}, {invert, &sys.invert_animations}, {show_title, &sys.show_title}}) {
		LcfWidgetBinding::connect<bool>(this, check);
		LcfWidgetBinding::bind(check, *value);
	}
	top->addWidget(graphics, 1);

	menu->setEnabled(rpg2k3);
	graphics->setEnabled(rpg2k3);

	// Test battle: the party (members without hero are left out) and the battle
	auto* test = new QGroupBox(tr("Test Battle"), page);
	auto* test_layout = new QVBoxLayout(test);
	auto* party = new QGridLayout();
	const QStringList headers = {tr("Hero"), tr("Level"), tr("Weapon"), tr("Shield"), tr("Armor"), tr("Helmet"), tr("Accessory")};
	for (int i = 0; i < headers.size(); ++i) {
		party->addWidget(new QLabel(headers[i], test), 0, i);
		party->setColumnStretch(i, i == 1 ? 0 : 1);
	}
	struct Member {
		QComboBox* hero;
		QSpinBox* level;
		std::array<QComboBox*, 5> equipment;
	};
	std::array<Member, 4> members;
	for (size_t m = 0; m < members.size(); ++m) {
		lcf::rpg::TestBattler battler;
		battler.actor_id = 0;
		if (m < sys.battletest_data.size()) {
			battler = sys.battletest_data[m];
		}
		auto& member = members[m];
		const int row = static_cast<int>(m) + 1;
		member.hero = new QComboBox(test);
		SystemFields::fillWithNone(member.hero, database.actors);
		member.hero->setCurrentIndex(std::max(0, member.hero->findData(battler.actor_id)));
		party->addWidget(member.hero, row, 0);
		member.level = new QSpinBox(test);
		member.level->setRange(1, rpg2k3 ? 99 : 50);
		member.level->setValue(battler.level);
		party->addWidget(member.level, row, 1);
		const std::array<int32_t, 5> equipped = {battler.weapon_id, battler.shield_id, battler.armor_id, battler.helmet_id, battler.accessory_id};
		for (int e = 0; e < 5; ++e) {
			auto* combo = new QComboBox(test);
			const int type = lcf::rpg::Item::Type_weapon + e;
			SystemFields::fillWithNone(combo, database.items, [type](const lcf::rpg::Item& item) { return item.type == type; });
			combo->setCurrentIndex(std::max(0, combo->findData(equipped[e])));
			party->addWidget(combo, row, e + 2);
			member.equipment[e] = combo;
		}
	}
	auto sync = [&sys, members] {
		sys.battletest_data.clear();
		for (const auto& member : members) {
			const int hero = member.hero->currentData().toInt();
			if (!hero) {
				continue;
			}
			lcf::rpg::TestBattler battler;
			battler.ID = static_cast<int>(sys.battletest_data.size()) + 1;
			battler.actor_id = hero;
			battler.level = member.level->value();
			battler.weapon_id = member.equipment[0]->currentData().toInt();
			battler.shield_id = member.equipment[1]->currentData().toInt();
			battler.armor_id = member.equipment[2]->currentData().toInt();
			battler.helmet_id = member.equipment[3]->currentData().toInt();
			battler.accessory_id = member.equipment[4]->currentData().toInt();
			sys.battletest_data.push_back(battler);
		}
	};
	for (const auto& member : members) {
		connect(member.hero, &QComboBox::currentIndexChanged, this, sync);
		connect(member.level, &QSpinBox::valueChanged, this, sync);
		for (auto* combo : member.equipment) {
			connect(combo, &QComboBox::currentIndexChanged, this, sync);
		}
	}
	test_layout->addLayout(party);

	auto* options = new QFormLayout();
	options->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	auto* formation = new QComboBox(test);
	formation->addItem(tr("Use a Loose Formation"), lcf::rpg::System::BattleFormation_loose);
	formation->addItem(tr("Use a Tight Formation"), lcf::rpg::System::BattleFormation_tight);
	formation->addItem(tr("Use the following Terrain:"), lcf::rpg::System::BattleFormation_terrain);
	auto* terrain = new TerrainRpgComboBox(test);
	terrain->makeModel(project);
	auto* formation_row = new QHBoxLayout();
	formation_row->addWidget(formation);
	formation_row->addWidget(terrain, 1);
	options->addRow(tr("Party Formation"), formation_row);
	auto* condition = new QComboBox(test);
	condition->addItem(tr("No Special Conditions"), lcf::rpg::System::BattleCondition_none);
	condition->addItem(tr("Back Attack"), lcf::rpg::System::BattleCondition_back);
	condition->addItem(tr("Surround Attack"), lcf::rpg::System::BattleCondition_surround);
	condition->addItem(tr("Pincers Attack"), lcf::rpg::System::BattleCondition_pincers);
	condition->addItem(tr("Initiative"), lcf::rpg::System::BattleCondition_initiative);
	options->addRow(tr("Special Battle Condition"), condition);
	options->addRow(tr("Background"), SystemFields::imageField(project, BACKDROP, sys.battletest_background, test));
	test_layout->addLayout(options);

	LcfWidgetBinding::connect<int32_t>(this, formation);
	LcfWidgetBinding::bind(formation, sys.battletest_formation);
	LcfWidgetBinding::connect<int32_t>(this, condition);
	LcfWidgetBinding::bind(condition, sys.battletest_condition);
	LcfWidgetBinding::connect<int32_t>(this, terrain);
	LcfWidgetBinding::bind(terrain, sys.battletest_terrain);

	// Formations and conditions are RPG Maker 2003 battles
	formation->setEnabled(rpg2k3);
	condition->setEnabled(rpg2k3);
	options->labelForField(formation_row)->setEnabled(rpg2k3);
	options->labelForField(condition)->setEnabled(rpg2k3);
	auto update_terrain = [=] {
		terrain->setEnabled(rpg2k3 && formation->currentData().toInt() == lcf::rpg::System::BattleFormation_terrain);
	};
	update_terrain();
	connect(formation, &QComboBox::currentIndexChanged, this, update_terrain);

	layout->addWidget(test);
	layout->addStretch(1);

	auto* outer = new QVBoxLayout(this);
	outer->setContentsMargins(0, 0, 0, 0);
	outer->addWidget(SystemFields::scrollable(page));
}
