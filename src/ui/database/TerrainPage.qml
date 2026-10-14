// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "TerrainWidget"

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * A terrain of the database, in RPG Maker 2003's three columns: its name,
 * damage, encounters and how sprites stand on it; the vehicles, its
 * footsteps and its special battles; the background of its battles, a
 * backdrop or two frames. RPG Maker 2000 only has the backdrop: the rest is
 * greyed in its games.
 */
Ez.EntryPage {
    id: root

    /** The background is made of frames (RPG Maker 2003) */
    readonly property bool frame: rpg2k3 && backgroundType.value === 1

    fillWidth: true

    RowLayout {
        id: columns
        spacing: Ez.Metrics.sectionSpacing
        Layout.fillWidth: true

        // the name, damage, encounters, sprites and grid
        ColumnLayout {
            spacing: Ez.Metrics.sectionSpacing
            Layout.alignment: Qt.AlignTop
            Layout.fillWidth: true
            Layout.preferredWidth: (root.column.room - 2 * columns.spacing) / 3
            Layout.minimumWidth: implicitWidth

            Ez.Section {
                objectName: "groupName"
                title: qsTr("Name")
                Ez.TextField {
                    objectName: "lineName"
                    jsonData: root.entry
                    key: "name"
                    limit: 16
                    Layout.fillWidth: true
                }
            }
            Ez.Section {
                objectName: "groupDamage"
                title: qsTr("Damage")
                Ez.SpinBox {
                    objectName: "spinDamage"
                    jsonData: root.entry
                    key: "damage"
                    from: -999
                    to: 999
                    Layout.fillWidth: true
                }
            }
            Ez.Section {
                objectName: "groupEncounter"
                title: qsTr("Encounter Multiplier")
                Ez.SpinBox {
                    objectName: "spinEncounterRate"
                    jsonData: root.entry
                    key: "encounter_rate"
                    from: 0
                    to: 1000
                    suffix: qsTr("%")
                    Layout.fillWidth: true
                }
            }
            Ez.Section {
                objectName: "groupSpriteDisplay"
                title: qsTr("Sprite Display")
                Ez.RadioGroup {
                    jsonData: root.entry
                    key: "bush_depth"
                    choices: [
                        { name: "radioBushNormal", text: qsTr("Normal") },
                        { name: "radioBushThird", text: qsTr("Obscure Bottom Third") },
                        { name: "radioBushHalf", text: qsTr("Obscure Bottom Half") },
                        { name: "radioBushFull", text: qsTr("Semi-Transparent") },
                    ]
                }
            }
            Ez.Section {
                objectName: "groupGrid"
                title: qsTr("Grid Depth of Field")
                enabled: root.rpg2k3
                Ez.RadioGroup {
                    jsonData: root.entry
                    key: "grid_location"
                    columns: 2
                    choices: [
                        { name: "radioGridShallow", text: qsTr("Shallow") },
                        { name: "radioGridDeep", text: qsTr("Deep") },
                    ]
                }
            }
        }

        // the vehicles, the footsteps and the special battles
        ColumnLayout {
            spacing: Ez.Metrics.sectionSpacing
            Layout.alignment: Qt.AlignTop
            Layout.fillWidth: true
            Layout.preferredWidth: (root.column.room - 2 * columns.spacing) / 3
            Layout.minimumWidth: implicitWidth

            Ez.Section {
                objectName: "groupVehicles"
                title: qsTr("Vehicle Interaction")
                Ez.CheckBox {
                    objectName: "checkBoatPass"
                    text: qsTr("Skiff can Pass")
                    jsonData: root.entry
                    key: "boat_pass"
                }
                Ez.CheckBox {
                    objectName: "checkShipPass"
                    text: qsTr("Ship can Pass")
                    jsonData: root.entry
                    key: "ship_pass"
                }
                Ez.CheckBox {
                    objectName: "checkAirshipPass"
                    text: qsTr("Airship can Pass")
                    jsonData: root.entry
                    key: "airship_pass"
                }
                Ez.CheckBox {
                    objectName: "checkAirshipLand"
                    text: qsTr("Airship can Land")
                    jsonData: root.entry
                    key: "airship_land"
                }
            }
            Ez.Section {
                objectName: "groupSound"
                title: qsTr("Sound Effect")
                enabled: root.rpg2k3
                Ez.AudioField {
                    fieldName: "lineFootstep"
                    jsonData: root.entry
                    key: "footstep"
                }
                Ez.CheckBox {
                    objectName: "checkOnDamageSe"
                    text: qsTr("Play only if Damaged")
                    jsonData: root.entry
                    key: "on_damage_se"
                }
            }
            Ez.Section {
                objectName: "groupSpecialBattle"
                title: qsTr("Special Battle Types")
                enabled: root.rpg2k3
                // a check box, its chance (greyed while unchecked) and who it favors
                GridLayout {
                    columns: 3
                    rowSpacing: Ez.Metrics.rowSpacing
                    columnSpacing: Ez.Metrics.columnSpacing

                    Ez.CheckBox {
                        id: initiative
                        objectName: "checkInitiative"
                        text: qsTr("Initiative")
                        jsonData: root.entry
                        key: "special_flags/back_party"
                        Layout.fillWidth: true
                    }
                    Ez.SpinBox {
                        objectName: "spinInitiative"
                        jsonData: root.entry
                        key: "special_back_party"
                        from: 0
                        to: 100
                        suffix: qsTr("%")
                        enabled: initiative.checked
                    }
                    Ez.Label {
                        objectName: "labelInitiative"
                        text: qsTr("(Party)")
                        font: Ez.Metrics.font
                    }
                    Ez.CheckBox {
                        id: backAttack
                        objectName: "checkBackAttack"
                        text: qsTr("Back Attack")
                        jsonData: root.entry
                        key: "special_flags/back_enemies"
                        Layout.fillWidth: true
                    }
                    Ez.SpinBox {
                        objectName: "spinBackAttack"
                        jsonData: root.entry
                        key: "special_back_enemies"
                        from: 0
                        to: 100
                        suffix: qsTr("%")
                        enabled: backAttack.checked
                    }
                    Ez.Label {
                        objectName: "labelBackAttack"
                        text: qsTr("(Enemy)")
                        font: Ez.Metrics.font
                    }
                    Ez.CheckBox {
                        id: surroundAttack
                        objectName: "checkSurroundAttack"
                        text: qsTr("Surround Attack")
                        jsonData: root.entry
                        key: "special_flags/lateral_party"
                        Layout.fillWidth: true
                    }
                    Ez.SpinBox {
                        objectName: "spinSurroundAttack"
                        jsonData: root.entry
                        key: "special_lateral_party"
                        from: 0
                        to: 100
                        suffix: qsTr("%")
                        enabled: surroundAttack.checked
                    }
                    Ez.Label {
                        objectName: "labelSurroundAttack"
                        text: qsTr("(Party)")
                        font: Ez.Metrics.font
                    }
                    Ez.CheckBox {
                        id: pincersAttack
                        objectName: "checkPincersAttack"
                        text: qsTr("Pincers Attack")
                        jsonData: root.entry
                        key: "special_flags/lateral_enemies"
                        Layout.fillWidth: true
                    }
                    Ez.SpinBox {
                        objectName: "spinPincersAttack"
                        jsonData: root.entry
                        key: "special_lateral_enemies"
                        from: 0
                        to: 100
                        suffix: qsTr("%")
                        enabled: pincersAttack.checked
                    }
                    Ez.Label {
                        objectName: "labelPincersAttack"
                        text: qsTr("(Enemy)")
                        font: Ez.Metrics.font
                    }
                }
            }
        }

        // the background of the battles: a backdrop, or frames (RPG Maker 2003)
        ColumnLayout {
            spacing: Ez.Metrics.sectionSpacing
            Layout.alignment: Qt.AlignTop
            Layout.fillWidth: true
            Layout.preferredWidth: (root.column.room - 2 * columns.spacing) / 3
            Layout.minimumWidth: implicitWidth

            Ez.Section {
                objectName: "groupBackground"
                title: qsTr("Background Association")

                Ez.JsonField {
                    id: backgroundType
                    source: root.entry
                    path: "background_type"
                }

                Ez.RadioButton {
                    objectName: "radioBackground"
                    text: qsTr("Background")
                    jsonData: root.entry
                    key: "background_type"
                    value: 0
                    enabled: root.rpg2k3
                }
                Ez.FileField {
                    fieldName: "lineBackdrop"
                    buttonName: "toolBackdrop"
                    jsonData: root.entry
                    key: "background_name"
                    folder: "Backdrop"
                    enabled: !root.frame
                }
                Ez.RadioButton {
                    objectName: "radioFrame"
                    text: qsTr("Frame")
                    jsonData: root.entry
                    key: "background_type"
                    value: 1
                    enabled: root.rpg2k3
                }
                Ez.Section {
                    objectName: "groupFrameBack"
                    title: qsTr("Background")
                    enabled: root.frame

                    Ez.FileField {
                        fieldName: "lineFrameBack"
                        buttonName: "toolFrameBack"
                        jsonData: root.entry
                        key: "background_a_name"
                        folder: "Frame"
                        button.text: qsTr("Set")
                    }
                    Ez.Form {
                        Ez.CheckBox {
                            id: backScrollH
                            objectName: "checkFrameBackScrollH"
                            text: qsTr("Horizontal Scroll")
                            jsonData: root.entry
                            key: "background_a_scrollh"
                            Layout.fillWidth: true
                        }
                        Ez.SpinBox {
                            objectName: "spinFrameBackScrollH"
                            jsonData: root.entry
                            key: "background_a_scrollh_speed"
                            from: -8
                            to: 8
                            enabled: backScrollH.checked
                        }
                        Ez.CheckBox {
                            id: backScrollV
                            objectName: "checkFrameBackScrollV"
                            text: qsTr("Vertical Scroll")
                            jsonData: root.entry
                            key: "background_a_scrollv"
                            Layout.fillWidth: true
                        }
                        Ez.SpinBox {
                            objectName: "spinFrameBackScrollV"
                            jsonData: root.entry
                            key: "background_a_scrollv_speed"
                            from: -8
                            to: 8
                            enabled: backScrollV.checked
                        }
                    }
                }
                Ez.Section {
                    objectName: "groupFrameFore"
                    title: qsTr("Foreground")
                    enabled: root.frame

                    Ez.CheckBox {
                        id: foreground
                        objectName: "checkFrameFore"
                        text: qsTr("Use")
                        jsonData: root.entry
                        key: "background_b"
                    }
                    Ez.FileField {
                        fieldName: "lineFrameFore"
                        buttonName: "toolFrameFore"
                        jsonData: root.entry
                        key: "background_b_name"
                        folder: "Frame"
                        button.text: qsTr("Set")
                        enabled: foreground.checked
                    }
                    Ez.Form {
                        enabled: foreground.checked

                        Ez.CheckBox {
                            id: foreScrollH
                            objectName: "checkFrameForeScrollH"
                            text: qsTr("Horizontal Scroll")
                            jsonData: root.entry
                            key: "background_b_scrollh"
                            Layout.fillWidth: true
                        }
                        Ez.SpinBox {
                            objectName: "spinFrameForeScrollH"
                            jsonData: root.entry
                            key: "background_b_scrollh_speed"
                            from: -8
                            to: 8
                            enabled: foreScrollH.checked
                        }
                        Ez.CheckBox {
                            id: foreScrollV
                            objectName: "checkFrameForeScrollV"
                            text: qsTr("Vertical Scroll")
                            jsonData: root.entry
                            key: "background_b_scrollv"
                            Layout.fillWidth: true
                        }
                        Ez.SpinBox {
                            objectName: "spinFrameForeScrollV"
                            jsonData: root.entry
                            key: "background_b_scrollv_speed"
                            from: -8
                            to: 8
                            enabled: foreScrollV.checked
                        }
                    }
                }
                Ez.Note {
                    objectName: "labelMapOverride"
                    text: qsTr("* Map Properties can override this setting.")
                }
            }
        }
    }
}
