// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "PreviewZoom"

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/** The Zoom Out and Zoom In buttons of an Ez.ZoomView, together */
RowLayout {
    id: root

    /** The Ez.ZoomView they zoom */
    property Ez.ZoomView view: null

    spacing: Ez.Metrics.columnSpacing

    Ez.Button {
        objectName: "pushZoomOut"
        text: qsTr("Zoom Out")
        enabled: root.view !== null && root.view.canZoomOut
        onClicked: root.view.zoomOut()
    }
    Ez.Button {
        objectName: "pushZoomIn"
        text: qsTr("Zoom In")
        enabled: root.view !== null && root.view.canZoomIn
        onClicked: root.view.zoomIn()
    }
}
