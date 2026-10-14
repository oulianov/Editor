// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * The picker of a sound effect: its settings, heard at once (Play), by the
 * steps of RPG Maker's trackbars (11 positions), each with its default
 * (lcf::rpg::Sound). A double-click on a file plays it. MusicPicker adds the
 * fade-in of a music.
 */
PickerBase {
    id: root

    /** A music: its fade-in too (MusicPicker) */
    property bool music: false

    acceptOnActivate: false
    onFileActivated: picker.play()

    Ez.Section {
        objectName: "groupFadeIn"
        visible: root.music
        title: qsTranslate("PickerAudioWidget", "Fade In Time")
        Ez.SliderField {
            name: "FadeIn"
            from: 0
            to: 10
            tickInterval: 1
            defaultValue: 0
            value: root.pickerData.fadein / 1000
            valueText: seconds => (Math.round(seconds * 10) / 10) + "s"
            onMoved: seconds => root.pickerData.fadein = Math.round(seconds * 1000)
        }
    }
    Ez.Section {
        title: qsTranslate("PickerAudioWidget", "Volume")
        Ez.SliderField {
            name: "Volume"
            from: 0
            to: 100
            tickInterval: 10
            defaultValue: 100
            value: root.pickerData.volume
            valueText: volume => volume + "%"
            onMoved: volume => root.pickerData.volume = Math.round(volume)
        }
    }
    Ez.Section {
        title: qsTranslate("PickerAudioWidget", "Tempo")
        Ez.SliderField {
            name: "Tempo"
            from: 50
            to: 150
            tickInterval: 10
            defaultValue: 100
            value: root.pickerData.tempo
            valueText: tempo => tempo + "%"
            onMoved: tempo => root.pickerData.tempo = Math.round(tempo)
        }
    }
    Ez.Section {
        title: qsTranslate("PickerAudioWidget", "Balance")
        Ez.SliderField {
            name: "Balance"
            from: 0
            to: 100
            tickInterval: 10
            defaultValue: 50
            value: root.pickerData.balance
            valueText: balance => (100 - balance) + ":" + balance
            onMoved: balance => root.pickerData.balance = Math.round(balance)
        }
    }
    RowLayout {
        spacing: Ez.Metrics.columnSpacing
        Ez.Button {
            objectName: "buttonPlay"
            text: qsTranslate("PickerAudioWidget", "Play")
            icon.name: "media-playback-start"
            onClicked: root.picker.play()
        }
        Ez.Button {
            objectName: "buttonStop"
            text: qsTranslate("PickerAudioWidget", "Stop")
            icon.name: "media-playback-stop"
            enabled: root.picker.playing
            onClicked: root.picker.stop()
        }
        Item {
            Layout.fillWidth: true
        }
    }
    Ez.Label {
        objectName: "labelStatus"
        text: root.picker.playStatus
        font: Ez.Metrics.font
        wrapMode: Text.Wrap
        Layout.fillWidth: true
        Layout.preferredWidth: 1
    }
}
