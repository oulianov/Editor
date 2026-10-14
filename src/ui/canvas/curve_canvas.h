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

#include <QList>
#include <QQuickPaintedItem>
#include <QtQmlIntegration>
#include <cstdint>
#include <vector>
#include "model/experience_curve.h"

/**
 * The graph of a parameter curve of a hero or a class (Ez.CurveCanvas), on
 * black as RPG Maker draws it (CurvePainter): the filled curve of the
 * database pages, or the bars of the Parameter Progression window, where a
 * click or a drag sets the values under the pointer (editable).
 *
 *   Ez.CurveCanvas { values: entry.value("parameters/maxhp"); curve: 0; levels: 99; maximum: 9999 }
 */
class CurveCanvas : public QQuickPaintedItem
{
	Q_OBJECT
	QML_ELEMENT
	/** The value of each level, from level 1 */
	Q_PROPERTY(QList<int> values READ values WRITE setValues NOTIFY valuesChanged)
	/** Which curve (0 Max HP ... 5 Agility, as ParameterCurves): its color */
	Q_PROPERTY(int curve READ curve WRITE setCurve NOTIFY curveChanged)
	/** The highest level: the width of the graph (99, or 50 in RPG Maker 2000) */
	Q_PROPERTY(int levels READ levels WRITE setLevels NOTIFY levelsChanged)
	/** The lowest and highest values: the height of the graph is maximum */
	Q_PROPERTY(int minimum READ minimum WRITE setMinimum NOTIFY minimumChanged)
	Q_PROPERTY(int maximum READ maximum WRITE setMaximum NOTIFY maximumChanged)
	/** A bar per level (the Parameter Progression window), else the filled curve */
	Q_PROPERTY(bool bars READ bars WRITE setBars NOTIFY barsChanged)
	/** A click or a drag sets values */
	Q_PROPERTY(bool editable READ editable WRITE setEditable NOTIFY editableChanged)
	/** The level shown lighter (1 to levels), bars only */
	Q_PROPERTY(int currentLevel READ currentLevel WRITE setCurrentLevel NOTIFY currentLevelChanged)

public:
	explicit CurveCanvas(QQuickItem* parent = nullptr);

	QList<int> values() const;
	void setValues(const QList<int>& values);
	int curve() const { return m_curve; }
	void setCurve(int curve);
	int levels() const { return m_levels; }
	void setLevels(int levels);
	int minimum() const { return m_minimum; }
	void setMinimum(int minimum);
	int maximum() const { return m_maximum; }
	void setMaximum(int maximum);
	bool bars() const { return m_bars; }
	void setBars(bool bars);
	bool editable() const { return m_editable; }
	void setEditable(bool editable);
	int currentLevel() const { return m_currentLevel; }
	void setCurrentLevel(int level);

	void paint(QPainter* painter) override;

	/** What it shows, for the scripts: "level 1: 50, level 99: 999" */
	Q_INVOKABLE QString automationText() const;

signals:
	void valuesChanged();
	void curveChanged();
	void levelsChanged();
	void minimumChanged();
	void maximumChanged();
	void barsChanged();
	void editableChanged();
	void currentLevelChanged();
	/** The mouse changed values; level: the one under the pointer */
	void edited(int level);

protected:
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;

private:
	void editAt(const QPointF& pos, bool drag);

	std::vector<int16_t> m_values;
	CurveStroke m_stroke;
	int m_curve = 0;
	int m_levels = 99;
	int m_minimum = 1;
	int m_maximum = 999;
	bool m_bars = false;
	bool m_editable = false;
	int m_currentLevel = 1;
};
