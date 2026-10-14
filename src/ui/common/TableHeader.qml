// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * The titles of the columns of a table (its syncView), compact, in the
 * colors of the theme (light and dark)
 */
Controls.HorizontalHeaderView {
    id: root

    clip: true
    boundsBehavior: Flickable.StopAtBounds

    delegate: Rectangle {
        id: cell
        required property string display

        implicitWidth: title.implicitWidth + 2 * Ez.Metrics.columnSpacing
        implicitHeight: title.implicitHeight + 2 * Ez.Metrics.optionPadding + 2
        color: Kirigami.Theme.alternateBackgroundColor
        Kirigami.Theme.colorSet: Kirigami.Theme.Button
        Kirigami.Theme.inherit: false

        Text {
            id: title
            anchors.fill: parent
            anchors.leftMargin: Ez.Metrics.columnSpacing
            anchors.rightMargin: Ez.Metrics.columnSpacing
            text: cell.display
            font: Ez.Metrics.font
            color: Kirigami.Theme.textColor
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        // the lines between the titles and under them
        Rectangle {
            anchors.right: parent.right
            width: 1
            height: parent.height
            color: Ez.Metrics.sectionBorderColor
        }
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Ez.Metrics.sectionBorderColor
        }
    }
}
