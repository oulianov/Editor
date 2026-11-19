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
#include "common/editor_settings.h"
#include "core.h"
#include "model/command_catalog.h"
#include "model/command_origin.h"
#include "model/event_command_list_model.h"
#include "model/event_editor_controller.h"
#include "model/move_route_model.h"
#include "model/project_data.h"
#include "qmlbinding/qml_host.h"
#include "ui/event/command_editor_registry.h"
#include <QQuickWindow>
#include <QSettings>
#include <QVariantMap>
#include <QWindow>
#include <memory>
#include <optional>

namespace {
using Code = CommandEditing::Code;

/** The window of the moves of a route */
bool execRoute(QWindow* over, MoveRouteModel& moves)
{
	return QmlHost::exec(QStringLiteral("MoveRouteEditor"), over,
		{{QStringLiteral("moves"), QVariant::fromValue(&moves)}, {QStringLiteral("moveEvent"), false}}) == 1;
}

CommandEditorRegistry::Context registryContext(bool battle, bool is_new, int troop_members = -1)
{
	CommandEditorRegistry::Context context;
	context.battle = battle;
	context.isNew = is_new;
	context.events = core().currentMapEvents();
	context.troopMembers = troop_members;
	return context;
}

/** The buttons of the palette, page by page: {title, origin, entries: {code, name, source, origin}},
 *  a gap: code 0, the entry of the battle event commands: -1. The origin of
 *  a command (CommandOrigin: "maniac"...), the one of all the commands of
 *  a page ("" for several or none): the palette greys those the game does
 *  not run, tags them (Ez.Origins) */
QVariantList palettePages(const std::vector<const CommandCatalog::Page*>& shown)
{
	QVariantList pages;
	for (const auto* page : shown) {
		QVariantList entries;
		std::optional<QString> page_origin;
		for (const auto& entry : page->entries) {
			if (entry.code == Code::END) {
				entries.append(QVariantMap{{QStringLiteral("code"), 0}});
				continue;
			}
			const QString origin = entry.code == CommandCatalog::battleCommands ? QString() : CommandOrigin::ofCode(entry.code);
			page_origin = !page_origin || *page_origin == origin ? origin : QString();
			entries.append(QVariantMap{
				{QStringLiteral("code"), static_cast<int>(entry.code)},
				{QStringLiteral("name"), CommandCatalog::translate(entry.name)},
				// the English name: the search finds a command by it too
				{QStringLiteral("source"), QString::fromUtf8(entry.name)},
				{QStringLiteral("origin"), origin},
			});
		}
		pages.append(QVariantMap{
			{QStringLiteral("title"), CommandCatalog::translate(page->title)},
			{QStringLiteral("origin"), page_origin.value_or(QString())},
			{QStringLiteral("entries"), entries},
		});
	}
	return pages;
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
	const auto registry = registryContext(context.battle, context.isNew, context.troopMembers);
	const bool in_list = context.commands && context.index >= 0 && context.index < static_cast<int>(context.commands->size());
	return in_list ? CommandEditorRegistry::edit(over, project, *context.commands, context.index, registry)
		: CommandEditorRegistry::edit(over, project, command, registry);
}

bool CommandEditing::editRaw(QWindow* over, ProjectData& project, lcf::rpg::EventCommand& command, bool show_warning)
{
	// the raw editor (RawCommandEditor.qml)
	return CommandEditorRegistry::editRaw(over, project, command, show_warning);
}

std::optional<Code> CommandEditing::pickCommand(QWindow* over, ProjectData& /* project */, bool battle)
{
	// every command: those the game does not run (RPG Maker 2003's, the
	// Maniac Patch's, EasyRPG Player's) greyed, the palette following
	// Ez.Game while it is open. A map event, a common event: their last
	// entry opens the battle event commands (the palette again, over it),
	// which are inserted as in a battle
	const QVariantList battle_pages = battle ? QVariantList() : palettePages({&CommandCatalog::battlePage()});
	int code = 0;
	// it opens on the tab shown when it was closed (the battle pages': theirs)
	const QString pageKey = battle ? QStringLiteral("command_palette_battle_page") : QStringLiteral("command_palette_page");
	QSettings settings;
	const int result = QmlHost::exec(QStringLiteral("CommandPalette"), over,
		{{QStringLiteral("pages"), palettePages(CommandCatalog::pages(battle, true, true, EditorSettings::instance().commandsByTopic()))},
			{QStringLiteral("battlePages"), battle_pages}, {QStringLiteral("battle"), battle},
			{QStringLiteral("startPage"), settings.value(pageKey, 0).toInt()}},
		[&code, &settings, &pageKey](QQuickWindow* window) {
			code = window->property("code").toInt();
			settings.setValue(pageKey, window->property("page").toInt());
		});
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
	// a copy, with the rules of the window (the game's, GameSetup): the page changes on OK only
	BattlePageConditions conditions(condition);
	conditions.setChoices(project.database(), troop);
	if (QmlHost::exec(QStringLiteral("BattleConditionDialog"), over,
			{{QStringLiteral("conditions"), QVariant::fromValue(&conditions)}}) != 1) {
		return false;
	}
	condition = conditions.condition();
	return true;
}
