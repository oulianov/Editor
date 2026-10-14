// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "JukeboxDialog"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * Tools > Jukebox: listens to the music and sound effects of the game and of
 * the RTP, like RPG Maker's music and sound test windows. Not modal: it can
 * stay open while editing; it stops playing when closed. The lists, the
 * settings and the player are an Ez.AudioLibraryModel's, kept between
 * openings.
 */
Ez.DialogWindow {
    id: root

    /** The lists and the player */
    property Ez.AudioLibraryModel library: null

    readonly property bool music: library !== null && library.kind === Ez.AudioLibraryModel.Music
    readonly property bool stopped: library === null || library.state === Ez.AudioPlayer.Stopped
    /** The list shown */
    readonly property Ez.ColumnList currentList: music ? listMusic : listSound

    /** From the filter to the lines it shows (the first one when none is selected) */
    function goToList(play) {
        const list = root.currentList
        list.view.forceActiveFocus()
        const lines = music ? library.music : library.sound
        if (lines.currentRow < 0 && list.view.count > 0) {
            lines.currentRow = 0
        }
        if (play) {
            library.play()
        }
    }

    objectName: "JukeboxDialog"
    title: qsTr("Jukebox")
    standardButtons: Controls.DialogButtonBox.Close

    Component.onCompleted: {
        const close = buttonBox.standardButton(Controls.DialogButtonBox.Close)
        if (close) {
            close.objectName = "buttonClose"
        }
    }

    Shortcut {
        sequences: [StandardKey.Find]
        onActivated: {
            filter.forceActiveFocus(Qt.ShortcutFocusReason)
            filter.selectAll()
        }
    }

    RowLayout {
        spacing: Ez.Metrics.sectionSpacing + Ez.Metrics.columnSpacing
        Layout.fillWidth: true
        Layout.fillHeight: true

        // the files
        ColumnLayout {
            spacing: Ez.Metrics.rowSpacing
            Layout.fillWidth: true
            Layout.fillHeight: true

            Ez.TextField {
                id: filter
                objectName: "editFilter"
                placeholderText: qsTr("Filter")
                Layout.fillWidth: true
                onTextChanged: root.library.filter = text
                Keys.onReturnPressed: root.goToList(true)
                Keys.onEnterPressed: root.goToList(true)
                Keys.onDownPressed: root.goToList(false)
            }
            Controls.TabBar {
                id: tabs
                objectName: "tabsJukebox"
                currentIndex: root.music ? 0 : 1
                Layout.fillWidth: true
                onCurrentIndexChanged: root.library.kind = currentIndex === 1 ? Ez.AudioLibraryModel.Sound
                    : Ez.AudioLibraryModel.Music

                Controls.TabButton {
                    text: qsTr("Music")
                    font: Ez.Metrics.font
                    width: implicitWidth
                }
                Controls.TabButton {
                    text: qsTr("Sound Effects")
                    font: Ez.Metrics.font
                    width: implicitWidth
                }
            }
            StackLayout {
                currentIndex: tabs.currentIndex
                Layout.fillWidth: true
                Layout.fillHeight: true

                AudioList {
                    id: listMusic
                    listName: "listMusic"
                    lines: root.library ? root.library.music : null
                    kind: Ez.AudioLibraryModel.Music
                }
                AudioList {
                    id: listSound
                    listName: "listSound"
                    lines: root.library ? root.library.sound : null
                    kind: Ez.AudioLibraryModel.Sound
                }
            }
        }

        // playing, and RPG Maker's settings: as wide as they need
        ColumnLayout {
            spacing: Ez.Metrics.rowSpacing
            Layout.fillWidth: false
            Layout.fillHeight: true

            RowLayout {
                spacing: Ez.Metrics.columnSpacing

                Ez.Button {
                    objectName: "buttonPlay"
                    text: qsTr("Play")
                    icon.name: "media-playback-start"
                    enabled: root.library.canPlay
                    Controls.ToolTip.visible: hovered
                    Controls.ToolTip.text: qsTr("Play the file selected (Return, or double-click it)")
                    onClicked: root.library.play()
                }
                Ez.Button {
                    objectName: "buttonPause"
                    text: qsTr("Pause")
                    icon.name: "media-playback-pause"
                    checkable: true
                    checked: root.library.state === Ez.AudioPlayer.Paused
                    enabled: !root.stopped
                    onClicked: root.library.pauseOrResume()
                }
                Ez.Button {
                    objectName: "buttonStop"
                    text: qsTr("Stop")
                    icon.name: "media-playback-stop"
                    enabled: !root.stopped
                    onClicked: root.library.stop()
                }
            }
            Ez.CheckBox {
                objectName: "checkLoop"
                text: qsTr("Loop")
                checked: root.library.loop
                Controls.ToolTip.visible: hovered
                Controls.ToolTip.text: qsTr("Repeat at the end. MIDI music uses its loop point when set.")
                onToggled: root.library.loop = checked
            }
            RowLayout {
                spacing: Ez.Metrics.columnSpacing
                Layout.fillWidth: true

                Ez.Slider {
                    id: position
                    objectName: "sliderPosition"
                    from: 0
                    to: Math.max(1, root.library.duration)
                    stepSize: 1000
                    snapMode: Controls.Slider.NoSnap
                    enabled: !root.stopped && root.library.duration > 0
                    Layout.fillWidth: true
                    // the position playing, unless the handle is held
                    Binding on value {
                        value: root.library.position
                        when: !position.pressed
                        restoreMode: Binding.RestoreNone
                    }
                    onMoved: {
                        if (!pressed) {
                            root.library.seek(value)
                        }
                    }
                    onPressedChanged: {
                        if (!pressed) {
                            root.library.seek(value)
                        }
                    }
                }
                Ez.Label {
                    objectName: "labelTime"
                    readonly property real shown: position.pressed ? position.value : root.library.position
                    text: root.library.duration > 0
                        ? "%1 / %2".arg(root.library.timeText(shown)).arg(root.library.timeText(root.library.duration))
                        : root.library.timeText(shown)
                    font: Ez.Metrics.font
                    horizontalAlignment: Text.AlignRight
                    // does not jump while playing
                    Layout.minimumWidth: timeMetrics.advanceWidth("00:00 / 00:00")

                    FontMetrics {
                        id: timeMetrics
                        font: Ez.Metrics.font
                    }
                }
            }
            Ez.Note {
                objectName: "labelStatus"
                text: root.library.status
                font.bold: root.library.statusIsError
            }

            // the four settings of RPG Maker's windows, with their scales and
            // the steps of its trackbars (11 positions), each with its default
            GridLayout {
                columns: 2
                rowSpacing: Ez.Metrics.sectionSpacing
                columnSpacing: Ez.Metrics.sectionSpacing

                Setting {
                    name: "FadeIn"
                    tickInterval: 1
                    defaultValue: 0
                    title: qsTr("Fade-in Time")
                    // RPG Maker's sound effects have no fade-in
                    enabled: root.music
                    from: 0
                    to: 10
                    value: root.library.fadeIn
                    marks: [qsTr("None"), qsTr("5 s"), qsTr("10 s")]
                    valueText: v => qsTr("%1 s").arg(v)
                    onMoved: v => root.library.fadeIn = v
                }
                Setting {
                    name: "Volume"
                    tickInterval: 10
                    defaultValue: 100
                    title: qsTr("Volume")
                    from: 0
                    to: 100
                    value: root.library.volume
                    marks: [qsTr("0%"), qsTr("50%"), qsTr("100%")]
                    valueText: v => qsTr("%1%").arg(v)
                    onMoved: v => root.library.volume = v
                }
                Setting {
                    name: "Tempo"
                    tickInterval: 10
                    defaultValue: 100
                    title: qsTr("Tempo")
                    from: 50
                    to: 150
                    value: root.library.tempo
                    marks: [qsTr("50%"), qsTr("100%"), qsTr("150%")]
                    valueText: v => qsTr("%1%").arg(v)
                    onMoved: v => root.library.tempo = v
                }
                Setting {
                    name: "Balance"
                    tickInterval: 10
                    defaultValue: 50
                    title: qsTr("Balance")
                    from: 0
                    to: 100
                    value: root.library.balance
                    marks: [qsTr("Left"), qsTr("Center"), qsTr("Right")]
                    valueText: v => "%1:%2".arg(100 - v).arg(v)
                    toolTip: qsTr("Left and right volume, like RPG Maker (50:50 is the center)")
                    onMoved: v => root.library.balance = v
                }
            }
            Item {
                Layout.fillHeight: true
            }
        }
    }

    /** A list of the library (music or sounds): its files, then the database's */
    component AudioList: Ez.ColumnList {
        id: list

        property Ez.AudioListModel lines: null
        property int kind: Ez.AudioLibraryModel.Music

        model: lines
        currentRow: lines ? lines.currentRow : -1
        sectionRole: "sectionTitle"
        columns: [
            { title: qsTr("Name"), role: "name" },
            { title: qsTr("Format"), role: "format", sample: "MIDI" },
            { title: qsTr("Source"), role: "source", sample: qsTr("Not found"),
              dimmed: line => line.origin !== Ez.AudioListModel.Game },
        ]
        onPicked: row => lines.currentRow = row
        onActivated: row => root.library.playRow(list.kind, row)
    }

    /** A setting of RPG Maker: its slider by steps, its value, its reset button, its scale under it */
    component Setting: Ez.Section {
        id: setting

        /** The end of the names of its controls: "group" + name, "slider" + name, "label" + name, "reset" + name */
        property string name
        property int from: 0
        property int to: 100
        property int tickInterval: 10
        property int defaultValue: 0
        property int value: 0
        /** The texts under the slider, from its start to its end */
        property var marks: []
        property var valueText: v => String(v)
        property string toolTip

        signal moved(int value)

        objectName: "group" + name
        Layout.fillWidth: true

        Ez.SliderField {
            name: setting.name
            from: setting.from
            to: setting.to
            tickInterval: setting.tickInterval
            defaultValue: setting.defaultValue
            value: setting.value
            marks: setting.marks
            valueText: setting.valueText
            toolTip: setting.toolTip
            // two settings side by side: the list keeps its room
            sliderWidth: 150
            onMoved: v => setting.moved(Math.round(v))
        }
    }
}
