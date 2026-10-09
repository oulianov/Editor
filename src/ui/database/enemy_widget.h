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
#include <optional>
#include <lcf/rpg/enemy.h>

class ProjectData;
class QGraphicsPixmapItem;
class QListWidget;

namespace Ui {
class EnemyWidget;
}

class EnemyWidget : public QWidget
{
	Q_OBJECT

public:
	using value_type = lcf::rpg::Enemy;

	explicit EnemyWidget(ProjectData& project, QWidget *parent = nullptr);
	~EnemyWidget() override;

	void setData(lcf::rpg::Enemy* enemy);

protected:
	bool eventFilter(QObject* watched, QEvent* event) override;

private:
	/** The items, attributes and conditions, listed again when they change */
	void fillLists();
	void chooseGraphic();
	void refreshGraphic();
	void fitGraphic();

	void refreshActions(int select = -1);
	/** Edits the action of a line of the list (the empty last line adds one) */
	void editAction(int index);
	void copyAction(bool cut);
	void pasteAction();
	void deleteAction();
	int currentActionIndex() const;

	void refreshRanks(QListWidget* list, const std::vector<uint8_t>& ranks);
	/** Next resistance rank (A to E) of a line, like a click in RPG Maker */
	void cycleRank(QListWidget* list, std::vector<uint8_t>& ranks, int row);

	Ui::EnemyWidget *ui;
	ProjectData& m_project;
	lcf::rpg::Enemy m_dummy;
	lcf::rpg::Enemy* m_current = nullptr;
	QGraphicsPixmapItem* m_graphicItem = nullptr;

	static std::optional<lcf::rpg::EnemyAction> s_clipboard;
};
