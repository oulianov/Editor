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

#include "command_editor_registry.h"
#include "common/dbstring.h"
#include "model/command_schema.h"
#include "model/event_branches.h"
#include "model/event_command_defaults.h"
#include "model/event_command_list.h"
#include "model/project_data.h"
#include "qmlbinding/command_edit_session.h"
#include "qmlbinding/qml_dialog_service.h"
#include "qmlbinding/qml_host.h"
#include "ui/picker/pickers.h"
#include "ui/shell/qml_engine.h"

#include <QCoreApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QScopeGuard>
#include <QPointer>
#include <utility>

namespace {
using Code = lcf::rpg::EventCommand::Code;
using Command = lcf::rpg::EventCommand;

/** The hand-written editors (the QML files of src/ui/commands) */
const std::vector<std::pair<Code, const char*>>& editors() {
	static const std::vector<std::pair<Code, const char*>> list = {
		{Code::ShowMessage, "ShowMessageEditor"},
		{Code::Comment, "CommentEditor"},
		{Code::ShowChoice, "ShowChoiceEditor"},
		{Code::ConditionalBranch, "ConditionalBranchEditor"},
		{Code::ConditionalBranch_B, "ConditionalBranchEditor"},
		{Code::EnemyEncounter, "EnemyEncounterEditor"},
		{Code::OpenShop, "OpenShopEditor"},
		{Code::ShowPicture, "PictureEditor"},
		{Code::MovePicture, "PictureEditor"},
		{Code::ErasePicture, "ErasePictureEditor"},
		{Code::MoveEvent, "MoveEventEditor"},
		{Code::ControlVars, "VariableOperationsEditor"},
		{Code::ChangeFaceGraphic, "ChangeFaceGraphicEditor"},
		{Code::ChangeActorFace, "ChangeActorFaceEditor"},
		{Code::ChangeSpriteAssociation, "ChangeActorGraphicEditor"},
		{Code::ChangeVehicleGraphic, "ChangeVehicleGraphicEditor"},
		{Code::ShowBattleAnimation_B, "ShowBattleAnimationEditor"},
		{Code::KeyInputProc, "KeyInputProcEditor"},
		{Code::Maniac_ControlStrings, "ControlStringsEditor"},
		{Code::Maniac_ShowStringPicture, "ShowStringPictureEditor"},
	};
	return list;
}

/** The next lines of a message or comment */
Code nextLineOf(Code code) {
	switch (code) {
		case Code::ShowMessage:
			return Code::ShowMessage_2;
		case Code::Comment:
			return Code::Comment_2;
		default:
			return Code::END;
	}
}

QVariantList eventsOf(const QMap<int, lcf::rpg::Event*>* events) {
	QVariantList list;
	if (!events) {
		return list;
	}
	for (auto it = events->cbegin(); it != events->cend(); ++it) {
		list.append(QVariantMap{{QStringLiteral("value"), it.key()},
			{QStringLiteral("text"), QStringLiteral("%1: %2").arg(it.key(), 4, 10, QChar(u'0')).arg(ToQString(it.value()->name))}});
	}
	return list;
}

/** The questions of OK over the editor shown */
QmlDialogService dialogsOver(QWindow* fallback) {
	QWindow* front = QmlHost::frontWindow();
	return QmlDialogService(front ? front : fallback);
}

// Show Choices: the choice lines are numbered 0-3, the cancel branch is 4
constexpr int cancelBranch = 4;
constexpr int cancelHandler = 5;

int parameterOf(const Command& command, int index, int fallback = 0) {
	return index < static_cast<int>(command.parameters.size()) ? command.parameters[index] : fallback;
}

void setParameterOf(Command& command, int index, int value) {
	if (index >= static_cast<int>(command.parameters.size())) {
		std::vector<int32_t> parameters(command.parameters.begin(), command.parameters.end());
		parameters.resize(index + 1);
		command.parameters = lcf::DBArray<int32_t>(parameters.begin(), parameters.end());
	}
	command.parameters[index] = value;
}

/** The else line of a Conditional Branch: the line of the block at its indent (END: none) */
Code elseOf(Code code) {
	return code == Code::ConditionalBranch_B ? Code::ElseBranch_B : Code::ElseBranch;
}

int elseLine(const std::vector<Command>& list, int index) {
	const Command& command = list[index];
	const auto [start, end] = EventCommandDefaults::range(list, index);
	const int code = static_cast<int>(elseOf(static_cast<Code>(command.code)));
	for (int i = start + 1; i < end; ++i) {
		if (list[i].indent == command.indent && list[i].code == code) {
			return i;
		}
	}
	return -1;
}

/**
 * What the editor of a block shows of its branches (the session's extras):
 * the texts of the choices (choices, and present: the choice has its line),
 * whether a Conditional Branch has its else (else)
 */
void readBranches(CommandEditSession& session, const std::vector<Command>& list, int index, bool alone) {
	const Command& command = list[index];
	switch (static_cast<Code>(command.code)) {
		case Code::ShowChoice: {
			QStringList texts(4);
			QVariantList present = {false, false, false, false};
			if (alone) {
				// no lines: the texts of the summary
				const QStringList parts = ToQString(command.string).split(u'/');
				for (int i = 0; i < 4 && i < parts.size(); ++i) {
					texts[i] = parts[i];
				}
			} else {
				const auto [start, end] = EventCommandDefaults::range(list, index);
				for (int i = start + 1; i < end; ++i) {
					const int choice = parameterOf(list[i], 0, -1);
					if (list[i].code == static_cast<int>(Code::ShowChoiceOption) && list[i].indent == command.indent
						&& choice >= 0 && choice < 4) {
						present[choice] = true;
						texts[choice] = ToQString(list[i].string);
					}
				}
			}
			session.setExtra(QStringLiteral("choices"), texts);
			session.setExtra(QStringLiteral("originalChoices"), texts);
			session.setExtra(QStringLiteral("present"), present);
			break;
		}
		case Code::ConditionalBranch:
		case Code::ConditionalBranch_B:
			session.setExtra(QStringLiteral("else"), alone ? parameterOf(command, 5) != 0 : elseLine(list, index) >= 0);
			break;
		default:
			break;
	}
}

/**
 * OK of Show Choices: empty fields are not choices, like in RPG Maker: the
 * next ones move up with their branches. A kept branch is matched by its old
 * number (new ones get numbers no old branch has), then all are renumbered
 */
bool applyChoices(QWindow* over, std::vector<Command>& list, int index, CommandEditSession& session) {
	const Command& original = list[index];
	Command edited = session.command();
	const QStringList texts = session.extra(QStringLiteral("choices")).toStringList();
	const QStringList oldTexts = session.extra(QStringLiteral("originalChoices")).toStringList();
	const QVariantList present = session.extra(QStringLiteral("present")).toList();
	const int cancel = parameterOf(edited, 0);
	if (texts == oldTexts && cancel == parameterOf(original, 0)) {
		list[index] = edited;
		return true;
	}

	auto dialogs = dialogsOver(over);
	const int indent = edited.indent;
	std::vector<Command> markers;
	QStringList kept;
	int newCancel = cancel;
	for (int i = 0; i < 4 && i < texts.size(); ++i) {
		const QString& text = texts[i];
		if (text.isEmpty()) {
			if (cancel == i + 1) {
				newCancel = 0;
			}
			continue;
		}
		if (cancel == i + 1) {
			newCancel = kept.size() + 1;
		}
		auto marker = EventCommandDefaults::make(Code::ShowChoiceOption, indent).front();
		setParameterOf(marker, 0, present.value(i).toBool() ? i : cancelHandler + 1 + i);
		marker.string = ToDBString(text);
		markers.push_back(marker);
		kept << text;
	}
	if (kept.isEmpty()) {
		dialogs.warning(QCoreApplication::translate("ShowChoiceWidget", "Show Choices"),
			QCoreApplication::translate("ShowChoiceWidget", "Enter at least one choice."));
		return false;
	}
	if (cancel == cancelHandler) {
		auto marker = EventCommandDefaults::make(Code::ShowChoiceOption, indent).front();
		setParameterOf(marker, 0, cancelBranch);
		markers.push_back(marker);
	}
	setParameterOf(edited, 0, newCancel);
	if (texts != oldTexts) {
		// the separator of the summary line depends on the RPG Maker version: the one used is kept
		QStringList old;
		for (const QString& text : oldTexts) {
			if (!text.isEmpty()) {
				old << text;
			}
		}
		const bool comma = old.size() > 1 && ToQString(original.string) == old.join(QStringLiteral(", "));
		edited.string = ToDBString(kept.join(comma ? QStringLiteral(", ") : QStringLiteral("/")));
	}
	if (!EventBranches::apply(list, index, edited, Code::ShowChoiceEnd, markers, dialogs, false, true)) {
		return false;
	}
	const auto [start, end] = EventCommandDefaults::range(list, index);
	int number = 0;
	for (int i = start + 1; i < end; ++i) {
		if (list[i].code == static_cast<int>(Code::ShowChoiceOption) && list[i].indent == indent
			&& !list[i].parameters.empty() && list[i].parameters[0] != cancelBranch) {
			list[i].parameters[0] = number++;
		}
	}
	return true;
}

/** OK of a Conditional Branch: its else added or removed, the commands of the then kept */
bool applyCondition(QWindow* over, std::vector<Command>& list, int index, CommandEditSession& session) {
	const Command& edited = session.command();
	const auto code = static_cast<Code>(edited.code);
	const bool wanted = session.extra(QStringLiteral("else")).toBool();
	const int existing = elseLine(list, index);
	if (wanted == (existing >= 0)) {
		list[index] = edited;
		return true;
	}
	std::vector<Command> markers;
	if (wanted) {
		markers.push_back(EventCommandDefaults::make(elseOf(code), edited.indent).front());
	}
	auto dialogs = dialogsOver(over);
	return EventBranches::apply(list, index, edited, code == Code::ConditionalBranch_B ? Code::EndBranch_B : Code::EndBranch,
		markers, dialogs, true);
}


/**
 * OK: the command edited and the lines it owns into the list (alone: no
 * other line). False: something was refused, the editor stays
 */
bool applyEdit(QWindow* over, std::vector<Command>& list, int index, CommandEditSession& session, bool alone) {
	const Command& edited = session.command();
	const auto code = static_cast<Code>(edited.code);
	if (const Code next = nextLineOf(code); next != Code::END) {
		list[index] = edited;
		EventCommandList lines(list, index);
		lines.setLines(next, alone ? QStringList{session.lines().value(0)} : session.lines());
		return true;
	}
	if (alone) {
		list[index] = edited;
		return true;
	}
	switch (code) {
		case Code::ShowChoice:
			return applyChoices(over, list, index, session);
		case Code::ConditionalBranch:
		case Code::ConditionalBranch_B:
			return applyCondition(over, list, index, session);
		case Code::EnemyEncounter:
		case Code::OpenShop:
		case Code::ShowInn: {
			const auto wanted = EventBranches::handlers(edited);
			if (wanted == EventBranches::handlers(list[index])) {
				list[index] = edited;
				return true;
			}
			std::vector<Command> markers;
			for (auto handler : wanted) {
				markers.push_back(EventCommandDefaults::make(handler, edited.indent).front());
			}
			auto dialogs = dialogsOver(over);
			return EventBranches::apply(list, index, edited, EventBranches::closingOf(code), markers, dialogs, false);
		}
		default:
			list[index] = edited;
			return true;
	}
}

/**
 * Teleport: RPG Maker 2003's Select Teleport Location, the map location
 * picker itself (its editor has nothing else), with the Character Facing
 * in RPG Maker 2003 games. OK writes the map, the cell and the facing
 */
bool pickTeleport(QWindow* over, ProjectData& project, Command& command) {
	const bool rpg2k3 = project.database().system.ldb_id == 2003;
	Pickers::LocationRequest request;
	request.current = {parameterOf(command, 0), parameterOf(command, 1), parameterOf(command, 2), parameterOf(command, 3)};
	request.facing = rpg2k3;
	request.title = QCoreApplication::translate("LocationCommandWidget", "Select Teleport Location");
	const auto chosen = Pickers::pickLocation(over, request);
	if (!chosen) {
		return false;
	}
	setParameterOf(command, 0, chosen->map);
	setParameterOf(command, 1, chosen->x);
	setParameterOf(command, 2, chosen->y);
	if (rpg2k3 || command.parameters.size() > 3) {
		setParameterOf(command, 3, chosen->direction);
	}
	return true;
}

bool run(QWindow* over, ProjectData& project, std::vector<Command>& commands, int index,
	const CommandEditorRegistry::Context& context, bool alone)
{
	const Command& command = commands[index];
	const auto code = static_cast<Code>(command.code);
	const QString type = CommandEditorRegistry::editorOf(code);
	if (type.isEmpty()) {
		// nothing to set
		return true;
	}
	if (code == Code::Teleport) {
		return pickTeleport(over, project, commands[index]);
	}
	// deleted after the window (the bindings of its editor read it until then)
	auto* session = new CommandEditSession(command);
	const auto cleanup = qScopeGuard([session] { session->deleteLater(); });
	session->setProject(&project);
	session->setContext(context.battle, context.isNew, context.mapId, eventsOf(context.events));
	if (const Code next = nextLineOf(code); next != Code::END) {
		QStringList lines = {ToQString(command.string)};
		for (size_t i = static_cast<size_t>(index) + 1; !alone && i < commands.size() && commands[i].code == static_cast<int>(next); ++i) {
			lines << ToQString(commands[i].string);
		}
		session->setLines(lines);
	}
	readBranches(*session, commands, index, alone);
	std::vector<Command> result;
	session->setAcceptor([&](CommandEditSession& edited) {
		result = commands;
		return applyEdit(over, result, index, edited, alone);
	});
	const int accepted = QmlHost::exec(QStringLiteral("CommandEditorWindow"), over, {
		{QStringLiteral("session"), QVariant::fromValue(session)},
		{QStringLiteral("editorType"), type},
	});
	if (accepted != 1) {
		return false;
	}
	commands = std::move(result);
	return true;
}
}

QString CommandEditorRegistry::handWrittenEditorOf(Code code) {
	for (const auto& [one, type] : editors()) {
		if (one == code) {
			return QString::fromLatin1(type);
		}
	}
	return {};
}

QList<CommandEditorRegistry::Code> CommandEditorRegistry::handWritten() {
	QList<Code> codes;
	for (const auto& one : editors()) {
		codes.append(one.first);
	}
	return codes;
}

QString CommandEditorRegistry::editorOf(Code code) {
	if (!CommandSchema::hasEditor(code)) {
		return {};
	}
	if (QString type = handWrittenEditorOf(code); !type.isEmpty()) {
		return type;
	}
	const CommandSchema::Schema* schema = CommandSchema::find(code);
	if (schema && !schema->custom) {
		return QStringLiteral("SchemaForm");
	}
	return QStringLiteral("RawCommandEditor");
}

bool CommandEditorRegistry::edit(QWindow* over, ProjectData& project, std::vector<Command>& commands, int index,
	const Context& context)
{
	if (index < 0 || index >= static_cast<int>(commands.size())) {
		return false;
	}
	return run(over, project, commands, index, context, false);
}

bool CommandEditorRegistry::edit(QWindow* over, ProjectData& project, Command& command, const Context& context) {
	std::vector<Command> alone{command};
	if (!run(over, project, alone, 0, context, true)) {
		return false;
	}
	command = alone.front();
	return true;
}

bool CommandEditorRegistry::editRaw(QWindow* over, ProjectData& project, Command& command, bool warn) {
	auto* session = new CommandEditSession(command);
	const auto cleanup = qScopeGuard([session] { session->deleteLater(); });
	session->setProject(&project);
	session->setExtra(QStringLiteral("warn"), warn);
	const int accepted = QmlHost::exec(QStringLiteral("CommandEditorWindow"), over, {
		{QStringLiteral("session"), QVariant::fromValue(session)},
		{QStringLiteral("editorType"), QStringLiteral("RawCommandEditor")},
	});
	if (accepted != 1) {
		return false;
	}
	command = session->command();
	return true;
}

QStringList CommandEditorRegistry::checkEditors(ProjectData& project) {
	QStringList problems;
	QQmlEngine* engine = QmlEngine::instance();
	QString current;
	const auto warnings = QObject::connect(engine, &QQmlEngine::warnings, engine, [&](const QList<QQmlError>& errors) {
		for (const auto& error : errors) {
			problems << QStringLiteral("%1: %2").arg(current, error.toString());
		}
	});
	const auto disconnect = qScopeGuard([warnings] { QObject::disconnect(warnings); });
	for (const auto& tag : Command::kCodeTags) {
		const auto code = static_cast<Code>(tag.value);
		const QString type = editorOf(code);
		if (type.isEmpty()) {
			continue;
		}
		current = QStringLiteral("%1 (%2)").arg(QString::fromLatin1(tag.name), type);
		// a command too special for a form has its own editor
		const CommandSchema::Schema* schema = CommandSchema::find(code);
		if (schema && schema->custom && handWrittenEditorOf(code).isEmpty()) {
			problems << current + QStringLiteral(": no hand-written editor");
		}
		const std::vector<Command> commands = EventCommandDefaults::make(code);
		CommandEditSession session(commands.front());
		session.setProject(&project);
		// the battle versions in a battle
		session.setContext(QString::fromLatin1(tag.name).endsWith(QLatin1String("_B")), true, 0, {});
		if (const Code next = nextLineOf(code); next != Code::END) {
			session.setLines({ToQString(commands.front().string)});
		}
		readBranches(session, commands, 0, false);
		// deleted once hidden
		QPointer<QQuickWindow> window = QmlHost::open(QStringLiteral("CommandEditorWindow"), nullptr, {
			{QStringLiteral("session"), QVariant::fromValue(&session)},
			{QStringLiteral("editorType"), type},
		});
		if (!window || !window->property("editor").value<QObject*>()) {
			problems << current + QStringLiteral(": not made");
		}
		if (window) {
			// drawn once: its layout settles
			window->grabWindow();
			window->close();
		}
		// the window and what its editor left go before the session
		QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
	}
	return problems;
}
