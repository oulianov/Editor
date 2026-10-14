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

#include <QMap>
#include <QString>
#include <QStringList>
#include <lcf/rpg/event.h>
#include <lcf/rpg/eventcommand.h>
#include <vector>

class ProjectData;
class QWindow;

/**
 * The editors of the event commands, in QML: the one hand-written for the
 * command (the QML files of src/ui/commands), else the form made from its schema
 * (SchemaForm.qml, CommandSchema), else the table of its raw parameters
 * (RawCommandEditor.qml). Each opens modal (QmlHost::exec) in a
 * CommandEditorWindow over a window, on a copy of the command
 * (CommandEditSession): OK puts the copy back, Cancel leaves the command.
 *
 *   CommandEditorRegistry::Context context;
 *   context.isNew = true;
 *   if (CommandEditorRegistry::edit(window, project, page.event_commands, index, context)) ...
 */
namespace CommandEditorRegistry {
	using Code = lcf::rpg::EventCommand::Code;

	/** Where the command is */
	struct Context {
		/** A command of a troop's battle page */
		bool battle = false;
		/** Just added: cancelling removes it (the raw editor warns only for older ones) */
		bool isNew = false;
		/** The map of the event (0: a common event, a battle page) */
		int mapId = 0;
		/** The events of the map of the event, for the commands naming one */
		const QMap<int, lcf::rpg::Event*>* events = nullptr;
	};

	/**
	 * The QML type of the editor of a command: a hand-written one
	 * ("ShowMessageEditor"...), "SchemaForm" or "RawCommandEditor"; "" for
	 * a command without settings
	 */
	QString editorOf(Code code);
	/** The QML type of the hand-written editor of a command, "" for none */
	QString handWrittenEditorOf(Code code);
	/** The commands with a hand-written editor */
	QList<Code> handWritten();

	/**
	 * Edits commands[index] (the first line of its command) and the lines it
	 * owns: the next lines of a message or comment, the branches of a block.
	 *
	 * @return Whether it was changed by OK (false: cancelled, nothing changed)
	 */
	bool edit(QWindow* over, ProjectData& project, std::vector<lcf::rpg::EventCommand>& commands, int index,
		const Context& context = {});

	/** Edits a command alone (a message is its first line, a block has no branches) */
	bool edit(QWindow* over, ProjectData& project, lcf::rpg::EventCommand& command, const Context& context = {});

	/**
	 * Edits the raw parameters of a command: its code, string and numbers
	 *
	 * @param warn Says that the command has no editor of its own
	 */
	bool editRaw(QWindow* over, ProjectData& project, lcf::rpg::EventCommand& command, bool warn = false);

	/**
	 * Opens the editor of every command with settings on a new command of
	 * its kind, without showing it, and closes it (the automation's check)
	 *
	 * @return The problems: an editor not made, the warnings of QML
	 */
	QStringList checkEditors(ProjectData& project);
}
