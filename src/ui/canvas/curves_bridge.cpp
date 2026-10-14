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

#include "curves_bridge.h"
#include "model/experience_curve.h"
#include "qmlbinding/json_t.h"
#include "qmlbinding/json_view.h"

#include <QRandomGenerator>
#include <algorithm>
#include <vector>

namespace {

std::vector<int16_t> toVector(const QList<int>& values) {
	std::vector<int16_t> vector;
	vector.reserve(static_cast<size_t>(values.size()));
	for (int value : values) {
		vector.push_back(static_cast<int16_t>(std::clamp(value, -32768, 32767)));
	}
	return vector;
}

QList<int> toList(const std::vector<int16_t>& values) {
	return QList<int>(values.begin(), values.end());
}

/** The system of the database a view shows (the copy of a database window) */
const lcf::rpg::System* systemOf(QObject* database) {
	auto* view = qobject_cast<JsonView*>(database);
	auto* json = view ? dynamic_cast<JsonT<lcf::rpg::Database>*>(view->json()) : nullptr;
	return json && json->data() ? &json->data()->system : nullptr;
}

} // namespace

CurvesBridge::CurvesBridge(QObject* parent) : QObject(parent) {
}

QColor CurvesBridge::color(int curve) const {
	return ParameterCurves::color(curve);
}

int CurvesBridge::minimum(int curve) const {
	return ParameterCurves::minimum(curve);
}

int CurvesBridge::maximum(int curve, int max_hp) const {
	return ParameterCurves::maximum(curve, max_hp);
}

QList<int> CurvesBridge::padded(const QList<int>& values, int curve, int levels) const {
	QList<int> result = values;
	// as lcf::rpg::Parameters::Setup: a level without a value reads as 0
	while (result.size() < levels) {
		result.append(curve == ParameterCurves::MaxMp ? 0 : 1);
	}
	return result;
}

QList<int> CurvesBridge::quickCurve(const QList<int>& values, int levels, int curve, int rank, int max_hp) const {
	auto vector = toVector(padded(values, curve, levels));
	ParameterCurves::quickCurve(vector, levels, curve, rank, ParameterCurves::minimum(curve),
		ParameterCurves::maximum(curve, max_hp), *QRandomGenerator::global());
	return toList(vector);
}

QList<int> CurvesBridge::endpoints(const QList<int>& values, int levels, int first, int last, int growth) const {
	auto vector = toVector(values);
	ParameterCurves::endpoints(vector, levels, first, last, growth);
	return toList(vector);
}

QVariantMap CurvesBridge::experience(QObject* database, int base, int inflation, int correction) const {
	const lcf::rpg::System* system = systemOf(database);
	lcf::rpg::System fallback;
	const auto curve = ExperienceCurve::of(system ? *system : fallback, base, inflation, correction);
	QVariantList total;
	QVariantList capped;
	for (int level = 1; level <= curve.max_level; ++level) {
		total.append(curve.total(level));
		capped.append(curve.capped(level));
	}
	QVariantList settings;
	for (int i = 0; i < 3; ++i) {
		const auto setting = curve.setting(i);
		settings.append(QVariantMap{{QStringLiteral("name"), setting.name},
			{QStringLiteral("minimum"), setting.minimum}, {QStringLiteral("maximum"), setting.maximum}});
	}
	return {
		{QStringLiteral("maxLevel"), curve.max_level},
		{QStringLiteral("total"), total},
		{QStringLiteral("capped"), capped},
		{QStringLiteral("summary"), curve.summary()},
		{QStringLiteral("settings"), settings},
	};
}
