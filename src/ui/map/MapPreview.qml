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

// A map drawn as the map canvas shows it, at a zoom: the panorama, the lower
// and the upper layers, the events and the start positions. The layers not
// edited are faded (the upper one, with the events) or darkened
// to half their brightness (the lower one, with the panorama, as RPG Maker
// does), except with the selection tool, which shows both as they are. Its
// size is the map's at the zoom. MapCanvas.qml builds on it.

import QtQuick
import org.easyrpg.editor as Ez

Item {
    id: root

    property Ez.MapDocument document
    // item pixels per pixel of a tile
    property real zoom: 1
    // the zoom rounded so that the pixels of the tiles stay sharp
    property bool snap: true
    readonly property real shownZoom: snap ? lower.snapZoom(zoom, Screen.devicePixelRatio) : zoom
    // the layer edited (Ez.EditorState.LOWER, UPPER or EVENT)
    property int editedLayer: Ez.EditorState.EVENT
    // the tool of the editor (Ez.EditorState.SELECT...), -1 for none
    property int tool: -1
    readonly property bool lowerDarkened: editedLayer === Ez.EditorState.UPPER && tool !== Ez.EditorState.SELECT
    readonly property bool upperDimmed: editedLayer === Ez.EditorState.LOWER && tool !== Ez.EditorState.SELECT
    // events (ids) and start positions dragged by the selection tool: drawn
    // moveOffset cells away (MapCanvas.qml)
    property list<int> movingEvents
    property list<int> movingStarts
    property point moveOffset

    // the parts, for MapCanvas.qml and the tests
    property alias panoramaView: panorama
    property alias lowerView: lower
    property alias upperView: upper
    property alias eventsView: events

    implicitWidth: lower.implicitWidth
    implicitHeight: lower.implicitHeight

    Ez.MapPanoramaView {
        id: panorama
        document: root.document
        zoom: root.shownZoom
        width: implicitWidth
        height: implicitHeight
        // seen through the lower layer
        darkened: root.lowerDarkened
    }

    Ez.MapLayerView {
        id: lower
        document: root.document
        zoom: root.shownZoom
        layers: Ez.MapLayerView.Lower
        width: implicitWidth
        height: implicitHeight
        darkened: root.lowerDarkened
    }

    Ez.MapLayerView {
        id: upper
        document: root.document
        zoom: root.shownZoom
        layers: Ez.MapLayerView.Upper
        width: implicitWidth
        height: implicitHeight
        dimmed: root.upperDimmed
    }

    Ez.MapEventsView {
        id: events
        document: root.document
        zoom: root.shownZoom
        editedLayer: root.editedLayer
        width: implicitWidth
        height: implicitHeight
        // faded with the upper layer
        dimmed: root.upperDimmed
        movingEvents: root.movingEvents
        movingStarts: root.movingStarts
        moveOffset: root.moveOffset
    }
}
