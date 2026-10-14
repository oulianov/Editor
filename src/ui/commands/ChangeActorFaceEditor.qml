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

    /** Shows the face of the hero chosen */
    function applyDefault() {
        if (hero > 0) {
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
            preview: "FaceSet"
            jsonData: root.session
            key: "/string"
            indexKey: "/parameters/1"
            zoom: 2
        }
    }
    Item {
        Layout.fillHeight: true
    }
}
