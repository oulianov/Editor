// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "ChangeActorFaceWidget"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * Change Hero Face: the hero (p0) and the face (the string of the command,
 * its index in p1). Another hero chosen shows the face of the database.
 */
ColumnLayout {
    id: root

    required property Ez.CommandEditSession session
    readonly property string title: qsTranslate("EventCommandPalette", "Change Hero Face")
    readonly property int hero: session.revision < 0 ? 0 : session.read("hero")
    property bool opened: false
    /** Maniac Patch: the hero from a variable (its mode in p2) */
    readonly property bool heroFromVariable: session.revision < 0 ? false : session.read("heroMode") !== 0
    /** Maniac Patch: the file from a string or the index from a variable (modes in p2) */
    readonly property bool modal: session.revision < 0 ? false : session.read("fileNameMode") !== 0 || session.read("fileIndexMode") !== 0

    /** Shows the face of the hero chosen */
    function applyDefault() {
        if (hero > 0 && !heroFromVariable && !modal) {
            root.session.set("/string", root.session.database.str("actors/%1/face_name".arg(hero - 1)))
            root.session.set("/parameters/1", root.session.database.num("actors/%1/face_index".arg(hero - 1)))
        }
    }

    // the face of the hero chosen; a new command too (RPG Maker's)
    onHeroChanged: if (opened) applyDefault()
    Component.onCompleted: {
        if (session.isNew && (session.value("/string") ?? "") === "") {
            applyDefault()
        }
        opened = true
    }

    spacing: Ez.Metrics.sectionSpacing
    Layout.fillWidth: true

    Ez.Section {
        title: qsTr("Actor")
        CommandField {
            session: root.session
            fieldId: "hero"
            name: "comboActor_arg0"
            Layout.fillWidth: true
            Layout.preferredWidth: 240
        }
    }
    Ez.Section {
        title: qsTr("Faceset")
        Ez.SpriteField {
            objectName: "viewFace"
            // not the face of a string or of a variable
            enabled: !root.modal
            preview: "FaceSet"
            jsonData: root.session
            key: "/string"
            indexKey: "/parameters/1"
            zoom: 2
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
    Item {
        Layout.fillHeight: true
    }
}
