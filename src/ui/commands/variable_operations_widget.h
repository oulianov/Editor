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
#include <QHash>

class QButtonGroup;
class QGridLayout;
class QRadioButton;

/**
 * Control Variables (10220), as EasyRPG Player reads it
 * (CommandControlVariables): the target (p0, p1, p2), the operation (p3) and
 * the operand (p4, values from p5). With Maniac Patch: ranges through
 * variables and expressions as targets, bit operations, and more operands
 * (party members, switches, functions, expressions) whose values can come
 * from variables (modes 4 bits per value).
 */
class VariableOperationsWidget : public ManiacCommandWidget
{
	Q_OBJECT

public:
	explicit VariableOperationsWidget(ProjectData& project, QWidget *parent);

protected:
	void build(QBoxLayout* layout) override;

private:
	QRadioButton* addRadio(QButtonGroup* group, QGridLayout* grid, int id, const QString& text, const QString& name,
		int row, int column = 0);
	/** An id from a list or, with Maniac Patch, from a variable (mode in parameter modeIndex, -1 for none) */
	QWidget* idField(const QString& name, int index, QWidget* constant, QComboBox* combo, bool database,
		int modeIndex, const std::vector<Mode>& modes);
	/** The controls of a radio button: enabled when it is checked, written when it gets checked */
	void addRow(QHash<int, QList<QWidget*>>& rows, QGridLayout* grid, int id, int row, QList<QWidget*> widgets);
	void update();
	/** Places the expressions in the parameters: the operand's after p5, the target's after the operand */
	void layoutExpressions();

	bool m_maniac = false;
	bool m_showManiac = false;
	QButtonGroup* m_target = nullptr;
	// the radio buttons of RPG Maker's window; with Maniac Patch: combo boxes
	QButtonGroup* m_operation = nullptr;
	QButtonGroup* m_operand = nullptr;
	QHash<int, QList<QWidget*>> m_targetRows;
	QHash<int, QList<QWidget*>> m_operandRows;
	std::vector<int32_t> m_targetWords;
	std::vector<int32_t> m_operandWords;
	/** Where the target expression is (its length, then its words), -1 for nowhere */
	int m_targetAt = -1;
};
