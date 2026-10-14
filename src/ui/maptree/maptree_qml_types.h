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

// The types of the model the map tree's windows use, registered in the QML
// module from here: editor_core does not link QtQml

#include <QObject>
#include <QtQmlIntegration>
#include "model/encounter_list_model.h"

/** The monster groups of a map or area (Map and Area Properties) */
struct EncounterListModelForeign
{
	Q_GADGET
	QML_FOREIGN(EncounterListModel)
	QML_NAMED_ELEMENT(EncounterListModel)
	QML_UNCREATABLE("Made by Map and Area Properties")
};
