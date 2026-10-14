// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "ChangeVehicleGraphicWidget"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * Change Vehicle Graphic: the vehicle (p0) and its character (the string of
 * the command, its index in p1). Another vehicle chosen shows its graphic
 * of the database.
 */
RowLayout {
    id: root

    required property Ez.CommandEditSession session
    readonly property string title: qsTranslate("EventCommandPalette", "Change Vehicle Graphic")
    readonly property int vehicle: session.revision < 0 ? 0 : session.read("vehicle")
    readonly property var keys: ["boat", "ship", "airship"]
    property bool opened: false

    /** Shows the graphic of the vehicle chosen */
    function applyDefault() {
        if (vehicle >= 0 && vehicle < keys.length) {
            root.session.set("/string", root.session.database.str("system/%1_name".arg(keys[vehicle])))
            root.session.set("/parameters/1", root.session.database.num("system/%1_index".arg(keys[vehicle])))
        }
    }

    // the graphic of the vehicle chosen; a new command too (RPG Maker's)
    onVehicleChanged: if (opened) applyDefault()
    Component.onCompleted: {
        if (session.isNew && (session.value("/string") ?? "") === "") {
            applyDefault()
        }
        opened = true
    }

    spacing: Ez.Metrics.columnSpacing
    Layout.fillWidth: true

    Ez.Section {
        title: qsTr("Vehicle")
        Layout.alignment: Qt.AlignTop
        Repeater {
            model: ["radioBoat", "radioSkiff", "radioAirship"]
            CommandRadio {
                required property string modelData
                required property int index
                session: root.session
                fieldId: "vehicle"
                value: index
                objectName: modelData
            }
        }
    }
    Ez.Section {
        title: qsTr("Graphic")
        Layout.alignment: Qt.AlignTop
        Ez.SpriteField {
            objectName: "viewVehicle"
            preview: "CharSet"
            jsonData: root.session
            key: "/string"
            indexKey: "/parameters/1"
        }
    }
    Item {
        Layout.fillWidth: true
    }
}
