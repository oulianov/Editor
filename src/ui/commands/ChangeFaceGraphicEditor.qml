// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "ChangeFaceGraphicWidget"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * Select Face Graphic: the face (the string of the command, its index in
 * p0; no file erases the face), where it is shown and whether it is flipped.
 */
ColumnLayout {
    id: root

    required property Ez.CommandEditSession session
    readonly property string title: qsTr("Face Graphics")
    readonly property int revision: session.revision
    readonly property string fileName: revision < 0 ? "" : session.value("/string") ?? ""

    spacing: Ez.Metrics.sectionSpacing
    Layout.fillWidth: true

    Ez.Section {
        title: qsTr("Face Graphic")
        RowLayout {
            spacing: Ez.Metrics.columnSpacing
            Ez.SpriteField {
                id: face
                objectName: "faceView"
                preview: "FaceSet"
                title: qsTr("Select Face Graphic")
                jsonData: root.session
                key: "/string"
                indexKey: "/parameters/0"
                zoom: 2
                Layout.alignment: Qt.AlignTop
            }
            ColumnLayout {
                spacing: Ez.Metrics.rowSpacing
                Layout.alignment: Qt.AlignTop
                Layout.fillWidth: true
                Ez.Button {
                    objectName: "faceSet"
                    text: qsTr("Set...")
                    onClicked: face.pick()
                }
                Ez.Button {
                    objectName: "faceErase"
                    text: qsTr("Erase")
                    onClicked: root.session.set("/string", "")
                }
                Ez.FormLabel {
                    objectName: "faceFile"
                    text: root.fileName === "" ? qsTr("(None)")
                                               : qsTr("%1, face %2").arg(root.fileName).arg(root.revision < 0 ? 0 : root.session.read("face"))
                }
            }
        }
    }

    Ez.Section {
        title: qsTr("Display Position")
        RowLayout {
            spacing: 2 * Ez.Metrics.columnSpacing
            Repeater {
                model: 2
                CommandRadio {
                    required property int index
                    session: root.session
                    fieldId: "position"
                    value: index
                    objectName: "facePosition" + index
                }
            }
        }
    }

    Ez.Section {
        title: qsTr("Option")
        CommandField {
            session: root.session
            fieldId: "flip"
            name: "faceFlip"
        }
    }

    Item {
        Layout.fillHeight: true
    }
}
