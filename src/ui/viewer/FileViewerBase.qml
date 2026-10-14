// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * A file shown by its name, and a "..." button opening its picker on
 * pickerData (SoundViewer, MusicViewer): OK writes the choice into it, then
 * accepted() (the page writes it into its data).
 */
RowLayout {
    id: root

    property alias value: component.text

    property Ez.PickerData pickerData: Ez.PickerData {}
    /** The folder of the game listed (Sound...) */
    property string folder
    /** What the picker shows of the file: a FilePickerModel.Preview name ("Sound"...) */
    property string preview: "Image"
    /** The title of the picker (empty: "Select from <folder>") */
    property string title

    /** Signal emitted when confirming in the picker dialog */
    signal accepted()

    /** Opens the picker on pickerData */
    function pick() {
        const request = root.pickerData.toRequest(root.folder, root.preview)
        request.title = root.title
        if (root.pickerData.fromChoice(Ez.Pickers.pickFile(request, root))) {
            root.accepted()
        }
    }

    spacing: Ez.Metrics.columnSpacing

    Ez.TextField {
        id: component
        text: root.pickerData.filename
        readOnly: true
        Layout.fillWidth: true
        Layout.minimumWidth: 100
    }

    Ez.FieldButton {
        onClicked: root.pick()
    }
}
