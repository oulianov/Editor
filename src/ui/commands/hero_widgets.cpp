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

#include "hero_widgets.h"
#include "common/dbstring.h"
#include "common/sortfilter_proxy_models.h"
#include "model/project_data.h"
#include "ui/common/operand_widget.h"
#include "ui/common/rpg_combobox.h"
#include "ui/common/rpg_spinbox.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QStandardItemModel>
#include <QVBoxLayout>
#include <algorithm>

HeroCommandWidget::HeroCommandWidget(ProjectData& project, QWidget* parent) :
	EventCommandBaseWidget(project, parent)
{
	new QVBoxLayout(this);
}

int HeroCommandWidget::parameter(int index) const {
	return index < static_cast<int>(m_cmd->parameters.size()) ? m_cmd->parameters[index] : 0;
}

void HeroCommandWidget::setParameter(int index, int value) {
	resizeCommandList(index);
	m_cmd->parameters[index] = value;
}

QGroupBox* HeroCommandWidget::group(const QString& title) {
	auto* box = new QGroupBox(title, this);
	layout()->addWidget(box);
	return box;
}

QFormLayout* HeroCommandWidget::section(const QString& title) {
	auto* form = new QFormLayout(group(title));
	form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	return form;
}

void HeroCommandWidget::operand(const QString& title, OperandWidgetBase* widget, int index) {
	// EventCommandBaseWidget::setData attaches it to parameters index and
	// index + 1, which it reads unchecked
	resizeCommandList(index + 1);
	widget->setObjectName(QStringLiteral("operand_arg%1").arg(index));
	// the group box already has the margins
	widget->layout()->setContentsMargins(0, 0, 0, 0);
	// the shared amount box keeps QSpinBox's 0-99 range: widen it before it
	// reads the amount, so larger values are not cut on opening
	if (auto* spin = widget->findChild<QSpinBox*>()) {
		const int value = parameter(index + 1);
		spin->setRange(std::min(0, value), std::max(9999999, value));
	}
	(new QVBoxLayout(group(title)))->addWidget(widget);
}

QComboBox* HeroCommandWidget::options(QFormLayout* form, const QString& label,
	const QString& name, const QStringList& labels, const QList<int>& values, int value)
{
	auto* combo = new QComboBox(this);
	combo->setObjectName(name);
	for (int i = 0; i < labels.size(); ++i) {
		combo->addItem(labels[i], values.isEmpty() ? i : values[i]);
	}
	// a value of a patch or a damaged file is shown and kept until changed
	int selected = combo->findData(value);
	if (selected < 0) {
		combo->addItem(tr("Unknown (%1)").arg(value), value);
		selected = combo->count() - 1;
	}
	combo->setCurrentIndex(selected);
	if (label.isEmpty()) {
		form->addRow(combo);
	} else {
		form->addRow(label, combo);
	}
	return combo;
}

QComboBox* HeroCommandWidget::choices(QFormLayout* form, const QString& label,
	const QString& name, int index, const QStringList& labels, const QList<int>& values)
{
	auto* combo = options(form, label, name, labels, values, parameter(index));
	connect(combo, &QComboBox::currentIndexChanged, this, [this, combo, index](int row) {
		if (row >= 0) {
			setParameter(index, combo->itemData(row).toInt());
		}
	});
	return combo;
}

RpgSpinBox* HeroCommandWidget::number(QFormLayout* form, const QString& label,
	const QString& name, int index, int maximum)
{
	auto* spin = new RpgSpinBox(this);
	spin->setObjectName(name);
	const int value = parameter(index);
	spin->setRange(std::min(0, value), std::max(maximum, value));
	spin->setValue(value);
	form->addRow(label, spin);
	connect(spin, &QSpinBox::valueChanged, this, [this, index](int value) { setParameter(index, value); });
	return spin;
}

QCheckBox* HeroCommandWidget::check(QFormLayout* form, const QString& label,
	const QString& name, int index)
{
	auto* box = new QCheckBox(label, this);
	box->setObjectName(name);
	box->setChecked(parameter(index) != 0);
	form->addRow(box);
	connect(box, &QCheckBox::toggled, this, [this, index](bool checked) { setParameter(index, checked ? 1 : 0); });
	return box;
}

RpgComboBoxBase* HeroCommandWidget::database(QFormLayout* form, const QString& label,
	RpgComboBoxBase* selector, const QString& name, int value, int index)
{
	// an ID missing from the database shows an empty field and is kept
	auto* combo = selector->comboBox();
	combo->setObjectName(name);
	combo->setCurrentIndex(combo->findData(value));
	if (label.isEmpty()) {
		form->addRow(selector);
	} else {
		form->addRow(label, selector);
	}
	if (index >= 0) {
		connect(combo, &QComboBox::currentIndexChanged, this, [this, combo, index](int row) {
			if (row >= 0) {
				setParameter(index, combo->currentData().toInt());
			}
		});
	}
	return selector;
}

void HeroCommandWidget::equipment() {
	// Parameter 3 is the item source when equipping, the slot when unequipping
	const bool equip = parameter(2) == 0;
	auto* operation = choices(section(tr("Operation")), QString(), QStringLiteral("heroOperation"), 2,
		{tr("Equip Item"), tr("Unequip Item")});

	auto* itemForm = section(tr("Item"));
	auto* source = options(itemForm, tr("Source:"), QStringLiteral("heroItemSource"),
		{tr("Specific Item"), tr("Item with ID Stored in")}, {}, equip ? parameter(3) : 0);
	auto* item = new ItemRpgComboBox(this);
	item->makeModel(m_project);
	std::vector<int> equipment;
	for (const auto& entry : m_project.database().items) {
		if (entry.type >= lcf::rpg::Item::Type_weapon && entry.type <= lcf::rpg::Item::Type_accessory) {
			equipment.push_back(entry.ID);
		}
	}
	item->setFilter(new SortFilterProxyModelIdFilter(equipment, item));
	auto* variable = new VariableRpgComboBox(this);
	variable->makeModel(m_project);
	// the field not in use starts on its first entry, ready if chosen
	database(itemForm, tr("Specific Item:"), item, QStringLiteral("heroItem"),
		equip && parameter(3) == 0 ? parameter(4) : item->comboBox()->itemData(0).toInt());
	database(itemForm, tr("Variable:"), variable, QStringLiteral("heroItemVariable"),
		equip && parameter(3) == 1 ? parameter(4) : 1);

	auto* slot = options(section(tr("Slot from which to Unequip Item")), QString(), QStringLiteral("heroSlot"),
		{tr("All"), tr("Weapon"), tr("Shield"), tr("Armor"), tr("Helmet"), tr("Accessory")},
		{5, 0, 1, 2, 3, 4}, equip ? 5 : parameter(3));

	auto* itemGroup = itemForm->parentWidget();
	auto* slotGroup = slot->parentWidget();
	auto update = [=] {
		itemGroup->setEnabled(operation->currentData().toInt() == 0);
		slotGroup->setEnabled(operation->currentData().toInt() == 1);
		item->setEnabled(source->currentData().toInt() == 0);
		variable->setEnabled(source->currentData().toInt() == 1);
	};
	update();
	auto store = [=, this] {
		update();
		const int mode = source->currentData().toInt();
		if (operation->currentData().toInt() == 1) {
			setParameter(3, slot->currentData().toInt());
			setParameter(4, 0);
		} else if (operation->currentData().toInt() == 0 && (mode == 0 || mode == 1)) {
			auto* combo = (mode == 0 ? static_cast<RpgComboBoxBase*>(item) : variable)->comboBox();
			setParameter(3, mode);
			if (combo->currentIndex() >= 0) {
				setParameter(4, combo->currentData().toInt());
			}
		}
	};
	for (auto* combo : {operation, source, slot, item->comboBox(), variable->comboBox()}) {
		connect(combo, &QComboBox::currentIndexChanged, this, store);
	}
}

void HeroCommandWidget::setData(lcf::rpg::EventCommand* cmd) {
	m_cmd = cmd;
	operand(tr("Target"), new PartyOperandWidget(this), 0);

	using Code = lcf::rpg::EventCommand::Code;
	const auto code = static_cast<Code>(cmd->code);
	switch (code) {
	case Code::ChangeParameters: {
		setWindowTitle(tr("Change Base Statistics"));
		auto* form = section(tr("Operation"));
		choices(form, tr("Operation:"), QStringLiteral("heroOperation"), 2,
			{tr("Increase Statistic"), tr("Decrease Statistic")});
		choices(form, tr("Statistic:"), QStringLiteral("heroStatistic"), 3,
			{tr("Maximum HP"), tr("Maximum MP"), tr("Attack"), tr("Defense"), tr("Intelligence"), tr("Agility")});
		operand(tr("Amount of Points"), new ValueOperandWidget(this), 4);
		break;
	}
	case Code::ChangeSkills:
		setWindowTitle(tr("Change Skills"));
		choices(section(tr("Operation")), QString(), QStringLiteral("heroOperation"), 2,
			{tr("Add Skill"), tr("Remove Skill")});
		operand(tr("Skill"), new SkillOperandWidget(this), 3);
		break;
	case Code::ChangeEquipment:
		setWindowTitle(tr("Change Equipped Items"));
		equipment();
		break;
	case Code::ChangeHP:
	case Code::ChangeSP: {
		const bool hp = code == Code::ChangeHP;
		setWindowTitle(hp ? tr("Change HP") : tr("Change MP"));
		auto* operation = choices(section(tr("Operation")), QString(), QStringLiteral("heroOperation"), 2,
			hp ? QStringList{tr("Increase HP"), tr("Decrease HP")}
				: QStringList{tr("Increase MP"), tr("Decrease MP")});
		operand(tr("Amount"), new ValueOperandWidget(this), 3);
		if (hp) {
			// only a decrease can kill
			auto* lethal = check(section(tr("Option")), tr("HP Reduction can Kill Target"),
				QStringLiteral("heroLethal"), 5);
			lethal->setEnabled(operation->currentData().toInt() == 1);
			connect(operation, &QComboBox::currentIndexChanged, this, [operation, lethal] {
				lethal->setEnabled(operation->currentData().toInt() == 1);
			});
		}
		break;
	}
	case Code::ChangeCondition: {
		setWindowTitle(tr("Change Character Condition"));
		choices(section(tr("Operation")), QString(), QStringLiteral("heroOperation"), 2,
			{tr("Add Condition"), tr("Remove Condition")});
		auto* state = new StateRpgComboBox(this);
		state->makeModel(m_project);
		database(section(tr("Condition")), QString(), state, QStringLiteral("heroCondition"), parameter(3), 3);
		break;
	}
	case Code::SimulatedAttack: {
		setWindowTitle(tr("Simulated Enemy Attack"));
		auto* form = section(tr("Attack"));
		number(form, tr("Attack Power:"), QStringLiteral("heroAttackPower"), 2, 9999);
		number(form, tr("Defense Effect:"), QStringLiteral("heroDefenseEffect"), 3, 100)->setSuffix(tr(" %"));
		number(form, tr("Intelligence Effect:"), QStringLiteral("heroIntelligenceEffect"), 4, 100)->setSuffix(tr(" %"));
		number(form, tr("Variance:"), QStringLiteral("heroVariance"), 5, 10);
		auto* damage = section(tr("Store Amount of Damage Dealt"));
		auto* store = check(damage, tr("Store in Variable"), QStringLiteral("heroStoreDamage"), 6);
		auto* variable = new VariableRpgComboBox(this);
		variable->makeModel(m_project);
		database(damage, tr("Variable:"), variable, QStringLiteral("heroDamageVariable"), parameter(7), 7);
		variable->setEnabled(store->isChecked());
		connect(store, &QCheckBox::toggled, this, [variable](bool checked) {
			variable->setEnabled(checked);
			if (checked && variable->comboBox()->currentIndex() < 0) {
				variable->comboBox()->setCurrentIndex(0);
			}
		});
		break;
	}
	case Code::ChangeClass: {
		setWindowTitle(tr("Change Class"));
		QStringList labels = {tr("(None)")};
		QList<int> ids = {0};
		for (const auto& entry : m_project.database().classes) {
			labels << QStringLiteral("%1: %2").arg(entry.ID, 4, 10, QChar('0')).arg(ToQString(entry.name));
			ids << entry.ID;
		}
		auto* form = section(tr("Class"));
		choices(form, tr("Class:"), QStringLiteral("heroClass"), 2, labels, ids);
		choices(form, tr("Level:"), QStringLiteral("heroClassLevel"), 3,
			{tr("Retain Current Level"), tr("Character Becomes Level 1")});
		choices(section(tr("Learned Skills")), QString(), QStringLiteral("heroClassSkills"), 4,
			{tr("No Change in Skills"), tr("Add Skills up to Current Level in New Class and Delete Old Skills"),
				tr("Add Skills up to Current Level in New Class to Old Skills")});
		choices(section(tr("Base Statistics")), QString(), QStringLiteral("heroClassStatistics"), 5,
			{tr("No Change in Statistics"), tr("Halve Statistics"),
				tr("Change Statistics to Level 1 in New Class"), tr("Change Statistics to Current Level in New Class")});
		check(section(tr("Option")), tr("Show Level-up Message if Level Differs After Class Change"),
			QStringLiteral("heroClassMessage"), 6);
		break;
	}
	case Code::ChangeBattleCommands: {
		setWindowTitle(tr("Change Battle Commands"));
		QStringList labels = {tr("All")};
		QList<int> ids = {0};
		for (const auto& entry : m_project.database().battlecommands.commands) {
			labels << QStringLiteral("%1: %2").arg(entry.ID, 4, 10, QChar('0')).arg(ToQString(entry.name));
			ids << entry.ID;
		}
		auto* form = section(tr("Battle Command"));
		auto* command = choices(form, tr("Command:"), QStringLiteral("heroBattleCommand"), 2, labels, ids);
		auto* operation = choices(form, tr("Operation:"), QStringLiteral("heroBattleOperation"), 3,
			{tr("Remove"), tr("Add")});
		// "All" (0) only removes: adding needs a command of the database
		auto update = [command, operation] {
			if (auto* model = qobject_cast<QStandardItemModel*>(command->model())) {
				model->item(0)->setEnabled(operation->currentData().toInt() != 1);
			}
		};
		update();
		connect(operation, &QComboBox::currentIndexChanged, this, [command, operation, update] {
			update();
			if (operation->currentData().toInt() == 1 && command->currentData().toInt() == 0 && command->count() > 1) {
				command->setCurrentIndex(1);
			}
		});
		break;
	}
	default:
		break;
	}

	// attaches the operand widgets
	EventCommandBaseWidget::setData(cmd);

	if ((code == Code::ChangeClass || code == Code::ChangeBattleCommands)
		&& m_project.database().system.ldb_id != 2003) {
		// RPG Maker 2000 has neither classes nor battle commands: kept, not editable
		for (auto* box : findChildren<QGroupBox*>(Qt::FindDirectChildrenOnly)) {
			box->setEnabled(false);
		}
		auto* notice = new QLabel(tr("This command requires RPG Maker 2003."), this);
		notice->setWordWrap(true);
		layout()->addWidget(notice);
	}
	static_cast<QVBoxLayout*>(layout())->addStretch();
}
