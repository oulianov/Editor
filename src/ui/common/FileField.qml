// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * A file of the game (a backdrop, a panorama, a picture...): its name, and
 * a "..." button opening the resource picker on it (Ez.Pickers.pickFile).
 * With jsonData and key it shows a file name of the project's data and
 * writes the one chosen, as the other Ez controls.
 *
 *   Ez.FileField { jsonData: terrain; key: "background_name"; folder: "Backdrop"; fieldName: "lineBackground" }
 *
 * The "..." is in the frame of the field (Ez.TextField.buttons); a click on
 * the name, Space and Alt+Down open the picker too. The field is named
 * fieldName, its button fieldName + "Picker".
 */
RowLayout {
    id: root

    property var jsonData: null
    property string key: ""
    /** The folder of the game listed (Backdrop...) */
    property string folder
    /** What the picker shows of the file: a FilePickerModel.Preview name ("Image", "None") */
    property string preview: "Image"
    /** The title of the picker (empty: "Select from <folder>") */
    property string title
    property string fieldName
    property string buttonName: fieldName + "Picker"
    /** The name shown (the data's, or one given without jsonData) */
    property alias text: field.text
    property alias field: field
    property alias button: button

    /** A file was chosen (OK of the picker): its choice */
    signal picked(var choice)

    /** Opens the picker on the file shown */
    function pick() {
        const choice = Ez.Pickers.pickFile({ folder: root.folder, preview: root.preview, name: field.text,
                                             title: root.title }, root)
        if (!choice) {
            return
        }
        if (value.valid) {
            value.value = choice.name
        } else {
            field.text = choice.name
        }
        root.picked(choice)
    }

    spacing: Ez.Metrics.columnSpacing
    Layout.fillWidth: true

    Ez.JsonField {
        id: value
        source: root.jsonData
        path: root.key
    }

    Ez.TextField {
        id: field
        objectName: root.fieldName
        jsonData: root.jsonData
        key: root.key
        readOnly: true
        Layout.fillWidth: true
        Layout.minimumWidth: 100 + buttonsWidth
        buttons: Ez.FieldButton {
            id: button
            objectName: root.buttonName
            fused: true
            onClicked: root.pick()
        }
    }
}
