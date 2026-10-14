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

#include "command_editing.h"
#include "core.h"
#include "model/command_catalog.h"
#include "model/event_command_list_model.h"
#include "model/event_editor_controller.h"
#include "model/game_patches.h"
#include "model/move_route_model.h"
#include "model/project_data.h"
#include "qmlbinding/qml_host.h"
#include "ui/event/command_editor_registry.h"
#include <QQuickWindow>
#include <QVariantMap>
#include <QWindow>
#include <memory>

namespace {
using Code = CommandEditing::Code;

/** The window of the moves of a route */
bool execRoute(QWindow* over, MoveRouteModel& moves)
{
	return QmlHost::exec(QStringLiteral("MoveRouteEditor"), over,
		{{QStringLiteral("moves"), QVariant::fromValue(&moves)}, {QStringLiteral("moveEvent"), false}}) == 1;
}

CommandEditorRegistry::Context registryContext(bool battle, bool is_new)
{
	CommandEditorRegistry::Context context;
	context.battle = battle;
	context.isNew = is_new;
	context.events = core().currentMapEvents();
	return context;
}
}

QString CommandEditing::title(Code code)
{
	return CommandCatalog::commandName(code);
}

bool CommandEditing::edit(QWindow* over, ProjectData& project, lcf::rpg::EventCommand& command, const Context& context)
{
	if (!EventCommandListModel::hasEditor(static_cast<Code>(command.code))) {
		return true;
	}
	// the QML editors (CommandEditorRegistry): hand-written, made from the
	// schema, or the raw one; those of several lines change the list
	const auto registry = registryContext(context.battle, context.isNew);
	const bool in_list = context.commands && context.index >= 0 && context.index < static_cast<int>(context.commands->size());
	return in_list ? CommandEditorRegistry::edit(over, project, *context.commands, context.index, registry)
		: CommandEditorRegistry::edit(over, project, command, registry);
}

bool CommandEditing::editRaw(QWindow* over, ProjectData& project, lcf::rpg::EventCommand& command, bool show_warning)
{
	// the raw editor (RawCommandEditor.qml)
	return CommandEditorRegistry::editRaw(over, project, command, show_warning);
}

std::optional<Code> CommandEditing::pickCommand(QWindow* over, ProjectData& project, bool battle)
{
	const bool rpg2k3 = project.database().system.ldb_id == 2003;
	const bool maniac = GamePatches::usesManiac(project.project());
	// the buttons, page by page: {title, entries: {code, name, rpg2k3Only, maniacOnly}}, a gap: code 0
	QVariantList pages;
	int number = 1;
	for (const auto* page : CommandCatalog::pages(battle, maniac)) {
		QVariantList entries;
		for (const auto& entry : page->entries) {
			if (entry.code == Code::END) {
				entries.append(QVariantMap{{QStringLiteral("code"), 0}});
				continue;
			}
			entries.append(QVariantMap{
				{QStringLiteral("code"), static_cast<int>(entry.code)},
				{QStringLiteral("name"), CommandCatalog::translate(entry.name)},
				{QStringLiteral("rpg2k3Only"), entry.rpg2k3_only},
				{QStringLiteral("maniacOnly"), entry.maniac_only},
			});
		}
		pages.append(QVariantMap{
			{QStringLiteral("title"), QStringLiteral("%1 %2").arg(number++).arg(CommandCatalog::translate(page->title))},
			{QStringLiteral("entries"), entries},
		});
	}
	int code = 0;
	const int result = QmlHost::exec(QStringLiteral("CommandPalette"), over,
		{{QStringLiteral("pages"), pages}, {QStringLiteral("battle"), battle}, {QStringLiteral("rpg2k3"), rpg2k3},
			{QStringLiteral("maniac"), maniac}},
		[&code](QQuickWindow* window) { code = window->property("code").toInt(); });
	if (result != 1 || code <= 0) {
		return std::nullopt;
	}
	return static_cast<Code>(code);
}

bool CommandEditing::editRoute(QWindow* over, lcf::rpg::MoveRoute& route)
{
	MoveRouteModel moves;
	moves.setRoute(route);
	if (!execRoute(over, moves)) {
		return false;
	}
	route = moves.route();
	return true;
}

bool CommandEditing::editMoveEvent(QWindow* over, ProjectData& project, lcf::rpg::EventCommand& command)
{
	// its QML editor (MoveEventEditor), through the registry
	return CommandEditorRegistry::edit(over, project, command, registryContext(false, false));
}

bool CommandEditing::editBattleTrigger(QWindow* over, ProjectData& project, const lcf::rpg::Troop& troop,
		lcf::rpg::TroopPageCondition& condition)
{
	// a copy, with the rules of the window: the page changes on OK only
	BattlePageConditions conditions(condition, project.database().system.ldb_id == 2003);
	conditions.setChoices(project.database(), troop);
	if (QmlHost::exec(QStringLiteral("BattleConditionDialog"), over,
			{{QStringLiteral("conditions"), QVariant::fromValue(&conditions)}}) != 1) {
		return false;
	}
	condition = conditions.condition();
	return true;
}
