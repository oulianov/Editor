// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * The list of a picker in its frame: one line per row of the model (its
 * text), the current one highlighted, dense as RPG Maker's lists. The
 * current row is the one clicked or reached with the keys (picked);
 * activated: a double-click or Return on a row.
 */
Rectangle {
    id: root

    /** The name of the ListView, for the scripts */
    property alias listName: list.objectName
    property alias model: list.model
    property alias currentIndex: list.currentIndex
    /** The ListView (its objectName is the one of the picker) */
    readonly property alias view: list
    /** The role of the text of a line */
    property string textRole: "display"
    /** The role of a line shown dimmed (a file of the RTP); "": none */
    property string dimRole: ""
    /**
     * Typing selects a row starting with the letters typed, as the file
     * lists of the systems: (text, from) => the first such row from the row
     * from on (wrapping), -1 for none. null: no search
     */
    property var findRow: null
    /** The letters typed so far (a pause starts a new search) */
    property string typed: ""

    signal picked(int row)
    signal activated(int row)

    /** Shows a row, in the middle of the list when it was not shown */
    function show(row) {
        list.currentIndex = row
        if (row >= 0) {
            list.positionViewAtIndex(row, ListView.Contain)
        }
    }

    Kirigami.Theme.colorSet: Kirigami.Theme.View
    Kirigami.Theme.inherit: false
    color: Kirigami.Theme.backgroundColor
    border.color: Ez.Metrics.sectionBorderColor
    radius: 2
    implicitWidth: 160
    implicitHeight: 160

    Timer {
        id: typing
        interval: 1000
    }

    ListView {
        id: list
        anchors.fill: parent
        anchors.margins: 1
        clip: true
        focus: true
        activeFocusOnTab: true
        keyNavigationEnabled: true
        highlightMoveDuration: 0
        highlightResizeDuration: 0
        boundsBehavior: Flickable.StopAtBounds
        currentIndex: -1
        // shown while the list is longer than its frame, as RPG Maker's
        Controls.ScrollBar.vertical: Controls.ScrollBar {
            id: bar
            policy: Controls.ScrollBar.AlwaysOn
        }
        // the keys move the current row: the picker follows
        onCurrentIndexChanged: {
            if (currentIndex >= 0) {
                root.picked(currentIndex)
            }
        }
        Keys.onReturnPressed: root.activated(currentIndex)
        Keys.onEnterPressed: root.activated(currentIndex)
        Keys.onPressed: event => {
            if (!root.findRow || event.text === "" || event.text.charCodeAt(0) < 32
                    || (event.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier))
                    || (event.text === " " && !typing.running)) {
                return
            }
            // a new search starts past the row selected, a longer one on it
            const from = typing.running ? Math.max(0, currentIndex) : currentIndex + 1
            root.typed = typing.running ? root.typed + event.text : event.text
            typing.restart()
            const row = root.findRow(root.typed, from)
            if (row >= 0) {
                root.show(row)
            }
            event.accepted = true
        }

        delegate: Controls.ItemDelegate {
            id: line

            required property int index
            required property var model

            width: ListView.view.width - (bar.visible ? bar.width : 0)
            text: model[root.textRole] ?? ""
            font: Ez.Metrics.font
            topPadding: 1
            bottomPadding: 1
            leftPadding: 4
            rightPadding: 4
            highlighted: ListView.isCurrentItem
            opacity: root.dimRole !== "" && model[root.dimRole] && !highlighted ? 0.6 : 1

            onClicked: {
                list.forceActiveFocus()
                list.currentIndex = index
            }
            onDoubleClicked: root.activated(index)
        }
    }
}
