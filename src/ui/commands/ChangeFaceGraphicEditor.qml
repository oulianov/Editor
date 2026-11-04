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
    /** Maniac Patch: the file from a string or the index from a variable (modes in p3) */
    readonly property bool modal: revision < 0 ? false : session.read("fileNameMode") !== 0 || session.read("fileIndexMode") !== 0
    /** Maniac Patch: an animation of the face, or its settings (p5 to p10) */
    readonly property bool animated: revision < 0 ? false : (session.value("/parameters") ?? []).slice(5).some(one => Number(one) !== 0)

    spacing: Ez.Metrics.sectionSpacing
    Layout.fillWidth: true

    Ez.Section {
        title: qsTr("Face Graphic")
        RowLayout {
            spacing: Ez.Metrics.columnSpacing
            Ez.SpriteField {
                id: face
                objectName: "faceView"
                // not the face of a string or of a variable
                enabled: !root.modal
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
                    enabled: !root.modal
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

    Ez.Section {
        // Maniac Patch: the file from a string, the index from a variable
        title: qsTranslate("ManiacCommandWidget", "Maniac Patch")
        visible: root.session.maniac || root.modal
        SchemaRows {
            session: root.session
            ids: ["fileName", "fileIndex"]
            names: ({ fileName: "faceManiacFile", fileIndex: "faceManiacIndex" })
        }
    }

    Ez.Section {
        // Maniac Patch: the faces from the index to the end index in turn
        title: qsTranslate("ManiacCommandWidget", "Animation")
        visible: root.session.maniac || root.animated
        SchemaRows {
            session: root.session
            ids: ["animated", "endIndex", "interval", "intervalVariation", "framesPerCell", "framesVariation",
                  "forceUpdate", "turnaround", "once", "intervalPositive", "framesPositive", "turnaroundInterval"]
            names: ({ animated: "faceAnimated", endIndex: "faceEndIndex", interval: "faceInterval",
                      intervalVariation: "faceIntervalVariation", framesPerCell: "faceFramesPerCell",
                      framesVariation: "faceFramesVariation", forceUpdate: "faceForceUpdate", turnaround: "faceTurnaround",
                      once: "faceOnce", intervalPositive: "faceIntervalPositive", framesPositive: "faceFramesPositive",
                      turnaroundInterval: "faceTurnaroundInterval" })
        }
    }

    Item {
        Layout.fillHeight: true
    }
}
