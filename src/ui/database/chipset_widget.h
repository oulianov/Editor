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
#include <QPixmap>
#include <lcf/rpg/chipset.h>

class ProjectData;
class QButtonGroup;
class QGraphicsScene;
class QGraphicsView;
class QLineEdit;
class QTabBar;
class QGroupBox;
class RpgComboBoxBase;

/**
 * The Chipset page of the database, like RPG Maker's: the graphic, then the
 * tiles of the lower or upper layer with their terrain, their passability
 * (passable, blocked, above the hero), the four directions that can be
 * walked through and the counter flag; and the water animation.
 */
class ChipsetWidget : public QWidget
{
	Q_OBJECT

public:
	using value_type = lcf::rpg::Chipset;

	explicit ChipsetWidget(ProjectData& project, QWidget *parent = nullptr);

	void setData(lcf::rpg::Chipset* chipset);

	enum class Mode { Terrain, Passability, Directions, Counter };

protected:
	bool eventFilter(QObject* watched, QEvent* event) override;

private:
	bool lowerLayer() const;
	int tileCount() const;
	/** The source rectangle of a tile in the chipset image, in tiles */
	static QPoint tilePosition(bool lower, int index);
	void loadGraphic();
	void redraw();
	void tileClicked(const QPointF& pos, bool right_button);

	ProjectData& m_project;
	lcf::rpg::Chipset* m_chipset = nullptr;
	QPixmap m_graphic;

	QLineEdit* m_name = nullptr;
	QLineEdit* m_file = nullptr;
	QTabBar* m_layer = nullptr;
	QButtonGroup* m_mode = nullptr;
	RpgComboBoxBase* m_terrain = nullptr;
	QGraphicsScene* m_scene = nullptr;
	QGraphicsView* m_view = nullptr;
	QButtonGroup* m_sequence = nullptr;
	QButtonGroup* m_speed = nullptr;
	QGroupBox* m_water = nullptr;
};
