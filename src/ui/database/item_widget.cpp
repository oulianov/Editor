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

#include "item_widget.h"

#include <QAbstractButton>
#include <QBoxLayout>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>

#include "common/lcf_widget_binding.h"
#include "model/project_data.h"
#include "ui/common/rpg_combobox.h"
#include "skill_item_common.h"

using Item = lcf::rpg::Item;

ItemWidget::ItemWidget(ProjectData& project, QWidget *parent) :
	QWidget(parent),
	m_project(project),
	m_current(&m_dummy)
{
	auto& database = project.database();
	const bool is2k3 = database.system.ldb_id == 2003;

	// Scrolls rather than clips on small screens
	auto* outer = new QVBoxLayout(this);
	outer->setContentsMargins(0, 0, 0, 0);
	auto* scroll = new QScrollArea(this);
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);
	outer->addWidget(scroll);
	auto* page = new QWidget(scroll);
	scroll->setWidget(page);
	auto* layout = new QVBoxLayout(page);

	auto addSpin = [&](QFormLayout* form, const QString& text, int32_t Item::* field, int min, int max, const QString& suffix = QString()) {
		auto* spin = new QSpinBox(form->parentWidget());
		spin->setRange(min, max);
		spin->setSuffix(suffix);
		form->addRow(text, spin);
		m_spins.emplace_back(spin, field);
		return spin;
	};
	auto addCheck = [&](QBoxLayout* box, const QString& text, bool Item::* field) {
		auto* check = new QCheckBox(text, box->parentWidget());
		box->addWidget(check);
		m_checks.emplace_back(check, field);
		return check;
	};
	auto addGroup = [&](const QString& title, QBoxLayout* column) {
		auto* group = new QGroupBox(title, page);
		column->addWidget(group);
		return group;
	};

	// Name, type, description, price and uses
	auto* header = new QGridLayout();
	layout->addLayout(header);
	auto addField = [&](const QString& text, QWidget* field, int row, int column, int span = 1) {
		auto* label = new QLabel(text, page);
		label->setBuddy(field);
		header->addWidget(label, row, column);
		header->addWidget(field, row, column + 1, 1, span);
		return label;
	};
	m_name = new QLineEdit(page);
	addField(tr("Name"), m_name, 0, 0);
	m_type = new QComboBox(page);
	m_type->setObjectName(QStringLiteral("itemType"));
	addChoices(m_type, {tr("Common Item"), tr("Weapon"), tr("Shield"), tr("Armor"), tr("Helmet"),
		tr("Accessory"), tr("Medicine"), tr("Skill Book"), tr("Seed"), tr("Skill Scroll"), tr("Switch")});
	addField(tr("Type"), m_type, 0, 2);
	m_description = new QLineEdit(page);
	addField(tr("Description"), m_description, 1, 0, 3);
	auto* price = new QSpinBox(page);
	price->setRange(0, 999999);
	m_spins.emplace_back(price, &Item::price);
	addField(tr("Price"), price, 2, 0);
	m_uses = new QComboBox(page);
	addChoices(m_uses, {tr("Unlimited Use"), tr("One Use (Normal)"), tr("Two Uses"), tr("Three Uses"),
		tr("Four Uses"), tr("Five Uses")});
	m_usesLabel = addField(tr("Number of Uses"), m_uses, 2, 2);
	header->setColumnStretch(1, 1);
	header->setColumnStretch(3, 1);

	// The settings of each type on the left, the lists on the right
	auto* columns = new QHBoxLayout();
	layout->addLayout(columns, 1);
	auto* first = new QVBoxLayout();
	auto* second = new QVBoxLayout();
	auto* third = new QVBoxLayout();
	auto* fourth = new QVBoxLayout();
	columns->addLayout(first);
	columns->addLayout(second);
	columns->addLayout(third, 1);
	columns->addLayout(fourth, 1);

	// Weapons
	auto* weapon = addGroup(tr("Weapon"), first);
	auto* weapon_form = new QFormLayout(weapon);
	auto* hands = new QHBoxLayout();
	m_hands = addRadios(weapon, hands, {{0, tr("One")}, {1, tr("Both")}});
	hands->addStretch();
	weapon_form->addRow(tr("Hands Required"), hands);
	addSpin(weapon_form, tr("MP Consumed"), &Item::sp_cost, 0, 999);
	addSpin(weapon_form, tr("Hit Probability"), &Item::hit, 0, 100, QStringLiteral("%"));
	addSpin(weapon_form, tr("Critical Bonus"), &Item::critical_hit, 0, 100, QStringLiteral("%"));
	auto* animation = addGroup(tr("Attack Animation"), first);
	// one above the other: the page must fit 900 pixels wide
	auto* animation_layout = new QVBoxLayout(animation);
	m_animation = new AnimationRpgComboBox(animation);
	m_animation->makeModel(project);
	animation_layout->addWidget(m_animation);
	m_battlerAnimation = new QPushButton(tr("Set..."), animation);
	m_battlerAnimation->setToolTip(tr("How each hero attacks with the weapon (RPG Maker 2003 battles)"));
	m_battlerAnimation->setEnabled(is2k3);
	animation_layout->addWidget(m_battlerAnimation, 0, Qt::AlignLeft);

	// Equipment
	m_statGroup = addGroup(tr("Statistic Change"), second);
	auto* stat_form = new QFormLayout(m_statGroup);
	addSpin(stat_form, tr("Attack"), &Item::atk_points1, -999, 999);
	addSpin(stat_form, tr("Defense"), &Item::def_points1, -999, 999);
	addSpin(stat_form, tr("Intelligence"), &Item::spi_points1, -999, 999);
	addSpin(stat_form, tr("Agility"), &Item::agi_points1, -999, 999);

	auto* weapon_options_group = addGroup(tr("Options"), second);
	m_weaponWidgets = {weapon, animation, weapon_options_group};
	auto* weapon_options = new QVBoxLayout(weapon_options_group);
	addCheck(weapon_options, tr("Always Attack First"), &Item::preemptive);
	addCheck(weapon_options, tr("Attack Twice"), &Item::dual_attack);
	addCheck(weapon_options, tr("Attack All Enemies"), &Item::attack_all);
	addCheck(weapon_options, tr("Ignore Enemy Evasion"), &Item::ignore_evasion);
	addCheck(weapon_options, tr("Cursed"), &Item::cursed)->setEnabled(is2k3);

	m_armorOptions = addGroup(tr("Options"), second);
	auto* armor_options = new QVBoxLayout(m_armorOptions);
	addCheck(armor_options, tr("Prevent Critical Hits"), &Item::prevent_critical);
	addCheck(armor_options, tr("Increased Evasion"), &Item::raise_evasion);
	addCheck(armor_options, tr("Half MP Consumption"), &Item::half_sp_cost);
	addCheck(armor_options, tr("No Terrain Damage"), &Item::no_terrain_damage);
	addCheck(armor_options, tr("Cursed"), &Item::cursed)->setEnabled(is2k3);

	m_attributeGroup = addGroup(QString(), third);
	auto* attribute_layout = new QVBoxLayout(m_attributeGroup);
	m_attributes = new BitArrayChecklist(m_attributeGroup);
	attribute_layout->addWidget(m_attributes);
	third->setStretchFactor(m_attributeGroup, 1);

	m_conditionGroup = addGroup(QString(), third);
	auto* condition_layout = new QVBoxLayout(m_conditionGroup);
	m_stateEffectRow = new QWidget(m_conditionGroup);
	auto* state_effect = new QFormLayout(m_stateEffectRow);
	state_effect->setContentsMargins(0, 0, 0, 0);
	auto* state_radios = new QVBoxLayout();
	// the texts depend on the type, see updateType
	m_stateEffect = addRadios(m_stateEffectRow, state_radios, {{0, QString()}, {1, QString()}});
	for (auto* button : m_stateEffect->buttons()) {
		button->setEnabled(is2k3);
	}
	state_effect->addRow(state_radios);
	auto* chance = new QSpinBox(m_stateEffectRow);
	chance->setRange(0, 100);
	chance->setSuffix(QStringLiteral("%"));
	m_spins.emplace_back(chance, &Item::state_chance);
	m_stateChanceLabel = new QLabel(m_stateEffectRow);
	state_effect->addRow(m_stateChanceLabel, chance);
	condition_layout->addWidget(m_stateEffectRow);
	m_states = new BitArrayChecklist(m_conditionGroup);
	condition_layout->addWidget(m_states);
	third->setStretchFactor(m_conditionGroup, 1);

	// Medicine
	auto* target = addGroup(tr("Target"), first);
	auto* target_layout = new QHBoxLayout(target);
	m_entireParty = addRadios(target, target_layout, {{0, tr("Single Ally")}, {1, tr("All Allies")}});
	target_layout->addStretch();
	auto* hp = addGroup(tr("HP Recovery"), first);
	auto* hp_form = new QFormLayout(hp);
	addSpin(hp_form, tr("Percentage of Max HP"), &Item::recover_hp_rate, 0, 100, QStringLiteral("%"));
	addSpin(hp_form, tr("Additional HP"), &Item::recover_hp, 0, 9999);
	auto* mp = addGroup(tr("MP Recovery"), first);
	auto* mp_form = new QFormLayout(mp);
	addSpin(mp_form, tr("Percentage of Max MP"), &Item::recover_sp_rate, 0, 100, QStringLiteral("%"));
	addSpin(mp_form, tr("Additional MP"), &Item::recover_sp, 0, 999);
	auto* medicine_options = addGroup(tr("Options"), first);
	auto* medicine_options_layout = new QVBoxLayout(medicine_options);
	addCheck(medicine_options_layout, tr("Only Usable on the Field Map"), &Item::occasion_field1);
	addCheck(medicine_options_layout, tr("Only Usable on Dead Characters"), &Item::ko_only);
	m_medicineWidgets = {target, hp, mp, medicine_options};

	// Seeds
	m_seedGroup = addGroup(tr("Base Statistic Change"), first);
	auto* seed_form = new QFormLayout(m_seedGroup);
	addSpin(seed_form, tr("Maximum HP"), &Item::max_hp_points, -9999, 9999);
	addSpin(seed_form, tr("Maximum MP"), &Item::max_sp_points, -999, 999);
	addSpin(seed_form, tr("Attack"), &Item::atk_points2, -999, 999);
	addSpin(seed_form, tr("Defense"), &Item::def_points2, -999, 999);
	addSpin(seed_form, tr("Intelligence"), &Item::spi_points2, -999, 999);
	addSpin(seed_form, tr("Agility"), &Item::agi_points2, -999, 999);

	// Skill books, skill scrolls, and equipment invoking a skill (RPG Maker 2003)
	m_skillGroup = addGroup(QString(), second);
	auto* skill_layout = new QVBoxLayout(m_skillGroup);
	m_useSkill = new QCheckBox(tr("Invoke Skill When Used"), m_skillGroup);
	m_useSkill->setEnabled(is2k3);
	skill_layout->addWidget(m_useSkill);
	m_skill = new SkillRpgComboBox(m_skillGroup);
	m_skill->makeModel(project);
	skill_layout->addWidget(m_skill);
	m_messageRow = new QWidget(m_skillGroup);
	auto* message_layout = new QVBoxLayout(m_messageRow);
	message_layout->setContentsMargins(0, 0, 0, 0);
	message_layout->addWidget(new QLabel(tr("Message Shown When Used"), m_messageRow));
	m_message = addRadios(m_messageRow, message_layout, {{0, tr("Item Name")}, {1, tr("Name of Associated Skill")}});
	skill_layout->addWidget(m_messageRow);

	// Switches
	m_switchGroup = addGroup(tr("Turn ON Switch"), first);
	auto* switch_layout = new QVBoxLayout(m_switchGroup);
	m_switch = new SwitchRpgComboBox(m_switchGroup);
	m_switch->makeModel(project);
	switch_layout->addWidget(m_switch);
	m_occasionGroup = addGroup(tr("Activation Allowed"), first);
	auto* occasion_layout = new QHBoxLayout(m_occasionGroup);
	addCheck(occasion_layout, tr("Field"), &Item::occasion_field2);
	addCheck(occasion_layout, tr("Battle"), &Item::occasion_battle);
	occasion_layout->addStretch();

	// Who can use it: RPG Maker 2003 chooses between heroes and classes, for all the items
	m_userGroup = addGroup(tr("Usable by:"), fourth);
	auto* user_layout = new QVBoxLayout(m_userGroup);
	auto* user_type = new QVBoxLayout();
	m_userType = addRadios(m_userGroup, user_type, {
		{lcf::rpg::System::EquipmentSetting_actor, tr("Hero")},
		{lcf::rpg::System::EquipmentSetting_class, tr("Class")}});
	for (auto* button : m_userType->buttons()) {
		button->setEnabled(is2k3);
		button->setToolTip(tr("Applies to all the items"));
	}
	user_layout->addLayout(user_type);
	m_users = new BitArrayChecklist(m_userGroup);
	user_layout->addWidget(m_users);
	fourth->setStretchFactor(m_userGroup, 1);

	first->addStretch();
	second->addStretch();

	// Bindings
	for (auto* line : {m_name, m_description}) {
		LcfWidgetBinding::connect(this, line);
	}
	for (auto* combo : {m_type, m_uses}) {
		LcfWidgetBinding::connect<int32_t>(this, combo);
	}
	for (const auto& [spin, field] : m_spins) {
		LcfWidgetBinding::connect<int32_t>(this, spin);
	}
	for (const auto& [check, field] : m_checks) {
		LcfWidgetBinding::connect<bool>(this, check);
	}
	LcfWidgetBinding::connect<bool>(this, m_useSkill);
	for (auto* group : {m_hands, m_stateEffect, m_entireParty}) {
		LcfWidgetBinding::connect<bool>(this, group);
	}
	LcfWidgetBinding::connect<int32_t>(this, m_message);
	LcfWidgetBinding::connect<int32_t>(this, m_userType);
	LcfWidgetBinding::connect<int32_t>(this, m_animation);
	LcfWidgetBinding::connect<int32_t>(this, m_skill);
	LcfWidgetBinding::connect<int32_t>(this, m_switch);

	// after the bindings: the item is up to date when they run
	connect(m_type, qOverload<int>(&QComboBox::currentIndexChanged), this, &ItemWidget::updateType);
	connect(m_useSkill, &QCheckBox::toggled, this, &ItemWidget::updateType);
	connect(m_userType, &QButtonGroup::idToggled, this, [this](int, bool checked) {
		if (checked) {
			bindUsers();
		}
	});
	connect(m_battlerAnimation, &QPushButton::clicked, this, [this] {
		BattlerAnimationDataEditor::exec(m_project, m_current->animation_data, m_current, this);
	});

	setData(nullptr);
}

void ItemWidget::setData(lcf::rpg::Item* item)
{
	m_current = item ? item : &m_dummy;
	auto& database = m_project.database();

	LcfWidgetBinding::bind(m_name, m_current->name);
	LcfWidgetBinding::bind(m_description, m_current->description);
	LcfWidgetBinding::bind(m_type, m_current->type);
	LcfWidgetBinding::bind(m_uses, m_current->uses);
	for (const auto& [spin, field] : m_spins) {
		LcfWidgetBinding::bind(spin, m_current->*field);
	}
	for (const auto& [check, field] : m_checks) {
		LcfWidgetBinding::bind(check, m_current->*field);
	}
	LcfWidgetBinding::bind(m_hands, m_current->two_handed);
	LcfWidgetBinding::bind(m_animation, m_current->animation_id);
	m_attributes->bind(m_current->attribute_set, databaseNames(database.attributes));
	LcfWidgetBinding::bind(m_stateEffect, m_current->reverse_state_effect);
	m_states->bind(m_current->state_set, databaseNames(database.states));
	LcfWidgetBinding::bind(m_entireParty, m_current->entire_party);
	LcfWidgetBinding::bind(m_useSkill, m_current->use_skill);
	LcfWidgetBinding::bind(m_skill, m_current->skill_id);
	LcfWidgetBinding::bind(m_message, m_current->using_message);
	LcfWidgetBinding::bind(m_switch, m_current->switch_id);
	LcfWidgetBinding::bind(m_userType, database.system.equipment_setting);
	bindUsers();

	updateType();
	setEnabled(item != nullptr);
}

void ItemWidget::updateType()
{
	const int type = m_current->type;
	const bool weapon = type == Item::Type_weapon;
	const bool armor = type >= Item::Type_shield && type <= Item::Type_accessory;
	const bool equipment = weapon || armor;
	const bool medicine = type == Item::Type_medicine;
	const bool book = type == Item::Type_book;
	const bool special = type == Item::Type_special;
	const bool switch_item = type == Item::Type_switch;

	// equipment is used up only by its skill
	m_usesLabel->setVisible(type >= Item::Type_medicine);
	m_uses->setVisible(type >= Item::Type_medicine);

	for (auto* widget : m_weaponWidgets) {
		widget->setVisible(weapon);
	}
	m_armorOptions->setVisible(armor);
	m_statGroup->setVisible(equipment);

	// weapons inflict attributes and conditions, armors protect from them
	m_attributeGroup->setVisible(equipment);
	m_attributeGroup->setTitle(weapon ? tr("Attack Attribute") : tr("Resistance"));
	m_conditionGroup->setVisible(equipment || medicine);
	m_conditionGroup->setTitle(medicine ? tr("Cure Condition") : tr("Conditions"));
	m_stateEffectRow->setVisible(equipment);
	m_stateEffect->button(0)->setText(weapon ? tr("Inflict") : tr("Resist"));
	m_stateEffect->button(1)->setText(weapon ? tr("Remove") : tr("Inflict"));
	m_stateChanceLabel->setText(weapon ? tr("Probability") : tr("Resistance"));

	for (auto* widget : m_medicineWidgets) {
		widget->setVisible(medicine);
	}
	m_seedGroup->setVisible(type == Item::Type_material);

	m_skillGroup->setVisible(equipment || book || special);
	m_skillGroup->setTitle(book ? tr("Skill Learned") : special ? tr("Invoke Skill") : tr("Skill"));
	m_useSkill->setVisible(equipment);
	const bool invokes = !equipment || m_current->use_skill;
	m_skill->setEnabled(invokes);
	m_messageRow->setVisible(special || equipment);
	m_messageRow->setEnabled(invokes);

	m_switchGroup->setVisible(switch_item);
	m_occasionGroup->setVisible(switch_item);

	m_userGroup->setVisible(type != Item::Type_normal && !switch_item);
}

void ItemWidget::bindUsers()
{
	auto& database = m_project.database();
	// the entries past the end of the arrays can use the item
	if (database.system.ldb_id == 2003 && database.system.equipment_setting == lcf::rpg::System::EquipmentSetting_class) {
		// the first entry is for the heroes without a class
		m_users->bind(m_current->class_set, QStringList(tr("(No Class)")) + databaseNames(database.classes), true);
	} else {
		m_users->bind(m_current->actor_set, databaseNames(database.actors), true);
	}
}
