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

// The types of the event windows' model in the QML module, registered from
// here: editor_core does not link QtQml

#include <QObject>
#include <QtQmlIntegration>
#include "model/event_command_list_model.h"
#include "model/event_editor_controller.h"
#include "model/move_route_model.h"

/** The event edited by an event window (EventEditorWindow.qml) */
struct EventEditorControllerForeign
{
	Q_GADGET
	QML_FOREIGN(EventEditorController)
	QML_NAMED_ELEMENT(EventEditorController)
	QML_UNCREATABLE("An event window is opened by the map")
};

/** The settings of the page shown (EventEditorController.page) */
struct EventPageSettingsForeign
{
	Q_GADGET
	QML_FOREIGN(EventPageSettings)
	QML_NAMED_ELEMENT(EventPageSettings)
	QML_UNCREATABLE("The page shown has its settings")
};

/** The triggers of a battle page, edited by BattleConditionDialog.qml */
struct BattlePageConditionsForeign
{
	Q_GADGET
	QML_FOREIGN(BattlePageConditions)
	QML_NAMED_ELEMENT(BattlePageConditions)
	QML_UNCREATABLE("The conditions of a battle page are given to their window")
};

/** The moves of a route (MoveRouteEditor.qml) */
struct MoveRouteModelForeign
{
	Q_GADGET
	QML_FOREIGN(MoveRouteModel)
	QML_NAMED_ELEMENT(MoveRouteModel)
	QML_UNCREATABLE("A route is given to its window")
};

/** The lines of a command list (CommandListEditor.model) */
struct EventCommandListModelForeign
{
	Q_GADGET
	QML_FOREIGN(EventCommandListModel)
	QML_NAMED_ELEMENT(EventCommandListModel)
	QML_UNCREATABLE("A command list has its lines")
};
