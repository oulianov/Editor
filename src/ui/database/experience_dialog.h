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

#include <QDialog>
#include <array>
#include <cstdint>

class ProjectData;
class QSpinBox;
class QTableWidget;

// The experience a hero needs for each level, computed like EasyRPG Player
// (Game_Actor::CalculateExp): RPG Maker 2000 and 2003 use different formulas
struct ExperienceCurve {
	int base = 0;
	int inflation = 0;
	int correction = 0;
	bool rpg2k_formula = false;
	int max_exp = 9999999;

	// The formula and the maximum of the project (EasyRPG settings included)
	static ExperienceCurve of(ProjectData& project, int base, int inflation, int correction);

	// Total experience to reach the level, from level 1 (0 for level 1)
	int total(int level) const;
};

// RPG Maker's Experience Curve window: the three settings and the table of
// experience for every level
class ExperienceDialog : public QDialog
{
	Q_OBJECT

public:
	ExperienceDialog(ProjectData& project, int32_t base, int32_t inflation, int32_t correction,
		QWidget* parent = nullptr);

	int32_t base() const;
	int32_t inflation() const;
	int32_t correction() const;

	// A short text of the three settings, for the hero and class pages
	static QString summary(ProjectData& project, int32_t base, int32_t inflation, int32_t correction);

	// Opens the dialog; true and the values changed when accepted
	static bool edit(QWidget* parent, ProjectData& project, int32_t& base, int32_t& inflation, int32_t& correction);

private:
	void refresh();

	ProjectData& m_project;
	int m_maxLevel = 99;
	std::array<QSpinBox*, 3> m_spins = {};
	QTableWidget* m_total = nullptr;
	QTableWidget* m_perLevel = nullptr;
};
