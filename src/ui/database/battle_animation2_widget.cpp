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

#include "battle_animation2_widget.h"
#include "battle_animation_widget.h"
#include "defines.h"
#include "common/dbstring.h"
#include "model/project_data.h"
#include "ui/common/rpg_combobox.h"
#include "ui/picker/picker_dialog.h"
#include <QButtonGroup>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QRadioButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {
// frames of a BattleCharSet and of a BattleWeapon
constexpr int battler_size = 48;
constexpr int weapon_size = 64;
// RPG Maker 2003 has 32 poses and 32 weapons in each battler animation
constexpr size_t slot_count = 32;

QTreeWidget* makeList(const QStringList& headers, QWidget* parent) {
	auto* list = new QTreeWidget(parent);
	list->setHeaderLabels(headers);
	list->setRootIsDecorated(false);
	list->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	return list;
}

/** A line with a file name and the button that picks it */
QWidget* makeFileRow(QLineEdit*& line, QToolButton*& button, QWidget* parent) {
	auto* row = new QWidget(parent);
	auto* layout = new QHBoxLayout(row);
	layout->setContentsMargins(0, 0, 0, 0);
	line = new QLineEdit(row);
	line->setReadOnly(true);
	button = new QToolButton(row);
	button->setText(QStringLiteral("..."));
	layout->addWidget(line, 1);
	layout->addWidget(button);
	return row;
}

QLabel* makePreview(int size, QWidget* parent) {
	auto* label = new QLabel(parent);
	label->setAlignment(Qt::AlignCenter);
	label->setFrameShape(QFrame::StyledPanel);
	label->setMinimumSize(2 * size, 2 * size);
	label->setWordWrap(true);
	return label;
}
}

BattleAnimation2Widget::BattleAnimation2Widget(ProjectData& project, QWidget *parent) :
	QWidget(parent), m_project(project)
{
	m_rpg2k3 = project.database().system.ldb_id == 2003;
	auto* layout = new QVBoxLayout(this);

	auto* top = new QHBoxLayout();
	auto* form = new QFormLayout();
	m_name = new QLineEdit(this);
	form->addRow(tr("Name"), m_name);
	top->addLayout(form, 1);
	auto* speed_box = new QGroupBox(tr("Animation Display Speed"), this);
	auto* speed_layout = new QHBoxLayout(speed_box);
	m_speed = new QButtonGroup(this);
	const std::vector<std::pair<int, QString>> speeds = {
		{lcf::rpg::BattlerAnimation::Speed_slow, tr("Slow")},
		{lcf::rpg::BattlerAnimation::Speed_medium, tr("Medium")},
		{lcf::rpg::BattlerAnimation::Speed_fast, tr("Fast")},
	};
	for (const auto& [id, text] : speeds) {
		auto* radio = new QRadioButton(text, speed_box);
		m_speed->addButton(radio, id);
		speed_layout->addWidget(radio);
	}
	top->addWidget(speed_box);
	layout->addLayout(top);

	m_tabs = new QTabWidget(this);
	m_tabs->setObjectName(QStringLiteral("tabBattlerAnimation"));
	layout->addWidget(m_tabs, 1);

	// poses
	auto* poses_page = new QWidget(m_tabs);
	auto* poses_layout = new QHBoxLayout(poses_page);
	m_poses = makeList({tr("No."), tr("Pose Label"), tr("Graphic")}, poses_page);
	m_poses->setObjectName(QStringLiteral("listPoses"));
	poses_layout->addWidget(m_poses, 1);
	auto* pose_edit = new QVBoxLayout();
	auto* pose_form = new QFormLayout();
	m_poseName = new QLineEdit(poses_page);
	pose_form->addRow(tr("Pose Label"), m_poseName);
	auto* type_row = new QVBoxLayout();
	m_poseType = new QButtonGroup(this);
	const QString types[] = {tr("Character Pose"), tr("Battle Animation")};
	for (int i = 0; i < 2; ++i) {
		auto* radio = new QRadioButton(types[i], poses_page);
		m_poseType->addButton(radio, i);
		type_row->addWidget(radio);
	}
	pose_form->addRow(tr("Animation Type"), type_row);
	QToolButton* pose_pick;
	m_poseFileRow = makeFileRow(m_poseFile, pose_pick, poses_page);
	pose_form->addRow(tr("Graphic"), m_poseFileRow);
	m_poseIndex = new QSpinBox(poses_page);
	m_poseIndex->setRange(1, 8);
	m_poseIndex->setToolTip(tr("The row of the graphic"));
	pose_form->addRow(tr("Index"), m_poseIndex);
	auto* animation = new AnimationRpgComboBox(poses_page);
	animation->makeModel(project);
	m_poseAnimation = animation;
	pose_form->addRow(tr("Battle Animation"), m_poseAnimation);
	pose_edit->addLayout(pose_form);
	m_posePreview = makePreview(battler_size, poses_page);
	pose_edit->addWidget(m_posePreview, 0, Qt::AlignHCenter);
	pose_edit->addStretch(1);
	poses_layout->addLayout(pose_edit);
	m_tabs->addTab(poses_page, tr("Poses"));

	// weapons
	auto* weapons_page = new QWidget(m_tabs);
	auto* weapons_layout = new QHBoxLayout(weapons_page);
	m_weapons = makeList({tr("No."), tr("Name"), tr("Graphic")}, weapons_page);
	m_weapons->setObjectName(QStringLiteral("listWeapons"));
	weapons_layout->addWidget(m_weapons, 1);
	auto* weapon_edit = new QVBoxLayout();
	auto* weapon_form = new QFormLayout();
	m_weaponName = new QLineEdit(weapons_page);
	weapon_form->addRow(tr("Name"), m_weaponName);
	QToolButton* weapon_pick;
	weapon_form->addRow(tr("Graphic"), makeFileRow(m_weaponFile, weapon_pick, weapons_page));
	m_weaponIndex = new QSpinBox(weapons_page);
	m_weaponIndex->setRange(1, 8);
	m_weaponIndex->setToolTip(tr("The row of the graphic"));
	weapon_form->addRow(tr("Index"), m_weaponIndex);
	weapon_edit->addLayout(weapon_form);
	m_weaponPreview = makePreview(weapon_size, weapons_page);
	weapon_edit->addWidget(m_weaponPreview, 0, Qt::AlignHCenter);
	weapon_edit->addStretch(1);
	weapons_layout->addLayout(weapon_edit);
	m_tabs->addTab(weapons_page, tr("Weapons"));

	m_timer = new QTimer(this);
	connect(m_timer, &QTimer::timeout, this, [this] {
		++m_step;
		updatePreview();
	});

	connect(m_name, &QLineEdit::textEdited, this, [this](const QString& text) {
		if (m_anim) m_anim->name = ToDBString(text);
	});
	connect(m_speed, &QButtonGroup::idClicked, this, [this](int id) {
		if (!m_anim) return;
		m_anim->speed = id;
		// the speed is the number of frames (at 60 per second) a pattern is shown
		m_timer->setInterval(id * 1000 / 60);
	});
	connect(m_tabs, &QTabWidget::currentChanged, this, &BattleAnimation2Widget::loadPreview);

	connect(m_poses, &QTreeWidget::currentItemChanged, this, &BattleAnimation2Widget::poseChanged);
	connect(m_poseName, &QLineEdit::textEdited, this, [this](const QString& text) {
		if (auto* pose = currentPose()) {
			pose->name = ToDBString(text);
			refreshPoseItem(m_poses->indexOfTopLevelItem(m_poses->currentItem()));
		}
	});
	connect(m_poseType, &QButtonGroup::idClicked, this, [this](int id) {
		if (auto* pose = currentPose()) {
			pose->animation_type = id;
			poseChanged();
		}
	});
	connect(pose_pick, &QToolButton::clicked, this, [this] {
		auto* pose = currentPose();
		if (!pose) return;
		PickerDialog dialog(m_project, FileFinder::FileType::Image, nullptr, this);
		connect(&dialog, &PickerDialog::fileSelected, this, [this, pose](const QString& name) {
			pose->battler_name = ToDBString(name);
			poseChanged();
		});
		dialog.setDirectoryAndFile(BATTLECHARSET, ToQString(pose->battler_name));
		dialog.exec();
	});
	connect(m_poseIndex, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
		if (auto* pose = currentPose()) {
			pose->battler_index = value - 1;
			refreshPoseItem(m_poses->indexOfTopLevelItem(m_poses->currentItem()));
			loadPreview();
		}
	});
	connect(m_poseAnimation->comboBox(), qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
		if (auto* pose = currentPose()) {
			pose->battle_animation_id = m_poseAnimation->comboBox()->currentData().toInt();
			refreshPoseItem(m_poses->indexOfTopLevelItem(m_poses->currentItem()));
		}
	});

	connect(m_weapons, &QTreeWidget::currentItemChanged, this, &BattleAnimation2Widget::weaponChanged);
	connect(m_weaponName, &QLineEdit::textEdited, this, [this](const QString& text) {
		if (auto* weapon = currentWeapon()) {
			weapon->name = ToDBString(text);
			refreshWeaponItem(m_weapons->indexOfTopLevelItem(m_weapons->currentItem()));
		}
	});
	connect(weapon_pick, &QToolButton::clicked, this, [this] {
		auto* weapon = currentWeapon();
		if (!weapon) return;
		PickerDialog dialog(m_project, FileFinder::FileType::Image, nullptr, this);
		connect(&dialog, &PickerDialog::fileSelected, this, [this, weapon](const QString& name) {
			weapon->weapon_name = ToDBString(name);
			weaponChanged();
		});
		dialog.setDirectoryAndFile(BATTLEWEAPON, ToQString(weapon->weapon_name));
		dialog.exec();
	});
	connect(m_weaponIndex, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
		if (auto* weapon = currentWeapon()) {
			weapon->weapon_index = value - 1;
			refreshWeaponItem(m_weapons->indexOfTopLevelItem(m_weapons->currentItem()));
			loadPreview();
		}
	});

	setData(nullptr);
}

void BattleAnimation2Widget::setData(lcf::rpg::BattlerAnimation* battler_anim) {
	m_anim = battler_anim;
	// battler animations are only used by RPG Maker 2003
	setEnabled(battler_anim && m_rpg2k3);
	m_name->setText(battler_anim ? ToQString(battler_anim->name) : QString());
	if (battler_anim) {
		if (m_rpg2k3) {
			// entries made outside of RPG Maker can have shorter lists
			auto fill = [](auto& items) {
				if (items.size() >= slot_count) return;
				items.resize(slot_count);
				for (size_t i = 0; i < items.size(); ++i) items[i].ID = static_cast<int>(i) + 1;
			};
			fill(battler_anim->poses);
			fill(battler_anim->weapons);
		}
		if (auto* button = m_speed->button(battler_anim->speed)) {
			button->setChecked(true);
		}
		m_timer->setInterval(battler_anim->speed * 1000 / 60);
	}
	refreshLists();
	if (battler_anim && m_rpg2k3) {
		m_timer->start();
	} else {
		m_timer->stop();
	}
}

lcf::rpg::BattlerAnimationPose* BattleAnimation2Widget::currentPose() {
	const int index = m_poses->indexOfTopLevelItem(m_poses->currentItem());
	if (!m_anim || index < 0 || index >= static_cast<int>(m_anim->poses.size())) {
		return nullptr;
	}
	return &m_anim->poses[index];
}

lcf::rpg::BattlerAnimationWeapon* BattleAnimation2Widget::currentWeapon() {
	const int index = m_weapons->indexOfTopLevelItem(m_weapons->currentItem());
	if (!m_anim || index < 0 || index >= static_cast<int>(m_anim->weapons.size())) {
		return nullptr;
	}
	return &m_anim->weapons[index];
}

void BattleAnimation2Widget::refreshLists() {
	{
		const QSignalBlocker block_poses(m_poses);
		const QSignalBlocker block_weapons(m_weapons);
		m_poses->clear();
		m_weapons->clear();
		if (m_anim) {
			for (size_t i = 0; i < m_anim->poses.size(); ++i) {
				m_poses->addTopLevelItem(new QTreeWidgetItem());
				refreshPoseItem(static_cast<int>(i));
			}
			for (size_t i = 0; i < m_anim->weapons.size(); ++i) {
				m_weapons->addTopLevelItem(new QTreeWidgetItem());
				refreshWeaponItem(static_cast<int>(i));
			}
		}
		m_poses->setCurrentItem(m_poses->topLevelItem(0));
		m_weapons->setCurrentItem(m_weapons->topLevelItem(0));
	}
	poseChanged();
	weaponChanged();
}

void BattleAnimation2Widget::refreshPoseItem(int index) {
	auto* item = m_poses->topLevelItem(index);
	if (!item || !m_anim) return;
	const auto& pose = m_anim->poses[index];
	QString graphic;
	if (pose.animation_type == lcf::rpg::BattlerAnimationPose::AnimType_battle) {
		const auto& animations = m_project.database().animations;
		const int id = pose.battle_animation_id;
		graphic = tr("Animation: %1").arg(id >= 1 && id <= static_cast<int>(animations.size())
			? ToQString(animations[id - 1].name) : QString::number(id));
	} else if (!pose.battler_name.empty()) {
		graphic = QStringLiteral("%1 (%2)").arg(ToQString(pose.battler_name)).arg(pose.battler_index + 1);
	}
	item->setText(0, QStringLiteral("%1").arg(index + 1, 2, 10, QLatin1Char('0')));
	item->setText(1, ToQString(pose.name));
	item->setText(2, graphic);
}

void BattleAnimation2Widget::refreshWeaponItem(int index) {
	auto* item = m_weapons->topLevelItem(index);
	if (!item || !m_anim) return;
	const auto& weapon = m_anim->weapons[index];
	item->setText(0, QStringLiteral("%1").arg(index + 1, 2, 10, QLatin1Char('0')));
	item->setText(1, ToQString(weapon.name));
	item->setText(2, weapon.weapon_name.empty() ? QString()
		: QStringLiteral("%1 (%2)").arg(ToQString(weapon.weapon_name)).arg(weapon.weapon_index + 1));
}

void BattleAnimation2Widget::poseChanged() {
	auto* pose = currentPose();
	const bool battle = pose && pose->animation_type == lcf::rpg::BattlerAnimationPose::AnimType_battle;
	m_poseName->setText(pose ? ToQString(pose->name) : QString());
	if (auto* button = m_poseType->button(pose ? pose->animation_type : 0)) {
		button->setChecked(true);
	}
	m_poseFile->setText(pose ? ToQString(pose->battler_name) : QString());
	{
		const QSignalBlocker block_index(m_poseIndex);
		const QSignalBlocker block_animation(m_poseAnimation->comboBox());
		m_poseIndex->setValue(pose ? pose->battler_index + 1 : 1);
		m_poseAnimation->comboBox()->setCurrentIndex(pose ? m_poseAnimation->comboBox()->findData(pose->battle_animation_id) : -1);
	}
	for (QWidget* widget : {static_cast<QWidget*>(m_poseName), static_cast<QWidget*>(m_poseType->button(0)), static_cast<QWidget*>(m_poseType->button(1))}) {
		widget->setEnabled(pose != nullptr);
	}
	// the graphic is used by character poses, the animation by the others
	m_poseFileRow->setEnabled(pose && !battle);
	m_poseIndex->setEnabled(pose && !battle);
	m_poseAnimation->setEnabled(battle);
	refreshPoseItem(m_poses->indexOfTopLevelItem(m_poses->currentItem()));
	loadPreview();
}

void BattleAnimation2Widget::weaponChanged() {
	auto* weapon = currentWeapon();
	m_weaponName->setText(weapon ? ToQString(weapon->name) : QString());
	m_weaponFile->setText(weapon ? ToQString(weapon->weapon_name) : QString());
	{
		const QSignalBlocker block(m_weaponIndex);
		m_weaponIndex->setValue(weapon ? weapon->weapon_index + 1 : 1);
	}
	m_weaponName->setEnabled(weapon != nullptr);
	m_weaponFile->parentWidget()->setEnabled(weapon != nullptr);
	m_weaponIndex->setEnabled(weapon != nullptr);
	refreshWeaponItem(m_weapons->indexOfTopLevelItem(m_weapons->currentItem()));
	loadPreview();
}

void BattleAnimation2Widget::loadPreview() {
	m_sheet = QPixmap();
	m_step = 0;
	const bool weapons = m_tabs->currentIndex() == 1;
	if (weapons) {
		if (auto* weapon = currentWeapon()) {
			m_sheet = BattleAnimationWidget::loadGraphic(m_project, BATTLEWEAPON, ToQString(weapon->weapon_name));
			m_sheetSize = weapon_size;
			m_sheetRow = weapon->weapon_index;
		}
	} else if (auto* pose = currentPose()) {
		if (pose->animation_type == lcf::rpg::BattlerAnimationPose::AnimType_character) {
			m_sheet = BattleAnimationWidget::loadGraphic(m_project, BATTLECHARSET, ToQString(pose->battler_name));
			m_sheetSize = battler_size;
			m_sheetRow = pose->battler_index;
		}
	}
	updatePreview();
}

void BattleAnimation2Widget::updatePreview() {
	QLabel* label = m_tabs->currentIndex() == 1 ? m_weaponPreview : m_posePreview;
	if (m_sheet.isNull()) {
		auto* pose = currentPose();
		label->setText(label == m_posePreview && pose && pose->animation_type == lcf::rpg::BattlerAnimationPose::AnimType_battle
			? tr("Battle animation") : QString());
		return;
	}
	// the three frames of a row, as the Player plays them
	static const int pattern[] = {0, 1, 2, 1};
	const QPixmap frame = m_sheet.copy(pattern[m_step % 4] * m_sheetSize, m_sheetRow * m_sheetSize, m_sheetSize, m_sheetSize);
	label->setPixmap(frame.scaled(2 * m_sheetSize, 2 * m_sheetSize));
}
