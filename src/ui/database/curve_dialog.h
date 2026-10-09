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

#include <QColor>
#include <QDialog>
#include <array>
#include <lcf/rpg/parameters.h>

class CurveEditor;
class QGroupBox;
class QSpinBox;
class QTabWidget;
class QTableWidget;

// Fills the first levels of a parameter curve
namespace CurveGenerator {
	// From the value of level 1 to the value of the last level; growth below 0
	// is faster in the early levels, above 0 in the late ones (-4 to 4)
	void endpoints(std::vector<int16_t>& values, int levels, int first, int last, int growth);
}

// RPG Maker's Parameter Progression window: the six curves, a graph to draw
// them, the values of each level, QuickCurve and Specify Endpoints
class CurveDialog : public QDialog
{
	Q_OBJECT

public:
	// tab: the curve shown first (0 Max HP ... 5 Agility)
	CurveDialog(const lcf::rpg::Parameters& parameters, int max_level, int max_hp, int tab, QWidget* parent = nullptr);

	const lcf::rpg::Parameters& parameters() const { return m_parameters; }

	// The colors of the six curves, also used by the small graphs of the pages
	static QColor color(int curve);

private:
	struct Curve {
		QString name;
		std::vector<int16_t>* values;
		int minimum;
		int maximum;
	};

	void showCurve(int index);
	void setLevel(int level);
	void refreshValues();
	void quickCurve(int rank);
	void specifyEndpoints();

	lcf::rpg::Parameters m_parameters;
	std::array<Curve, 6> m_curves;
	int m_current = 0;
	int m_maxLevel = 99;
	bool m_updating = false;

	QTabWidget* m_tabs = nullptr;
	QWidget* m_content = nullptr;
	CurveEditor* m_graph = nullptr;
	QSpinBox* m_level = nullptr;
	QGroupBox* m_valueGroup = nullptr;
	QSpinBox* m_value = nullptr;
	QTableWidget* m_table = nullptr;
};
