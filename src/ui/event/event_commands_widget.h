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

#include <QTreeWidget>
#include <lcf/rpg/commonevent.h>
#include <lcf/rpg/trooppage.h>
#include <lcf/rpg/eventpage.h>

class ProjectData;

class EventCommandsWidget : public QTreeWidget
{
	Q_OBJECT

public:
	explicit EventCommandsWidget(QWidget *parent = nullptr);

	void setData(ProjectData& project, lcf::rpg::CommonEvent* event);
	void setData(ProjectData& project, lcf::rpg::TroopPage* event);
	void setData(ProjectData& project, lcf::rpg::EventPage* event);

private:
	template<typename T>
	void setDataInternal(ProjectData& project, T* event);
	std::vector<lcf::rpg::EventCommand>	* m_commands = nullptr;

	void addRawEvent(QTreeWidgetItem* item);
	void editRawEvent(QTreeWidgetItem* item, int column, bool show_warning);
	void deleteEvent(QTreeWidgetItem* item);
	void refreshList();

	/** Pick a command and insert it before line index (-1: at the end) */
	void insertCommand(int index);
	/** Opens the editor of the command; false when cancelled */
	bool editCommand(int index, bool is_new);
	void copyCommands(QTreeWidgetItem* item, bool cut);
	void pasteCommands(QTreeWidgetItem* item);
	int indexOf(QTreeWidgetItem* item) const;
	/** The first line of the command at index (a message starts before its next lines) */
	int commandStart(int index) const;
	/** The lines of the command of item, with its block */
	std::pair<int, int> commandRange(QTreeWidgetItem* item) const;
	void highlightCommand(QTreeWidgetItem* current);
	void selectIndex(int index);

	ProjectData* m_project = nullptr;
	bool m_battle = false;
	static std::vector<lcf::rpg::EventCommand> s_clipboard;
	/** Lines drawn selected: the command of the current line */
	std::pair<int, int> m_highlight = {-1, -1};

protected:
	void keyPressEvent(QKeyEvent* event) override;
	void drawRow(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

private slots:
	void editEvent(QTreeWidgetItem* item, int column);
	void showContextMenu(const QPoint& pos);
};
