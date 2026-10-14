// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez
// the base of the title bars of KDDockWidgets, in its resources
// qmllint disable import unused-imports
import "qrc:/kddockwidgets/qtquick/views/qml/" as KDDW
// qmllint enable import unused-imports

/**
 * The title bar of a dock of the main window (the palette, the map tree),
 * thin and in the colors of the window, light or dark: its title, and the
 * button that floats it (the docks are not closed: the View menu hides
 * them).
 */
// qmllint disable import unresolved-type
KDDW.TitleBarBase {
    id: root

    color: colors.window
    heightWhenVisible: Math.max(18, title.implicitHeight + 4)

    SystemPalette {
        id: colors
    }

    Ez.Label {
        id: title
        anchors.left: parent.left
        anchors.leftMargin: 5
        anchors.right: buttons.left
        anchors.verticalCenter: parent.verticalCenter
        text: root.title
        elide: Text.ElideRight
        font: Ez.Metrics.sectionTitleFont
        color: Ez.Metrics.mutedTextColor
    }

    Row {
        id: buttons
        anchors.right: parent.right
        anchors.rightMargin: 2
        anchors.verticalCenter: parent.verticalCenter
        spacing: 2

        Controls.ToolButton {
            objectName: "floatButton"
            visible: root.floatButtonVisible
            width: root.height - 2
            height: root.height - 2
            padding: 1
            focusPolicy: Qt.NoFocus
            icon.source: "qrc:/img/dock-float.png"
            icon.color: colors.windowText
            icon.width: 10
            icon.height: 10
            onClicked: root.floatButtonClicked()
        }
        Controls.ToolButton {
            objectName: "closeButton"
            visible: root.closeButtonEnabled
            width: root.height - 2
            height: root.height - 2
            padding: 1
            focusPolicy: Qt.NoFocus
            icon.source: "qrc:/img/close.png"
            icon.color: colors.windowText
            icon.width: 10
            icon.height: 10
            onClicked: root.closeButtonClicked()
        }
    }
}
// qmllint enable import unresolved-type
