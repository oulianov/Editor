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

#include <QWidget>
#include <array>
#include <lcf/rpg/class.h>
#include "class_editors.h"
#include "ui/viewer/battle_animation_graphics_item.h"
#include "ui/viewer/stat_curve_graphics_item.h"

class ProjectData;
class QGraphicsView;

namespace Ui {
class ClassWidget;
}

class ClassWidget : public QWidget
{
	Q_OBJECT

public:
	using value_type = lcf::rpg::Class;

	explicit ClassWidget(ProjectData& project, QWidget *parent = nullptr);
	~ClassWidget() override;

	void setData(lcf::rpg::Class* cls);

protected:
	void resizeEvent(QResizeEvent* event) override;

private:
	void refreshBattler();
	void refreshCurves();
	void refreshSkills();
	void refreshExperience();
	void editCurves(int tab);
	ClassEditors::CommandCombos commandCombos() const;
	std::array<QGraphicsView*, 6> curveViews() const;

	Ui::ClassWidget *ui;

	BattleAnimationItem* m_battlerItem = nullptr;
	std::array<CurveItem*, 6> m_curves = {};

	lcf::rpg::Class m_dummy;
	lcf::rpg::Class* m_current = nullptr;
	ProjectData& m_project;
};
