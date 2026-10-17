// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * A graphic of the project's data drawn as the game does: a character of a
 * CharSet (and its pose), a face of a FaceSet, a monster with its hue. A
 * click (or Space) opens the picker on it (Ez.Pickers.pickFile); OK writes
 * the file and the part chosen.
 *
 *   Ez.SpriteField { objectName: "graphicsCharset"; jsonData: hero; preview: "CharSet"
 *                    key: "character_name"; indexKey: "character_index" }
 *
 * key: the file name; indexKey: the character or face; directionKey and
 * patternKey: the pose (preview "CharSetPose"); hueKey: the monster's hue.
 * animated: the character turns and walks, as RPG Maker shows a hero's or a
 * vehicle's.
 * Without jsonData, the values shown are given (fileName...) and picked()
 * says what was chosen.
 */
Rectangle {
    id: root

    property var jsonData: null
    property string key: ""
    property string indexKey: ""
    property string directionKey: ""
    property string patternKey: ""
    property string hueKey: ""
    /** A FilePickerModel.Preview name: "CharSet", "CharSetPose", "FaceSet" or "Monster" */
    property string preview: "CharSet"
    /** The folder of the game (by default the preview's: CharSet, FaceSet, Monster) */
    property string folder: preview === "FaceSet" ? "FaceSet" : preview === "Monster" ? "Monster" : "CharSet"
    /** The title of the picker (empty: "Select from <folder>") */
    property string title
    /** The sprite drawn bigger (pixel art: whole numbers) */
    property real zoom: preview === "FaceSet" || preview === "Monster" ? 1 : 2
    /** Drawn half transparent (a monster's or a hero's option) */
    property bool transparent: false
    /** The character turns and walks (preview "CharSet") */
    property bool animated: false

    /** The values shown (the data's) */
    property string fileName: nameField.value ?? ""
    property int spriteIndex: indexField.valid ? Number(indexField.value) : 0
    property int direction: directionField.valid ? Number(directionField.value) : 2
    property int pattern: patternField.valid ? Number(patternField.value) : 1
    property int hue: hueField.valid ? Number(hueField.value) : 0
    /** The pose drawn, by default the one of the data (an animation shows others) */
    property int shownDirection: direction
    property int shownPattern: pattern

    /** A graphic was chosen (OK of the picker): its choice */
    signal picked(var choice)

    /** Opens the picker on the graphic shown */
    function pick() {
        const choice = Ez.Pickers.pickFile({ folder: root.folder, preview: root.preview, name: root.fileName,
                                             index: root.spriteIndex, direction: root.direction,
                                             pattern: root.pattern, hue: root.hue, title: root.title }, root)
        if (!choice) {
            return
        }
        if (nameField.valid) {
            nameField.value = choice.name
        }
        if (indexField.valid) {
            indexField.value = choice.index
        }
        if (directionField.valid) {
            directionField.value = choice.direction
        }
        if (patternField.valid) {
            patternField.value = choice.pattern
        }
        if (hueField.valid) {
            hueField.value = choice.hue
        }
        root.picked(choice)
    }

    Kirigami.Theme.colorSet: Kirigami.Theme.View
    Kirigami.Theme.inherit: false
    color: Kirigami.Theme.backgroundColor
    border.color: activeFocus ? Kirigami.Theme.focusColor : Ez.Metrics.sectionBorderColor
    radius: 2
    activeFocusOnTab: true
    implicitWidth: (preview === "FaceSet" ? 48 : preview === "Monster" ? 120 : 24) * zoom + 8
    implicitHeight: (preview === "FaceSet" ? 48 : preview === "Monster" ? 120 : 32) * zoom + 8

    Ez.JsonField {
        id: nameField
        source: root.jsonData
        path: root.key
    }
    Ez.JsonField {
        id: indexField
        source: root.indexKey !== "" ? root.jsonData : null
        path: root.indexKey
    }
    Ez.JsonField {
        id: directionField
        source: root.directionKey !== "" ? root.jsonData : null
        path: root.directionKey
    }
    Ez.JsonField {
        id: patternField
        source: root.patternKey !== "" ? root.jsonData : null
        path: root.patternKey
    }
    Ez.JsonField {
        id: hueField
        source: root.hueKey !== "" ? root.jsonData : null
        path: root.hueKey
    }

    Loader {
        anchors.centerIn: parent
        sourceComponent: root.preview === "FaceSet" ? face : root.preview === "Monster" ? monster : character
    }
    Component {
        id: character
        Ez.CharSetPaintedItem {
            projectData: Ez.ProjectData
            filename: root.fileName
            cellIndex: root.spriteIndex
            facing: root.shownDirection
            frame: root.shownPattern
            spin: root.animated
            walk: root.animated
            transparent: root.transparent
            width: 24 * root.zoom
            height: 32 * root.zoom
        }
    }
    Component {
        id: face
        Ez.FaceSetPaintedItem {
            projectData: Ez.ProjectData
            filename: root.fileName
            cellIndex: root.spriteIndex
            transparent: root.transparent
            width: 48 * root.zoom
            height: 48 * root.zoom
        }
    }
    Component {
        id: monster
        Ez.MonsterPaintedItem {
            projectData: Ez.ProjectData
            filename: root.fileName
            hue: root.hue
            transparent: root.transparent
            width: (root.width - 8)
            height: (root.height - 8)
        }
    }

    TapHandler {
        onTapped: {
            root.forceActiveFocus()
            root.pick()
        }
    }
    Keys.onSpacePressed: root.pick()
    Keys.onReturnPressed: root.pick()
}
