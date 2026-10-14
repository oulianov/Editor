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
#include <QList>
#include <QObject>
#include <QVariantMap>
#include <QtQmlIntegration>

/**
 * The curves of heroes and classes for QML (the singleton Ez.Curves), as
 * the model computes them (ParameterCurves, ExperienceCurve):
 *
 *   Ez.Curves.quickCurve(values, 99, Ez.Curves.Attack, 2, 9999)  // Average
 *   Ez.Curves.experience(page.database, 30, 30, 0).summary
 */
class CurvesBridge : public QObject
{
	Q_OBJECT
	QML_NAMED_ELEMENT(Curves)
	QML_SINGLETON

public:
	enum Curve { MaxHp, MaxMp, Attack, Defense, Intelligence, Agility };
	Q_ENUM(Curve)

	explicit CurvesBridge(QObject* parent = nullptr);

	/** Its color on the black graphs */
	Q_INVOKABLE QColor color(int curve) const;
	/** Its lowest value: 0 for Max MP, else 1 */
	Q_INVOKABLE int minimum(int curve) const;
	/** Its highest value: max_hp for Max HP, else 999 */
	Q_INVOKABLE int maximum(int curve, int max_hp) const;
	/** values with levels values at least, the new ones as the game reads a missing one (Parameters::Setup) */
	Q_INVOKABLE QList<int> padded(const QList<int>& values, int curve, int levels) const;
	/** QuickCurve: a random curve of a rank (0 Outstanding ... 3 Below Average) */
	Q_INVOKABLE QList<int> quickCurve(const QList<int>& values, int levels, int curve, int rank, int max_hp) const;
	/** Specify Endpoints: from first to last, growth -4 (early) to 4 (late) */
	Q_INVOKABLE QList<int> endpoints(const QList<int>& values, int levels, int first, int last, int growth) const;

	/**
	 * The experience of the three settings in the game of database (a
	 * JsonView of the database: its engine and EasyRPG settings):
	 * {maxLevel, total: [level 1, level 2...], capped: [bool per level],
	 * summary, settings: [{name, minimum, maximum} x 3]}
	 */
	Q_INVOKABLE QVariantMap experience(QObject* database, int base, int inflation, int correction) const;
};
