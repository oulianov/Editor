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

#include "maniac_widgets.h"
#include <array>

class QCheckBox;
class QComboBox;
class QFormLayout;
class QLineEdit;
class QStackedWidget;
class RpgComboBoxBase;
class RpgSpinBox;

class LabelWidget : public EventCommandBaseWidget {
	Q_OBJECT
public:
	LabelWidget(ProjectData& project, QWidget* parent);
	void setData(lcf::rpg::EventCommand* command) override;
};

class KeyInputProcWidget : public EventCommandBaseWidget {
	Q_OBJECT
public:
	KeyInputProcWidget(ProjectData& project, QWidget* parent);
	void setData(lcf::rpg::EventCommand* command) override;

private:
	void setKey(int key, bool checked);
	void modernize();
	RpgComboBoxBase* m_result = nullptr;
	RpgComboBoxBase* m_timeVariable = nullptr;
	QCheckBox* m_wait = nullptr;
	QCheckBox* m_timed = nullptr;
	std::array<QCheckBox*, 9> m_keys{};
	bool m_rpg2003 = false;
	bool m_modern2003 = false;
	bool m_modern2000 = false;
};

/**
 * Conditional Branch (12010) and its battle version (13310). The map one
 * also has the conditions of Maniac Patch (types 12-16, ids from variables),
 * as EasyRPG Player reads them (CommandConditionalBranch).
 */
class ConditionalBranchWidget : public ManiacCommandWidget {
	Q_OBJECT
public:
	ConditionalBranchWidget(ProjectData& project, QWidget* parent);
	void setData(EventCommandList* commands) override;
	bool apply();

protected:
	ConditionalBranchWidget(ProjectData& project, QWidget* parent, bool battle);

private:
	void store();
	/** The Maniac Patch conditions (12-16): pages of the helpers, written when shown */
	void addManiacPages(bool enabled);
	/** A mode (number or variable) for the id of a condition, with the variable it may use;
	 * stored: the condition is the one of the command, else the mode starts from a number */
	QComboBox* addIdMode(QFormLayout* form, const QString& name, int modeIndex, QWidget* constant,
		RpgComboBoxBase*& variable, bool stored);
	static bool fromVariable(QComboBox* mode);
	lcf::rpg::EventCommand m_edited;
	bool m_battle = false;
	bool m_loading = false;
	QComboBox* m_kind = nullptr;
	QStackedWidget* m_pages = nullptr;
	QCheckBox* m_else = nullptr;
	RpgComboBoxBase* m_switch = nullptr;
	QComboBox* m_switchState = nullptr;
	RpgComboBoxBase* m_variable = nullptr;
	QComboBox* m_comparison = nullptr;
	QComboBox* m_operandKind = nullptr;
	RpgSpinBox* m_operandNumber = nullptr;
	RpgComboBoxBase* m_operandVariable = nullptr;
	RpgSpinBox* m_minutes = nullptr;
	RpgSpinBox* m_seconds = nullptr;
	QComboBox* m_timerComparison = nullptr;
	RpgSpinBox* m_money = nullptr;
	QComboBox* m_moneyComparison = nullptr;
	RpgComboBoxBase* m_item = nullptr;
	QComboBox* m_possession = nullptr;
	RpgComboBoxBase* m_hero = nullptr;
	QComboBox* m_heroTest = nullptr;
	QStackedWidget* m_heroPages = nullptr;
	QLineEdit* m_name = nullptr;
	RpgSpinBox* m_level = nullptr;
	RpgSpinBox* m_hp = nullptr;
	RpgComboBoxBase* m_skill = nullptr;
	RpgComboBoxBase* m_equipment = nullptr;
	RpgComboBoxBase* m_condition = nullptr;
	QComboBox* m_character = nullptr;
	QComboBox* m_direction = nullptr;
	QComboBox* m_vehicle = nullptr;
	QComboBox* m_other = nullptr;
	QComboBox* m_monster = nullptr;
	RpgComboBoxBase* m_commandHero = nullptr;
	QComboBox* m_battleCommand = nullptr;
	// Maniac Patch: ids given by variables
	QComboBox* m_itemMode = nullptr;
	QComboBox* m_heroMode = nullptr;
	QComboBox* m_characterMode = nullptr;
	RpgComboBoxBase* m_itemVariable = nullptr;
	RpgComboBoxBase* m_heroVariable = nullptr;
	RpgComboBoxBase* m_characterVariable = nullptr;
	QHash<int, QList<QWidget*>> m_maniacRows;
};

class ConditionalBranchBattleWidget : public ConditionalBranchWidget {
	Q_OBJECT
public:
	ConditionalBranchBattleWidget(ProjectData& project, QWidget* parent);
};
