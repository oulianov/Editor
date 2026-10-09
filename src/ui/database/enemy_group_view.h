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

#include <QGraphicsView>
#include <lcf/rpg/troop.h>

class ProjectData;
class QGraphicsPixmapItem;

/**
 * The battle screen of a monster group: its members on the battle
 * background (320x240), moved with the mouse. The context menu of a member
 * hides it at the start of the battle ("Invisible").
 */
class EnemyGroupView : public QGraphicsView {
	Q_OBJECT
public:
	explicit EnemyGroupView(QWidget* parent = nullptr);

	/** Shows the members of troop (nullptr: none) */
	void setData(const ProjectData* project, lcf::rpg::Troop* troop);
	/** Draws the members again, after the list of members changed */
	void refresh();
	void setBackdrop(const QPixmap& backdrop);

	/** Index of the selected member, -1 when none */
	int selectedMember() const;
	void selectMember(int index);

protected:
	void resizeEvent(QResizeEvent* event) override;
	void contextMenuEvent(QContextMenuEvent* event) override;

private:
	const ProjectData* m_project = nullptr;
	lcf::rpg::Troop* m_troop = nullptr;
	QGraphicsPixmapItem* m_backdrop = nullptr;
};
