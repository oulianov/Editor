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

#include "battler_animation_editor.h"
#include <QTimer>

BattlerAnimationEditor::BattlerAnimationEditor(QObject* parent) :
	QObject(parent)
{
	m_timer = new QTimer(this);
	connect(m_timer, &QTimer::timeout, this, &BattlerAnimationEditor::nextStep);
}

lcf::rpg::BattlerAnimation* BattlerAnimationEditor::animation() const {
	return m_anim;
}

void BattlerAnimationEditor::setAnimation(lcf::rpg::BattlerAnimation* anim, bool rpg2k3) {
	m_anim = anim;
	if (anim && rpg2k3) {
		fillSlots(*anim);
	}
	if (anim) {
		m_timer->setInterval(interval(anim->speed));
	}
	m_pose = anim && !anim->poses.empty() ? 0 : -1;
	m_weapon = anim && !anim->weapons.empty() ? 0 : -1;
	m_step = 0;
	if (anim && rpg2k3) {
		m_timer->start();
	} else {
		m_timer->stop();
	}
	emit currentPoseChanged();
	emit currentWeaponChanged();
}

void BattlerAnimationEditor::fillSlots(lcf::rpg::BattlerAnimation& anim) {
	auto fill = [](auto& items) {
		const size_t count = kSlotCount;
		if (items.size() >= count) return;
		items.resize(count);
		for (size_t i = 0; i < items.size(); ++i) items[i].ID = static_cast<int>(i) + 1;
	};
	fill(anim.poses);
	fill(anim.weapons);
}

void BattlerAnimationEditor::setSpeed(int speed) {
	if (!m_anim) return;
	m_anim->speed = speed;
	m_timer->setInterval(interval(speed));
}

int BattlerAnimationEditor::interval(int speed) {
	return speed * 1000 / 60;
}

int BattlerAnimationEditor::currentPose() const {
	return m_pose;
}

void BattlerAnimationEditor::setCurrentPose(int index) {
	const int count = m_anim ? static_cast<int>(m_anim->poses.size()) : 0;
	if (index < -1 || index >= count || index == m_pose) return;
	m_pose = index;
	emit currentPoseChanged();
}

lcf::rpg::BattlerAnimationPose* BattlerAnimationEditor::pose() const {
	if (!m_anim || m_pose < 0 || m_pose >= static_cast<int>(m_anim->poses.size())) {
		return nullptr;
	}
	return &m_anim->poses[m_pose];
}

int BattlerAnimationEditor::currentWeapon() const {
	return m_weapon;
}

void BattlerAnimationEditor::setCurrentWeapon(int index) {
	const int count = m_anim ? static_cast<int>(m_anim->weapons.size()) : 0;
	if (index < -1 || index >= count || index == m_weapon) return;
	m_weapon = index;
	emit currentWeaponChanged();
}

lcf::rpg::BattlerAnimationWeapon* BattlerAnimationEditor::weapon() const {
	if (!m_anim || m_weapon < 0 || m_weapon >= static_cast<int>(m_anim->weapons.size())) {
		return nullptr;
	}
	return &m_anim->weapons[m_weapon];
}

bool BattlerAnimationEditor::isPoseNameFixed(int index) {
	return index < kFixedPoses;
}

bool BattlerAnimationEditor::usesAnimation(const lcf::rpg::BattlerAnimationPose& pose) {
	return pose.animation_type == lcf::rpg::BattlerAnimationPose::AnimType_battle;
}

const lcf::rpg::BattlerAnimationPose* BattlerAnimationEditor::handPose(int hand) const {
	const int poses[] = {lcf::rpg::BattlerAnimation::Pose_AttackRight, lcf::rpg::BattlerAnimation::Pose_AttackLeft};
	if (!m_anim || hand < RightHand || hand > LeftHand || poses[hand] >= static_cast<int>(m_anim->poses.size())) {
		return nullptr;
	}
	const auto& attack = m_anim->poses[poses[hand]];
	return attack.animation_type == lcf::rpg::BattlerAnimationPose::AnimType_character ? &attack : nullptr;
}

int BattlerAnimationEditor::step() const {
	return m_step;
}

void BattlerAnimationEditor::restart() {
	m_step = 0;
}

void BattlerAnimationEditor::nextStep() {
	++m_step;
	emit stepped();
}

int BattlerAnimationEditor::frameColumn() const {
	// the three frames of a row, as the Player plays them
	static const int pattern[] = {0, 1, 2, 1};
	return pattern[m_step % 4];
}

int BattlerAnimationEditor::shownHand(int choice) const {
	return choice == BothHands ? (m_step / 4) % 2 : choice;
}

bool BattlerAnimationEditor::isWeaponBehind(int hand) {
	return hand == LeftHand;
}

int BattlerAnimationEditor::rowAt(const QPoint& pos, int size) {
	if (size <= 0 || pos.x() < 0 || pos.y() < 0 || pos.x() >= kRowColumns * size) {
		return -1;
	}
	const int row = (pos.y() / size) * kRowColumns + pos.x() / size;
	return row < kSheetRows ? row : -1;
}
