// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "MapPreview"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * The zoom buttons of RPG Maker's map views (TFORMMAPSETPLACE: 1/1, 1/2,
 * 1/4 beside the location), for a MapCellView: the icons of the zoom
 * actions of the main window's map, 1/8 too, the one shown pressed.
 */
RowLayout {
    id: root

    /** The MapCellView they zoom */
    required property var target

    spacing: 0
    Repeater {
        model: [
            { name: "zoomFull", text: qsTr("1/1"), tip: qsTranslate("MainWindow", "1/1 Ratio"), icon: "zoom-ratio-1", size: 32 },
            { name: "zoomHalf", text: qsTr("1/2"), tip: qsTranslate("MainWindow", "1/2 Ratio"), icon: "zoom-ratio-2", size: 16 },
            { name: "zoomQuarter", text: qsTr("1/4"), tip: qsTranslate("MainWindow", "1/4 Ratio"), icon: "zoom-ratio-4", size: 8 },
            { name: "zoomEighth", text: qsTr("1/8"), tip: qsTranslate("MainWindow", "1/8 Ratio"), icon: "zoom-ratio-8", size: 4 },
        ]
        Controls.ToolButton {
            id: button
            required property var modelData
            objectName: modelData.name
            text: modelData.text
            display: Controls.AbstractButton.IconOnly
            icon.source: (palette.window.hslLightness >= 0.5 ? "qrc:/bright/" : "qrc:/dark/") + modelData.icon
            icon.width: Ez.Metrics.toolIconSize
            icon.height: Ez.Metrics.toolIconSize
            padding: Ez.Metrics.toolButtonPadding
            focusPolicy: Qt.NoFocus
            checkable: true
            checked: root.target.cellSize === modelData.size
            Controls.ToolTip.text: modelData.tip
            Controls.ToolTip.visible: hovered
            Controls.ToolTip.delay: 600
            onClicked: {
                root.target.zoomTo(modelData.size)
                // checked from the view again: the click toggled it
                checked = Qt.binding(() => root.target.cellSize === button.modelData.size)
            }
        }
    }
}
