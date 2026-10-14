// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "TestBattleDialog"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * RPG Maker's Test Battle window of a monster group, over a database
 * window: the party, the formation and the special condition of the
 * battle, kept in the window's database (System); "Start Test" applies the
 * database and runs the EasyRPG Player in a battle test. The rules are
 * TestBattle's (Ez.TestBattleModel).
 *
 *   Ez.Editor.exec("TestBattleDialog", { database: page.database, troopId: 3 }, item)
 */
Ez.DialogWindow {
    id: root

    /** The root of the database window's copy */
    property var database: null
    /** The monster group fought */
    property int troopId: 0

    objectName: "TestBattleDialog"
    title: qsTr("Test Battle")

    Ez.TestBattleModel {
        id: battle
        database: root.database
        troopId: root.troopId
    }
    Ez.TestBattlePartyModel {
        id: party
        session: battle.session
    }

    function save() {
        party.save()
        battle.save()
    }
    onAccepting: (check) => save()

    // the buttons' name for scripts: buttonBoxTestBattle
    Binding {
        target: root.buttonBox
        property: "objectName"
        value: "buttonBoxTestBattle"
    }

    RowLayout {
        spacing: Ez.Metrics.sectionSpacing
        Layout.fillWidth: true
        Layout.fillHeight: true

        Ez.TestBattlePartyView {
            party: party
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        ColumnLayout {
            spacing: Ez.Metrics.sectionSpacing
            Layout.alignment: Qt.AlignTop

            Ez.Section {
                id: formationGroup
                objectName: "groupFormation"
                title: qsTr("Party Formation")
                // the formation places whoever is placed automatically: the
                // heroes (Battle Layout) or the monsters (the group's alignment)
                enabled: battle.formationEnabled

                Repeater {
                    model: [
                        { value: 1, text: qsTr("Use a Loose Formation") },
                        { value: 2, text: qsTr("Use a Tight Formation") },
                        { value: 0, text: qsTr("Use the following Terrain:") },
                    ]
                    Ez.RadioButton {
                        required property var modelData
                        objectName: "radioFormation" + modelData.value
                        text: modelData.text
                        checked: battle.formation === modelData.value
                        onToggled: {
                            if (checked) {
                                battle.formation = modelData.value
                            }
                        }
                    }
                }
                Ez.ComboBox {
                    objectName: "comboTerrain"
                    model: root.database ? root.database.list("terrains") : null
                    enabled: battle.terrainEnabled
                    implicitWidth: 160
                    Layout.fillWidth: true
                    Component.onCompleted: currentIndex = Math.max(0, indexOfValue(battle.terrainId))
                    onActivated: battle.terrainId = currentValue
                }
            }
            Ez.Section {
                objectName: "groupCondition"
                title: qsTr("Special Battle Condition")
                enabled: battle.rpg2k3

                Repeater {
                    model: [
                        { value: 0, text: qsTr("No Special Conditions") },
                        { value: 2, text: qsTr("Back Attack") },
                        { value: 3, text: qsTr("Surround Attack") },
                        { value: 4, text: qsTr("Pincers Attack") },
                        { value: 1, text: qsTr("Initiative") },
                    ]
                    Ez.RadioButton {
                        required property var modelData
                        objectName: "radioCondition" + modelData.value
                        text: modelData.text
                        checked: battle.condition === modelData.value
                        enabled: battle.conditionAllowed(modelData.value)
                        onToggled: {
                            if (checked) {
                                battle.condition = modelData.value
                            }
                        }
                        Controls.ToolTip.visible: hovered && !enabled
                        Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                        Controls.ToolTip.text: qsTr("Not with the manual placement of the Battle Layout")
                    }
                }
            }

            Ez.Button {
                objectName: "pushStartTest"
                text: qsTr("Start Test")
                Layout.fillWidth: true
                Controls.ToolTip.visible: hovered
                Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                Controls.ToolTip.text: qsTr("Saves the database, then starts the EasyRPG Player in a battle against this monster group")
                onClicked: {
                    root.save()
                    // RPG Maker closes the window when the battle starts
                    if (battle.startTest(this)) {
                        root.accept()
                    }
                }
            }
        }
    }
}
