// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "EncounterList"

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * Define Encounter: the monster group met, chosen in the list of the
 * database (searchable). Drawn in the window of the list.
 */
Ez.Dialog {
    id: root

    /** The monster group shown first */
    property int troopId: 1

    /** OK: the monster group chosen */
    signal chosen(int troopId)

    objectName: "encounterDialog"
    title: qsTr("Define Encounter")
    anchors.centerIn: Controls.Overlay.overlay
    standardButtons: Controls.Dialog.Ok | Controls.Dialog.Cancel
    font: Ez.Metrics.font
    padding: Ez.Metrics.dialogMargin

    onAboutToShow: troop.currentIndex = Math.max(0, Math.min(troopId - 1, troop.count - 1))
    onOpened: troop.forceActiveFocus()
    onAccepted: {
        if (troop.currentIndex >= 0) {
            chosen(troop.currentValue)
        }
    }

    contentItem: Ez.Section {
        title: qsTr("Monster Group")

        Ez.RefComboBox {
            id: troop
            objectName: "comboTroop"
            database: Ez.ProjectData.database()
            listKey: "troops"
            Layout.fillWidth: true
            Layout.preferredWidth: 220
        }
    }
}
