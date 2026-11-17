// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * A graphic of the project's data drawn as the game does: a character of a
 * CharSet (and its pose), a face of a FaceSet, a monster with its hue, a
 * picture of a folder (preview "Image": at its size or magnified, smaller
 * when it does not fit; turned, turning or wavering as EasyRPG Player draws
 * a picture's effects: scaleX, scaleY, angle, turnSpeed, waver). A click
 * (or Space) opens the picker on it (Ez.Pickers.pickFile);
 * OK writes the file and the part chosen. Without a file it says so
 * (Ez.PickHint).
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
    /** A click (or Space) chooses the file; false: it is shown only (no hint when empty) */
    property bool pickable: true
    property string indexKey: ""
    property string directionKey: ""
    property string patternKey: ""
    property string hueKey: ""
    /** A FilePickerModel.Preview name: "CharSet", "CharSetPose", "FaceSet", "Monster" or "Image" */
    property string preview: "CharSet"
    /** The folder of the game (by default the preview's: CharSet, FaceSet, Monster; Picture for an Image) */
    property string folder: preview === "FaceSet" ? "FaceSet" : preview === "Monster" ? "Monster"
                          : preview === "Image" ? "Picture" : "CharSet"
    /** The title of the picker (empty: "Select from <folder>") */
    property string title
    /** The sprite drawn bigger (pixel art: whole numbers) */
    property real zoom: preview === "FaceSet" || preview === "Monster" ? 1 : 2
    /** Drawn half transparent (a monster's or a hero's option) */
    property bool transparent: false
    /** What the picture of a preview "Image" is drawn with (ProjectImageProvider's query: "tone=100,50,100,100") */
    property string imageQuery: ""
    /** The character turns and walks (preview "CharSet") */
    property bool animated: false
    /** The picture (preview "Image") turned by it: degrees clockwise, around its center */
    property real angle: 0
    /** The picture (preview "Image") keeps turning: degrees a second, clockwise above 0 */
    property real turnSpeed: 0
    /** The picture (preview "Image") wavers: its rows swing from side to side by up to this many of
     *  its pixels, a wave every 32 rows, 1.875 waves a second (EasyRPG Player's) */
    property real waver: 0
    /** The picture (preview "Image") magnified across and down (1: its size; 0: not drawn) */
    property real scaleX: 1
    property real scaleY: 1
    /** The angle the picture is drawn at, its turn included: what is drawn over it turns as much */
    readonly property real shownAngle: preview === "Image" && graphic.item ? graphic.item.rotation : 0

    /** The values shown (the data's) */
    property string fileName: nameField.value ?? ""
    property int spriteIndex: indexField.valid ? Number(indexField.value) : 0
    property int direction: directionField.valid ? Number(directionField.value) : 2
    property int pattern: patternField.valid ? Number(patternField.value) : 1
    property int hue: hueField.valid ? Number(hueField.value) : 0
    /** The pose drawn, by default the one of the data (an animation shows others) */
    property int shownDirection: direction
    property int shownPattern: pattern
    /** Where a picture (preview "Image") is drawn, in the field: what is drawn over it lines up */
    readonly property rect pictureRect: {
        const image = preview === "Image" ? graphic.item : null
        if (!image || image.paintedWidth <= 0) {
            return Qt.rect(0, 0, 0, 0)
        }
        return Qt.rect(graphic.x + (image.width - image.paintedWidth) / 2, graphic.y + (image.height - image.paintedHeight) / 2,
                       image.paintedWidth, image.paintedHeight)
    }

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
    implicitWidth: preview === "Image" ? 164 : (preview === "FaceSet" ? 48 : preview === "Monster" ? 120 : 24) * zoom + 8
    implicitHeight: preview === "Image" ? 124 : (preview === "FaceSet" ? 48 : preview === "Monster" ? 120 : 32) * zoom + 8

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
        id: graphic
        anchors.centerIn: parent
        sourceComponent: root.preview === "FaceSet" ? face : root.preview === "Monster" ? monster
                       : root.preview === "Image" ? picture : character
    }
    Ez.PickHint {
        anchors.centerIn: parent
        availableWidth: root.width
        visible: root.fileName === "" && root.pickable
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

    Component {
        id: picture
        Item {
            id: shown
            // at its size (magnified), smaller when it does not fit, with room
            // for its effect: turning, its diagonal; turned, its corners;
            // wavering, the swing of its rows
            readonly property real fit: {
                const w = image.implicitWidth * Math.abs(root.scaleX)
                const h = image.implicitHeight * Math.abs(root.scaleY)
                if (w <= 0 || h <= 0) {
                    return 1
                }
                let across = w
                let down = h
                if (root.turnSpeed !== 0) {
                    across = down = Math.hypot(w, h)
                } else if (root.angle % 180 !== 0) {
                    const turn = root.angle * Math.PI / 180
                    across = w * Math.abs(Math.cos(turn)) + h * Math.abs(Math.sin(turn))
                    down = w * Math.abs(Math.sin(turn)) + h * Math.abs(Math.cos(turn))
                }
                if (root.waver !== 0) {
                    across = w + 2 * Math.abs(root.waver * root.scaleX)
                }
                return Math.max(0, Math.min(1, (root.width - 8) / across, (root.height - 8) / down))
            }
            /** A pixel of the picture in the field, across and down */
            readonly property real zoomX: Math.abs(root.scaleX) * fit
            readonly property real zoomY: Math.abs(root.scaleY) * fit
            readonly property real paintedWidth: image.implicitWidth * zoomX
            readonly property real paintedHeight: image.implicitHeight * zoomY
            /** How far it turned (turnSpeed), and where its waver is */
            property real turned: 0
            property real phase: 0
            width: paintedWidth
            height: paintedHeight
            rotation: root.angle + turned
            opacity: root.transparent ? 0.5 : 1

            Image {
                id: image
                source: root.fileName === "" ? ""
                      : "image://project/" + root.folder + "/" + root.fileName + (root.imageQuery !== "" ? "?" + root.imageQuery : "")
                // a picture per value tried: not kept
                cache: root.imageQuery === ""
                anchors.fill: parent
                smooth: false
                // loaded at once, not asynchronously: the project's images are
                // read on this thread (ProjectImageProvider), and a load cut
                // short by the window closing kept the Editor from quitting
            }
            // wavering: each row of pixels shifted as EasyRPG Player shifts
            // it (the rows of the field: as its own magnified)
            Repeater {
                model: root.waver !== 0 && shown.zoomY > 0 ? Math.ceil(shown.height) : 0
                ShaderEffectSource {
                    required property int index
                    sourceItem: image
                    hideSource: true
                    sourceRect: Qt.rect(0, index, shown.width, 1)
                    smooth: false
                    x: Math.round(root.waver * shown.zoomX * Math.sin(shown.phase + index / shown.zoomY * Math.PI / 16))
                    y: index
                    width: shown.width
                    height: 1
                }
            }
            FrameAnimation {
                running: root.visible && (root.turnSpeed !== 0 || root.waver !== 0)
                onTriggered: {
                    shown.turned = (shown.turned + root.turnSpeed * frameTime) % 360
                    // a wave each 32 frames, 60 frames a second
                    shown.phase = (shown.phase + Math.PI * 2 * 1.875 * frameTime) % (Math.PI * 2)
                }
                onRunningChanged: {
                    if (!running) {
                        shown.turned = 0
                        shown.phase = 0
                    }
                }
            }
        }
    }

    TapHandler {
        enabled: root.pickable
        onTapped: {
            root.forceActiveFocus()
            root.pick()
        }
    }
    Keys.onSpacePressed: if (root.pickable) root.pick()
    Keys.onReturnPressed: if (root.pickable) root.pick()
}
