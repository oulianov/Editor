// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import org.easyrpg.editor as Ez

/**
 * What an empty preview of a resource shows (no file chosen yet, a click on
 * the preview choosing it): an image icon and "Click to choose a picture",
 * in the muted color of the titles. The text wraps to the width there is,
 * and is left out of a preview too small for it (a charset's: the icon
 * alone).
 *
 *   Ez.PickHint { anchors.centerIn: parent; visible: name === ""; availableWidth: parent.width }
 */
Column {
    id: root

    /** The width of the preview it is in */
    property real availableWidth: parent ? parent.width : 200

    spacing: 4

    Ez.EditorIcon {
        name: "image"
        width: 24
        height: 24
        opacity: 0.62
        anchors.horizontalCenter: parent.horizontalCenter
    }
    Ez.Label {
        objectName: "pickHint"
        text: qsTr("Click to choose a picture")
        color: Ez.Metrics.mutedTextColor
        font: Ez.Metrics.font
        width: Math.min(implicitWidth, root.availableWidth - 8)
        wrapMode: Text.WordWrap
        horizontalAlignment: Text.AlignHCenter
        visible: root.availableWidth >= 90
        anchors.horizontalCenter: parent.horizontalCenter
    }
}
