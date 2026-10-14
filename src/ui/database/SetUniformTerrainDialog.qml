// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "ChipsetWidget"

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * RPG Maker's "Set Uniform Terrain" of the Chipset page: OK gives the
 * terrain chosen to every tile of the lower layer of the chipset shown.
 *
 *   Ez.Editor.exec("SetUniformTerrainDialog", { database: page.database, canvas: tiles, terrain: 0 }, item)
 */
Ez.DialogWindow {
    id: root

    /** The root of the database (its terrains) */
    property var database: null
    /** The tiles given the terrain (an Ez.ChipsetCanvas) */
    property var canvas: null
    /** The terrain shown first (its index) */
    property int terrain: 0

    objectName: "SetUniformTerrainDialog"
    title: qsTr("Set Uniform Terrain")

    onAccepting: (check) => {
        if (canvas && terrains.currentIndex >= 0) {
            canvas.setUniformTerrain(Number(terrains.currentValue))
        }
    }

    Ez.Section {
        title: qsTr("Select Terrain")
        Ez.RefComboBox {
            id: terrains
            objectName: "comboUniformTerrain"
            database: root.database
            listKey: "terrains"
            Layout.fillWidth: true
            Layout.minimumWidth: 200
            Component.onCompleted: currentIndex = Math.max(0, root.terrain)
        }
    }
}
