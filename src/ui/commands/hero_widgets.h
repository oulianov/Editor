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

#include "ui/event/event_command_base_widget.h"
#include <QStringList>

class QCheckBox;
class QComboBox;
class QFormLayout;
class QGroupBox;
class OperandWidgetBase;
class RpgComboBoxBase;
class RpgSpinBox;

/** Commands acting on heroes. Parameters 0 and 1 (which heroes) and the
 * value-or-variable pairs use the shared operand widgets; the other fields are
 * written only when edited, so unknown values and trailing parameters survive
 * an unchanged OK. */
class HeroCommandWidget : public EventCommandBaseWidget
{
	Q_OBJECT
public:
	HeroCommandWidget(ProjectData& project, QWidget* parent);
	void setData(lcf::rpg::EventCommand* cmd) override;

private:
	int parameter(int index) const;
	void setParameter(int index, int value);
	QGroupBox* group(const QString& title);
	QFormLayout* section(const QString& title);
	void operand(const QString& title, OperandWidgetBase* operand, int index);
	QComboBox* options(QFormLayout* form, const QString& label, const QString& name,
		const QStringList& labels, const QList<int>& values, int value);
	QComboBox* choices(QFormLayout* form, const QString& label, const QString& name,
		int index, const QStringList& labels, const QList<int>& values = {});
	RpgSpinBox* number(QFormLayout* form, const QString& label, const QString& name,
		int index, int maximum);
	QCheckBox* check(QFormLayout* form, const QString& label, const QString& name, int index);
	RpgComboBoxBase* database(QFormLayout* form, const QString& label, RpgComboBoxBase* selector,
		const QString& name, int value, int index = -1);
	void equipment();
};

using ChangeParametersWidget = HeroCommandWidget;
using ChangeSkillsWidget = HeroCommandWidget;
using ChangeEquipmentWidget = HeroCommandWidget;
using ChangeHPWidget = HeroCommandWidget;
using ChangeSPWidget = HeroCommandWidget;
using ChangeConditionWidget = HeroCommandWidget;
using SimulatedAttackWidget = HeroCommandWidget;
using ChangeClassWidget = HeroCommandWidget;
using ChangeBattleCommandsWidget = HeroCommandWidget;
