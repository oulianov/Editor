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

#include <QAbstractListModel>
#include <QString>
#include <optional>
#include <utility>
#include <vector>
#include <lcf/rpg/eventcommand.h>

class QUndoStack;

/**
 * The command list of an event page, a common event or a battle page (its
 * std::vector<lcf::rpg::EventCommand>) as a list model: one row per line,
 * as RPG Maker shows them, then the empty line ending the page (RPG Maker
 * does not store it, it only shows it: commands are inserted there).
 *
 * Every change of the list goes through it: it keeps the indents of the
 * lines as their blocks make them and records the change for undo. The
 * editors of the commands are the interface's: they change the lines in
 * place, then commandsEdited() shows and records it.
 */
class EventCommandListModel : public QAbstractListModel
{
	Q_OBJECT

public:
	using Code = lcf::rpg::EventCommand::Code;
	using Commands = std::vector<lcf::rpg::EventCommand>;
	/** The lines from first to second, excluded (first == second: none) */
	using Range = std::pair<int, int>;

	enum Role {
		/** The line with its indent, as Copy as text writes it: "<>"
		 *  before a command, ":" before the other lines of a command.
		 *  (DisplayRole: the line as the list shows it, without its indent) */
		TextRole = Qt::UserRole + 1,
		IndentRole,
		CodeRole,
		/** CommandColors::Kind of the line, for its color */
		ColorKindRole,
		/** The keys stop on the line: the first line of a command, not the
		 *  next lines of a message nor ": Else", ": End"... */
		CommandStartRole,
		/** A line not selected alone: the structure of a block (Else,
		 *  End, a choice option...), its block goes with its first line */
		StructuralRole,
		/** The empty line ending the page */
		PageEndRole,
	};

	explicit EventCommandListModel(QObject* parent = nullptr);
	~EventCommandListModel() override;

	/** The list shown, nullptr for none: its undo history starts over */
	void setCommands(Commands* commands);
	Commands* commands() const;

	// QAbstractItemModel
	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	Qt::ItemFlags flags(const QModelIndex& index) const override;
	QHash<int, QByteArray> roleNames() const override;

	/*
	 * The lines
	 */

	/** The command opens a window with its settings (the others have none) */
	static bool hasEditor(Code code);
	/** The line as the list shows it, without its indent: "<>" for an empty
	 *  line, ": text" for the next lines of a message or a comment */
	QString lineText(int index) const;
	/** A line selected alone (StructuralRole: not), the page end too */
	bool isSelectable(int index) const;
	/** CommandStartRole */
	bool isCommandLine(int index) const;
	/** The first line of the command at index (a message starts before its next lines) */
	int commandStart(int index) const;
	/** The lines of the command at index, with its block (first == second:
	 *  a line of the structure of a block, not removed alone) */
	Range commandRange(int index) const;
	/** The lines from first to last (excluded) grown to whole commands and blocks */
	Range wholeCommands(int first, int last) const;

	/*
	 * The changes: each is one undo step
	 */

	/** Inserts lines before index (out of the list: at its end); where they start */
	int insertLines(int index, const Commands& lines);
	/** Removes the lines of range (none when empty) */
	void removeLines(Range range);
	/** Replaces the lines of range with lines (an edit made on a copy) */
	void replaceLines(Range range, const Commands& lines);
	/** The editors changed lines in place: shows and records it */
	void commandsEdited();
	/** Undo and redo of the changes */
	QUndoStack* undoStack() const;

	/*
	 * Copy and paste, shared by every list
	 */

	/** Copy (Cut: and removes them): the lines go to the clipboard of the
	 *  lists, and as text to the one of the system, for text editors */
	void copyLines(Range range, bool cut = false);
	/** Pastes the lines copied before index; where they start, -1 when none */
	int paste(int index);
	static bool canPaste();
	/** Commands as text: the lines of the list, as RPG Maker shows them */
	static QString commandsText(const Commands& commands);

	/*
	 * The settings of the last command of each kind (QSettings, by game)
	 */

	/** The lines of a new command: the last one accepted of its kind in the
	 *  game of project_dir, else the defaults */
	static Commands newCommand(const QString& project_dir, Code code, int indent);
	/** The settings of the command at index: the next new one of its kind starts from them */
	void rememberCommand(int index, const QString& project_dir) const;

signals:
	/** The lines changed, any change (after the signals of the model): a
	 *  view drawing them all draws them again */
	void commandsChanged();

private:
	int lineCount() const;
	/** Every change ends here: indents, the undo step, commandsChanged */
	void finishChange();
	/** The indents as the blocks make them (RPG Maker reads them) */
	void updateIndents();
	/** Shows the list as it was (undo / redo) */
	void restore(const Commands& commands);

	Commands* m_commands = nullptr;
	QUndoStack* m_undoStack = nullptr;
	/** The list as last recorded, to know what a change replaced */
	std::optional<Commands> m_recorded;
};
