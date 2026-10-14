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

// The controllers of the main window in the QML module, registered from
// here: editor_core does not link QtQml

#include <QObject>
#include <QtQmlIntegration>
#include "controller/action_catalog.h"
#include "controller/map_tabs_controller.h"
#include "controller/shell_controller.h"
#include "model/map_tree_model.h"

/** What the main window does (ShellController): its title, its status texts */
struct ShellControllerForeign
{
	Q_GADGET
	QML_FOREIGN(ShellController)
	QML_NAMED_ELEMENT(ShellController)
	QML_UNCREATABLE("The main window's controller is made by its shell")
};

/** The actions of the main window (ActionCatalog), a list model */
struct ActionCatalogForeign
{
	Q_GADGET
	QML_FOREIGN(ActionCatalog)
	QML_NAMED_ELEMENT(ActionCatalog)
	QML_UNCREATABLE("The actions are made by the main window's controller")
};

/** The maps open in tabs (MapTabsController), a list model */
struct MapTabsControllerForeign
{
	Q_GADGET
	QML_FOREIGN(MapTabsController)
	QML_NAMED_ELEMENT(MapTabsController)
	QML_UNCREATABLE("The map tabs are made by the main window's controller")
};

/** The map tree of the project (MapTreeModel), a tree model */
struct MapTreeModelForeign
{
	Q_GADGET
	QML_FOREIGN(MapTreeModel)
	QML_NAMED_ELEMENT(MapTreeModel)
	QML_UNCREATABLE("The map tree is the project's")
};
