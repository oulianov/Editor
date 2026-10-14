// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import org.easyrpg.editor as Ez

/**
 * A graphic shown as the game draws it (viewerContent) which opens its
 * picker when clicked (or with Space or Return): the picker starts on
 * pickerData, and OK writes the choice into it, then accepted() (the page
 * writes it into its data).
 */
Item {
    id: root

    property Ez.PickerData pickerData: Ez.PickerData {}
    /** The folder of the game listed (CharSet...) */
    property string folder
    /** What the picker shows of the file: a FilePickerModel.Preview name ("CharSet"...) */
    property string preview: "Image"
    /** The title of the picker (empty: "Select from <folder>") */
    property string title

    /** Signal emitted when confirming in the picker dialog */
    signal accepted()

    default property alias viewerContent: container.data

    /** Opens the picker on pickerData */
    function pick() {
        const request = root.pickerData.toRequest(root.folder, root.preview)
        request.title = root.title
        if (root.pickerData.fromChoice(Ez.Pickers.pickFile(request, root))) {
            root.accepted()
        }
    }

    activeFocusOnTab: true
    Keys.onSpacePressed: root.pick()
    Keys.onReturnPressed: root.pick()

    Item {
        id: container
        anchors.fill: parent
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            root.forceActiveFocus()
            root.pick()
        }
    }
}
