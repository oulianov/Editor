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
#include <QPoint>
#include <QSize>
#include <QString>
#include <cstdint>
#include <vector>
#include <lcf/rpg/parameters.h>
#include <lcf/rpg/system.h>

class QRandomGenerator;

/**
 * The experience a hero needs for each level, computed like EasyRPG Player
 * (Game_Actor::CalculateExp): RPG Maker 2000 and 2003 use different
 * formulas, of three settings (base, inflation, correction).
 */
struct ExperienceCurve {
	int base = 0;
	int inflation = 0;
	int correction = 0;
	bool rpg2k_formula = false;
	int max_exp = 9999999;
	/** The highest level of a hero: 99 in RPG Maker 2003, 50 in 2000 */
	int max_level = 99;

	/** One of the three settings, as the Experience Curve window shows it */
	struct Setting {
		/** Translated, in the formula's engine: Base Value or Primary... */
		QString name;
		/** RPG Maker's range, widened to keep a value stored outside it */
		int minimum = 0;
		int maximum = 0;
	};

	/** The formula and the maximums of the project (EasyRPG settings included) */
	static ExperienceCurve of(const lcf::rpg::System& system, int base, int inflation, int correction);

	/** Total experience to reach the level, from level 1 (0 for level 1) */
	int total(int level) const;
	/** Whether the level is at the maximum experience: it cannot be
	 *  reached, RPG Maker draws it dotted */
	bool capped(int level) const;

	/** A setting: 0 base, 1 inflation, 2 correction */
	int value(int index) const;
	Setting setting(int index) const;
	/** A short text of the three settings, for the hero and class pages */
	QString summary() const;
};

/**
 * The six parameter curves of a hero or a class (lcf::rpg::Parameters), in
 * RPG Maker's order, and how its Parameter Progression window fills them
 */
namespace ParameterCurves {

enum Curve { MaxHp, MaxMp, Attack, Defense, Intelligence, Agility, Count };

/** The values of a curve, one per level */
std::vector<int16_t>& values(lcf::rpg::Parameters& parameters, int curve);
/** The lowest value of a curve: 0 for Max MP, else 1 */
int minimum(int curve);
/** The highest value of a curve: max_hp for Max HP, else 999 */
int maximum(int curve, int max_hp);
/** The colors of the curves, readable on the black background of the graphs */
QColor color(int curve);

/**
 * Fills the first levels from the value of level 1 to the value of the
 * last level; growth below 0 is faster in the early levels, above 0 in
 * the late ones (-4 to 4)
 */
void endpoints(std::vector<int16_t>& values, int levels, int first, int last, int growth);

/**
 * RPG Maker's QuickCurve: a random curve of a rank (0 Outstanding, 1 Above
 * Average, 2 Average, 3 Below Average) in the first levels, kept between
 * minimum and maximum
 */
void quickCurve(std::vector<int16_t>& values, int levels, int curve, int rank, int minimum, int maximum,
	QRandomGenerator& random);

}

/**
 * Drawing a curve on its graph, one bar per level, like the bars of RPG
 * Maker's Parameter Progression: a click or a drag sets the value under
 * the pointer for the level under it
 */
class CurveStroke
{
public:
	/** The first levels values of the vector are edited (nullptr: none) */
	void setCurve(std::vector<int16_t>* values, int levels, int minimum, int maximum);

	std::vector<int16_t>* values() const { return m_values; }
	int levels() const { return m_levels; }
	int minimum() const { return m_minimum; }
	/** At least 1: the height of the graph */
	int maximum() const { return m_maximum; }

	/**
	 * Sets the value at a point of a graph of the given size (y down); a
	 * drag continues from the last point, filling the levels a fast drag
	 * skipped. The level under the point, 0 when nothing was set.
	 */
	int editAt(const QPoint& pos, const QSize& size, bool drag);

private:
	std::vector<int16_t>* m_values = nullptr;
	int m_levels = 0;
	int m_minimum = 0;
	int m_maximum = 999;
	int m_lastLevel = 1;
	int m_lastValue = 0;
};
