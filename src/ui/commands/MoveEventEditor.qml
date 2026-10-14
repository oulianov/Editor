// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "MoveRouteDialog"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * Move Event, like RPG Maker's window: who moves and how often, the moves
 * on the left (inserted before the line selected, the empty "<>" line at
 * the end), a button per kind of move on the right. The moves are the
 * session's MoveRouteModel (moveRoute): each change is the command's.
 *
 * Several moves are selected with Ctrl or Shift: Delete, Cut, Copy and
 * Paste (keys and menu) act on them; Enter or a double-click asks again
 * for the switch, graphic or sound of a move.
 */
ColumnLayout {
    id: root

    required property Ez.CommandEditSession session
    readonly property string title: qsTr("Move Event")
    readonly property var moves: session.moveRoute
    readonly property bool moveEvent: true
    /** Who can be moved: [{text, value}]; a target the map does not have is kept */
    readonly property var targets: {
        const list = [{ value: 10001, text: qsTr("Hero") }, { value: 10002, text: qsTr("Boat") },
                      { value: 10003, text: qsTr("Ship") }, { value: 10004, text: qsTr("Airship") },
                      { value: 10005, text: qsTr("This Event") }].concat(session.events)
        const current = moves.target
        if (!list.some(one => one.value === current)) {
            list.push({ value: current, text: String(current) })
        }
        return list
    }

    spacing: Ez.Metrics.sectionSpacing
    Layout.fillWidth: true
    Layout.fillHeight: true
    // on the end line, where moves go
    Component.onCompleted: select(moves.moveCount)

    /** The current line, and the lines selected (in order) */
    property int currentRow: 0
    property var selectedRows: []
    property int anchorRow: 0

    function isSelected(row) {
        return selectedRows.indexOf(row) >= 0
    }
    /** A line alone, the current one */
    function select(row) {
        const shown = Math.max(0, Math.min(row, moves.moveCount))
        currentRow = shown
        anchorRow = shown
        selectedRows = [shown]
        list.positionViewAtIndex(shown, ListView.Contain)
    }
    /** From the anchor to a line (Shift), or one more line (Ctrl) */
    function extend(row, toggle) {
        currentRow = row
        if (toggle) {
            const rows = selectedRows.slice()
            const at = rows.indexOf(row)
            if (at >= 0) {
                rows.splice(at, 1)
            } else {
                rows.push(row)
            }
            selectedRows = rows.sort((a, b) => a - b)
        } else {
            const rows = []
            for (let i = Math.min(anchorRow, row); i <= Math.max(anchorRow, row); ++i) {
                rows.push(i)
            }
            selectedRows = rows
        }
        list.positionViewAtIndex(row, ListView.Contain)
    }
    /** The moves selected (not the end line) */
    function selectedMoves() {
        return selectedRows.filter(row => row < moves.moveCount)
    }

    /** Asks for the switch, graphic or sound of a move: its parameters, null when cancelled */
    function askParameters(code, current) {
        if (code === 32 || code === 33) {
            // the switch list, as RPG Maker's (its names can be changed there)
            const id = root.session.pickEntry("switches", current.a, false, list)
            return id === null || id === undefined ? null : { a: id }
        }
        if (code === 34) {
            const graphic = Ez.Pickers.pickFile({ folder: "CharSet", preview: "CharSet", name: current.name, index: current.a }, list)
            return graphic ? { name: graphic.name, a: graphic.index } : null
        }
        if (code === 35) {
            const sound = Ez.Pickers.pickFile({ folder: "Sound", preview: "Sound", name: current.name, volume: current.a,
                                                tempo: current.b, balance: current.c }, list)
            return sound ? { name: sound.name, a: sound.volume, b: sound.tempo, c: sound.balance } : null
        }
        return {}
    }
    function setParameters(row, code, parameters) {
        if (code === 32 || code === 33) {
            moves.setSwitch(row, parameters.a)
        } else if (code === 34) {
            moves.setGraphic(row, parameters.name, parameters.a)
        } else if (code === 35) {
            moves.setSound(row, parameters.name, parameters.a, parameters.b, parameters.c)
        }
    }
    /** A new move before the line selected, as RPG Maker does */
    function insert(code) {
        let parameters = {}
        if (moves.hasParameters(code)) {
            // RPG Maker's: switch 1, a sound at its full volume and tempo, centered
            parameters = askParameters(code, { name: "", a: code === 35 ? 100 : 1, b: 100, c: 50 })
            if (parameters === null) {
                return
            }
        }
        const after = moves.insertCommand(currentRow, code)
        setParameters(after - 1, code, parameters)
        select(after)
    }
    /** The line of a move (its delegate: the roles of the move), null when not made */
    function lineAt(row) {
        return list.itemAtIndex(row)
    }
    /** The switch, graphic or sound of the move of a line, asked again */
    function editRow(row) {
        const item = lineAt(row)
        if (row < 0 || row >= moves.moveCount || !item || !item.hasParameters) {
            return
        }
        const code = item.code
        const parameters = askParameters(code, { name: item.parameterString, a: item.parameterA,
                                                 b: item.parameterB, c: item.parameterC })
        if (parameters !== null) {
            setParameters(row, code, parameters)
            select(row)
        }
    }
    function removeSelected() {
        const first = moves.removeMoves(selectedMoves())
        if (first >= 0) {
            select(first)
        }
    }
    function copySelected(cut) {
        const rows = selectedMoves()
        if (moves.copyMoves(rows, cut) && cut) {
            select(rows[0])
        }
    }
    function paste() {
        const row = moves.paste(currentRow)
        if (row >= 0) {
            select(row)
        }
    }

    Ez.Form {
        visible: root.moveEvent

        Ez.FormLabel {
            text: qsTr("Target")
        }
        Ez.RefComboBox {
            objectName: "moveTarget"
            choices: root.targets
            Layout.fillWidth: true
            Component.onCompleted: currentIndex = Math.max(0, indexOfValue(root.moves.target))
            onActivated: root.moves.target = currentValue
        }
        Ez.FormLabel {
            text: qsTr("Frequency")
        }
        Ez.ComboBox {
            objectName: "moveFrequency"
            model: [1, 2, 3, 4, 5, 6, 7, 8]
            textRole: ""
            valueRole: ""
            Layout.fillWidth: true
            Component.onCompleted: currentIndex = Math.max(0, Math.min(7, root.moves.frequency - 1))
            onActivated: root.moves.frequency = currentIndex + 1
        }
    }

    RowLayout {
        spacing: Ez.Metrics.sectionSpacing * 2
        Layout.fillWidth: true
        Layout.fillHeight: true

        // the route
        ColumnLayout {
            spacing: Ez.Metrics.rowSpacing
            Layout.fillWidth: true
            Layout.fillHeight: true

            Rectangle {
                Kirigami.Theme.colorSet: Kirigami.Theme.View
                Kirigami.Theme.inherit: false
                color: Kirigami.Theme.backgroundColor
                border.color: list.activeFocus ? Kirigami.Theme.focusColor : Ez.Metrics.sectionBorderColor
                radius: 2
                implicitWidth: 240
                implicitHeight: 200
                Layout.fillWidth: true
                Layout.fillHeight: true

                ListView {
                    id: list

                    readonly property real rowHeight: Math.ceil(lineMetrics.height) + 2 * Ez.Metrics.listRowPadding

                    objectName: "listMoves"
                    anchors.fill: parent
                    anchors.margins: 1
                    clip: true
                    focus: true
                    model: root.moves
                    currentIndex: root.currentRow
                    keyNavigationEnabled: false
                    highlightFollowsCurrentItem: false
                    boundsBehavior: Flickable.StopAtBounds
                    Controls.ScrollBar.vertical: Controls.ScrollBar {}

                    FontMetrics {
                        id: lineMetrics
                        font: Ez.Metrics.font
                    }

                    delegate: Rectangle {
                        id: move

                        required property int index
                        required property string display
                        required property int code
                        required property bool hasParameters
                        required property string parameterString
                        required property int parameterA
                        required property int parameterB
                        required property int parameterC
                        required property bool isEnd
                        readonly property bool selected: root.isSelected(index)

                        width: ListView.view.width
                        height: list.rowHeight
                        color: selected ? Kirigami.Theme.highlightColor : "transparent"

                        Text {
                            x: Ez.Metrics.listTextPadding
                            width: parent.width - 2 * Ez.Metrics.listTextPadding
                            anchors.verticalCenter: parent.verticalCenter
                            text: move.display
                            elide: Text.ElideRight
                            font: Ez.Metrics.font
                            color: move.selected ? Kirigami.Theme.highlightedTextColor
                                                 : move.isEnd ? Kirigami.Theme.disabledTextColor : Kirigami.Theme.textColor
                        }
                        MouseArea {
                            id: lineMouse
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            onPressed: (event) => {
                                list.forceActiveFocus()
                                if (event.button === Qt.RightButton && move.selected) {
                                    return
                                }
                                if (event.modifiers & Qt.ShiftModifier) {
                                    root.extend(move.index, false)
                                } else if (event.modifiers & (Qt.ControlModifier | Qt.MetaModifier)) {
                                    root.extend(move.index, true)
                                } else {
                                    root.select(move.index)
                                }
                            }
                            onClicked: (event) => {
                                if (event.button === Qt.RightButton) {
                                    menu.popupAt(lineMouse, event.x, event.y)
                                }
                            }
                            onDoubleClicked: (event) => {
                                const row = move.index
                                // after the click: the line may be made again by the window it opens
                                Qt.callLater(() => root.editRow(row))
                            }
                        }
                    }

                    Keys.onPressed: (event) => {
                        if (event.key === Qt.Key_Delete || event.key === Qt.Key_Backspace) {
                            root.removeSelected()
                        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                            const row = root.currentRow
                            Qt.callLater(() => root.editRow(row))
                        } else if (event.matches(StandardKey.Cut)) {
                            root.copySelected(true)
                        } else if (event.matches(StandardKey.Copy)) {
                            root.copySelected(false)
                        } else if (event.matches(StandardKey.Paste)) {
                            root.paste()
                        } else if (event.key === Qt.Key_Up || event.key === Qt.Key_Down) {
                            const row = Math.max(0, Math.min(root.moves.moveCount, root.currentRow + (event.key === Qt.Key_Up ? -1 : 1)))
                            if (event.modifiers & Qt.ShiftModifier) {
                                root.extend(row, false)
                            } else {
                                root.select(row)
                            }
                        } else if (event.key === Qt.Key_Home || event.key === Qt.Key_End) {
                            root.select(event.key === Qt.Key_Home ? 0 : root.moves.moveCount)
                        } else {
                            return
                        }
                        event.accepted = true
                    }
                }
            }

            RowLayout {
                spacing: Ez.Metrics.columnSpacing
                Ez.Button {
                    objectName: "pushDelete"
                    text: qsTr("Delete")
                    onClicked: root.removeSelected()
                }
                Ez.Button {
                    objectName: "pushClear"
                    text: qsTr("Clear List")
                    onClicked: {
                        root.moves.clear()
                        root.select(0)
                    }
                }
                Item {
                    Layout.fillWidth: true
                }
            }
            Ez.CheckBox {
                objectName: "checkRepeat"
                text: qsTr("Repeat the moves")
                checked: root.moves.repeat
                onToggled: root.moves.repeat = checked
            }
            Ez.CheckBox {
                objectName: "checkSkippable"
                text: qsTr("Skip moves that cannot be done")
                checked: root.moves.skippable
                onToggled: root.moves.skippable = checked
            }
        }

        // a button per move, in groups, two columns
        Repeater {
            model: [[0, 1], [2, 3]]

            ColumnLayout {
                id: column

                required property var modelData

                spacing: 8
                Layout.alignment: Qt.AlignTop

                Repeater {
                    model: column.modelData

                    ColumnLayout {
                        id: group

                        required property int modelData

                        spacing: 2
                        Layout.fillWidth: true

                        Repeater {
                            model: root.moves.commandGroupList()[group.modelData] ?? []

                            Ez.Button {
                                required property int modelData

                                text: root.moves.buttonText(modelData)
                                Layout.fillWidth: true
                                onClicked: root.insert(modelData)
                            }
                        }
                    }
                }
            }
        }
    }

    Ez.ContextMenu {
        id: menu

        readonly property bool selection: root.selectedMoves().length > 0
        readonly property string iconFolder: Kirigami.Theme.backgroundColor.hslLightness < 0.5 ? "qrc:/dark/" : "qrc:/bright/"

        font: Ez.Metrics.font

        Controls.MenuItem {
            text: qsTranslate("EditActions", "Cut")
            icon.source: menu.iconFolder + "edit-cut"
            enabled: menu.selection
            onTriggered: root.copySelected(true)
        }
        Controls.MenuItem {
            text: qsTranslate("EditActions", "Copy")
            icon.source: menu.iconFolder + "edit-copy"
            enabled: menu.selection
            onTriggered: root.copySelected(false)
        }
        Controls.MenuItem {
            text: qsTranslate("EditActions", "Paste")
            icon.source: menu.iconFolder + "edit-paste"
            enabled: root.moves.canPaste
            onTriggered: root.paste()
        }
        Controls.MenuItem {
            text: qsTranslate("EditActions", "Delete")
            icon.source: menu.iconFolder + "edit-delete"
            enabled: menu.selection
            onTriggered: root.removeSelected()
        }
    }
}
