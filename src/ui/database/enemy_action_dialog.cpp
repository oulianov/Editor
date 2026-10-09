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

#include "enemy_action_dialog.h"
#include "common/dbstring.h"
#include "model/project_data.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

using lcf::rpg::EnemyAction;

namespace {
	template<typename T>
	QString nameOf(const std::vector<T>& list, int id) {
		if (id < 1 || id > static_cast<int>(list.size())) {
			return QStringLiteral("<%1?>").arg(id);
		}
		return ToQString(list[id - 1].name);
	}

	QString idName(const QString& name, int id) {
		return QStringLiteral("%1:%2").arg(id, 4, 10, QChar('0')).arg(name);
	}

	template<typename T>
	void selectId(RpgComboBox<T>* combo, int id) {
		combo->setCurrentIndex(combo->comboBox()->findData(id));
	}

	/** The selected entry, or fallback when the combo has none */
	template<typename T>
	int selectedId(RpgComboBox<T>* combo, int fallback) {
		int id = combo->comboBox()->currentData().toInt();
		return id > 0 ? id : fallback;
	}

	QStringList basicNames() {
		return {
			EnemyActionDialog::tr("Attack"),
			EnemyActionDialog::tr("Double Attack"),
			EnemyActionDialog::tr("Defend"),
			EnemyActionDialog::tr("Observe Battle"),
			EnemyActionDialog::tr("Charge Up"),
			EnemyActionDialog::tr("Self-Destruct"),
			EnemyActionDialog::tr("Escape"),
			EnemyActionDialog::tr("No Action")
		};
	}

	/** Range of the two parameters of a precondition type */
	std::pair<int, int> paramRange(int type) {
		switch (type) {
			case EnemyAction::ConditionType_turn:
				return {0, 9999};
			case EnemyAction::ConditionType_actors:
				return {0, 8};
			case EnemyAction::ConditionType_party_lvl:
				return {1, 99};
			default:
				return {0, 100};
		}
	}
}

EnemyActionDialog::EnemyActionDialog(ProjectData& project, lcf::rpg::EnemyAction& action, QWidget* parent) :
	QDialog(parent), m_action(action)
{
	setWindowTitle(tr("Monster Behavior"));

	// Behavior trigger: precondition and priority
	auto* groupTrigger = new QGroupBox(tr("Behavior Trigger"), this);
	auto* groupCondition = new QGroupBox(tr("Preconditions"), groupTrigger);
	m_comboCondition = new QComboBox(groupCondition);
	m_comboCondition->setObjectName(QStringLiteral("comboCondition"));
	m_comboCondition->addItems({
		tr("None"), tr("Switch"), tr("Turns Elapsed"), tr("Monsters Present"),
		tr("Monster HP"), tr("Monster MP"), tr("Party Level Average"), tr("Party Exhaustion")
	});
	m_comboConditionSwitch = new SwitchRpgComboBox(groupCondition);
	m_comboConditionSwitch->setObjectName(QStringLiteral("comboConditionSwitch"));
	m_comboConditionSwitch->makeModel(project);
	m_labelSwitch = new QLabel(tr("is ON"), groupCondition);
	m_spinParam1 = new QSpinBox(groupCondition);
	m_spinParam1->setObjectName(QStringLiteral("spinParam1"));
	m_labelParam1 = new QLabel(groupCondition);
	m_spinParam2 = new QSpinBox(groupCondition);
	m_spinParam2->setObjectName(QStringLiteral("spinParam2"));
	m_labelParam2 = new QLabel(groupCondition);

	auto* paramRow = new QHBoxLayout();
	paramRow->addWidget(m_comboConditionSwitch, 1);
	paramRow->addWidget(m_labelSwitch);
	paramRow->addWidget(m_spinParam1);
	paramRow->addWidget(m_labelParam1);
	paramRow->addWidget(m_spinParam2);
	paramRow->addWidget(m_labelParam2);
	paramRow->addStretch();
	auto* conditionLayout = new QVBoxLayout(groupCondition);
	conditionLayout->addWidget(m_comboCondition);
	conditionLayout->addLayout(paramRow);

	auto* groupRating = new QGroupBox(tr("Priority"), groupTrigger);
	m_spinRating = new QSpinBox(groupRating);
	m_spinRating->setObjectName(QStringLiteral("spinRating"));
	m_spinRating->setRange(1, 100);
	auto* ratingLayout = new QVBoxLayout(groupRating);
	ratingLayout->addWidget(m_spinRating);
	ratingLayout->addStretch();

	auto* triggerLayout = new QHBoxLayout(groupTrigger);
	triggerLayout->addWidget(groupCondition, 1);
	triggerLayout->addWidget(groupRating);

	// Action: basic, skill or transformation
	auto* groupAction = new QGroupBox(tr("Action"), this);
	m_radioBasic = new QRadioButton(tr("Basic"), groupAction);
	m_radioBasic->setObjectName(QStringLiteral("radioBasic"));
	m_radioSkill = new QRadioButton(tr("Skill"), groupAction);
	m_radioSkill->setObjectName(QStringLiteral("radioSkill"));
	m_radioTransform = new QRadioButton(tr("Transform"), groupAction);
	m_radioTransform->setObjectName(QStringLiteral("radioTransform"));
	m_comboBasic = new QComboBox(groupAction);
	m_comboBasic->setObjectName(QStringLiteral("comboBasic"));
	m_comboBasic->addItems(basicNames());
	m_comboSkill = new SkillRpgComboBox(groupAction);
	m_comboSkill->setObjectName(QStringLiteral("comboSkill"));
	m_comboSkill->makeModel(project);
	m_comboTransform = new EnemyRpgComboBox(groupAction);
	m_comboTransform->setObjectName(QStringLiteral("comboTransform"));
	m_comboTransform->makeModel(project);

	auto* actionLayout = new QGridLayout(groupAction);
	actionLayout->addWidget(m_radioBasic, 0, 0);
	actionLayout->addWidget(m_comboBasic, 0, 1);
	actionLayout->addWidget(m_radioSkill, 1, 0);
	actionLayout->addWidget(m_comboSkill, 1, 1);
	actionLayout->addWidget(m_radioTransform, 2, 0);
	actionLayout->addWidget(m_comboTransform, 2, 1);
	actionLayout->setColumnStretch(1, 1);

	// Switches turned on / off when the action is taken
	auto makeSwitchGroup = [&](const QString& title, const QString& name, QCheckBox*& check, SwitchRpgComboBox*& combo) {
		auto* group = new QGroupBox(title, this);
		check = new QCheckBox(tr("Use"), group);
		check->setObjectName(QStringLiteral("check") + name);
		combo = new SwitchRpgComboBox(group);
		combo->setObjectName(QStringLiteral("combo") + name);
		combo->makeModel(project);
		auto* layout = new QHBoxLayout(group);
		layout->addWidget(check);
		layout->addWidget(combo, 1);
		return group;
	};
	auto* groupSwitchOn = makeSwitchGroup(tr("Action Turns Switch ON"), QStringLiteral("SwitchOn"), m_checkSwitchOn, m_comboSwitchOn);
	auto* groupSwitchOff = makeSwitchGroup(tr("Action Turns Switch OFF"), QStringLiteral("SwitchOff"), m_checkSwitchOff, m_comboSwitchOff);

	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(buttons, &QDialogButtonBox::accepted, this, &EnemyActionDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &EnemyActionDialog::reject);

	auto* layout = new QVBoxLayout(this);
	layout->addWidget(groupTrigger);
	layout->addWidget(groupAction);
	layout->addWidget(groupSwitchOn);
	layout->addWidget(groupSwitchOff);
	layout->addWidget(buttons);

	// Show the action
	m_comboCondition->setCurrentIndex(action.condition_type);
	showConditionParams(action.condition_type);
	m_spinParam1->setValue(action.condition_param1);
	m_spinParam2->setValue(action.condition_param2);
	selectId(m_comboConditionSwitch, action.switch_id);
	m_spinRating->setValue(action.rating);

	m_radioBasic->setChecked(action.kind == EnemyAction::Kind_basic);
	m_radioSkill->setChecked(action.kind == EnemyAction::Kind_skill);
	m_radioTransform->setChecked(action.kind == EnemyAction::Kind_transformation);
	m_comboBasic->setCurrentIndex(action.basic);
	selectId(m_comboSkill, action.skill_id);
	selectId(m_comboTransform, action.enemy_id);

	m_checkSwitchOn->setChecked(action.switch_on);
	selectId(m_comboSwitchOn, action.switch_on_id);
	m_checkSwitchOff->setChecked(action.switch_off);
	selectId(m_comboSwitchOff, action.switch_off_id);

	connect(m_comboCondition, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int type) {
		showConditionParams(type);
		// A new type starts with its whole range (turns: every turn)
		auto range = paramRange(type);
		if (type == EnemyAction::ConditionType_turn) {
			range = {1, 0};
		}
		m_spinParam1->setValue(range.first);
		m_spinParam2->setValue(range.second);
	});
	for (QAbstractButton* button : std::initializer_list<QAbstractButton*>{
			m_radioBasic, m_radioSkill, m_radioTransform, m_checkSwitchOn, m_checkSwitchOff}) {
		connect(button, &QAbstractButton::toggled, this, &EnemyActionDialog::updateEnabled);
	}
	updateEnabled();
}

void EnemyActionDialog::showConditionParams(int type) {
	const bool isSwitch = type == EnemyAction::ConditionType_switch;
	const bool hasParams = type > EnemyAction::ConditionType_switch;
	m_comboConditionSwitch->setVisible(isSwitch);
	m_labelSwitch->setVisible(isSwitch);
	for (QWidget* widget : {static_cast<QWidget*>(m_spinParam1), static_cast<QWidget*>(m_labelParam1),
			static_cast<QWidget*>(m_spinParam2), static_cast<QWidget*>(m_labelParam2)}) {
		widget->setVisible(hasParams);
	}
	if (!hasParams) {
		return;
	}

	auto range = paramRange(type);
	m_spinParam1->setRange(range.first, range.second);
	m_spinParam2->setRange(range.first, range.second);

	const bool percent = type == EnemyAction::ConditionType_hp
		|| type == EnemyAction::ConditionType_sp
		|| type == EnemyAction::ConditionType_party_fatigue;
	if (type == EnemyAction::ConditionType_turn) {
		// Every param1 turns, from turn param2
		m_labelParam1->setText(tr("x +"));
		m_labelParam2->setText(QString());
	} else {
		m_labelParam1->setText(percent ? tr("% -") : tr("-"));
		m_labelParam2->setText(percent ? tr("%") : QString());
	}
}

void EnemyActionDialog::updateEnabled() {
	m_comboBasic->setEnabled(m_radioBasic->isChecked());
	m_comboSkill->setEnabled(m_radioSkill->isChecked());
	m_comboTransform->setEnabled(m_radioTransform->isChecked());
	m_comboSwitchOn->setEnabled(m_checkSwitchOn->isChecked());
	m_comboSwitchOff->setEnabled(m_checkSwitchOff->isChecked());
}

void EnemyActionDialog::accept() {
	auto& a = m_action;
	a.condition_type = m_comboCondition->currentIndex();
	if (a.condition_type > EnemyAction::ConditionType_switch) {
		a.condition_param1 = m_spinParam1->value();
		a.condition_param2 = m_spinParam2->value();
	}
	a.switch_id = selectedId(m_comboConditionSwitch, a.switch_id);
	a.rating = m_spinRating->value();

	if (m_radioSkill->isChecked()) {
		a.kind = EnemyAction::Kind_skill;
	} else if (m_radioTransform->isChecked()) {
		a.kind = EnemyAction::Kind_transformation;
	} else {
		a.kind = EnemyAction::Kind_basic;
	}
	a.basic = m_comboBasic->currentIndex();
	a.skill_id = selectedId(m_comboSkill, a.skill_id);
	a.enemy_id = selectedId(m_comboTransform, a.enemy_id);

	a.switch_on = m_checkSwitchOn->isChecked();
	a.switch_on_id = selectedId(m_comboSwitchOn, a.switch_on_id);
	a.switch_off = m_checkSwitchOff->isChecked();
	a.switch_off_id = selectedId(m_comboSwitchOff, a.switch_off_id);

	QDialog::accept();
}

QString EnemyActionDialog::actionText(const ProjectData& project, const lcf::rpg::EnemyAction& action) {
	const auto& db = project.database();
	switch (action.kind) {
		case EnemyAction::Kind_skill:
			return nameOf(db.skills, action.skill_id);
		case EnemyAction::Kind_transformation:
			return tr("Transform into %1").arg(nameOf(db.enemies, action.enemy_id));
		default: {
			auto names = basicNames();
			return action.basic >= 0 && action.basic < names.size() ? names[action.basic] : QStringLiteral("<%1?>").arg(action.basic);
		}
	}
}

QString EnemyActionDialog::conditionText(const ProjectData& project, const lcf::rpg::EnemyAction& action) {
	const int p1 = action.condition_param1;
	const int p2 = action.condition_param2;
	const QString range = QStringLiteral("%1-%2").arg(p1).arg(p2);
	switch (action.condition_type) {
		case EnemyAction::ConditionType_switch:
			return tr("Switch [%1] ON").arg(idName(nameOf(project.database().switches, action.switch_id), action.switch_id));
		case EnemyAction::ConditionType_turn:
			// Same forms as RPG Maker: only the parts that are set
			if (p1 == 0) {
				return tr("Turns Elapsed [%1]").arg(p2);
			} else if (p2 == 0) {
				return tr("Turns Elapsed [%1x]").arg(p1);
			}
			return tr("Turns Elapsed [%1x+%2]").arg(p1).arg(p2);
		case EnemyAction::ConditionType_actors:
			return tr("Monsters Present [%1]").arg(range);
		case EnemyAction::ConditionType_hp:
			return tr("Monster HP between [%1]%").arg(range);
		case EnemyAction::ConditionType_sp:
			return tr("Monster MP between [%1]%").arg(range);
		case EnemyAction::ConditionType_party_lvl:
			return tr("Average Character Level [%1]").arg(range);
		case EnemyAction::ConditionType_party_fatigue:
			return tr("Party Exhaustion [%1]%").arg(range);
		default:
			return tr("No Precondition");
	}
}
