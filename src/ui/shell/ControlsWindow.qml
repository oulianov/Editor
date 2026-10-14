// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

// The Ez controls bound to the project's data, for the scripts
// (tests/scripts/qml_controls.txt: "qml-open ControlsWindow"): what one
// control writes, the others showing the same value follow

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

Ez.DialogWindow {
    id: root

    readonly property var database: Ez.ProjectData.database()
    readonly property var hero: database ? database.subtree("actors/0") : null

    objectName: "qmlControls"
    title: "Ez controls"
    preferredWidth: 420

    Ez.Section {
        title: "First hero"
        Ez.Form {
            Ez.FormLabel {
                text: "Name"
            }
            Ez.TextField {
                objectName: "controlName"
                jsonData: root.hero
                key: "name"
                limit: 12
                Layout.fillWidth: true
            }
            Ez.FormLabel {
                text: "Again"
            }
            Ez.Label {
                objectName: "controlNameShown"
                text: shownName.value ?? ""
                Ez.JsonField {
                    id: shownName
                    source: root.database
                    path: "actors/0/name"
                }
            }
            Ez.FormLabel {
                text: "Level"
            }
            RowLayout {
                spacing: Ez.Metrics.columnSpacing
                Ez.SpinBox {
                    objectName: "controlLevel"
                    jsonData: root.hero
                    key: "initial_level"
                    from: 1
                    to: 99
                }
                Ez.Label {
                    objectName: "controlLevelShown"
                    text: shownLevel.value ?? ""
                    Ez.JsonField {
                        id: shownLevel
                        source: root.database
                        path: "actors/0/initial_level"
                    }
                }
            }
            Ez.FormLabel {
                text: "Class"
            }
            Ez.ComboBox {
                objectName: "controlClass"
                jsonData: root.hero
                key: "class_id"
                model: {
                    const classes = root.database ? root.database.list("classes") : null
                    if (classes) {
                        classes.fallbackString = "(None)"
                    }
                    return classes
                }
                Layout.fillWidth: true
            }
        }
        Ez.CheckBox {
            objectName: "controlTransparent"
            text: "Translucent"
            jsonData: root.hero
            key: "transparent"
        }
        Ez.CheckBox {
            objectName: "controlTransparentAgain"
            text: "Translucent (again)"
            jsonData: root.hero
            key: "transparent"
        }
    }

    Ez.Section {
        title: "First item"
        Ez.BitCheckBox {
            objectName: "controlItemState2"
            text: "Changes condition 2"
            jsonData: root.database ? root.database.subtree("items/0") : null
            key: "state_set"
            bit: 1
        }
        Ez.BitCheckBox {
            objectName: "controlItemState2Again"
            text: "Changes condition 2 (again)"
            jsonData: root.database
            key: "items/0/state_set"
            bit: 1
        }
        // a number of a wide range, with a suffix (scrubbed by
        // tests/scripts/qml_spinbox_scrub.txt)
        RowLayout {
            spacing: Ez.Metrics.columnSpacing
            Ez.SpinBox {
                objectName: "controlPrice"
                jsonData: root.database ? root.database.subtree("items/0") : null
                key: "price"
                from: 0
                to: 999999
                suffix: " G"
            }
            Ez.Label {
                objectName: "controlPriceShown"
                text: shownPrice.value ?? ""
                Ez.JsonField {
                    id: shownPrice
                    source: root.database
                    path: "items/0/price"
                }
            }
        }
        RowLayout {
            spacing: Ez.Metrics.columnSpacing
            Ez.RadioButton {
                objectName: "controlScope0"
                text: "Scope 0"
                jsonData: root.database ? root.database.subtree("items/0") : null
                key: "type"
                value: 0
            }
            Ez.RadioButton {
                objectName: "controlScope1"
                text: "Scope 1"
                jsonData: root.database ? root.database.subtree("items/0") : null
                key: "type"
                value: 1
            }
        }
    }
}
