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

#include "experience_curve.h"
#include <QCoreApplication>
#include <QRandomGenerator>
#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace {
struct SettingInfo {
	const char* name2k;
	const char* name2k3;
	int min2k, max2k;
	int min2k3, max2k3;
};
// RPG Maker 2000: 10-50, 10-50 and 0-40. RPG Maker 2003: its help gives
// 1-2000 for Primary; the same maximum is used for the two others.
// The texts are the Experience Curve window's
const std::array<SettingInfo, 3> kSettings = {{
	{QT_TRANSLATE_NOOP("ExperienceDialog", "Base Value"), QT_TRANSLATE_NOOP("ExperienceDialog", "Primary"), 10, 50, 1, 2000},
	{QT_TRANSLATE_NOOP("ExperienceDialog", "Extra Value"), QT_TRANSLATE_NOOP("ExperienceDialog", "Secondary"), 10, 50, 0, 2000},
	{QT_TRANSLATE_NOOP("ExperienceDialog", "Correction"), QT_TRANSLATE_NOOP("ExperienceDialog", "Tertiary"), 0, 40, 0, 2000},
}};
}

ExperienceCurve ExperienceCurve::of(const lcf::rpg::System& system, int base, int inflation, int correction) {
	const bool is2k3 = system.ldb_id == 2003;
	ExperienceCurve curve;
	curve.base = base;
	curve.inflation = inflation;
	curve.correction = correction;
	// EasyRPG can give a game the formula of the other engine
	curve.rpg2k_formula = system.easyrpg_alternative_exp > 0 ? system.easyrpg_alternative_exp == 1 : !is2k3;
	curve.max_exp = system.easyrpg_max_exp == -1 ? (is2k3 ? 9999999 : 999999) : system.easyrpg_max_exp;
	curve.max_level = is2k3 ? 99 : 50;
	return curve;
}

int ExperienceCurve::total(int level) const {
	// Player's CalculateExp(n) is the total to go from level 1 to level n + 1
	const int n = level - 1;
	if (n < 1) {
		return 0;
	}
	// 64 bits: large settings must reach the maximum, not wrap around
	int64_t result = 0;
	if (rpg2k_formula) {
		double b = base;
		double rate = 1.5 + inflation * 0.01;
		for (int i = n; i >= 1 && result < max_exp; --i) {
			result += static_cast<int64_t>(correction + b);
			b = b * rate;
			rate = ((n + 1) * 0.002 + 0.8) * (rate - 1) + 1;
		}
	} else {
		for (int i = 1; i <= n && result < max_exp; ++i) {
			result += base + static_cast<int64_t>(i) * inflation + correction;
		}
	}
	return static_cast<int>(std::min<int64_t>(result, max_exp));
}

bool ExperienceCurve::capped(int level) const {
	return level > 1 && total(level) >= max_exp;
}

int ExperienceCurve::value(int index) const {
	return index == 0 ? base : index == 1 ? inflation : correction;
}

ExperienceCurve::Setting ExperienceCurve::setting(int index) const {
	const auto& s = kSettings[static_cast<size_t>(std::clamp(index, 0, 2))];
	const int stored = value(index);
	return {QCoreApplication::translate("ExperienceDialog", rpg2k_formula ? s.name2k : s.name2k3),
		std::min(rpg2k_formula ? s.min2k : s.min2k3, stored), std::max(rpg2k_formula ? s.max2k : s.max2k3, stored)};
}

QString ExperienceCurve::summary() const {
	return QCoreApplication::translate("ExperienceDialog", "%1: %2, %3: %4, %5: %6 (level %7: %8)")
		.arg(setting(0).name).arg(base)
		.arg(setting(1).name).arg(inflation)
		.arg(setting(2).name).arg(correction)
		.arg(max_level).arg(total(max_level));
}

std::vector<int16_t>& ParameterCurves::values(lcf::rpg::Parameters& parameters, int curve) {
	switch (curve) {
		case MaxHp: return parameters.maxhp;
		case MaxMp: return parameters.maxsp;
		case Attack: return parameters.attack;
		case Defense: return parameters.defense;
		case Intelligence: return parameters.spirit;
		default: return parameters.agility;
	}
}

int ParameterCurves::minimum(int curve) {
	return curve == MaxMp ? 0 : 1;
}

int ParameterCurves::maximum(int curve, int max_hp) {
	return curve == MaxHp ? max_hp : 999;
}

QColor ParameterCurves::color(int curve) {
	static const std::array<QColor, Count> colors = {
		QColor(224, 48, 48), QColor(208, 64, 208), QColor(232, 200, 40),
		QColor(64, 192, 64), QColor(96, 112, 255), QColor(48, 192, 224)};
	return colors[static_cast<size_t>(std::clamp(curve, 0, Count - 1))];
}

void ParameterCurves::endpoints(std::vector<int16_t>& values, int levels, int first, int last, int growth) {
	// RPG Maker does not document its formula: a power curve between the two
	// values, 2^(growth / 2) as the exponent (0.25 for Early to 4 for Late)
	const double exponent = std::pow(2.0, growth / 2.0);
	levels = std::min(levels, static_cast<int>(values.size()));
	for (int i = 0; i < levels; ++i) {
		const double t = levels > 1 ? static_cast<double>(i) / (levels - 1) : 0.0;
		values[static_cast<size_t>(i)] = static_cast<int16_t>(std::lround(first + (last - first) * std::pow(t, exponent)));
	}
}

void ParameterCurves::quickCurve(std::vector<int16_t>& values, int levels, int curve, int rank, int minimum, int maximum,
	QRandomGenerator& random) {
	// RPG Maker makes a random curve of the chosen rank and does not document
	// how: here the values of an average hero, scaled by the rank and varied a
	// little, joined by a slightly accelerating curve
	const bool is2k3 = levels > 50;
	// first and last level of an average hero, per parameter
	static const std::array<std::pair<int, int>, Count> average2k3 = {{{60, 3000}, {25, 600}, {20, 300}, {20, 300}, {20, 300}, {20, 300}}};
	static const std::array<std::pair<int, int>, Count> average2k = {{{40, 600}, {20, 300}, {15, 200}, {15, 200}, {15, 200}, {15, 200}}};
	static const std::array<double, 4> scale = {1.5, 1.25, 1.0, 0.75};
	const auto& [first, last] = (is2k3 ? average2k3 : average2k)[static_cast<size_t>(std::clamp(curve, 0, Count - 1))];
	const double rank_scale = scale[static_cast<size_t>(std::clamp(rank, 0, 3))];
	auto vary = [&](int value) {
		return static_cast<int>(std::lround(value * rank_scale * (0.95 + random.bounded(0.1))));
	};
	const int start = std::clamp(vary(first), minimum, maximum);
	const int end = std::clamp(vary(last), start, maximum);
	endpoints(values, levels, start, end, 1);
}

void CurveStroke::setCurve(std::vector<int16_t>* values, int levels, int minimum, int maximum) {
	m_values = values;
	m_levels = std::min(levels, values ? static_cast<int>(values->size()) : 0);
	m_minimum = minimum;
	m_maximum = std::max(maximum, 1);
}

int CurveStroke::editAt(const QPoint& pos, const QSize& size, bool drag) {
	if (!m_values || m_levels <= 0 || size.width() <= 0 || size.height() <= 0) {
		return 0;
	}
	const int level = std::clamp(pos.x() * m_levels / size.width() + 1, 1, m_levels);
	const int value = std::clamp(static_cast<int>(std::lround(static_cast<double>(size.height() - pos.y()) * m_maximum / size.height())),
		m_minimum, m_maximum);
	// a fast drag skips levels: fill them on the line from the last point
	const int from = drag ? std::clamp(m_lastLevel, 1, m_levels) : level;
	const int first = std::min(from, level);
	const int last = std::max(from, level);
	for (int l = first; l <= last; ++l) {
		int v = value;
		if (last > first) {
			const double t = static_cast<double>(l - from) / (level - from);
			v = static_cast<int>(std::lround(m_lastValue + (value - m_lastValue) * t));
		}
		(*m_values)[static_cast<size_t>(l) - 1] = static_cast<int16_t>(v);
	}
	m_lastLevel = level;
	m_lastValue = value;
	return level;
}
