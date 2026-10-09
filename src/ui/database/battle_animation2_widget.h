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

#pragma once

#include <QPixmap>
#include <QWidget>
#include <lcf/rpg/battleranimation.h>

class ProjectData;
class QButtonGroup;
class QLabel;
class QLineEdit;
class QSpinBox;
class QTabWidget;
class QTimer;
class QTreeWidget;
class RpgComboBoxBase;

/**
 * The Battler Animations page of the database (RPG Maker 2003 only): how a
 * hero is drawn in battle. Each pose (idle, attacks, damage...) is a row of a
 * BattleCharSet graphic or a battle animation; each weapon a row of a
 * BattleWeapon graphic. The previews play the three frames of the row at the
 * display speed.
 */
class BattleAnimation2Widget : public QWidget
{
	Q_OBJECT

public:
	using value_type = lcf::rpg::BattlerAnimation;

	explicit BattleAnimation2Widget(ProjectData& project, QWidget *parent = nullptr);

	void setData(lcf::rpg::BattlerAnimation* battler_anim);

private:
	lcf::rpg::BattlerAnimationPose* currentPose();
	lcf::rpg::BattlerAnimationWeapon* currentWeapon();
	void refreshLists();
	void refreshPoseItem(int index);
	void refreshWeaponItem(int index);
	void poseChanged();
	void weaponChanged();
	void loadPreview();
	void updatePreview();

	ProjectData& m_project;
	lcf::rpg::BattlerAnimation* m_anim = nullptr;
	bool m_rpg2k3 = false;
	/** The graphic of the pose or weapon shown, its frame size and row */
	QPixmap m_sheet;
	int m_sheetSize = 0;
	int m_sheetRow = 0;
	int m_step = 0;

	QLineEdit* m_name = nullptr;
	QButtonGroup* m_speed = nullptr;
	QTabWidget* m_tabs = nullptr;
	QTimer* m_timer = nullptr;

	QTreeWidget* m_poses = nullptr;
	QLineEdit* m_poseName = nullptr;
	QButtonGroup* m_poseType = nullptr;
	QLineEdit* m_poseFile = nullptr;
	QWidget* m_poseFileRow = nullptr;
	QSpinBox* m_poseIndex = nullptr;
	RpgComboBoxBase* m_poseAnimation = nullptr;
	QLabel* m_posePreview = nullptr;

	QTreeWidget* m_weapons = nullptr;
	QLineEdit* m_weaponName = nullptr;
	QLineEdit* m_weaponFile = nullptr;
	QSpinBox* m_weaponIndex = nullptr;
	QLabel* m_weaponPreview = nullptr;
};
