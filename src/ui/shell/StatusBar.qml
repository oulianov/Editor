// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "MainWindow"

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * The status bar of the main window: the hint of the action hovered (else a
 * note of the project, a file another program changed...), then the events
 * of the cell under the pointer, the map shown and the cell, each named for
 * the scripts. The cell is in a monospaced font, as wide as the widest
 * ("(499, 499)"), so that nothing moves with the pointer.
 */
Controls.ToolBar {
    id: root

    required property Ez.Shell shell
    /** The hint of the action hovered, empty for none */
    property string hint
    /** The cell under the pointer, "(x, y)" */
    property string cellText
    /** Its events on the event layer ("Event 3: name"), else empty */
    property string cellEvents

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
            objectName: "labelCellEvents"
            text: root.shell && root.shell.currentEditor ? root.cellEvents : ""
            visible: text !== ""
            elide: Text.ElideRight
            Layout.maximumWidth: root.width / 3
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
            font: Ez.Metrics.monospaceFont
            Layout.minimumWidth: cellMetrics.advanceWidth
            TextMetrics {
                id: cellMetrics
                font: Ez.Metrics.monospaceFont
                text: "(000, 000)"
            }
        }
    }
}
