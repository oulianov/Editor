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

#include "skill_item_common.h"

#include <QBoxLayout>
#include <QButtonGroup>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QSignalBlocker>
#include <lcf/rpg/item.h>

#include "common/lcf_widget_binding.h"
#include "model/project_data.h"
#include "ui/common/rpg_combobox.h"

void addChoices(QComboBox* combo, const QStringList& texts) {
	for (int i = 0; i < texts.size(); ++i) {
		combo->addItem(texts[i], i);
	}
}

QButtonGroup* addRadios(QWidget* parent, QBoxLayout* layout, const std::vector<std::pair<int, QString>>& buttons) {
	auto* group = new QButtonGroup(parent);
	for (const auto& [id, text] : buttons) {
		auto* radio = new QRadioButton(text, parent);
		group->addButton(radio, id);
		layout->addWidget(radio);
	}
	return group;
}

BitArrayChecklist::BitArrayChecklist(QWidget* parent) : QListWidget(parent) {
	setUniformItemSizes(true);

	connect(this, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) {
		if (!m_bits) {
			return;
		}
		const auto index = static_cast<size_t>(row(item));
		if (index >= m_bits->size()) {
			lcf::DBBitArray bits(static_cast<size_t>(count()), m_missing);
			const lcf::DBBitArray& old = *m_bits;
			for (size_t i = 0; i < old.size(); ++i) {
				bits[i] = static_cast<bool>(old[i]);
			}
			*m_bits = std::move(bits);
		}
		(*m_bits)[index] = item->checkState() == Qt::Checked;
	});
}

void BitArrayChecklist::bind(lcf::DBBitArray& bits, const QStringList& names, bool missing) {
	// filling the list must not write into the previous array
	m_bits = nullptr;
	m_missing = missing;
	clear();
	const lcf::DBBitArray& values = bits;
	for (int i = 0; i < names.size(); ++i) {
		auto* item = new QListWidgetItem(names[i], this);
		item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
		const bool on = static_cast<size_t>(i) < values.size() ? static_cast<bool>(values[i]) : missing;
		item->setCheckState(on ? Qt::Checked : Qt::Unchecked);
	}
	m_bits = &bits;
}

void BattlerAnimationDataEditor::exec(ProjectData& project, std::vector<lcf::rpg::BattlerAnimationItemSkill>& target,
		lcf::rpg::Item* weapon, QWidget* parent) {
	auto& database = project.database();

	// Edited on a copy holding an entry for every hero, applied on OK
	auto data = target;
	for (size_t i = data.size(); i < database.actors.size(); ++i) {
		data.emplace_back();
		data.back().ID = static_cast<int>(i) + 1;
	}
	int32_t trajectory = weapon ? weapon->ranged_trajectory : 0;
	int32_t ranged_target = weapon ? weapon->ranged_target : 0;

	QDialog dialog(parent);
	dialog.setWindowTitle(weapon ? tr("Weapon Animation") : tr("Skill Animation"));
	auto* layout = new QVBoxLayout(&dialog);
	auto* form = new QFormLayout();
	layout->addLayout(form);

	auto* hero = new QComboBox(&dialog);
	hero->addItems(databaseNames(database.actors));
	form->addRow(tr("Hero"), hero);

	auto* movement = new QComboBox(&dialog);
	addChoices(movement, {tr("None"), tr("Step Forward"), tr("Jump Forward")});
	if (weapon) {
		movement->addItem(tr("Move to Target"), lcf::rpg::BattlerAnimationItemSkill::Movement_move);
	}
	LcfWidgetBinding::connect<int32_t>(&dialog, movement);
	form->addRow(weapon ? tr("Movement Before Attack") : tr("Movement"), movement);

	auto* afterimage_row = new QHBoxLayout();
	auto* afterimage = addRadios(&dialog, afterimage_row, {
		{lcf::rpg::BattlerAnimationItemSkill::Afterimage_add, tr("Add")},
		{lcf::rpg::BattlerAnimationItemSkill::Afterimage_none, tr("None")}});
	afterimage_row->addStretch();
	LcfWidgetBinding::connect<int32_t>(&dialog, afterimage);
	form->addRow(tr("Afterimage"), afterimage_row);

	// Skills: the pose of the battler
	QComboBox* pose = nullptr;
	// Weapons: the weapon drawn or a battle animation, how many attacks, ranged attack
	QButtonGroup* anim_type = nullptr;
	QComboBox* weapon_anim = nullptr;
	AnimationRpgComboBox* battle_anim = nullptr;
	QComboBox* attacks = nullptr;
	QGroupBox* ranged = nullptr;
	QComboBox* ranged_anim = nullptr;
	QComboBox* speed = nullptr;

	if (!weapon) {
		pose = new QComboBox(&dialog);
		LcfWidgetBinding::connect<int32_t>(&dialog, pose);
		form->addRow(tr("Pose"), pose);
	} else {
		auto* type_box = new QGroupBox(tr("Animation Type"), &dialog);
		auto* type_form = new QFormLayout(type_box);
		auto* weapon_radio = new QRadioButton(tr("Weapon"), type_box);
		auto* battle_radio = new QRadioButton(tr("Battle Animation"), type_box);
		anim_type = new QButtonGroup(&dialog);
		anim_type->addButton(weapon_radio, lcf::rpg::BattlerAnimationItemSkill::AnimType_weapon);
		anim_type->addButton(battle_radio, lcf::rpg::BattlerAnimationItemSkill::AnimType_battle);
		weapon_anim = new QComboBox(type_box);
		battle_anim = new AnimationRpgComboBox(type_box);
		battle_anim->makeModel(project);
		type_form->addRow(weapon_radio, weapon_anim);
		type_form->addRow(battle_radio, battle_anim);
		LcfWidgetBinding::connect<int32_t>(&dialog, anim_type);
		LcfWidgetBinding::connect<int32_t>(&dialog, weapon_anim);
		LcfWidgetBinding::connect<int32_t>(&dialog, battle_anim);
		QObject::connect(anim_type, &QButtonGroup::idToggled, &dialog, [=](int id, bool checked) {
			if (checked) {
				weapon_anim->setEnabled(id == lcf::rpg::BattlerAnimationItemSkill::AnimType_weapon);
				battle_anim->setEnabled(id == lcf::rpg::BattlerAnimationItemSkill::AnimType_battle);
			}
		});
		layout->addWidget(type_box);

		auto* attack_form = new QFormLayout();
		attacks = new QComboBox(&dialog);
		addChoices(attacks, {tr("Once"), tr("Twice"), tr("Three Times")});
		LcfWidgetBinding::connect<int32_t>(&dialog, attacks);
		attack_form->addRow(tr("Number of Attacks"), attacks);
		layout->addLayout(attack_form);

		ranged = new QGroupBox(tr("Ranged Weapon"), &dialog);
		ranged->setCheckable(true);
		LcfWidgetBinding::connect<bool>(&dialog, ranged);
		auto* ranged_form = new QFormLayout(ranged);
		ranged_anim = new QComboBox(ranged);
		LcfWidgetBinding::connect<int32_t>(&dialog, ranged_anim);
		ranged_form->addRow(tr("Weapon Animation"), ranged_anim);
		// trajectory and targets belong to the weapon, not to each hero
		auto* trajectory_combo = new QComboBox(ranged);
		addChoices(trajectory_combo, {tr("Fly Straight to Target"), tr("Return to User After Striking")});
		LcfWidgetBinding::connect<int32_t>(&dialog, trajectory_combo);
		LcfWidgetBinding::bind(trajectory_combo, trajectory);
		ranged_form->addRow(tr("Trajectory"), trajectory_combo);
		auto* target_combo = new QComboBox(ranged);
		addChoices(target_combo, {tr("Single Target"), tr("Fly Down the Center of the Screen"),
			tr("Strike Each Enemy Simultaneously"), tr("Strike Each Enemy in Turn")});
		LcfWidgetBinding::connect<int32_t>(&dialog, target_combo);
		LcfWidgetBinding::bind(target_combo, ranged_target);
		ranged_form->addRow(tr("Targeting"), target_combo);
		speed = new QComboBox(ranged);
		addChoices(speed, {tr("Fast"), tr("Medium"), tr("Slow")});
		LcfWidgetBinding::connect<int32_t>(&dialog, speed);
		ranged_form->addRow(tr("Animation Speed"), speed);
		layout->addWidget(ranged);
	}

	auto show_hero = [&](int index) {
		if (index < 0 || index >= static_cast<int>(data.size())) {
			return;
		}
		auto& entry = data[static_cast<size_t>(index)];
		// the poses and weapons are those of the hero's battle graphics
		const auto& actor = database.actors[static_cast<size_t>(index)];
		const lcf::rpg::BattlerAnimation* battler = nullptr;
		if (actor.battler_animation > 0 && actor.battler_animation <= static_cast<int>(database.battleranimations.size())) {
			battler = &database.battleranimations[static_cast<size_t>(actor.battler_animation) - 1];
		}

		LcfWidgetBinding::bind(movement, entry.movement);
		LcfWidgetBinding::bind(afterimage, entry.after_image);

		if (pose) {
			QStringList names = {tr("Idle Stance"), tr("Right Hand"), tr("Left Hand"), tr("Skill Use"),
				tr("Dead"), tr("Damage"), tr("Bad Status"), tr("Defending"), tr("Walking Left"),
				tr("Walking Right"), tr("Victory"), tr("Item")};
			if (battler && !battler->poses.empty()) {
				names = databaseNames(battler->poses);
			}
			{
				QSignalBlocker blocker(pose);
				pose->clear();
				addChoices(pose, names);
			}
			LcfWidgetBinding::bind(pose, entry.pose);
			return;
		}

		const QStringList weapons = battler ? databaseNames(battler->weapons) : QStringList();
		for (auto* combo : {weapon_anim, ranged_anim}) {
			QSignalBlocker blocker(combo);
			combo->clear();
			addChoices(combo, weapons);
		}
		LcfWidgetBinding::bind(anim_type, entry.type);
		weapon_anim->setEnabled(entry.type == lcf::rpg::BattlerAnimationItemSkill::AnimType_weapon);
		battle_anim->setEnabled(entry.type == lcf::rpg::BattlerAnimationItemSkill::AnimType_battle);
		LcfWidgetBinding::bind(weapon_anim, entry.weapon_animation_id);
		LcfWidgetBinding::bind(battle_anim, entry.battle_animation_id);
		LcfWidgetBinding::bind(attacks, entry.attacks);
		LcfWidgetBinding::bind(ranged, entry.ranged);
		LcfWidgetBinding::bind(ranged_anim, entry.ranged_animation_id);
		LcfWidgetBinding::bind(speed, entry.ranged_speed);
	};
	QObject::connect(hero, qOverload<int>(&QComboBox::currentIndexChanged), &dialog, show_hero);
	show_hero(hero->currentIndex());
	if (data.empty()) {
		// no hero to set
		for (auto* child : dialog.findChildren<QWidget*>()) {
			child->setEnabled(false);
		}
	}

	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
	QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	layout->addWidget(buttons);

	if (dialog.exec() != QDialog::Accepted) {
		return;
	}
	target = std::move(data);
	if (weapon) {
		weapon->ranged_trajectory = trajectory;
		weapon->ranged_target = ranged_target;
	}
}
