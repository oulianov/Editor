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
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QVariantList>
#include <QtQmlIntegration>
#include <vector>
#include <lcf/rpg/eventcommand.h>
#include "model/event_command_list_model.h"
#include "model/event_editor_controller.h"
#include "qmlbinding/database_session.h"
#include "qmlbinding/project_data_gadget.h"

class JsonView;
class ProjectData;
class QQuickItem;

/**
 * The command list of an event page, a common event or a battle page for
 * CommandList.qml: its lines (an EventCommandListModel) and what the list
 * does with them, the windows it opens (the commands to insert, their
 * editors: CommandEditing) modal over the window of the list.
 *
 * The changes return the line to select after them (-1: none); the list
 * keeps its selection itself.
 */
class CommandListEditor : public QObject
{
	Q_OBJECT
	QML_ELEMENT
	/** The lines: one row per line, then the end of the page */
	Q_PROPERTY(EventCommandListModel* model READ model CONSTANT)
	/** The project the editors show (Ez.ProjectData) */
	Q_PROPERTY(ProjectDataGadget* projectData READ projectData WRITE setProjectData NOTIFY projectChanged)
	/** A database window's copy of the project, shown instead (the battle pages of its troops) */
	Q_PROPERTY(DatabaseSession* session READ session WRITE setSession NOTIFY projectChanged)
	/** A battle page: the battle commands are offered */
	Q_PROPERTY(bool battle READ battle WRITE setBattle NOTIFY battleChanged)
	/** Where rpgsync writes the list (--map 1 --event 2 --page 1), empty: in no script */
	Q_PROPERTY(QStringList scriptLocation READ scriptLocation WRITE setScriptLocation NOTIFY scriptLocationChanged)
	/** The lines of commands (the end of the page not counted) */
	Q_PROPERTY(int lineCount READ lineCount NOTIFY linesChanged)
	Q_PROPERTY(bool canUndo READ canUndo NOTIFY undoChanged)
	Q_PROPERTY(bool canRedo READ canRedo NOTIFY undoChanged)

public:
	explicit CommandListEditor(QObject* parent = nullptr);
	~CommandListEditor() override;

	EventCommandListModel* model() const;
	ProjectDataGadget* projectData() const;
	void setProjectData(ProjectDataGadget* gadget);
	DatabaseSession* session() const;
	void setSession(DatabaseSession* session);
	/** The project of the editors: the session's copy, else the project shown */
	ProjectData* project() const;
	bool battle() const;
	void setBattle(bool battle);
	QStringList scriptLocation() const;
	void setScriptLocation(const QStringList& location);
	int lineCount() const;
	bool canUndo() const;
	bool canRedo() const;

	/** The lines shown, nullptr for none: the undo history starts over */
	void setCommands(std::vector<lcf::rpg::EventCommand>* commands);
	/** The commands of a page of the event of an event window, and its
	 *  script; their place changes with the pages: shown again then */
	Q_INVOKABLE void showEventPage(EventEditorController* controller, int page);
	/** The commands of a battle page of a troop, by their index (the
	 *  session's copy, else the project's) */
	Q_INVOKABLE void showTroopPage(int troop, int page);
	/** The commands of a common event of a database window's page (its
	 *  view, "/commonevents/2"): edited in the window's copy */
	Q_INVOKABLE void showCommonEvent(JsonView* entry);

	/*
	 * The lines (EventCommandListModel)
	 */

	/** A line selected alone: not the structure of a block (Else, End...) */
	Q_INVOKABLE bool isSelectable(int line) const;
	/** The keys stop on it: the first line of a command */
	Q_INVOKABLE bool isCommandLine(int line) const;
	/** The first line of the command at line (a message starts before its next lines) */
	Q_INVOKABLE int commandStart(int line) const;
	/** The lines of the command at line with its block: [first, end) */
	Q_INVOKABLE QVariantList commandRange(int line) const;
	/** The lines from first to end grown to whole commands and blocks: [first, end) */
	Q_INVOKABLE QVariantList wholeCommands(int first, int end) const;
	/** The command at line has a window (Edit) */
	Q_INVOKABLE bool hasEditor(int line) const;
	/** An empty line ("<>" closing a block, or the end of the page): commands are inserted there */
	Q_INVOKABLE bool isEmptyLine(int line) const;
	/** The code of the line, 0 for the end of the page */
	Q_INVOKABLE int code(int line) const;

	/*
	 * The changes: the line to select after them, -1 when nothing changed.
	 * The windows they open are over the window of from
	 */

	/** Picks a command (CommandPalette.qml) and inserts it before index, in
	 *  its window: Cancel takes it back. As in RPG Maker, the line selected
	 *  after is the one it went before */
	Q_INVOKABLE int insertCommand(int index, QQuickItem* from);
	/** The window of the command at line (an empty line: inserts there) */
	Q_INVOKABLE int editCommand(int line, QQuickItem* from);
	/** A command by its code and parameters, after the command at line */
	Q_INVOKABLE int addRaw(int line, QQuickItem* from);
	/** The parameters of the command at line */
	Q_INVOKABLE int editRaw(int line, QQuickItem* from);
	Q_INVOKABLE int removeLines(int first, int end);
	/** Copy (cut: and remove): to the clipboard of the lists, and as text to the system's */
	Q_INVOKABLE int copyLines(int first, int end, bool cut);
	/** The lines copied before index */
	Q_INVOKABLE int paste(int index);
	Q_INVOKABLE static bool canPaste();
	Q_INVOKABLE void undo();
	Q_INVOKABLE void redo();

	/** Why the script cannot open (no rpgsync...), empty when it can */
	Q_INVOKABLE QString scriptUnavailableReason() const;
	/** Opens the script at the command of line (rpgsync games) */
	Q_INVOKABLE void openScript(int line, QQuickItem* from);

	/*
	 * The look
	 */

	/** The colors of the lines by CommandColors::Kind for a base and a text
	 *  color (light or dark); without colors the commands keep the text's,
	 *  the empty lines are dimmed (disabled) */
	Q_INVOKABLE QVariantList lineColors(const QColor& base, const QColor& text, const QColor& disabled, bool colored) const;
	/** The tint of every other line */
	Q_INVOKABLE QColor stripeColor(const QColor& base) const;
	/** What starts the first line of a message or a comment ("Message",
	 *  "Comment") for the code of their next lines, whose text is under the
	 *  first one's; empty for other codes */
	Q_INVOKABLE QString firstLineLabel(int code) const;

signals:
	void projectChanged();
	void battleChanged();
	void scriptLocationChanged();
	/** Any change of the lines (after the model's signals) */
	void linesChanged();
	void undoChanged();

private:
	std::vector<lcf::rpg::EventCommand>* commands() const;
	/** Where the game keeps the last command of each kind */
	QString projectDir() const;
	bool validLine(int line) const;
	/** The commands of the page of an event window, and its script */
	void followEventPage();

	EventCommandListModel* m_model = nullptr;
	QPointer<ProjectDataGadget> m_gadget;
	QPointer<DatabaseSession> m_session;
	bool m_battle = false;
	QStringList m_scriptLocation;
	QPointer<EventEditorController> m_controller;
	int m_page = -1;
	QMetaObject::Connection m_pagesConnection;
};
