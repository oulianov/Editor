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

#include <QObject>
#include <QtQmlIntegration>

#include "model/event_editor_controller.h"
class QQuickItem;

/**
 * What the event windows ask of the Editor (the singleton Ez.EventTools):
 * the windows of a page's route and of its tile, the tiles of a chipset,
 * and how the Player animates an event standing still.
 */
class EventTools : public QObject
{
	Q_OBJECT
	QML_NAMED_ELEMENT(EventTools)
	QML_SINGLETON

public:
	explicit EventTools(QObject* parent = nullptr);

	/** Define Pattern: the custom route of the page in its window
	 *  (MoveRouteEditor.qml) over the window of from; false when cancelled */
	Q_INVOKABLE bool editRoute(EventPageSettings* page, QQuickItem* from);
	/** A tile of the upper layer of a chipset (UpperTileDialog.qml), -1 when cancelled */
	Q_INVOKABLE int pickUpperTile(int chipsetId, int current, QQuickItem* from);
	/** The passability of a tile of the upper layer of a chipset of the
	 *  project (EventPageSettings::setTile), -1 without such a chipset */
	Q_INVOKABLE int upperTileFlags(int chipsetId, int tile) const;
	/** The chipset has tiles to choose (its picture was found) */
	Q_INVOKABLE bool hasTiles(int chipsetId) const;
	/** Frames (at 60 a second) per step or quarter turn of an event standing still */
	Q_INVOKABLE int stepFrames(int moveSpeed) const;
	Q_INVOKABLE int turnFrames(int moveSpeed) const;
};
