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

#include "command_list_editor.h"
#include "command_editing.h"
#include "common/script_link.h"
#include "model/command_colors.h"
#include "model/event_command_defaults.h"
#include "model/event_command_list_model.h"
#include "model/event_editor_controller.h"
#include "model/project.h"
#include "model/project_data.h"
#include "qmlbinding/database_entry.h"
#include "qmlbinding/database_session.h"
#include "qmlbinding/project_data_gadget.h"
#include "qmlbinding/qml_dialog_service.h"
#include "stringizer.h"
#include "ui/picker/pickers.h"
#include <QPalette>
#include <QPointer>
#include <QWindow>
#include <QQuickItem>
#include <QUndoStack>
#include <algorithm>

namespace {
using Cmd = lcf::rpg::EventCommand::Code;

QVariantList range(int first, int end)
{
	return {first, end};
}
}

CommandListEditor::CommandListEditor(QObject* parent) :
	QObject(parent), m_model(new EventCommandListModel(this))
{
	connect(m_model, &EventCommandListModel::commandsChanged, this, &CommandListEditor::linesChanged);
	connect(m_model->undoStack(), &QUndoStack::canUndoChanged, this, &CommandListEditor::undoChanged);
	connect(m_model->undoStack(), &QUndoStack::canRedoChanged, this, &CommandListEditor::undoChanged);
}

CommandListEditor::~CommandListEditor() = default;

EventCommandListModel* CommandListEditor::model() const
{
	return m_model;
}

ProjectDataGadget* CommandListEditor::projectData() const
{
	return m_gadget;
}

void CommandListEditor::setProjectData(ProjectDataGadget* gadget)
{
	if (m_gadget != gadget) {
		m_gadget = gadget;
		emit projectChanged();
	}
}

DatabaseSession* CommandListEditor::session() const
{
	return m_session;
}

void CommandListEditor::setSession(DatabaseSession* session)
{
	if (m_session != session) {
		m_session = session;
		emit projectChanged();
	}
}

ProjectData* CommandListEditor::project() const
{
	if (m_session) {
		return &m_session->copy();
	}
	return m_gadget ? m_gadget->projectData() : nullptr;
}

bool CommandListEditor::battle() const
{
	return m_battle;
}

void CommandListEditor::setBattle(bool battle)
{
	if (m_battle != battle) {
		m_battle = battle;
		emit battleChanged();
	}
}

QStringList CommandListEditor::scriptLocation() const
{
	return m_scriptLocation;
}

void CommandListEditor::setScriptLocation(const QStringList& location)
{
	if (m_scriptLocation != location) {
		m_scriptLocation = location;
		emit scriptLocationChanged();
	}
}

int CommandListEditor::lineCount() const
{
	return commands() ? static_cast<int>(commands()->size()) : 0;
}

bool CommandListEditor::canUndo() const
{
	return m_model->undoStack()->canUndo();
}

bool CommandListEditor::canRedo() const
{
	return m_model->undoStack()->canRedo();
}

std::vector<lcf::rpg::EventCommand>* CommandListEditor::commands() const
{
	return m_model->commands();
}

QString CommandListEditor::projectDir() const
{
	const ProjectData* data = project();
	return data && data->hasProject() ? data->project().projectDir().absolutePath() : QString();
}

void CommandListEditor::setCommands(std::vector<lcf::rpg::EventCommand>* commands)
{
	m_model->setCommands(commands);
	emit undoChanged();
}

void CommandListEditor::showEventPage(EventEditorController* controller, int page)
{
	disconnect(m_pagesConnection);
	m_controller = controller;
	m_page = page;
	if (controller) {
		// the pages moved (one added, removed...): their commands too
		m_pagesConnection = connect(controller, &EventEditorController::pagesChanged, this, &CommandListEditor::followEventPage);
	}
	followEventPage();
}

void CommandListEditor::followEventPage()
{
	// shown again even at the same place: the pages were replaced (a page
	// cleared, the event read again)
	auto* commands = m_controller ? m_controller->commands(m_page) : nullptr;
	setCommands(commands);
	setScriptLocation(m_controller && m_controller->mapId() > 0 && commands ? m_controller->scriptLocation(m_page) : QStringList());
}

void CommandListEditor::showTroopPage(int troop, int page)
{
	disconnect(m_pagesConnection);
	m_controller = nullptr;
	ProjectData* data = project();
	std::vector<lcf::rpg::EventCommand>* commands = nullptr;
	QStringList location;
	if (data && troop >= 0 && troop < static_cast<int>(data->database().troops.size())) {
		auto& pages = data->database().troops[static_cast<size_t>(troop)].pages;
		if (page >= 0 && page < static_cast<int>(pages.size())) {
			commands = &pages[static_cast<size_t>(page)].event_commands;
			// its script (rpgsync): the page of the troop, in order
			location = {QStringLiteral("--troop"), QString::number(data->database().troops[static_cast<size_t>(troop)].ID),
				QStringLiteral("--page"), QString::number(page + 1)};
		}
	}
	setCommands(commands);
	setScriptLocation(location);
}

void CommandListEditor::showCommonEvent(JsonView* entry)
{
	disconnect(m_pagesConnection);
	m_controller = nullptr;
	const DatabaseEntry found = DatabaseEntry::of(entry);
	if (found.session) {
		setSession(found.session);
	}
	std::vector<lcf::rpg::EventCommand>* commands = nullptr;
	QStringList location;
	if (found.valid() && found.list == QLatin1String("commonevents")
			&& found.index < static_cast<int>(found.project->database().commonevents.size())) {
		auto& event = found.project->database().commonevents[static_cast<size_t>(found.index)];
		commands = &event.event_commands;
		location = {QStringLiteral("--common-event"), QString::number(event.ID)};
	}
	setCommands(commands);
	setScriptLocation(location);
}

bool CommandListEditor::validLine(int line) const
{
	return commands() && line >= 0 && line < lineCount();
}

bool CommandListEditor::isSelectable(int line) const
{
	return m_model->isSelectable(line);
}

bool CommandListEditor::isCommandLine(int line) const
{
	return m_model->isCommandLine(line);
}

int CommandListEditor::commandStart(int line) const
{
	return m_model->commandStart(line);
}

QVariantList CommandListEditor::commandRange(int line) const
{
	const auto [first, end] = m_model->commandRange(line);
	return range(first, end);
}

QVariantList CommandListEditor::wholeCommands(int first, int end) const
{
	const auto [from, to] = m_model->wholeCommands(first, end);
	return range(from, to);
}

bool CommandListEditor::hasEditor(int line) const
{
	return validLine(line) && EventCommandListModel::hasEditor(
		static_cast<Cmd>((*commands())[static_cast<size_t>(m_model->commandStart(line))].code));
}

bool CommandListEditor::isEmptyLine(int line) const
{
	return commands() && (line == lineCount() || (validLine(line) && static_cast<Cmd>((*commands())[static_cast<size_t>(line)].code) == Cmd::END));
}

int CommandListEditor::code(int line) const
{
	return validLine(line) ? (*commands())[static_cast<size_t>(line)].code : 0;
}

int CommandListEditor::insertCommand(int index, QQuickItem* from)
{
	ProjectData* data = project();
	if (!commands() || !data) {
		return -1;
	}
	// before the line selected, as RPG Maker does; out of the list: at its end
	if (index < 0 || index > lineCount()) {
		index = lineCount();
	}
	QWindow* over = Pickers::windowOf(from);
	const auto code = CommandEditing::pickCommand(over, *data, m_battle);
	// the list may have gone with its window meanwhile
	if (!code || !commands()) {
		return -1;
	}
	index = std::min(index, lineCount());
	const int indent = index < lineCount() ? (*commands())[static_cast<size_t>(index)].indent : 0;
	// the settings of the last one, as left in its window
	m_model->insertLines(index, EventCommandListModel::newCommand(projectDir(), *code, indent));

	// set up now; Cancel takes it back
	const bool kept = CommandEditing::edit(over, *data, (*commands())[static_cast<size_t>(index)],
		{commands(), index, true, m_battle});
	if (kept) {
		m_model->rememberCommand(index, projectDir());
	}
	auto lines = EventCommandDefaults::range(*commands(), index);
	lines.second = std::max(lines.second, lines.first + 1);
	if (!kept) {
		m_model->removeLines(lines);
		lines.second = lines.first;
	} else {
		m_model->commandsEdited();
	}
	// as in RPG Maker, the line stays the one after the new command: Enter
	// again inserts the next one under it
	return lines.second;
}

int CommandListEditor::editCommand(int line, QQuickItem* from)
{
	ProjectData* data = project();
	if (!commands() || !data || line < 0 || line > lineCount()) {
		return -1;
	}
	// an empty line closing a block, or the end of the page: a command is inserted there
	if (isEmptyLine(line)) {
		return insertCommand(line, from);
	}
	// the next lines of a message or a comment edit its first line
	const int start = m_model->commandStart(line);
	if (!EventCommandListModel::hasEditor(static_cast<Cmd>((*commands())[static_cast<size_t>(start)].code))) {
		return line;
	}
	// the editors change the lines in place: shown and recorded after
	if (!CommandEditing::edit(Pickers::windowOf(from), *data, (*commands())[static_cast<size_t>(start)],
			{commands(), start, false, m_battle})) {
		return line;
	}
	if (!commands()) {
		return -1;
	}
	m_model->rememberCommand(start, projectDir());
	m_model->commandsEdited();
	return std::min(line, lineCount());
}

int CommandListEditor::addRaw(int line, QQuickItem* from)
{
	ProjectData* data = project();
	if (!commands() || !data) {
		return -1;
	}
	lcf::rpg::EventCommand command;
	if (!CommandEditing::editRaw(Pickers::windowOf(from), *data, command, false) || !commands()) {
		return -1;
	}
	// after the line, past the next lines of its message or comment
	int index = lineCount();
	if (validLine(line)) {
		index = line + 1;
		while (index < lineCount()) {
			const auto next = static_cast<Cmd>((*commands())[static_cast<size_t>(index)].code);
			if (next != Cmd::ShowMessage_2 && next != Cmd::Comment_2) {
				break;
			}
			++index;
		}
	}
	return m_model->insertLines(index, {command});
}

int CommandListEditor::editRaw(int line, QQuickItem* from)
{
	ProjectData* data = project();
	if (!data || !validLine(line)) {
		return -1;
	}
	CommandEditing::editRaw(Pickers::windowOf(from), *data, (*commands())[static_cast<size_t>(line)], false);
	if (!commands()) {
		return -1;
	}
	m_model->commandsEdited();
	return std::min(line, lineCount());
}

int CommandListEditor::removeLines(int first, int end)
{
	// a line of the structure of a block alone: nothing (its block goes with its command)
	if (!commands() || first >= end) {
		return -1;
	}
	// one change of the list: one step of undo
	m_model->removeLines({first, end});
	return std::min(first, lineCount());
}

int CommandListEditor::copyLines(int first, int end, bool cut)
{
	if (!commands() || first >= end) {
		return -1;
	}
	m_model->copyLines({first, end}, cut);
	return cut ? std::min(first, lineCount()) : -1;
}

int CommandListEditor::paste(int index)
{
	return m_model->paste(index);
}

bool CommandListEditor::canPaste()
{
	return EventCommandListModel::canPaste();
}

void CommandListEditor::undo()
{
	m_model->undoStack()->undo();
}

void CommandListEditor::redo()
{
	m_model->undoStack()->redo();
}

QString CommandListEditor::scriptUnavailableReason() const
{
	return ScriptLink::unavailableReason();
}

void CommandListEditor::openScript(int line, QQuickItem* from)
{
	if (m_scriptLocation.isEmpty()) {
		return;
	}
	QPointer<QWindow> over = Pickers::windowOf(from);
	// the line of the command (a message opens at its first line)
	ScriptLink::open(this, m_scriptLocation, validLine(line) ? m_model->commandStart(line) : -1,
		[over](const QString& error) {
			QmlDialogService(over).warning(ScriptLink::title(), error);
		});
}

QVariantList CommandListEditor::lineColors(const QColor& base, const QColor& text, const QColor& disabled, bool colored) const
{
	using CommandColors::Kind;
	QPalette palette;
	palette.setColor(QPalette::Base, base);
	palette.setColor(QPalette::Text, text);
	QVariantList colors;
	for (int i = 0; i < static_cast<int>(Kind::Count); ++i) {
		const auto kind = static_cast<Kind>(i);
		if (kind == Kind::End && !colored) {
			// the empty lines are dimmed when commands are not colored
			colors.append(disabled);
		} else if (colored && kind != Kind::Text && kind != Kind::End) {
			colors.append(CommandColors::color(kind, palette));
		} else {
			colors.append(text);
		}
	}
	return colors;
}

QColor CommandListEditor::stripeColor(const QColor& base) const
{
	QPalette palette;
	palette.setColor(QPalette::Base, base);
	return CommandColors::stripeColor(palette);
}

QString CommandListEditor::firstLineLabel(int code) const
{
	switch (static_cast<Cmd>(code)) {
		case Cmd::ShowMessage_2:
			return Stringizer::tr("Message");
		case Cmd::Comment_2:
			return Stringizer::tr("Comment");
		default:
			return QString();
	}
}
