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

#include "event_tools.h"
#include "command_editing.h"
#include "core.h"
#include "model/event_editor_controller.h"
#include "model/project.h"
#include "qmlbinding/qml_host.h"
#include "ui/map/map_layer_item.h"
#include "ui/picker/pickers.h"
#include <QQuickItem>
#include <QQuickWindow>

EventTools::EventTools(QObject* parent) :
	QObject(parent)
{
}

bool EventTools::editRoute(EventPageSettings* page, QQuickItem* from)
{
	if (!page || !page->hasPage()) {
		return false;
	}
	lcf::rpg::MoveRoute route = page->moveRoute();
	if (!CommandEditing::editRoute(Pickers::windowOf(from), route)) {
		return false;
	}
	page->setMoveRoute(route);
	return true;
}

int EventTools::pickUpperTile(int chipsetId, int current, QQuickItem* from)
{
	int tile = -1;
	const int result = QmlHost::exec(QStringLiteral("UpperTileDialog"), Pickers::windowOf(from),
		{{QStringLiteral("chipsetId"), chipsetId}, {QStringLiteral("tile"), current}},
		[&tile](QQuickWindow* window) { tile = window->property("tile").toInt(); });
	return result == 1 ? tile : -1;
}

int EventTools::upperTileFlags(int chipsetId, int tile) const
{
	const auto& project = core().project();
	return project ? EventPageSettings::upperTileFlags(project->database().chipsets, chipsetId, tile) : -1;
}

bool EventTools::hasTiles(int chipsetId) const
{
	const auto tiles = MapLayerView::sharedChipsetTiles(chipsetId);
	return tiles && !tiles->isNull();
}

int EventTools::stepFrames(int moveSpeed) const
{
	return EventPageSettings::stepFrames(moveSpeed);
}

int EventTools::turnFrames(int moveSpeed) const
{
	return EventPageSettings::turnFrames(moveSpeed);
}
