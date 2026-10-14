// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "ChangeActorGraphicWidget"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * Change Hero Graphic: the hero (p0), the character (the string of the
 * command, its index in p1) and its transparency. Another hero chosen shows
 * the character of the database.
 */
ColumnLayout {
    id: root

    required property Ez.CommandEditSession session
    readonly property string title: qsTranslate("EventCommandPalette", "Change Hero Graphic")
    readonly property int hero: session.revision < 0 ? 0 : session.read("hero")
    property bool opened: false

    /** Shows the character of the hero chosen */
    function applyDefault() {
        if (hero > 0) {
            root.session.set("/string", root.session.database.str("actors/%1/character_name".arg(hero - 1)))
            root.session.set("/parameters/1", root.session.database.num("actors/%1/character_index".arg(hero - 1)))
        }
    }

    // the character of the hero chosen; a new command too (RPG Maker's)
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
        title: qsTr("Character Graphic")
        Ez.SpriteField {
            objectName: "viewChar"
            preview: "CharSet"
            jsonData: root.session
            key: "/string"
            indexKey: "/parameters/1"
            transparent: root.session.revision < 0 ? false : root.session.read("translucent") !== 0
        }
        CommandField {
            session: root.session
            fieldId: "translucent"
            name: "checkTrans_arg2"
        }
    }
    Item {
        Layout.fillHeight: true
    }
}
