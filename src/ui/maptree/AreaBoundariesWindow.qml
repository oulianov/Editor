// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "AreaBoundariesDialog"

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * Define Area Boundaries (RPG Maker's TFORMMAPSETAREA): the map the area is
 * on, on which a box is dragged around the cells the area covers.
 */
Ez.DialogWindow {
    id: root

    required property Ez.LocationPicker picker
    property bool readable: true
    /** The cells of the area, in cells; empty: no box yet */
    property rect area

    objectName: "AreaBoundariesDialog"
    title: qsTr("Define Area Boundaries")
    preferredWidth: 760
    height: Math.max(minimumHeight, 540)
    acceptable: view.hasSelection

    Component.onCompleted: {
        view.select(area)
        if (area.width > 0 && area.height > 0) {
            view.centerOn(Math.floor(area.x + area.width / 2), Math.floor(area.y + area.height / 2))
        }
        view.forceActiveFocus()
    }
    onAccepted: area = view.selection

    Ez.Note {
        text: qsTr("Drag a box around the part of the map the area covers.")
    }
    MapCellView {
        id: view
        objectName: "mapPreview"
        document: root.picker.document
        rectangleMode: true
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumWidth: 240
        Layout.minimumHeight: 180
    }
    RowLayout {
        spacing: Ez.Metrics.columnSpacing
        MapZoomButtons {
            target: view
        }
        Ez.Label {
            objectName: "labelBoundaries"
            text: !root.readable ? qsTr("The map cannot be read.")
                : view.hasSelection ? qsTr("X: %1  Y: %2  Width: %3  Height: %4").arg(view.selection.x).arg(view.selection.y)
                                          .arg(view.selection.width).arg(view.selection.height)
                : ""
            font: Ez.Metrics.font
            Layout.fillWidth: true
        }
    }
}
