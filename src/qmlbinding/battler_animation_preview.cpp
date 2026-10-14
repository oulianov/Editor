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

#include "battler_animation_preview.h"
#include "common/dbstring.h"
#include "defines.h"
#include "model/project_data.h"
#include "qmlbinding/database_entry.h"
#include "render/game_graphic.h"
#include <QPainter>

namespace {
constexpr int battler_size = BattlerAnimationEditor::kBattlerSize;
constexpr int weapon_size = BattlerAnimationEditor::kWeaponSize;
}

BattlerAnimationPreview::BattlerAnimationPreview(QObject* parent) : QObject(parent)
{
	connect(&m_editor, &BattlerAnimationEditor::stepped, this, &BattlerAnimationPreview::updatePreview);
	connect(&m_editor, &BattlerAnimationEditor::currentPoseChanged, this, &BattlerAnimationPreview::currentPoseChanged);
	connect(&m_editor, &BattlerAnimationEditor::currentWeaponChanged, this, &BattlerAnimationPreview::currentWeaponChanged);
}

JsonView* BattlerAnimationPreview::entry() const
{
	return m_entry;
}

void BattlerAnimationPreview::setEntry(JsonView* entry)
{
	if (m_entry == entry) {
		return;
	}
	if (m_entry) {
		disconnect(m_entry, nullptr, this, nullptr);
	}
	m_entry = entry;
	if (entry) {
		connect(entry, &JsonView::valueChanged, this, &BattlerAnimationPreview::onDataChanged);
	}
	m_graphics.clear();
	findAnimation();
	emit entryChanged();
}

bool BattlerAnimationPreview::rpg2k3() const
{
	return m_rpg2k3;
}

void BattlerAnimationPreview::setRpg2k3(bool rpg2k3)
{
	if (m_rpg2k3 != rpg2k3) {
		m_rpg2k3 = rpg2k3;
		findAnimation();
		emit entryChanged();
	}
}

ProjectData* BattlerAnimationPreview::project() const
{
	return DatabaseEntry::of(m_entry).project;
}

void BattlerAnimationPreview::findAnimation()
{
	const DatabaseEntry found = DatabaseEntry::of(m_entry);
	lcf::rpg::BattlerAnimation* anim = nullptr;
	if (found.valid() && found.list == QLatin1String("battleranimations")
			&& found.index < static_cast<int>(found.project->database().battleranimations.size())) {
		anim = &found.project->database().battleranimations[static_cast<size_t>(found.index)];
	}
	// the same entry again (its list moved): the same pose and weapon
	const bool same = anim && anim == m_editor.animation();
	const int pose = m_editor.currentPose();
	const int weapon = m_editor.currentWeapon();
	m_editor.setAnimation(anim, m_rpg2k3);
	if (same) {
		m_editor.setCurrentPose(pose);
		m_editor.setCurrentWeapon(weapon);
	}
	loadPreview();
}

void BattlerAnimationPreview::onDataChanged(const QString& jsonPtr)
{
	if (jsonPtr.isEmpty()) {
		findAnimation();
	} else if (jsonPtr == QLatin1String("/speed")) {
		if (auto* anim = m_editor.animation()) {
			m_editor.setSpeed(anim->speed);
		}
	} else {
		loadPreview();
	}
}

int BattlerAnimationPreview::currentPose() const
{
	return m_editor.currentPose();
}

void BattlerAnimationPreview::setCurrentPose(int index)
{
	if (index != m_editor.currentPose()) {
		m_editor.setCurrentPose(index);
		loadPreview();
	}
}

int BattlerAnimationPreview::currentWeapon() const
{
	return m_editor.currentWeapon();
}

void BattlerAnimationPreview::setCurrentWeapon(int index)
{
	if (index != m_editor.currentWeapon()) {
		m_editor.setCurrentWeapon(index);
		loadPreview();
	}
}

bool BattlerAnimationPreview::weapons() const
{
	return m_weapons;
}

void BattlerAnimationPreview::setWeapons(bool weapons)
{
	if (m_weapons != weapons) {
		m_weapons = weapons;
		emit weaponsChanged();
		loadPreview();
	}
}

int BattlerAnimationPreview::hand() const
{
	return m_hand;
}

void BattlerAnimationPreview::setHand(int hand)
{
	if (m_hand != hand) {
		m_hand = hand;
		emit handChanged();
		loadPreview();
	}
}

QImage BattlerAnimationPreview::preview() const
{
	return m_preview;
}

QImage BattlerAnimationPreview::rows() const
{
	return m_rows;
}

int BattlerAnimationPreview::frameSize() const
{
	return m_weapons ? weapon_size : battler_size;
}

QColor BattlerAnimationPreview::highlight() const
{
	return m_highlight;
}

void BattlerAnimationPreview::setHighlight(const QColor& color)
{
	if (m_highlight != color) {
		m_highlight = color;
		loadPreview();
	}
}

int BattlerAnimationPreview::rowAt(qreal x, qreal y) const
{
	return BattlerAnimationEditor::rowAt(QPoint(qFloor(x), qFloor(y)), frameSize());
}

bool BattlerAnimationPreview::isPoseNameFixed(int index) const
{
	return BattlerAnimationEditor::isPoseNameFixed(index);
}

int BattlerAnimationPreview::step() const
{
	return m_editor.step();
}

QImage BattlerAnimationPreview::graphic(const QString& dir, const QString& name)
{
	const QString key = dir + QLatin1Char('/') + name;
	auto it = m_graphics.constFind(key);
	if (it != m_graphics.constEnd()) {
		return *it;
	}
	const ProjectData* data = project();
	const QImage image = data ? GameGraphic::load(*data, dir, name) : QImage();
	m_graphics.insert(key, image);
	return image;
}

void BattlerAnimationPreview::loadPreview()
{
	m_sheet = QImage();
	m_handSheets = {};
	m_editor.restart();
	const int size = frameSize();
	if (m_weapons) {
		if (auto* weapon = m_editor.weapon()) {
			m_sheet = graphic(BATTLEWEAPON, ToQString(weapon->weapon_name));
			m_sheetRow = weapon->weapon_index;
		}
		// the attack poses of the right and left hands
		for (int hand : {BattlerAnimationEditor::RightHand, BattlerAnimationEditor::LeftHand}) {
			if (const auto* pose = m_editor.handPose(hand)) {
				m_handSheets[static_cast<size_t>(hand)] = graphic(BATTLECHARSET, ToQString(pose->battler_name));
				m_handRows[static_cast<size_t>(hand)] = pose->battler_index;
			}
		}
	} else if (auto* pose = m_editor.pose()) {
		if (!BattlerAnimationEditor::usesAnimation(*pose)) {
			m_sheet = graphic(BATTLECHARSET, ToQString(pose->battler_name));
			m_sheetRow = pose->battler_index;
		}
	}
	m_sheetSize = size;

	// the middle frame of each row, the one used framed
	m_rows = QImage(BattlerAnimationEditor::kRowColumns * size, 2 * size, QImage::Format_ARGB32_Premultiplied);
	m_rows.fill(Qt::transparent);
	if (!m_sheet.isNull()) {
		QPainter painter(&m_rows);
		for (int row = 0; row < BattlerAnimationEditor::kSheetRows; ++row) {
			painter.drawImage(QPoint((row % 4) * size, (row / 4) * size), m_sheet, QRect(size, row * size, size, size));
		}
		painter.setPen(QPen(m_highlight, 2));
		painter.drawRect(QRect((m_sheetRow % 4) * size, (m_sheetRow / 4) * size, size, size).adjusted(1, 1, -1, -1));
	}
	emit rowsChanged();
	updatePreview();
}

void BattlerAnimationPreview::updatePreview()
{
	const int column = m_editor.frameColumn();
	m_preview = QImage();
	if (m_weapons) {
		const int hand = m_editor.shownHand(m_hand);
		const bool with_hero = hand <= BattlerAnimationEditor::LeftHand && !m_handSheets[static_cast<size_t>(hand)].isNull();
		if (!m_sheet.isNull() || with_hero) {
			m_preview = QImage(weapon_size, weapon_size, QImage::Format_ARGB32_Premultiplied);
			m_preview.fill(Qt::transparent);
			QPainter painter(&m_preview);
			auto draw_hero = [&] {
				if (!with_hero) {
					return;
				}
				// both centered at the same place, as in the Player
				const int offset = (weapon_size - battler_size) / 2;
				painter.drawImage(QPoint(offset, offset), m_handSheets[static_cast<size_t>(hand)],
					QRect(column * battler_size, m_handRows[static_cast<size_t>(hand)] * battler_size, battler_size, battler_size));
			};
			auto draw_weapon = [&] {
				if (!m_sheet.isNull()) {
					painter.drawImage(QPoint(0, 0), m_sheet, QRect(column * weapon_size, m_sheetRow * weapon_size, weapon_size, weapon_size));
				}
			};
			if (BattlerAnimationEditor::isWeaponBehind(hand)) {
				draw_weapon();
				draw_hero();
			} else {
				draw_hero();
				draw_weapon();
			}
		}
	} else if (!m_sheet.isNull()) {
		m_preview = m_sheet.copy(column * m_sheetSize, m_sheetRow * m_sheetSize, m_sheetSize, m_sheetSize);
	}
	emit previewChanged();
}
