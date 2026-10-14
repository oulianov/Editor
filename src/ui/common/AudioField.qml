// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * A music or a sound effect of the project's data (a Music or a Sound: its
 * name, volume, tempo, balance, and fade-in for a music): its name, and a
 * "..." button opening the picker, where it is heard and its settings
 * changed. OK writes the file and its settings.
 *
 *   Ez.AudioField { jsonData: system; key: "title_music"; music: true; fieldName: "lineTitleMusic" }
 *
 * The field is named fieldName, its button fieldName + "Picker".
 */
RowLayout {
    id: root

    property var jsonData: null
    /** The Music or Sound object in jsonData */
    property string key: ""
    /** A music (Music folder, fade-in), else a sound effect (Sound folder) */
    property bool music: false
    /** The title of the picker (empty: "Select from <folder>") */
    property string title
    property string fieldName
    property string buttonName: fieldName + "Picker"
    property alias field: field
    property alias button: button

    /** A file was chosen (OK of the picker): its choice */
    signal picked(var choice)

    readonly property string prefix: root.key === "" ? "" : root.key + "/"

    /** Opens the picker on the file and its settings */
    function pick() {
        const request = { folder: root.music ? "Music" : "Sound", preview: root.music ? "Music" : "Sound",
                          name: name.value ?? "", title: root.title }
        if (volume.valid) {
            request.volume = volume.value
            request.tempo = tempo.value
            request.balance = balance.value
        }
        if (root.music && fadeIn.valid) {
            request.fadeIn = fadeIn.value
        }
        const choice = Ez.Pickers.pickFile(request, root)
        if (!choice) {
            return
        }
        if (name.valid) {
            name.value = choice.name
            volume.value = choice.volume
            tempo.value = choice.tempo
            balance.value = choice.balance
            if (root.music) {
                fadeIn.value = choice.fadeIn
            }
        }
        root.picked(choice)
    }

    spacing: Ez.Metrics.columnSpacing
    Layout.fillWidth: true

    Ez.JsonField {
        id: name
        source: root.jsonData
        path: root.prefix + "name"
    }
    Ez.JsonField {
        id: fadeIn
        source: root.jsonData
        path: root.prefix + "fadein"
    }
    Ez.JsonField {
        id: volume
        source: root.jsonData
        path: root.prefix + "volume"
    }
    Ez.JsonField {
        id: tempo
        source: root.jsonData
        path: root.prefix + "tempo"
    }
    Ez.JsonField {
        id: balance
        source: root.jsonData
        path: root.prefix + "balance"
    }

    Ez.TextField {
        id: field
        objectName: root.fieldName
        jsonData: root.jsonData
        key: root.prefix + "name"
        readOnly: true
        Layout.fillWidth: true
        Layout.minimumWidth: 100
    }
    Ez.FieldButton {
        id: button
        objectName: root.buttonName
        onClicked: root.pick()
    }
}
