// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "AreaPropertiesDialog"

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * Area Properties (RPG Maker's TFORMEDAREA), of a new area too: the name,
 * the boundaries on the map the area is on (typed, or a box dragged on the
 * map: Define Area Boundaries) and the monster groups met inside it.
 */
Ez.DialogWindow {
    id: root

    required property Ez.AreaPropertiesForm form

    objectName: "AreaPropertiesDialog"
    title: qsTr("Area Properties")
    preferredWidth: 340
    height: Math.max(minimumHeight, 440)

    Component.onCompleted: {
        // the position first: it sets the largest width and height
        left.value = form.area.x
        top.value = form.area.y
        areaWidth.value = form.area.width
        areaHeight.value = form.area.height
    }
    onAccepted: form.area = Qt.rect(left.value, top.value, areaWidth.value, areaHeight.value)

    Ez.Section {
        objectName: "groupName"
        title: qsTr("Name")
        Layout.fillHeight: false
        Ez.TextField {
            objectName: "lineName"
            jsonData: root.form.info
            key: "name"
            // the length RPG Maker allows
            maximumLength: 24
            Layout.fillWidth: true
        }
    }

    // the rectangle on the map, inside it
    Ez.Section {
        objectName: "groupBoundaries"
        title: qsTr("Boundaries")
        Layout.fillHeight: false
        GridLayout {
            columns: 4
            rowSpacing: Ez.Metrics.rowSpacing
            columnSpacing: Ez.Metrics.columnSpacing

            Ez.FormLabel {
                text: qsTr("X")
            }
            Ez.SpinBox {
                id: left
                objectName: "spinLeft"
                from: 0
                to: root.form.mapWidth - 1
                Layout.fillWidth: true
            }
            Ez.FormLabel {
                text: qsTr("Y")
            }
            Ez.SpinBox {
                id: top
                objectName: "spinTop"
                from: 0
                to: root.form.mapHeight - 1
                Layout.fillWidth: true
            }
            Ez.FormLabel {
                text: qsTr("Width")
            }
            Ez.SpinBox {
                id: areaWidth
                objectName: "spinWidth"
                from: 1
                to: root.form.mapWidth - left.value
                Layout.fillWidth: true
            }
            Ez.FormLabel {
                text: qsTr("Height")
            }
            Ez.SpinBox {
                id: areaHeight
                objectName: "spinHeight"
                from: 1
                to: root.form.mapHeight - top.value
                Layout.fillWidth: true
            }
            Ez.Label {
                objectName: "labelMapSize"
                text: qsTr("Map size: %1 x %2").arg(root.form.mapWidth).arg(root.form.mapHeight)
                font: Ez.Metrics.font
                color: Ez.Metrics.mutedTextColor
                Layout.columnSpan: 2
                Layout.fillWidth: true
            }
            // RPG Maker's [...] button: a box dragged on the map
            Ez.Button {
                id: define
                objectName: "pushBoundaries"
                text: qsTr("Define Boundaries...")
                Layout.columnSpan: 2
                Layout.fillWidth: true
                onClicked: {
                    const area = root.form.defineBoundaries(
                        Qt.rect(left.value, top.value, areaWidth.value, areaHeight.value), define)
                    if (area.width > 0 && area.height > 0) {
                        left.value = area.x
                        top.value = area.y
                        areaWidth.value = area.width
                        areaHeight.value = area.height
                    }
                }
            }
        }
    }

    Ez.Section {
        objectName: "groupEncounters"
        title: qsTr("Enemy Encounters")
        Layout.fillHeight: true

        EncounterTable {
            id: encounters
            encounters: root.form.encounters
            visibleRows: 5
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
        RowLayout {
            spacing: Ez.Metrics.columnSpacing
            Ez.Button {
                objectName: "pushAddEncounter"
                text: qsTr("Add")
                Layout.fillWidth: true
                onClicked: encounters.add()
            }
            Ez.Button {
                objectName: "pushRemoveEncounter"
                text: qsTr("Delete")
                Layout.fillWidth: true
                onClicked: encounters.remove()
            }
        }
    }
}
