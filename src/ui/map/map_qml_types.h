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

// The types of the map's model in the QML module, registered from here:
// editor_core does not link QtQml (nor does the test of MapEditor)

#include <QObject>
#include <QtQmlIntegration>
#include "model/editor_state.h"
#include "model/map_document.h"
#include "map_editor.h"

/** A map being edited (MapDocument), given to the map's items */
struct MapDocumentForeign
{
	Q_GADGET
	QML_FOREIGN(MapDocument)
	QML_NAMED_ELEMENT(MapDocument)
	QML_UNCREATABLE("A map is opened by the Editor")
};

/** The layer and the tool (EditorState): its enums, EditorState.EVENT... */
struct EditorStateForeign
{
	Q_GADGET
	QML_FOREIGN(EditorState)
	QML_NAMED_ELEMENT(EditorState)
	QML_UNCREATABLE("The Editor's state is shared by all maps")
};

/** The editing of a map (MapEditor), made by the view's host with its document */
struct MapEditorForeign
{
	Q_GADGET
	QML_FOREIGN(MapEditor)
	QML_NAMED_ELEMENT(MapEditor)
	QML_UNCREATABLE("A map editor is made with its document")
};
