// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "EnemyEncounterWidget"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * Enemy Encounter, in two columns as RPG Maker's: the monster group, the
 * background and the first strike on the left, the handlers of escape and
 * defeat and the special conditions (RPG Maker 2003) on the right.
 * CommandEditorRegistry adds or removes the handler branches on OK.
 */
RowLayout {
    id: root

    required property Ez.CommandEditSession session
    readonly property string title: qsTr("Enemy Encounter")

    spacing: 2 * Ez.Metrics.columnSpacing
    Layout.fillWidth: true

    /** The radio buttons of a choice in a column */
    component Radios: ColumnLayout {
        id: radios
        required property string fieldId
        required property string name
        required property var values
        spacing: Ez.Metrics.rowSpacing
        Repeater {
            model: radios.values
            CommandRadio {
                required property int modelData
                session: root.session
                fieldId: radios.fieldId
                value: modelData
                objectName: radios.name + modelData
            }
        }
    }

    ColumnLayout {
        spacing: Ez.Metrics.sectionSpacing
        Layout.alignment: Qt.AlignTop
        Layout.fillWidth: true

        Ez.Section {
            title: qsTr("Monster Group")
            GridLayout {
                columns: 2
                rowSpacing: Ez.Metrics.rowSpacing
                columnSpacing: Ez.Metrics.columnSpacing
                CommandRadio {
                    session: root.session
                    fieldId: "troopKind"
                    value: 0
                    objectName: "encounterTroopKind0"
                }
                CommandField {
                    session: root.session
                    fieldId: "troop"
                    name: "encounterTroop"
                    Layout.fillWidth: true
                    Layout.preferredWidth: 200
                }
                CommandRadio {
                    session: root.session
                    fieldId: "troopKind"
                    value: 1
                    objectName: "encounterTroopKind1"
                }
                CommandField {
                    session: root.session
                    fieldId: "troopVariable"
                    name: "encounterTroopVariable"
                    Layout.fillWidth: true
                    Layout.preferredWidth: 200
                }
            }
        }

        Ez.Section {
            title: qsTr("Battle Background")
            GridLayout {
                columns: 2
                rowSpacing: Ez.Metrics.rowSpacing
                columnSpacing: Ez.Metrics.columnSpacing
                CommandRadio {
                    session: root.session
                    fieldId: "background"
                    value: 0
                    objectName: "encounterBackground0"
                    Layout.columnSpan: 2
                }
                CommandRadio {
                    session: root.session
                    fieldId: "background"
                    value: 1
                    objectName: "encounterBackground1"
                }
                CommandField {
                    session: root.session
                    fieldId: "backgroundFile"
                    name: "encounterBackgroundFile"
                    Layout.fillWidth: true
                    Layout.preferredWidth: 200
                }
                CommandRadio {
                    session: root.session
                    fieldId: "background"
                    value: 2
                    objectName: "encounterBackground2"
                }
                CommandField {
                    session: root.session
                    fieldId: "terrain"
                    name: "encounterTerrain"
                    Layout.fillWidth: true
                    Layout.preferredWidth: 200
                }
                Ez.FormLabel {
                    text: qsTr("Formation")
                }
                CommandField {
                    session: root.session
                    fieldId: "formation"
                    name: "encounterFormation"
                    Layout.fillWidth: true
                }
            }
        }

        Ez.Section {
            title: qsTr("Option")
            CommandField {
                session: root.session
                fieldId: "firstStrike"
                name: "encounterFirstStrike"
            }
        }
        Item {
            Layout.fillHeight: true
        }
    }

    ColumnLayout {
        spacing: Ez.Metrics.sectionSpacing
        Layout.alignment: Qt.AlignTop

        Ez.Section {
            title: qsTr("Escape Handler")
            Radios {
                fieldId: "escape"
                name: "encounterEscape"
                values: [0, 1, 2]
            }
        }
        Ez.Section {
            title: qsTr("Defeat Handler")
            Radios {
                fieldId: "defeat"
                name: "encounterDefeat"
                values: [0, 1]
            }
        }
        Ez.Section {
            title: qsTr("Special Battle Conditions")
            Radios {
                fieldId: "condition"
                name: "encounterCondition"
                values: [0, 1, 2, 3, 4]
            }
        }
        Item {
            Layout.fillHeight: true
        }
    }
}
