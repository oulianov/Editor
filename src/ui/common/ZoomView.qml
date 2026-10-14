// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * A picture of the game in a scrolled view, enlarged pixel for pixel (the
 * previews of the Resource Manager and of the image import), centered
 * when smaller than the view. Ez.ZoomButtons go to the next whole-pixel
 * level (x1, x2, x4...), sharp for pixel art; a trackpad pinch zooms
 * freely, under the fingers.
 *
 * picking: a click or a drag on the picture tells the pixel under it.
 */
Rectangle {
    id: root

    property alias image: view.image
    property real zoom: 1
    readonly property real minimumZoom: 1
    readonly property real maximumZoom: 16
    readonly property bool canZoomIn: zoom < maximumZoom
    readonly property bool canZoomOut: zoom > minimumZoom
    property bool picking: false

    /** The pixel of the picture clicked (picking) */
    signal picked(int x, int y)

    /** Zooms to value; the point at x, y of the view (by default its centre) stays in place */
    function setZoom(value, x, y) {
        value = Math.max(minimumZoom, Math.min(maximumZoom, value))
        if (value === zoom) {
            return
        }
        const anchorX = x ?? flick.width / 2
        const anchorY = y ?? flick.height / 2
        const pointX = (flick.contentX + anchorX) / zoom
        const pointY = (flick.contentY + anchorY) / zoom
        zoom = value
        flick.contentX = place(pointX * zoom - anchorX, flick.contentWidth, flick.width, flick.leftMargin)
        flick.contentY = place(pointY * zoom - anchorY, flick.contentHeight, flick.height, flick.topMargin)
    }
    // a level just reached by a pinch counts as reached (x1.999 goes to x2, not x4)
    function zoomIn() {
        setZoom(Math.pow(2, Math.floor(Math.log2(zoom) + 1e-6) + 1))
    }
    function zoomOut() {
        setZoom(Math.pow(2, Math.ceil(Math.log2(zoom) - 1e-6) - 1))
    }
    // a scroll position within the bounds (centered: before the content)
    function place(position, content, size, margin) {
        return content <= size ? -margin : Math.max(0, Math.min(content - size, position))
    }

    implicitWidth: 120
    implicitHeight: 120
    Kirigami.Theme.colorSet: Kirigami.Theme.View
    Kirigami.Theme.inherit: false
    color: Kirigami.Theme.backgroundColor
    border.color: Ez.Metrics.sectionBorderColor

    Ez.ZoomImageView {
        id: view
        objectName: "zoomImage"
        anchors.fill: flick
        zoom: root.zoom
        contentX: flick.contentX
        contentY: flick.contentY
    }

    Flickable {
        id: flick
        anchors.fill: parent
        anchors.margins: 1
        clip: true
        contentWidth: view.contentWidth
        contentHeight: view.contentHeight
        leftMargin: Math.max(0, (width - contentWidth) / 2)
        topMargin: Math.max(0, (height - contentHeight) / 2)
        boundsBehavior: Flickable.StopAtBounds
        Controls.ScrollBar.horizontal: Controls.ScrollBar {}
        Controls.ScrollBar.vertical: Controls.ScrollBar {}

        MouseArea {
            width: flick.contentWidth
            height: flick.contentHeight
            enabled: root.picking
            cursorShape: root.picking ? Qt.CrossCursor : Qt.ArrowCursor
            // a drag picks too, it does not scroll
            preventStealing: true

            function pick(mouse) {
                root.picked(Math.floor(mouse.x / root.zoom), Math.floor(mouse.y / root.zoom))
            }
            onPressed: mouse => pick(mouse)
            onPositionChanged: mouse => {
                if (pressed) {
                    pick(mouse)
                }
            }
        }
    }

    PinchHandler {
        property real startZoom: 1

        target: null
        onActiveChanged: {
            if (active) {
                startZoom = root.zoom
            }
        }
        onActiveScaleChanged: {
            if (active) {
                const point = flick.mapFromItem(root, centroid.position.x, centroid.position.y)
                root.setZoom(startZoom * activeScale, point.x, point.y)
            }
        }
    }
}
