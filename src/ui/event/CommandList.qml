// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "EventCommandsWidget"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * The command list of an event page, a common event or a battle page, as
 * RPG Maker 2003 shows it: one line per line of the commands, indented by
 * their blocks, colored by kind of command, every other line tinted, the
 * empty "<>" lines where commands are inserted, then the end of the page.
 *
 *   Ez.CommandListEditor { id: editor; projectData: Ez.ProjectData }
 *   CommandList { editor: editor }
 *
 * The keys of RPG Maker: Enter (or Insert, a double-click) inserts a command
 * before the line, Space (or a click on the line selected) edits it, the
 * arrows go from command to command (Left and Right as Up and Down), PgUp
 * and PgDn by half a page, Shift with them (or a click) selects several
 * commands; Delete, Ctrl+C, Ctrl+X, Ctrl+V, Ctrl+Z, Ctrl+Shift+Z, Ctrl+A,
 * and Ctrl+Shift+O opens the script (rpgsync games). The right click opens
 * the menu of the lines.
 *
 * One line is current (the list's currentIndex); the other lines of its
 * command (the lines of a message, a whole branch...) are selected with
 * it, and with Shift the commands up to the line where the selection
 * started (anchor).
 */
FocusScope {
    id: root

    /** The lines and what is done with them (Ez.CommandListEditor) */
    required property Ez.CommandListEditor editor
    /** The list itself (objectName "commands", which scripts use) */
    readonly property alias view: view
    property alias currentIndex: view.currentIndex

    /** The line where a selection with Shift started (-1: none) */
    property int anchor: -1
    /** The lines drawn selected: [highlightFirst, highlightEnd) */
    property int highlightFirst: -1
    property int highlightEnd: -1
    /** The lines Cut, Copy and Delete act on: [selectionFirst, selectionEnd) */
    property int selectionFirst: -1
    property int selectionEnd: -1
    readonly property bool multiSelection: anchor >= 0 && view.currentIndex >= 0 && anchor !== view.currentIndex

    /** Selects the first line from index that can be selected */
    function selectIndex(index) {
        for (let line = Math.max(0, index); line < view.count; ++line) {
            if (editor.isSelectable(line)) {
                setCurrent(line, false)
                return
            }
        }
    }
    /** A line current and the list focused (the scripts' select-command); false when there is none */
    function selectLine(line) {
        if (line < 0 || line >= view.count) {
            return false
        }
        setCurrent(line, false)
        view.forceActiveFocus()
        return true
    }
    /** Every command of the list, like Ctrl+A */
    function selectAll() {
        if (editor.lineCount === 0) {
            return
        }
        // from the first line to the end of the page, the current one first
        setCurrent(0, true)
        anchor = editor.lineCount
        updateHighlight()
    }
    /** Half a page up or down, to the next command, like PgUp and PgDn */
    function pageStep(up) {
        if (view.currentIndex < 0) {
            selectIndex(0)
            return
        }
        const half = Math.max(1, Math.floor(Math.floor(view.height / view.rowHeight) / 2))
        setCurrent(nextCommandLine(view.currentIndex, up, half), false)
    }
    /** The line and the scroll shown, to show them again (restoreViewState) */
    function viewState() {
        return { line: view.currentIndex, top: view.contentY }
    }
    function restoreViewState(state) {
        if (!state) {
            return
        }
        if (state.line !== undefined && state.line >= 0 && state.line < view.count) {
            setCurrent(state.line, false)
        }
        if (state.top !== undefined) {
            view.contentY = Math.max(0, Math.min(state.top, view.contentHeight - view.height))
        }
    }

    // the current line; extend: the anchor stays (Shift)
    property bool extending: false
    function setCurrent(line, extend) {
        extending = extend
        view.currentIndex = line
        extending = false
        if (!extend) {
            anchor = line
        }
        view.positionViewAtIndex(line, ListView.Contain)
        updateHighlight()
    }
    function updateHighlight() {
        const line = view.currentIndex
        let selection = [-1, -1]
        let highlight = [-1, -1]
        if (line >= 0 && multiSelection) {
            selection = editor.wholeCommands(Math.min(anchor, line), Math.max(anchor, line) + 1)
            highlight = selection[0] < selection[1] ? selection : [line, line + 1]
        } else if (line >= 0 && editor.isSelectable(line)) {
            selection = editor.commandRange(line)
            highlight = selection[0] < selection[1] ? selection : [line, line + 1]
        }
        selectionFirst = selection[0]
        selectionEnd = selection[1]
        highlightFirst = highlight[0]
        highlightEnd = highlight[1]
    }
    /** The lines Cut, Copy and Delete act on for a line: the selection it is in, else its command */
    function rangeFor(line) {
        if (line === view.currentIndex || (multiSelection && line >= highlightFirst && line < highlightEnd)) {
            return [selectionFirst, selectionEnd]
        }
        return editor.commandRange(line)
    }
    /** Where Insert and Paste put commands for a line: before the selection, never between the lines of a message */
    function insertionIndexFor(line) {
        if (multiSelection && line >= highlightFirst && line < highlightEnd) {
            return highlightFirst
        }
        return line >= 0 ? editor.commandStart(line) : view.count - 1
    }
    /** The next line the keys stop on, up or down from a line, after skipping lines (a page) */
    function nextCommandLine(from, up, skip) {
        const step = up ? -1 : 1
        let target = from
        for (let i = 0; i < skip && target + step >= 0 && target + step < view.count; ++i) {
            target += step
        }
        for (let next = target + step; next >= 0 && next < view.count; next += step) {
            if (editor.isCommandLine(next)) {
                return next
            }
        }
        // none further: the first or last line a page reached, else no move
        return target !== from && editor.isCommandLine(target) ? target : from
    }
    /** The first (or last) line that can be selected, from a line */
    function selectableFrom(line, up) {
        for (let i = line; i >= 0 && i < view.count; i += up ? -1 : 1) {
            if (editor.isSelectable(i)) {
                return i
            }
        }
        return view.currentIndex
    }

    // The windows of the commands are modal, opened once the event that
    // asked for them is done: the lines are made again meanwhile
    function insertAt(index) {
        Qt.callLater(() => {
            const line = root.editor.insertCommand(index, view)
            if (line >= 0) {
                root.selectIndex(line)
            }
            view.forceActiveFocus()
        })
    }
    function edit(line) {
        Qt.callLater(() => {
            const shown = root.editor.editCommand(line, view)
            if (shown >= 0) {
                root.selectIndex(shown)
            }
            view.forceActiveFocus()
        })
    }
    function addRaw(line) {
        Qt.callLater(() => {
            const added = root.editor.addRaw(line, view)
            if (added >= 0) {
                root.selectIndex(added)
            }
        })
    }
    function editRaw(line) {
        Qt.callLater(() => {
            const shown = root.editor.editRaw(line, view)
            if (shown >= 0) {
                root.selectIndex(shown)
            }
        })
    }
    function remove(range) {
        const line = editor.removeLines(range[0], range[1])
        if (line >= 0) {
            selectIndex(line)
        }
    }
    function copy(range, cut) {
        const line = editor.copyLines(range[0], range[1], cut)
        if (cut && line >= 0) {
            selectIndex(line)
        }
    }
    function paste(index) {
        const line = editor.paste(index)
        if (line >= 0) {
            selectIndex(line)
        }
    }
    function openScript() {
        if (editor.scriptLocation.length > 0 && editor.scriptUnavailableReason() === "") {
            editor.openScript(view.currentIndex, view)
        }
    }

    implicitWidth: 240
    implicitHeight: 160

    // every change of the lines (undo too): the selection starts from the current line again
    Connections {
        target: root.editor
        function onLinesChanged() {
            root.anchor = -1
            root.widest = 0
            root.updateHighlight()
        }
    }

    // the look: the colors of the kinds of commands, the stripes
    Kirigami.Theme.colorSet: Kirigami.Theme.View
    Kirigami.Theme.inherit: false
    readonly property var kindColors: editor.lineColors(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor,
                                                        Kirigami.Theme.disabledTextColor, Ez.Editor.commandColors)
    readonly property color stripeColor: editor.stripeColor(Kirigami.Theme.backgroundColor)
    readonly property bool stripes: Ez.Editor.commandStripes
    // a nested line starts under the text of its parent, after its "<>"
    readonly property real indentWidth: Math.max(8, metrics.advanceWidth("<>"))
    // the next lines of a message (a comment) start under the text of the first one
    readonly property real messageOffset: metrics.advanceWidth(editor.firstLineLabel(20110) + ": ") - metrics.advanceWidth(": ")
    readonly property real commentOffset: metrics.advanceWidth(editor.firstLineLabel(22410) + ": ") - metrics.advanceWidth(": ")
    // the widest line shown: lines are as wide (wider ones scroll sideways, as RPG Maker)
    property real widest: 0
    onWidestChanged: Qt.callLater(view.updateContentWidth)

    FontMetrics {
        id: metrics
        font: Ez.Metrics.font
    }

    Rectangle {
        id: frame
        anchors.fill: parent
        color: Kirigami.Theme.backgroundColor
        border.color: view.activeFocus ? Kirigami.Theme.focusColor : Ez.Metrics.sectionBorderColor
        radius: 2
    }

    ListView {
        id: view

        readonly property real rowHeight: Math.ceil(metrics.height) + 2 * Ez.Metrics.listRowPadding

        objectName: "commands"
        anchors.fill: parent
        anchors.margins: 1
        clip: true
        focus: true
        model: root.editor.model
        currentIndex: -1
        keyNavigationEnabled: false
        highlightFollowsCurrentItem: false
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.AutoFlickIfNeeded
        // set once what it depends on settled: as a binding, the scroll bars
        // it shows or hides change it again (a binding loop)
        function updateContentWidth() {
            contentWidth = Math.max(width - (vbar.visible ? vbar.width : 0), root.widest)
        }
        onWidthChanged: Qt.callLater(updateContentWidth)
        Component.onCompleted: updateContentWidth()
        // the empty space below the lines is the end of the page's (a click selects it)
        footer: Item {
            // not the content's width: it would be measured from the footer
            width: 1
            height: Math.max(0, view.height - view.count * view.rowHeight)
        }

        Controls.ScrollBar.vertical: Controls.ScrollBar {
            id: vbar
            onVisibleChanged: Qt.callLater(view.updateContentWidth)
            onWidthChanged: Qt.callLater(view.updateContentWidth)
        }
        Controls.ScrollBar.horizontal: Controls.ScrollBar {
            id: hbar
        }

        onCurrentIndexChanged: {
            if (!root.extending) {
                root.anchor = currentIndex
            }
            root.updateHighlight()
        }
        onModelChanged: root.widest = 0
        // the keyboard reaches the list: a line is current, as in a tree
        onActiveFocusChanged: {
            if (!activeFocus) {
                editTimer.stop()
            } else if (currentIndex < 0 && count > 0) {
                root.selectIndex(0)
            }
        }
        Keys.onPressed: (event) => root.handleKey(event)

        delegate: Item {
            id: line

            required property int index
            required property string display
            required property int indent
            required property int code
            required property int colorKind
            readonly property bool selected: index >= root.highlightFirst && index < root.highlightEnd

            width: view.contentWidth
            height: view.rowHeight
            implicitWidth: label.x + label.implicitWidth + Ez.Metrics.listTextPadding
            onImplicitWidthChanged: root.widest = Math.max(root.widest, implicitWidth)
            Component.onCompleted: root.widest = Math.max(root.widest, implicitWidth)

            // every other line tinted, as RPG Maker 2003
            Rectangle {
                anchors.fill: parent
                visible: line.selected || (root.stripes && line.index % 2 === 1)
                color: line.selected ? Kirigami.Theme.highlightColor : root.stripeColor
            }
            Text {
                id: label
                x: Ez.Metrics.listTextPadding + Math.max(0, line.indent) * root.indentWidth
                   + (line.code === 20110 ? root.messageOffset : line.code === 22410 ? root.commentOffset : 0)
                anchors.verticalCenter: parent.verticalCenter
                text: line.display
                textFormat: Text.PlainText
                font: Ez.Metrics.font
                color: line.selected ? Kirigami.Theme.highlightedTextColor
                                     : (root.kindColors[line.colorKind] ?? Kirigami.Theme.textColor)
            }
        }
    }

    // The mouse on the lines (not on the scroll bars, the wheel goes through)
    MouseArea {
        id: mouse

        // the line under a point, the end of the page below the lines
        function lineAt(y) {
            const line = view.indexAt(view.contentX + 1, view.contentY + y)
            if (line >= 0) {
                return line
            }
            return view.contentY + y >= view.count * view.rowHeight + view.originY ? view.count - 1 : -1
        }

        property int hovered: -1
        // pressed on the line already selected, alone: released there, it is edited
        property bool pressOnCurrent: false

        anchors.fill: view
        anchors.rightMargin: vbar.visible ? vbar.width : 0
        anchors.bottomMargin: hbar.visible ? hbar.height : 0
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        hoverEnabled: true

        onPressed: (event) => {
            view.forceActiveFocus()
            editTimer.stop()
            const line = lineAt(event.y)
            pressOnCurrent = line >= 0 && line === view.currentIndex && event.button === Qt.LeftButton
                             && event.modifiers === Qt.NoModifier && !root.multiSelection
            if (line < 0 || !root.editor.isSelectable(line)) {
                return
            }
            // a right click in the selection keeps it, for its menu
            if (event.button === Qt.RightButton && root.multiSelection
                    && line >= root.highlightFirst && line < root.highlightEnd) {
                return
            }
            // Shift+click selects the commands from the current line to this one
            root.setCurrent(line, (event.modifiers & Qt.ShiftModifier) && event.button === Qt.LeftButton)
        }
        onPositionChanged: (event) => {
            hovered = lineAt(event.y)
            // dragging selects from the line pressed, like Shift+click
            if (pressed && (event.buttons & Qt.LeftButton) && hovered >= 0 && hovered !== view.currentIndex
                    && root.editor.isSelectable(hovered)) {
                root.setCurrent(hovered, true)
            }
        }
        onExited: hovered = -1
        onReleased: (event) => {
            if (pressOnCurrent && lineAt(event.y) === view.currentIndex) {
                editTimer.start()
            }
            pressOnCurrent = false
        }
        onClicked: (event) => {
            if (event.button === Qt.RightButton) {
                menu.line = lineAt(event.y)
                menu.popupAt(mouse, event.x, event.y)
            }
        }
        // as in RPG Maker, a double-click inserts a command before the line, like Enter
        onDoubleClicked: (event) => {
            editTimer.stop()
            pressOnCurrent = false
            const line = lineAt(event.y)
            if (event.button === Qt.LeftButton && line >= 0 && root.editor.isSelectable(line)) {
                root.insertAt(root.insertionIndexFor(line))
            }
        }
    }

    // A click on the line already selected edits its command, once no
    // double-click (insert) follows
    Timer {
        id: editTimer
        interval: Qt.styleHints.mouseDoubleClickInterval
        onTriggered: {
            const line = view.currentIndex
            // a command to edit: not the empty line closing a block
            if (root.visible && !root.multiSelection && line >= 0 && line < root.editor.lineCount
                    && !root.editor.isEmptyLine(line)) {
                root.edit(line)
            }
        }
    }

    // the keys of the list (RPG Maker's)
    function handleKey(event) {
        editTimer.stop()
        const line = view.currentIndex
        const shift = (event.modifiers & Qt.ShiftModifier) !== 0
        if (event.key === Qt.Key_O && event.modifiers === (Qt.ControlModifier | Qt.ShiftModifier)) {
            openScript()
        } else if (event.matches(StandardKey.Undo)) {
            editor.undo()
        } else if (event.matches(StandardKey.Redo)) {
            editor.redo()
        } else if (event.matches(StandardKey.SelectAll)) {
            selectAll()
        } else if (event.matches(StandardKey.Copy)) {
            if (line >= 0) {
                copy(rangeFor(line), false)
            }
        } else if (event.matches(StandardKey.Cut)) {
            if (line >= 0) {
                copy(rangeFor(line), true)
            }
        } else if (event.matches(StandardKey.Paste)) {
            paste(insertionIndexFor(line))
        } else if (event.key === Qt.Key_Delete || event.key === Qt.Key_Backspace) {
            if (line >= 0) {
                remove(rangeFor(line))
            }
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Insert) {
            // Enter inserts and Space edits, as in RPG Maker
            insertAt(insertionIndexFor(line))
        } else if (event.key === Qt.Key_Space) {
            if (line >= 0) {
                edit(line)
            }
        } else if (event.key === Qt.Key_Up || event.key === Qt.Key_Left || event.key === Qt.Key_Down || event.key === Qt.Key_Right) {
            // as in RPG Maker 2003: Left and Right as Up and Down, from command
            // to command (a message and its lines are one), without wrapping
            const up = event.key === Qt.Key_Up || event.key === Qt.Key_Left
            if (line < 0) {
                selectIndex(0)
            } else {
                setCurrent(nextCommandLine(line, up, 0), shift)
            }
        } else if (event.key === Qt.Key_PageUp || event.key === Qt.Key_PageDown) {
            if (line < 0) {
                selectIndex(0)
            } else {
                const half = Math.max(1, Math.floor(Math.floor(view.height / view.rowHeight) / 2))
                setCurrent(nextCommandLine(line, event.key === Qt.Key_PageUp, half), shift)
            }
        } else if (event.key === Qt.Key_Home) {
            setCurrent(selectableFrom(0, false), shift)
        } else if (event.key === Qt.Key_End) {
            setCurrent(selectableFrom(view.count - 1, true), shift)
        } else {
            return
        }
        event.accepted = true
    }

    // the icons drawn for the colors of the menus
    readonly property string iconFolder: Kirigami.Theme.backgroundColor.hslLightness < 0.5 ? "qrc:/dark/" : "qrc:/bright/"

    // The menu of the lines: a line of the selection acts on the whole selection
    Ez.ContextMenu {
        id: menu

        /** The line clicked */
        property int line: -1
        readonly property var range: line >= 0 ? root.rangeFor(line) : [-1, -1]
        readonly property bool removable: range[0] >= 0 && range[0] < range[1]
        property bool canPaste: false
        property string scriptReason: ""

        font: Ez.Metrics.font
        onAboutToShow: {
            canPaste = root.editor.canPaste()
            scriptReason = root.editor.scriptUnavailableReason()
        }

        Controls.MenuItem {
            text: qsTranslate("EditActions", "Undo")
            icon.source: root.iconFolder + "edit-undo"
            enabled: root.editor.canUndo
            onTriggered: root.editor.undo()
        }
        Controls.MenuItem {
            text: qsTranslate("EditActions", "Redo")
            icon.source: root.iconFolder + "edit-redo"
            enabled: root.editor.canRedo
            onTriggered: root.editor.redo()
        }
        Controls.MenuSeparator {}
        Controls.MenuItem {
            text: qsTranslate("EditActions", "Insert...")
            icon.source: root.iconFolder + "new"
            onTriggered: root.insertAt(root.insertionIndexFor(menu.line))
        }
        Controls.MenuItem {
            // Space, as in RPG Maker: Enter inserts
            text: qsTranslate("EditActions", "Edit...")
            icon.source: root.iconFolder + "properties"
            visible: menu.line >= 0 && root.editor.hasEditor(menu.line)
            onTriggered: root.edit(menu.line)
        }
        Controls.MenuSeparator {}
        Controls.MenuItem {
            text: qsTranslate("EditActions", "Cut")
            icon.source: root.iconFolder + "edit-cut"
            enabled: menu.removable
            onTriggered: root.copy(menu.range, true)
        }
        Controls.MenuItem {
            text: qsTranslate("EditActions", "Copy")
            icon.source: root.iconFolder + "edit-copy"
            enabled: menu.removable
            onTriggered: root.copy(menu.range, false)
        }
        Controls.MenuItem {
            text: qsTranslate("EditActions", "Paste")
            icon.source: root.iconFolder + "edit-paste"
            enabled: menu.canPaste
            onTriggered: root.paste(root.insertionIndexFor(menu.line))
        }
        Controls.MenuItem {
            text: qsTranslate("EditActions", "Delete")
            icon.source: root.iconFolder + "edit-delete"
            enabled: menu.removable
            onTriggered: root.remove(menu.range)
        }
        Controls.MenuSeparator {}
        Controls.MenuItem {
            text: qsTranslate("EditActions", "Select All")
            icon.source: root.iconFolder + "select-all"
            enabled: root.editor.lineCount > 0
            onTriggered: root.selectAll()
        }
        Controls.MenuSeparator {
            visible: script.visible
        }
        Controls.MenuItem {
            id: script
            // greyed without rpgsync, the tooltip says how to set it up
            text: qsTr("Open in Script Editor")
            icon.source: root.iconFolder + "code"
            visible: root.editor.scriptLocation.length > 0
            enabled: menu.scriptReason === ""
            hoverEnabled: true
            onTriggered: root.openScript()
            Controls.ToolTip.visible: hovered
            Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
            Controls.ToolTip.text: menu.scriptReason !== "" ? menu.scriptReason : qsTr("Open the script at the line of this command")
        }
        Controls.MenuSeparator {}
        Controls.MenuItem {
            text: qsTr("Add raw") + "..."
            onTriggered: root.addRaw(menu.line)
        }
        Controls.MenuItem {
            text: qsTr("Edit raw") + "..."
            visible: menu.line >= 0 && menu.line < root.editor.lineCount
            onTriggered: root.editRaw(menu.line)
        }
    }
}
