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

#include <QPalette>
#include <QTimer>
#include <QTreeWidget>
#include <array>
#include <optional>
#include <vector>
#include "ui/event/command_colors.h"
#include <lcf/rpg/commonevent.h>
#include <lcf/rpg/trooppage.h>
#include <lcf/rpg/eventpage.h>

class ProjectData;
class QUndoStack;

class EventCommandsWidget : public QTreeWidget
{
	Q_OBJECT

public:
	explicit EventCommandsWidget(QWidget *parent = nullptr);

	void setData(ProjectData& project, lcf::rpg::CommonEvent* event);
	void setData(ProjectData& project, lcf::rpg::TroopPage* event);
	void setData(ProjectData& project, lcf::rpg::EventPage* event);

	/** Selects the first line from index that can be selected */
	void selectIndex(int index);
	/** Selects every command of the list, like Ctrl+A */
	void selectAllCommands();
	/** Moves up or down half a page, to the next command, like PgUp and PgDn */
	void pageStep(bool up);
	/** Where the rpgsync script writes this list (rpgsync locate arguments:
	 *  --map 1 --event 2 --page 1...); empty: not in a script */
	void setScriptLocation(const QStringList& location);
	/** Opens the script at the command selected (rpgsync games) */
	void openScript();

private:
	template<typename T>
	void setDataInternal(ProjectData& project, T* event);
	std::vector<lcf::rpg::EventCommand>	* m_commands = nullptr;

	void addRawEvent(QTreeWidgetItem* item);
	void editRawEvent(QTreeWidgetItem* item, int column, bool show_warning);
	void refreshList();

	/** Pick a command and insert it before line index (-1: at the end) */
	void insertCommand(int index);
	/** Opens the editor of the command; false when cancelled */
	bool editCommand(int index, bool is_new);
	/** The settings of the command at index: the next new one of its kind starts from them */
	void rememberCommand(int index);
	/** The lines of a new command: the last one accepted of its kind, else the defaults */
	std::vector<lcf::rpg::EventCommand> newCommand(lcf::rpg::EventCommand::Code code, int indent) const;
	static bool hasEditor(lcf::rpg::EventCommand::Code code);
	/** Cut, Copy and Delete act on these lines (first == second: nothing) */
	void copyLines(std::pair<int, int> range, bool cut);
	void deleteLines(std::pair<int, int> range);
	void pasteCommands(int index);
	int indexOf(QTreeWidgetItem* item) const;
	/** Whether the keys stop on the line: the first line of a command, not
	 *  the next lines of a message nor ": Else", ": End"... */
	bool isCommandLine(QTreeWidgetItem* item) const;
	/** The first line of the command at index (a message starts before its next lines) */
	int commandStart(int index) const;
	/** The lines of the command of item, with its block */
	std::pair<int, int> commandRange(QTreeWidgetItem* item) const;
	void highlightCommand(QTreeWidgetItem* current);
	/** The lines from first to last (excluded) grown to whole commands and blocks */
	std::pair<int, int> wholeCommands(int first, int last) const;
	/** Several lines are selected (Shift+click, Shift+arrows, Select All) */
	bool multiSelection() const;
	/** The lines Cut, Copy and Delete act on, for the line item */
	std::pair<int, int> rangeFor(QTreeWidgetItem* item) const;
	/** Where Insert and Paste put commands, for the line item */
	int insertionIndexFor(QTreeWidgetItem* item) const;

	ProjectData* m_project = nullptr;
	bool m_battle = false;
	QStringList m_scriptLocation;
	static std::vector<lcf::rpg::EventCommand> s_clipboard;
	/** Lines drawn selected: the commands from the anchor to the current line */
	std::pair<int, int> m_highlight = {-1, -1};
	/** The lines Cut, Copy and Delete act on (first == second: none) */
	std::pair<int, int> m_selection = {-1, -1};
	/** The line where a selection with Shift started */
	int m_anchor = -1;
	/** While the current line moves with Shift: the anchor stays */
	bool m_extending = false;

	/** Changes of the list (inserting, editing, pasting...). Every change of
	 *  the list ends with refreshList(), which records it here. */
	QUndoStack* m_undoStack = nullptr;
	/** The list as last shown, to know what a change replaced */
	std::optional<std::vector<lcf::rpg::EventCommand>> m_shown;
	/** Shows the list as it was (undo / redo) */
	void showCommands(const std::vector<lcf::rpg::EventCommand>& commands);

	/** Reads the color and stripe options and prepares what lines are drawn with */
	void updateLook();
	/** A click on the line already selected edits its command, once no
	 *  double-click (insert) follows */
	QTimer m_clickEdit;
	bool m_pressOnCurrent = false;
	bool m_colors = true;
	bool m_stripes = true;
	QColor m_stripe;
	/** The palette of a line by the kind of its command (its text color),
	 *  made once: drawing a line only copies one */
	std::array<QPalette, static_cast<size_t>(CommandColors::Kind::Count)> m_linePalettes;

protected:
	void keyPressEvent(QKeyEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void mouseDoubleClickEvent(QMouseEvent* event) override;
	QModelIndex moveCursor(CursorAction action, Qt::KeyboardModifiers modifiers) override;
	void drawRow(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
	void drawBranches(QPainter* painter, const QRect& rect, const QModelIndex& index) const override;
	void changeEvent(QEvent* event) override;
	void resizeEvent(QResizeEvent* event) override;

private slots:
	void editEvent(QTreeWidgetItem* item, int column);
	void showContextMenu(const QPoint& pos);
};
