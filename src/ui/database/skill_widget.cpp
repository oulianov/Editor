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

#include "skill_widget.h"

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
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>

#include "common/lcf_widget_binding.h"
#include "model/project_data.h"
#include "ui/common/rpg_audio_lineedit.h"
#include "ui/common/rpg_combobox.h"
#include "skill_item_common.h"
#include "ui/common/text_limit.h"

using Skill = lcf::rpg::Skill;

SkillWidget::SkillWidget(ProjectData& project, QWidget *parent) :
	QWidget(parent),
	m_project(project),
	m_current(&m_dummy)
{
	auto& database = project.database();
	const bool is2k3 = database.system.ldb_id == 2003;
	// 2003 games can use the battle system of RPG Maker 2000, and its messages
	const bool has2kMessages = !is2k3 || database.system.easyrpg_use_rpg2k_battle_system;

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

	// Name, description, type, cost and target
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
	TextLimit::mark(m_name, 20);
	addField(tr("Name"), m_name, 0, 0);
	m_type = new QComboBox(page);
	m_type->setObjectName(QStringLiteral("skillType"));
	addChoices(m_type, {tr("Normal"), tr("Teleport"), tr("Escape"), tr("Switch")});
	if (is2k3) {
		// RPG Maker 2003: a type per battle command listing a subset of the skills
		int type = Skill::Type_subskill;
		for (const auto& command : database.battlecommands.commands) {
			if (command.type == lcf::rpg::BattleCommand::Type_subskill) {
				m_type->addItem(ToQString(command.name), type++);
			}
		}
	}
	addField(tr("Type"), m_type, 0, 2);
	m_description = new QLineEdit(page);
	TextLimit::mark(m_description, 50);
	addField(tr("Description"), m_description, 1, 0, 3);

	auto* mp = new QWidget(page);
	auto* mp_row = new QHBoxLayout(mp);
	mp_row->setContentsMargins(0, 0, 0, 0);
	m_mpCost = new QSpinBox(mp);
	mp_row->addWidget(m_mpCost);
	m_mpType = addRadios(mp, mp_row, {{Skill::SpType_cost, tr("Number")}, {Skill::SpType_percent, tr("% of Max MP")}});
	m_mpType->button(Skill::SpType_percent)->setEnabled(is2k3);
	mp_row->addStretch();
	addField(tr("MP Consumed"), mp, 2, 0);

	m_scope = new QComboBox(page);
	m_scope->setObjectName(QStringLiteral("skillScope"));
	addChoices(m_scope, {tr("Single Enemy"), tr("All Enemies"), tr("Self"), tr("Single Ally"), tr("All Allies")});
	m_normalWidgets.push_back(addField(tr("Target"), m_scope, 2, 2));
	m_normalWidgets.push_back(m_scope);
	header->setColumnStretch(1, 1);
	header->setColumnStretch(3, 1);

	auto* columns = new QHBoxLayout();
	layout->addLayout(columns, 1);
	auto* left = new QVBoxLayout();
	auto* middle = new QVBoxLayout();
	auto* right = new QVBoxLayout();
	columns->addLayout(left, 1);
	columns->addLayout(middle, 1);
	columns->addLayout(right, 1);

	// Normal skills: animation, power and effects
	auto* animation = new QGroupBox(tr("Battle Animation"), page);
	auto* animation_row = new QHBoxLayout(animation);
	m_animation = new AnimationRpgComboBox(animation);
	m_animation->makeModel(project);
	animation_row->addWidget(m_animation, 1);
	m_battlerAnimation = new QPushButton(tr("Set..."), animation);
	m_battlerAnimation->setToolTip(tr("How each hero moves when using the skill (RPG Maker 2003 battles)"));
	m_battlerAnimation->setEnabled(is2k3);
	animation_row->addWidget(m_battlerAnimation);
	left->addWidget(animation);
	m_normalWidgets.push_back(animation);

	auto* power = new QGroupBox(tr("Power"), page);
	auto* power_form = new QFormLayout(power);
	auto addRate = [&](const QString& text, int32_t Skill::* field) {
		auto* row = new QHBoxLayout();
		auto* slider = new QSlider(Qt::Horizontal, power);
		slider->setRange(0, 10);
		slider->setPageStep(1);
		slider->setTickPosition(QSlider::TicksBelow);
		auto* value = new QLabel(QStringLiteral("10"), power);
		value->setMinimumWidth(value->sizeHint().width());
		value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
		row->addWidget(slider, 1);
		row->addWidget(value);
		power_form->addRow(text, row);
		connect(slider, &QSlider::valueChanged, this, [this, value, field](int v) {
			value->setNum(v);
			m_current->*field = v;
		});
		m_rates.push_back({slider, value, field});
	};
	addRate(tr("Attack Influence"), &Skill::physical_rate);
	addRate(tr("Intelligence Influence"), &Skill::magical_rate);
	addRate(tr("Variance"), &Skill::variance);
	m_power = new QSpinBox(power);
	m_power->setRange(0, 9999);
	power_form->addRow(tr("Effect Rating"), m_power);
	m_hit = new QSpinBox(power);
	m_hit->setRange(0, 100);
	m_hit->setSuffix(QStringLiteral("%"));
	power_form->addRow(tr("Success Rate"), m_hit);
	left->addWidget(power);
	m_normalWidgets.push_back(power);

	m_effects = new QGroupBox(page);
	auto* effects_layout = new QVBoxLayout(m_effects);
	for (auto& [check, text] : std::vector<std::pair<QCheckBox**, QString>>{
			{&m_affectHp, tr("HP")},
			{&m_affectSp, tr("MP")},
			{&m_affectAttack, tr("Attack")},
			{&m_affectDefense, tr("Defense")},
			{&m_affectSpirit, tr("Intelligence")},
			{&m_affectAgility, tr("Agility")},
			{&m_absorb, tr("Absorb")},
			{&m_ignoreDefense, tr("Ignore Defense")}}) {
		*check = new QCheckBox(text, m_effects);
		effects_layout->addWidget(*check);
		LcfWidgetBinding::connect<bool>(this, *check);
	}
	middle->addWidget(m_effects);
	m_normalWidgets.push_back(m_effects);

	auto* conditions = new QGroupBox(tr("Condition"), page);
	auto* conditions_layout = new QVBoxLayout(conditions);
	auto* condition_effect = new QHBoxLayout();
	// the texts depend on the target, see updateScope
	m_stateEffect = addRadios(conditions, condition_effect, {{0, QString()}, {1, QString()}});
	condition_effect->addStretch();
	for (auto* button : m_stateEffect->buttons()) {
		button->setEnabled(is2k3);
	}
	conditions_layout->addLayout(condition_effect);
	m_states = new BitArrayChecklist(conditions);
	conditions_layout->addWidget(m_states);
	right->addWidget(conditions, 1);
	m_normalWidgets.push_back(conditions);

	auto* attributes = new QGroupBox(tr("Attack Attribute"), page);
	auto* attributes_layout = new QVBoxLayout(attributes);
	m_attributes = new BitArrayChecklist(attributes);
	attributes_layout->addWidget(m_attributes);
	m_attributeDefense = new QCheckBox(attributes);
	attributes_layout->addWidget(m_attributeDefense);
	right->addWidget(attributes, 1);
	m_normalWidgets.push_back(attributes);

	// Teleport, escape and switch skills
	m_soundGroup = new QGroupBox(tr("Sound Effect"), page);
	auto* sound_layout = new QVBoxLayout(m_soundGroup);
	m_sound = new RpgSoundLineEdit(m_soundGroup);
	m_sound->makeModel(project);
	sound_layout->addWidget(m_sound);
	left->addWidget(m_soundGroup);

	m_switchGroup = new QGroupBox(tr("Turn ON Switch"), page);
	auto* switch_layout = new QVBoxLayout(m_switchGroup);
	m_switch = new SwitchRpgComboBox(m_switchGroup);
	m_switch->makeModel(project);
	switch_layout->addWidget(m_switch);
	left->addWidget(m_switchGroup);

	m_occasionGroup = new QGroupBox(tr("Activation Allowed"), page);
	auto* occasion_layout = new QHBoxLayout(m_occasionGroup);
	m_occasionField = new QCheckBox(tr("Field"), m_occasionGroup);
	m_occasionBattle = new QCheckBox(tr("Battle"), m_occasionGroup);
	occasion_layout->addWidget(m_occasionField);
	occasion_layout->addWidget(m_occasionBattle);
	occasion_layout->addStretch();
	left->addWidget(m_occasionGroup);

	// RPG Maker 2000 battle messages
	auto* messages = new QGroupBox(tr("Using Message"), page);
	auto* messages_form = new QFormLayout(messages);
	m_message1 = new QLineEdit(messages);
	m_message2 = new QLineEdit(messages);
	for (auto* line : {m_message1, m_message2}) {
		line->setToolTip(tr("Shown after the name of the user, in the RPG Maker 2000 battle system"));
	}
	messages_form->addRow(tr("Line 1"), m_message1);
	messages_form->addRow(tr("Line 2"), m_message2);
	m_failure = new QComboBox(messages);
	// the messages of the vocabulary
	const auto& terms = database.terms;
	int failure = 0;
	for (const auto* text : {&terms.skill_failure_a, &terms.skill_failure_b, &terms.skill_failure_c, &terms.dodge}) {
		m_failure->addItem(text->empty() ? tr("Message %1").arg(failure + 1) : ToQString(*text), failure);
		++failure;
	}
	messages_form->addRow(tr("Failure"), m_failure);
	messages->setEnabled(has2kMessages);
	middle->addWidget(messages);

	left->addStretch();
	middle->addStretch();

	// Bindings
	for (auto* line : {m_name, m_description, m_message1, m_message2}) {
		LcfWidgetBinding::connect(this, line);
	}
	for (auto* combo : {m_type, m_scope, m_failure}) {
		LcfWidgetBinding::connect<int32_t>(this, combo);
	}
	for (auto* spin : {m_mpCost, m_power, m_hit}) {
		LcfWidgetBinding::connect<int32_t>(this, spin);
	}
	LcfWidgetBinding::connect<int32_t>(this, m_mpType);
	LcfWidgetBinding::connect<bool>(this, m_stateEffect);
	for (auto* check : {m_attributeDefense, m_occasionField, m_occasionBattle}) {
		LcfWidgetBinding::connect<bool>(this, check);
	}
	LcfWidgetBinding::connect<int32_t>(this, m_animation);
	LcfWidgetBinding::connect<int32_t>(this, m_switch);

	// after the bindings: the skill is up to date when they run
	connect(m_type, qOverload<int>(&QComboBox::currentIndexChanged), this, &SkillWidget::updateType);
	connect(m_scope, qOverload<int>(&QComboBox::currentIndexChanged), this, &SkillWidget::updateScope);
	connect(m_mpType, &QButtonGroup::idToggled, this, [this](int id, bool checked) {
		if (checked) {
			bindMpCost(id == Skill::SpType_percent);
		}
	});
	connect(m_battlerAnimation, &QPushButton::clicked, this, [this] {
		BattlerAnimationDataEditor::exec(m_project, m_current->battler_animation_data, nullptr, this);
	});

	setData(nullptr);
}

void SkillWidget::setData(lcf::rpg::Skill* skill)
{
	m_current = skill ? skill : &m_dummy;
	auto& database = m_project.database();

	LcfWidgetBinding::bind(m_name, m_current->name);
	LcfWidgetBinding::bind(m_description, m_current->description);
	LcfWidgetBinding::bind(m_type, m_current->type);
	LcfWidgetBinding::bind(m_mpType, m_current->sp_type);
	bindMpCost(m_current->sp_type == Skill::SpType_percent);
	LcfWidgetBinding::bind(m_scope, m_current->scope);
	LcfWidgetBinding::bind(m_animation, m_current->animation_id);
	for (const auto& rate : m_rates) {
		QSignalBlocker blocker(rate.slider);
		rate.slider->setValue(m_current->*rate.field);
		rate.value->setNum(m_current->*rate.field);
	}
	LcfWidgetBinding::bind(m_power, m_current->power);
	LcfWidgetBinding::bind(m_hit, m_current->hit);
	LcfWidgetBinding::bind(m_affectHp, m_current->affect_hp);
	LcfWidgetBinding::bind(m_affectSp, m_current->affect_sp);
	LcfWidgetBinding::bind(m_affectAttack, m_current->affect_attack);
	LcfWidgetBinding::bind(m_affectDefense, m_current->affect_defense);
	LcfWidgetBinding::bind(m_affectSpirit, m_current->affect_spirit);
	LcfWidgetBinding::bind(m_affectAgility, m_current->affect_agility);
	LcfWidgetBinding::bind(m_absorb, m_current->absorb_damage);
	LcfWidgetBinding::bind(m_ignoreDefense, m_current->ignore_defense);
	LcfWidgetBinding::bind(m_stateEffect, m_current->reverse_state_effect);
	m_states->bind(m_current->state_effects, databaseNames(database.states));
	m_attributes->bind(m_current->attribute_effects, databaseNames(database.attributes));
	LcfWidgetBinding::bind(m_attributeDefense, m_current->affect_attr_defence);
	m_sound->bindSound(m_current->sound_effect);
	LcfWidgetBinding::bind(m_switch, m_current->switch_id);
	LcfWidgetBinding::bind(m_occasionField, m_current->occasion_field);
	LcfWidgetBinding::bind(m_occasionBattle, m_current->occasion_battle);
	LcfWidgetBinding::bind(m_message1, m_current->using_message1);
	LcfWidgetBinding::bind(m_message2, m_current->using_message2);
	LcfWidgetBinding::bind(m_failure, m_current->failure_message);

	updateType();
	updateScope();
	setEnabled(skill != nullptr);
}

void SkillWidget::updateType()
{
	const int type = m_current->type;
	// the subset types of RPG Maker 2003 are normal skills
	const bool normal = type == Skill::Type_normal || type >= Skill::Type_subskill;
	for (auto* widget : m_normalWidgets) {
		widget->setVisible(normal);
	}
	m_soundGroup->setVisible(!normal);
	m_switchGroup->setVisible(type == Skill::Type_switch);
	m_occasionGroup->setVisible(type == Skill::Type_switch);
}

void SkillWidget::updateScope()
{
	// skills on allies heal and raise, skills on enemies hurt and lower
	const bool allies = m_current->scope >= Skill::Scope_self;
	m_effects->setTitle(allies ? tr("Increase") : tr("Decrease"));
	m_stateEffect->button(0)->setText(allies ? tr("Remove") : tr("Inflict"));
	m_stateEffect->button(1)->setText(allies ? tr("Inflict") : tr("Remove"));
	m_attributeDefense->setText(allies ? tr("Increase Resistance") : tr("Reduce Resistance"));
}

void SkillWidget::bindMpCost(bool percent)
{
	// no write while the range changes
	QSignalBlocker blocker(m_mpCost);
	m_mpCost->setRange(0, percent ? 100 : 999);
	m_mpCost->setSuffix(percent ? QStringLiteral("%") : QString());
	LcfWidgetBinding::bind(m_mpCost, percent ? m_current->sp_percent : m_current->sp_cost);
}
