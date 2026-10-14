// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "EventPageWidget"

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * A tile of the upper layer of a chipset, as the graphic of an event page:
 * the tiles six a row, as in the palette; a click picks one, a double-click
 * picks it and closes. Opened by Ez.EventTools.pickUpperTile, which reads
 * tile.
 */
Ez.DialogWindow {
    id: root

    /** The chipset of the map */
    property int chipsetId: 0
    /** The tile chosen (from 0), -1 for none */
    property int tile: -1

    readonly property int columns: 6
    readonly property int count: 144

    objectName: "tileDialog"
    title: qsTr("Tile")

    Controls.ScrollView {
        id: scroll

        Kirigami.Theme.colorSet: Kirigami.Theme.View
        Kirigami.Theme.inherit: false
        // six rows in sight, the whole width
        implicitWidth: tiles.width + effectiveScrollBarWidth
        implicitHeight: 6 * tiles.cellSize
        contentWidth: tiles.width
        contentHeight: tiles.height
        Layout.fillWidth: true
        Layout.fillHeight: true
        background: Rectangle {
            color: Kirigami.Theme.backgroundColor
        }

        Ez.MapLayerView {
            id: tiles
            objectName: "tileView"
            chipsetId: root.chipsetId
            zoom: 2
            gridColumns: root.columns
            gridTiles: {
                const ids = []
                for (let i = 0; i < root.count; ++i) {
                    ids.push(10000 + i)
                }
                return ids
            }
            width: implicitWidth
            height: implicitHeight

            Rectangle {
                visible: root.tile >= 0
                x: (root.tile % root.columns) * tiles.cellSize
                y: Math.floor(root.tile / root.columns) * tiles.cellSize
                width: tiles.cellSize
                height: tiles.cellSize
                color: "transparent"
                border.color: "yellow"
                border.width: 2
            }

            MouseArea {
                anchors.fill: parent
                function pick(x, y) {
                    const column = Math.floor(x / tiles.cellSize)
                    const row = Math.floor(y / tiles.cellSize)
                    if (column < 0 || column >= root.columns || row < 0 || row * root.columns + column >= root.count) {
                        return false
                    }
                    root.tile = row * root.columns + column
                    return true
                }
                onPressed: (event) => pick(event.x, event.y)
                onDoubleClicked: (event) => {
                    if (pick(event.x, event.y)) {
                        root.accept()
                    }
                }
            }
        }
    }
}
