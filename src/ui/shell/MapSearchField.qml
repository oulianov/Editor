// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "MainWindow"

import QtQuick
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * The search above the map tree: the number of a map or a part of its name.
 * What is typed filters the tree at once (MapTree.filter): only the maps
 * found show, with the nodes they are in. Up and Down select the map found
 * before or after the one selected and show it, as the arrows on the tree
 * (all the nodes while nothing is typed); Return keeps it in its tab (the
 * map of the number typed, else the first one found, when none is
 * selected). Nothing found: the text is red. The cross at the right, or
 * Escape, empties it (Escape gives the keys back to the tree). A magnifier
 * before the text, in a frame of the theme's lines.
 */
Ez.TextField {
    id: root

    /** The map tree it filters */
    required property MapTree tree

    objectName: "treeMapSearch"
    placeholderText: qsTr("Map name or ID")
    enabled: tree.view.rows > 0 || text !== ""
    color: tree.filtering && tree.matchCount === 0 ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.textColor
    // a magnifier before the text, the cross after it, in a frame of the
    // theme's lines (the accent while typed in), not Breeze's grey one
    leftPadding: magnifier.x + magnifier.width + 4
    rightPadding: clear.visible ? clear.width + 4 : 4
    background: Rectangle {
        radius: 3
        color: Ez.Metrics.palette.base
        border.width: 1
        border.color: root.activeFocus ? Kirigami.Theme.focusColor : Ez.Metrics.sectionBorderColor
    }

    onTextChanged: tree.filter = text
    Keys.onUpPressed: tree.step(-1, tree.filtering)
    Keys.onDownPressed: tree.step(1, tree.filtering)
    onAccepted: tree.openFound()
    Keys.onEscapePressed: {
        text = ""
        tree.forceActiveFocus()
    }
    // another project, a node selected that the filter hides
    Connections {
        target: root.tree
        function onClearFilterRequested() {
            root.text = ""
        }
    }

    Ez.EditorIcon {
        id: magnifier
        name: "edit-find"
        x: 5
        width: 14
        height: 14
        anchors.verticalCenter: parent.verticalCenter
    }
    // empties the search, the field keeps the keys
    Controls.ToolButton {
        id: clear
        objectName: "treeMapSearchClear"
        visible: root.text !== ""
        text: "×"
        padding: 0
        leftPadding: 4
        rightPadding: 4
        height: parent.height - 4
        anchors.right: parent.right
        anchors.rightMargin: 2
        anchors.verticalCenter: parent.verticalCenter
        focusPolicy: Qt.NoFocus
        Controls.ToolTip.text: qsTr("Clear the search")
        Controls.ToolTip.visible: hovered
        Controls.ToolTip.delay: 600
        onClicked: root.text = ""

        // the pointer of a button, not the text cursor of the field
        HoverHandler {
            cursorShape: Qt.ArrowCursor
        }
    }
}
