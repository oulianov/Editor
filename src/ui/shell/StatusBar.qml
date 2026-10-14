// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "MainWindow"

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * The status bar of the main window, as RPG Maker's: the hint of the action
 * hovered (else a note of the project, a file another program changed...),
 * then the edit mode and the tool, the map shown and the cell under the
 * pointer, each named for the scripts.
 */
Controls.ToolBar {
    id: root

    required property Ez.Shell shell
    /** The hint of the action hovered, empty for none */
    property string hint
    /** The cell under the pointer and its events */
    property string cellText

    objectName: "statusBar"
    position: Controls.ToolBar.Footer
    topPadding: 2
    bottomPadding: 2
    leftPadding: 6
    rightPadding: 6

    contentItem: RowLayout {
        spacing: 12

        Ez.Label {
            objectName: "labelStatus"
            text: root.hint !== "" ? root.hint : root.shell ? root.shell.statusMessage : ""
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
        Ez.Label {
            objectName: "labelEditMode"
            text: root.shell ? root.shell.controller.editModeText : ""
            visible: text !== ""
        }
        Ez.Label {
            objectName: "labelMapName"
            text: root.shell ? root.shell.controller.mapText : ""
            visible: text !== ""
        }
        Ez.Label {
            objectName: "labelCell"
            text: root.shell && root.shell.currentEditor ? root.cellText : ""
            visible: text !== ""
        }
    }
}
