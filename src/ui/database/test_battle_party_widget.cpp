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

#include "test_battle_party_widget.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>
#include <algorithm>

#include "common/dbstring.h"
#include "model/actor.h"
#include "model/project_data.h"

namespace {
QString entryText(int id, const lcf::DBString& name) {
	return QStringLiteral("%1: %2").arg(id, 4, 10, QChar('0')).arg(ToQString(name));
}

lcf::rpg::Actor* findActor(lcf::rpg::Database& database, int id) {
	return id >= 1 && id <= static_cast<int>(database.actors.size()) ? &database.actors[static_cast<size_t>(id) - 1] : nullptr;
}

const lcf::rpg::Item* findItem(const lcf::rpg::Database& database, int id) {
	return id >= 1 && id <= static_cast<int>(database.items.size()) ? &database.items[static_cast<size_t>(id) - 1] : nullptr;
}

/** A parameter of the hero's curve at a level */
int curveValue(const std::vector<int16_t>& curve, int level) {
	return level >= 1 && level <= static_cast<int>(curve.size()) ? curve[static_cast<size_t>(level) - 1] : 0;
}
}

TestBattlePartyWidget::TestBattlePartyWidget(ProjectData& project, QWidget *parent) :
	QWidget(parent),
	m_project(project)
{
	auto* layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);

	auto* size_group = new QGroupBox(tr("Number of Party Members"), this);
	size_group->setObjectName(QStringLiteral("groupPartySize"));
	auto* size_layout = new QHBoxLayout(size_group);
	m_size = new QButtonGroup(this);
	m_size->setObjectName(QStringLiteral("partySize"));
	for (int i = 0; i <= kMembers; ++i) {
		auto* radio = new QRadioButton(i == 0 ? tr("None") : QString::number(i), size_group);
		radio->setObjectName(QStringLiteral("radioPartySize%1").arg(i));
		m_size->addButton(radio, i);
		size_layout->addWidget(radio);
	}
	size_layout->addStretch(1);
	layout->addWidget(size_group);

	auto* parameters_group = new QGroupBox(tr("Character Parameters"), this);
	auto* parameters_layout = new QVBoxLayout(parameters_group);
	m_tabs = new QTabWidget(parameters_group);
	m_tabs->setObjectName(QStringLiteral("tabPartyMembers"));
	for (int i = 0; i < kMembers; ++i) {
		m_tabs->addTab(makeMemberPage(i), tr("Hero %1").arg(i + 1));
	}
	parameters_layout->addWidget(m_tabs);
	layout->addWidget(parameters_group, 1);

	connect(m_size, &QButtonGroup::idClicked, this, &TestBattlePartyWidget::updateEnabled);
	load();
}

QWidget* TestBattlePartyWidget::makeMemberPage(int index) {
	auto* page = new QWidget(m_tabs);
	auto& member = m_members[static_cast<size_t>(index)];
	const QString suffix = QString::number(index + 1);
	auto& database = m_project.database();

	auto* columns = new QHBoxLayout(page);
	auto* left = new QVBoxLayout();
	auto* form = new QFormLayout();
	member.hero = new QComboBox(page);
	member.hero->setObjectName(QStringLiteral("comboHero") + suffix);
	member.hero->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
	member.hero->setMinimumContentsLength(14);
	for (const auto& actor : database.actors) {
		member.hero->addItem(entryText(actor.ID, actor.name), actor.ID);
	}
	form->addRow(tr("Hero"), member.hero);
	auto* level_row = new QHBoxLayout();
	member.level = new QSpinBox(page);
	member.level->setObjectName(QStringLiteral("spinLevel") + suffix);
	auto* initial = new QPushButton(tr("Initial Stats"), page);
	initial->setObjectName(QStringLiteral("pushInitialStats") + suffix);
	initial->setToolTip(tr("The level and equipment the hero starts the game with"));
	level_row->addWidget(member.level);
	level_row->addWidget(initial);
	level_row->addStretch(1);
	form->addRow(tr("Level"), level_row);
	left->addLayout(form);

	auto* equipment = new QGroupBox(tr("Equipment"), page);
	auto* equipment_form = new QFormLayout(equipment);
	const std::array<QString, SlotCount> slot_names = {tr("Weapon"), tr("Shield"), tr("Armor"), tr("Helmet"), tr("Accessory")};
	const std::array<QString, SlotCount> slot_objects = {
		QStringLiteral("comboWeapon"), QStringLiteral("comboShield"), QStringLiteral("comboArmor"),
		QStringLiteral("comboHelmet"), QStringLiteral("comboAccessory")};
	for (int slot = 0; slot < SlotCount; ++slot) {
		auto* combo = new QComboBox(equipment);
		combo->setObjectName(slot_objects[static_cast<size_t>(slot)] + suffix);
		combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
		combo->setMinimumContentsLength(14);
		member.equipment[static_cast<size_t>(slot)] = combo;
		auto* label = new QLabel(slot_names[static_cast<size_t>(slot)], equipment);
		if (slot == Shield) {
			member.shieldLabel = label;
		}
		equipment_form->addRow(label, combo);
		connect(combo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, index, slot, combo] {
			slotData(m_battlers[static_cast<size_t>(index)], slot) = combo->currentData().toInt();
			updateParameters(index);
		});
	}
	left->addWidget(equipment);
	left->addStretch(1);
	columns->addLayout(left, 1);

	auto* parameters = new QGroupBox(tr("Parameters"), page);
	auto* parameters_form = new QFormLayout(parameters);
	const std::array<QString, ParameterCount> parameter_names = {
		tr("Max HP"), tr("Max MP"), tr("Attack"), tr("Defense"), tr("Intellect"), tr("Agility")};
	for (int i = 0; i < ParameterCount; ++i) {
		auto* value = new QLabel(parameters);
		value->setObjectName(QStringLiteral("labelParameter%1_%2").arg(i).arg(suffix));
		member.parameters[static_cast<size_t>(i)] = value;
		parameters_form->addRow(parameter_names[static_cast<size_t>(i)], value);
	}
	auto* right = new QVBoxLayout();
	right->addWidget(parameters);
	right->addStretch(1);
	columns->addLayout(right);

	connect(member.hero, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, index] {
		// another hero starts from their own stats, like in RPG Maker
		m_battlers[static_cast<size_t>(index)].actor_id = m_members[static_cast<size_t>(index)].hero->currentData().toInt();
		initialStats(index);
	});
	connect(member.level, qOverload<int>(&QSpinBox::valueChanged), this, [this, index](int value) {
		m_battlers[static_cast<size_t>(index)].level = value;
		updateParameters(index);
	});
	connect(initial, &QPushButton::clicked, this, [this, index] { initialStats(index); });
	return page;
}

int32_t& TestBattlePartyWidget::slotData(lcf::rpg::TestBattler& battler, int slot) {
	switch (slot) {
		case Weapon: return battler.weapon_id;
		case Shield: return battler.shield_id;
		case Armor: return battler.armor_id;
		case Helmet: return battler.helmet_id;
		default: return battler.accessory_id;
	}
}

void TestBattlePartyWidget::load() {
	auto& database = m_project.database();
	const auto& data = database.system.battletest_data;
	for (int i = 0; i < kMembers; ++i) {
		auto& battler = m_battlers[static_cast<size_t>(i)];
		if (i < static_cast<int>(data.size())) {
			battler = data[static_cast<size_t>(i)];
			// RPG Maker leaves garbage in the upper bits (see Game_Party::SetupBattleTest)
			for (int slot = 0; slot < SlotCount; ++slot) {
				slotData(battler, slot) &= 0xFFFF;
			}
		} else {
			battler = lcf::rpg::TestBattler();
			battler.actor_id = std::min(i + 1, static_cast<int>(database.actors.size()));
			initialStats(i);
		}
	}
	const int count = std::min(static_cast<int>(data.size()), kMembers);
	m_size->button(count)->setChecked(true);
	for (int i = 0; i < kMembers; ++i) {
		showMember(i);
	}
	updateEnabled();
}

void TestBattlePartyWidget::save() {
	auto& database = m_project.database();
	auto& data = database.system.battletest_data;
	data.clear();
	for (int i = 0; i < std::max(0, m_size->checkedId()); ++i) {
		// the Player adds every hero of the list: no unknown one
		if (findActor(database, m_battlers[static_cast<size_t>(i)].actor_id)) {
			data.push_back(m_battlers[static_cast<size_t>(i)]);
			data.back().ID = static_cast<int>(data.size());
		}
	}
}

void TestBattlePartyWidget::initialStats(int index) {
	auto& battler = m_battlers[static_cast<size_t>(index)];
	if (const auto* actor = findActor(m_project.database(), battler.actor_id)) {
		battler.level = actor->initial_level;
		battler.weapon_id = actor->initial_equipment.weapon_id;
		battler.shield_id = actor->initial_equipment.shield_id;
		battler.armor_id = actor->initial_equipment.armor_id;
		battler.helmet_id = actor->initial_equipment.helmet_id;
		battler.accessory_id = actor->initial_equipment.accessory_id;
	}
	if (m_members[static_cast<size_t>(index)].hero) {
		showMember(index);
	}
}

void TestBattlePartyWidget::showMember(int index) {
	auto& database = m_project.database();
	auto& member = m_members[static_cast<size_t>(index)];
	auto& battler = m_battlers[static_cast<size_t>(index)];
	auto* actor = findActor(database, battler.actor_id);

	std::vector<QObject*> controls = {member.hero, member.level};
	controls.insert(controls.end(), member.equipment.begin(), member.equipment.end());
	for (auto* control : controls) {
		control->blockSignals(true);
	}

	member.hero->setCurrentIndex(member.hero->findData(battler.actor_id));
	const int max_level = actor && actor->final_level > 0 ? actor->final_level
		: (database.system.ldb_id == 2003 ? 99 : 50);
	member.level->setRange(1, max_level);
	member.level->setValue(battler.level);
	battler.level = member.level->value();

	// The items the hero can equip in each slot; heroes with two weapons
	// hold a weapon in the shield slot
	const bool two_weapons = actor && actor->two_weapon;
	member.shieldLabel->setText(two_weapons ? tr("Weapon") : tr("Shield"));
	const std::array<int, SlotCount> types = {lcf::rpg::Item::Type_weapon,
		two_weapons ? lcf::rpg::Item::Type_weapon : lcf::rpg::Item::Type_shield,
		lcf::rpg::Item::Type_armor, lcf::rpg::Item::Type_helmet, lcf::rpg::Item::Type_accessory};
	for (int slot = 0; slot < SlotCount; ++slot) {
		auto* combo = member.equipment[static_cast<size_t>(slot)];
		combo->clear();
		combo->addItem(tr("<None>"), 0);
		for (const auto& item : database.items) {
			if (item.type == types[static_cast<size_t>(slot)] && (!actor || ActorModel(m_project, *actor).IsItemUsable(item))) {
				combo->addItem(entryText(item.ID, item.name), item.ID);
			}
		}
		const int id = slotData(battler, slot);
		if (combo->findData(id) < 0) {
			const auto* item = findItem(database, id);
			combo->addItem(item ? entryText(id, item->name) : tr("<%1?>").arg(id), id);
		}
		combo->setCurrentIndex(combo->findData(id));
	}

	for (auto* control : controls) {
		control->blockSignals(false);
	}
	updateParameters(index);
}

void TestBattlePartyWidget::updateParameters(int index) {
	const auto& database = m_project.database();
	const auto& battler = m_battlers[static_cast<size_t>(index)];
	auto& labels = m_members[static_cast<size_t>(index)].parameters;
	std::array<int, ParameterCount> values{};
	if (const auto* actor = findActor(m_project.database(), battler.actor_id)) {
		const auto& curves = actor->parameters;
		values = {curveValue(curves.maxhp, battler.level), curveValue(curves.maxsp, battler.level),
			curveValue(curves.attack, battler.level), curveValue(curves.defense, battler.level),
			curveValue(curves.spirit, battler.level), curveValue(curves.agility, battler.level)};
	}
	// what the equipment adds, as the Player counts it
	for (int id : {battler.weapon_id, battler.shield_id, battler.armor_id, battler.helmet_id, battler.accessory_id}) {
		if (const auto* item = findItem(database, id)) {
			values[Attack] += item->atk_points1;
			values[Defense] += item->def_points1;
			values[Intellect] += item->spi_points1;
			values[Agility] += item->agi_points1;
		}
	}
	for (int i = 0; i < ParameterCount; ++i) {
		labels[static_cast<size_t>(i)]->setText(QString::number(values[static_cast<size_t>(i)]));
	}
}

void TestBattlePartyWidget::updateEnabled() {
	const int count = std::max(0, m_size->checkedId());
	for (int i = 0; i < kMembers; ++i) {
		m_tabs->setTabEnabled(i, i < count);
	}
	m_tabs->setEnabled(count > 0 && !m_project.database().actors.empty());
}
