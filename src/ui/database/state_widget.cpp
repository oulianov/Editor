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

#include "state_widget.h"
#include "ui_state_widget.h"

#include "battle_animation_widget.h"
#include "common/dbstring.h"
#include "common/lcf_widget_binding.h"
#include "common/signal_blocker.h"
#include "defines.h"
#include "model/project_data.h"
#include "model/state.h"

namespace {
// RPG Maker 2003 battler animations have 32 poses; a condition uses one of them
constexpr int kPoseCount = 32;
constexpr int kBattlerSize = 48;

/** The battler animation of the first hero, whose poses are shown */
const lcf::rpg::BattlerAnimation* firstHeroBattler(const lcf::rpg::Database& database) {
	if (database.actors.empty()) {
		return nullptr;
	}
	const int id = database.actors.front().battler_animation;
	if (id < 1 || id > static_cast<int>(database.battleranimations.size())) {
		return nullptr;
	}
	return &database.battleranimations[static_cast<size_t>(id) - 1];
}
}

StateWidget::StateWidget(ProjectData& project, QWidget *parent) :
	QWidget(parent),
	ui(new Ui::StateWidget),
	m_project(project)
{
	auto& database = project.database();

	const bool isRPG2k3 = database.system.ldb_id == 2003;
	ui->setupUi(this);

	m_current = nullptr;

	ui->comboExtents->addItem(tr("Ends after battle"), 0);
	ui->comboExtents->addItem(tr("Persists after battle"), 1);

	ui->comboColor->setup(project);

	ui->comboRestriction->addItem(tr("No restriction"), 0);
	ui->comboRestriction->addItem(tr("No action allowed"), 1);
	ui->comboRestriction->addItem(tr("Attack enemies randomly"), 2);
	ui->comboRestriction->addItem(tr("Attack allies randomly"), 3);

	m_buttonGroupStatAlteration = new QButtonGroup(this);
	m_buttonGroupStatAlteration->addButton(ui->radioStatAlterationNone);
	m_buttonGroupStatAlteration->setId(ui->radioStatAlterationNone, 2);
	m_buttonGroupStatAlteration->addButton(ui->radioStatAlterationHalf);
	m_buttonGroupStatAlteration->setId(ui->radioStatAlterationHalf, 0);
	m_buttonGroupStatAlteration->addButton(ui->radioStatAlterationDouble);
	m_buttonGroupStatAlteration->setId(ui->radioStatAlterationDouble, 1);

	m_buttonGroupHPEffect = new QButtonGroup(this);
	m_buttonGroupHPEffect->addButton(ui->radioHPEffectNone);
	m_buttonGroupHPEffect->setId(ui->radioHPEffectNone, 2);
	m_buttonGroupHPEffect->addButton(ui->radioHPEffectRecovery);
	m_buttonGroupHPEffect->setId(ui->radioHPEffectRecovery, 1);
	m_buttonGroupHPEffect->addButton(ui->radioHPEffectDamage);
	m_buttonGroupHPEffect->setId(ui->radioHPEffectDamage, 0);

	m_buttonGroupMPEffect = new QButtonGroup(this);
	m_buttonGroupMPEffect->addButton(ui->radioMPEffectNone);
	m_buttonGroupMPEffect->setId(ui->radioMPEffectNone, 2);
	m_buttonGroupMPEffect->addButton(ui->radioMPEffectRecovery);
	m_buttonGroupMPEffect->setId(ui->radioMPEffectRecovery, 1);
	m_buttonGroupMPEffect->addButton(ui->radioMPEffectDamage);
	m_buttonGroupMPEffect->setId(ui->radioMPEffectDamage, 0);

	LcfWidgetBinding::connect(this, ui->lineName);

	for (auto& uis : {
			ui->comboExtents,
			ui->comboRestriction }) {
		LcfWidgetBinding::connect<int32_t>(this, uis);
	}

	LcfWidgetBinding::connect<int32_t>(this, ui->comboColor);

	for (auto& uis : {
			ui->spinPriority,
			ui->spinAccuracy }) {
		LcfWidgetBinding::connect<int32_t>(this, uis);
	}

	for (auto& uis : {
			ui->checkEvadeAll,
			ui->checkReflectSkills,
			ui->checkNoEquipmentChange }) {
		LcfWidgetBinding::connect<bool>(this, uis);
		uis->setEnabled(isRPG2k3);
	}

	for (auto& uis : {
			ui->spinRateA,
			ui->spinRateB,
			ui->spinRateC,
			ui->spinRateD,
			ui->spinRateE }) {
		LcfWidgetBinding::connect<int32_t>(this, uis);
	}

	for (auto& uis : {
			ui->checkRestrictAttackInfluence,
			ui->checkRestrictIntellectInfluence }) {
		LcfWidgetBinding::connect<bool>(this, uis);
	}

	for (auto& uis : {
			ui->spinRestrictAttackInfluence,
			ui->spinRestrictIntellectInfluence }) {
		LcfWidgetBinding::connect<int32_t>(this, uis);
	}

	LcfWidgetBinding::connect<int32_t>(this, m_buttonGroupStatAlteration);
	for (auto& uis : {
			ui->checkStatAffectAttack,
			ui->checkStatAffectDefense,
			ui->checkStatAffectIntelligence,
			ui->checkStatAffectAgility }) {
		LcfWidgetBinding::connect<bool>(this, uis);
	}

	for (auto& uis : {
			ui->spinRecoverTurns,
			ui->spinRecoverChance,
			ui->spinRecoverDamageChance }) {
		LcfWidgetBinding::connect<int32_t>(this, uis);
	}

	LcfWidgetBinding::connect<int32_t>(this, m_buttonGroupHPEffect);
	for (auto& uis : {
			ui->spinHPEffectPercentage,
			ui->spinHPEffectValue,
			ui->spinHPEffectFieldValue,
			ui->spinHPEffectFieldSteps }) {
		LcfWidgetBinding::connect<int32_t>(this, uis);
	}

	LcfWidgetBinding::connect<int32_t>(this, m_buttonGroupMPEffect);
	for (auto& uis : {
			ui->spinMPEffectPercentage,
			ui->spinMPEffectValue,
			ui->spinMPEffectFieldValue,
			ui->spinMPEffectFieldSteps }) {
		LcfWidgetBinding::connect<int32_t>(this, uis);
	}

	for (auto& uis : {
			ui->lineMessageInflictionActor,
			ui->lineMessageInflictionEnemy,
			ui->lineMessageAlreadyAffected,
			ui->lineMessagePerTurn,
			ui->lineMessageRecovery }) {
		LcfWidgetBinding::connect(this, uis);
		uis->setEnabled(!isRPG2k3);
	}

	for (auto& uis : {
			ui->radioStatAlterationNone,
			ui->radioStatAlterationHalf,
			ui->radioStatAlterationDouble,
			ui->radioHPEffectNone,
			ui->radioHPEffectRecovery,
			ui->radioHPEffectDamage,
			ui->radioMPEffectNone,
			ui->radioMPEffectRecovery,
			ui->radioMPEffectDamage }) {
		uis->setEnabled(isRPG2k3);
	}

	LcfWidgetBinding::connect<int32_t>(this, ui->comboBattlerPose);
	connect(ui->comboBattlerPose, qOverload<int>(&QComboBox::currentIndexChanged), this, &StateWidget::updatePosePreview);
	ui->comboBattlerPose->setEnabled(isRPG2k3);
	ui->labelBattlerPose->setMinimumSize(kBattlerSize, kBattlerSize);
}

StateWidget::~StateWidget() {
	delete ui;
}

void StateWidget::setData(lcf::rpg::State* state) {
	on_currentStateChanged(state);
}

void StateWidget::on_currentStateChanged(lcf::rpg::State *state) {
	if (!state) {
		state = &dummy;
	}
	m_current = state;

	auto& database = m_project.database();
	const bool isRPG2k3 = database.system.ldb_id == 2003;

	LcfWidgetBinding::bind(ui->lineName, state->name);
	LcfWidgetBinding::bind(ui->comboExtents, state->type);
	LcfWidgetBinding::bind(ui->comboColor, state->color);
	LcfWidgetBinding::bind(ui->spinPriority, state->priority);
	LcfWidgetBinding::bind(ui->comboRestriction, state->restriction);
	LcfWidgetBinding::bind(ui->spinAccuracy, state->reduce_hit_ratio);
	LcfWidgetBinding::bind(ui->checkEvadeAll, state->avoid_attacks);
	LcfWidgetBinding::bind(ui->checkReflectSkills, state->reflect_magic);
	LcfWidgetBinding::bind(ui->checkNoEquipmentChange, state->cursed);
	LcfWidgetBinding::bind(ui->spinRateA, state->a_rate);
	LcfWidgetBinding::bind(ui->spinRateB, state->b_rate);
	LcfWidgetBinding::bind(ui->spinRateC, state->c_rate);
	LcfWidgetBinding::bind(ui->spinRateD, state->d_rate);
	LcfWidgetBinding::bind(ui->spinRateE, state->e_rate);
	LcfWidgetBinding::bind(ui->checkRestrictAttackInfluence, state->restrict_skill);
	LcfWidgetBinding::bind(ui->spinRestrictAttackInfluence, state->restrict_skill_level);
	LcfWidgetBinding::bind(ui->checkRestrictIntellectInfluence, state->restrict_magic);
	LcfWidgetBinding::bind(ui->spinRestrictIntellectInfluence, state->restrict_magic_level);
	LcfWidgetBinding::bind(m_buttonGroupStatAlteration, state->affect_type);
	LcfWidgetBinding::bind(ui->checkStatAffectAttack, state->affect_attack);
	LcfWidgetBinding::bind(ui->checkStatAffectDefense, state->affect_defense);
	LcfWidgetBinding::bind(ui->checkStatAffectIntelligence, state->affect_spirit);
	LcfWidgetBinding::bind(ui->checkStatAffectAgility, state->affect_agility);
	LcfWidgetBinding::bind(ui->spinRecoverTurns, state->hold_turn);
	LcfWidgetBinding::bind(ui->spinRecoverChance, state->auto_release_prob);
	LcfWidgetBinding::bind(ui->spinRecoverDamageChance, state->release_by_damage);
	LcfWidgetBinding::bind(m_buttonGroupHPEffect, state->hp_change_type);
	LcfWidgetBinding::bind(ui->spinHPEffectPercentage, state->hp_change_max);
	LcfWidgetBinding::bind(ui->spinHPEffectValue, state->hp_change_val);
	LcfWidgetBinding::bind(ui->spinHPEffectFieldValue, state->hp_change_map_val);
	LcfWidgetBinding::bind(ui->spinHPEffectFieldSteps, state->hp_change_map_steps);
	LcfWidgetBinding::bind(m_buttonGroupMPEffect, state->sp_change_type);
	LcfWidgetBinding::bind(ui->spinMPEffectPercentage, state->sp_change_max);
	LcfWidgetBinding::bind(ui->spinMPEffectValue, state->sp_change_val);
	LcfWidgetBinding::bind(ui->spinMPEffectFieldValue, state->sp_change_map_val);
	LcfWidgetBinding::bind(ui->spinMPEffectFieldSteps, state->sp_change_map_steps);
	LcfWidgetBinding::bind(ui->lineMessageInflictionActor, state->message_actor);
	LcfWidgetBinding::bind(ui->lineMessageInflictionEnemy, state->message_enemy);
	LcfWidgetBinding::bind(ui->lineMessageAlreadyAffected, state->message_already);
	LcfWidgetBinding::bind(ui->lineMessagePerTurn, state->message_affected);
	LcfWidgetBinding::bind(ui->lineMessageRecovery, state->message_recovery);
	fillPoses();
	LcfWidgetBinding::bind(ui->comboBattlerPose, state->battler_animation_id);
	updatePosePreview();

	this->setEnabled(state != &dummy);

	for (auto& uis : {
			ui->checkRestrictAttackInfluence,
			ui->checkRestrictIntellectInfluence,
			ui->checkStatAffectAttack,
			ui->checkStatAffectDefense,
			ui->checkStatAffectIntelligence,
			ui->checkStatAffectAgility }) {
		uis->setEnabled(state->ID != 1);
	}
	ui->comboColor->setEnabled(state->ID != 1);
	ui->comboRestriction->setEnabled(state->ID != 1);
	for (auto& uis : {
			ui->spinPriority,
			ui->spinAccuracy,
			ui->spinRestrictAttackInfluence,
			ui->spinRestrictIntellectInfluence,
			ui->spinRecoverTurns,
			ui->spinRecoverChance,
			ui->spinRecoverDamageChance,
			ui->spinHPEffectPercentage,
			ui->spinHPEffectValue,
			ui->spinHPEffectFieldValue,
			ui->spinHPEffectFieldSteps,
			ui->spinMPEffectPercentage,
			ui->spinMPEffectValue,
			ui->spinMPEffectFieldValue,
			ui->spinMPEffectFieldSteps }) {
		uis->setEnabled(state->ID != 1);
	}

	for (auto& uis : {
			ui->checkEvadeAll,
			ui->checkReflectSkills,
			ui->checkNoEquipmentChange }) {
		uis->setEnabled(state->ID != 1 && isRPG2k3);
	}
	for (auto& uis : {
			ui->lineMessageInflictionActor,
			ui->lineMessageInflictionEnemy,
			ui->lineMessageRecovery }) {
		uis->setEnabled(!isRPG2k3);
	}
	for (auto& uis : {
			ui->lineMessageAlreadyAffected,
			ui->lineMessagePerTurn }) {
		uis->setEnabled(state->ID != 1 && !isRPG2k3);
	}
	for (auto& uis : {
			ui->radioStatAlterationNone,
			ui->radioStatAlterationHalf,
			ui->radioStatAlterationDouble,
			ui->radioHPEffectNone,
			ui->radioHPEffectRecovery,
			ui->radioHPEffectDamage,
			ui->radioMPEffectNone,
			ui->radioMPEffectRecovery,
			ui->radioMPEffectDamage }) {
		uis->setEnabled(state->ID != 1 && isRPG2k3);
	}
	ui->comboBattlerPose->setEnabled(state->ID != 1 && isRPG2k3);
}

void StateWidget::fillPoses() {
	// The pose labels of the first hero's battle graphics, like the
	// Battle Animation 2 page; the 12 poses RPG_RT uses by default else
	const QStringList defaults = {tr("Idle Stance"), tr("Right Hand"), tr("Left Hand"), tr("Skill Use"),
		tr("Dead"), tr("Damage"), tr("Bad Status"), tr("Defending"), tr("Walking Left"),
		tr("Walking Right"), tr("Victory"), tr("Item")};
	const auto* battler = firstHeroBattler(m_project.database());
	SignalBlocker blocker(ui->comboBattlerPose);
	ui->comboBattlerPose->clear();
	for (int i = 0; i < kPoseCount; ++i) {
		QString name;
		if (battler && i < static_cast<int>(battler->poses.size())) {
			name = ToQString(battler->poses[static_cast<size_t>(i)].name);
		}
		if (name.isEmpty()) {
			name = i < defaults.size() ? defaults[i] : QString();
		}
		ui->comboBattlerPose->addItem(QStringLiteral("%1: %2").arg(i + 1, 2, 10, QChar('0')).arg(name), i);
	}
	// keep a value out of the range visible
	const int value = m_current ? m_current->battler_animation_id : 0;
	if (ui->comboBattlerPose->findData(value) < 0) {
		ui->comboBattlerPose->addItem(tr("<%1?>").arg(value + 1), value);
	}
}

void StateWidget::updatePosePreview() {
	ui->labelBattlerPose->clear();
	const auto* battler = firstHeroBattler(m_project.database());
	const int index = ui->comboBattlerPose->currentData().toInt();
	if (!battler || index < 0 || index >= static_cast<int>(battler->poses.size())) {
		return;
	}
	const auto& pose = battler->poses[static_cast<size_t>(index)];
	if (pose.animation_type == lcf::rpg::BattlerAnimationPose::AnimType_battle) {
		ui->labelBattlerPose->setText(tr("Battle animation"));
		return;
	}
	// the first frame of the pose's row, as Battle Animation 2 draws it
	const QPixmap sheet = BattleAnimationWidget::loadGraphic(m_project, BATTLECHARSET, ToQString(pose.battler_name));
	if (!sheet.isNull()) {
		ui->labelBattlerPose->setPixmap(sheet.copy(0, pose.battler_index * kBattlerSize, kBattlerSize, kBattlerSize));
	}
}
