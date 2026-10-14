// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

// The fields opening the pickers, bound to the project's data, for the
// scripts (tests/scripts/qml_pickers.txt: "qml-open PickerFieldsWindow"):
// the first hero's graphics, the first monster, the title music, the cursor
// sound, the first terrain's background, and a switch

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

Ez.DialogWindow {
    id: root

    readonly property var database: Ez.ProjectData.database()
    readonly property var hero: database ? database.subtree("actors/0") : null
    readonly property var monster: database ? database.subtree("enemies/0") : null
    readonly property var system: database ? database.subtree("system") : null
    readonly property var terrain: database ? database.subtree("terrains/0") : null

    objectName: "qmlPickerFields"
    title: "Ez picker fields"
    preferredWidth: 420

    Ez.Section {
        title: "First hero"
        RowLayout {
            spacing: Ez.Metrics.columnSpacing
            Ez.SpriteField {
                objectName: "graphicsCharset"
                jsonData: root.hero
                preview: "CharSet"
                key: "character_name"
                indexKey: "character_index"
            }
            Ez.SpriteField {
                objectName: "graphicsFaceset"
                jsonData: root.hero
                preview: "FaceSet"
                key: "face_name"
                indexKey: "face_index"
            }
            Ez.SpriteField {
                objectName: "graphicsPose"
                jsonData: root.hero
                preview: "CharSetPose"
                key: "character_name"
                indexKey: "character_index"
            }
        }
        Ez.Label {
            objectName: "heroShown"
            font: Ez.Metrics.font
            text: (character.value ?? "") + " " + (characterIndex.value ?? "") + ", " + (face.value ?? "") + " " + (faceIndex.value ?? "")
            Ez.JsonField {
                id: character
                source: root.hero
                path: "character_name"
            }
            Ez.JsonField {
                id: characterIndex
                source: root.hero
                path: "character_index"
            }
            Ez.JsonField {
                id: face
                source: root.hero
                path: "face_name"
            }
            Ez.JsonField {
                id: faceIndex
                source: root.hero
                path: "face_index"
            }
        }
    }

    Ez.Section {
        title: "First monster"
        Ez.SpriteField {
            objectName: "graphicsMonster"
            jsonData: root.monster
            preview: "Monster"
            key: "battler_name"
            hueKey: "battler_hue"
        }
    }

    Ez.Section {
        title: "Files"
        Ez.Form {
            Ez.FormLabel {
                text: "Title music"
            }
            Ez.AudioField {
                fieldName: "lineTitleMusic"
                jsonData: root.system
                key: "title_music"
                music: true
            }
            Ez.FormLabel {
                text: "Cursor"
            }
            Ez.AudioField {
                fieldName: "lineCursorSe"
                jsonData: root.system
                key: "cursor_se"
            }
            Ez.FormLabel {
                text: "Background"
            }
            Ez.FileField {
                fieldName: "lineBackground"
                jsonData: root.terrain
                key: "background_name"
                folder: "Backdrop"
            }
        }
        Ez.Label {
            objectName: "musicShown"
            font: Ez.Metrics.font
            text: (musicName.value ?? "") + " " + (musicVolume.value ?? "") + "%"
            Ez.JsonField {
                id: musicName
                source: root.system
                path: "title_music/name"
            }
            Ez.JsonField {
                id: musicVolume
                source: root.system
                path: "title_music/volume"
            }
        }
    }

    Ez.Section {
        title: "Switch"
        RowLayout {
            spacing: Ez.Metrics.columnSpacing
            Ez.Button {
                objectName: "pickSwitch"
                text: "Switch..."
                onClicked: {
                    const id = Ez.Pickers.pickEntry({ list: "switches", id: 2, none: true }, this)
                    switchShown.text = id === null || id === undefined ? "cancel" : String(id)
                }
            }
            Ez.Label {
                id: switchShown
                objectName: "switchShown"
                font: Ez.Metrics.font
            }
        }
    }
}
