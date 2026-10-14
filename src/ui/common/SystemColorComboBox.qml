// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "SystemColorComboBox"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * One of the 20 colors of the game's text (the color of a condition's
 * name...), each shown as the System graphic of the database draws it:
 * with jsonData and key, the number of the data is the color.
 *
 *   Ez.SystemColorComboBox { database: page.database; jsonData: page.entry; key: "color" }
 */
Ez.ComboBox {
    id: root

    /** The database edited (a JsonView): its System graphic */
    property var database: null
    /** The picture of the colors (the System graphic) */
    readonly property string systemImage: systemName.valid && systemName.value !== ""
                                          ? "image://project/System/" + systemName.value : ""

    model: {
        const colors = []
        for (let i = 0; i < 20; ++i) {
            colors.push({ text: qsTr("Color %1").arg(i), value: i })
        }
        return colors
    }
    leftPadding: horizontalPadding + swatchSize + Ez.Metrics.columnSpacing

    readonly property int swatchSize: 16

    Ez.JsonField {
        id: systemName
        source: root.database
        path: "system/system_name"
    }

    /** A color as the System graphic draws it: its square under the window skin */
    component Swatch: Item {
        id: swatch
        property int color: 0
        implicitWidth: root.swatchSize
        implicitHeight: root.swatchSize
        clip: true
        Image {
            x: -(swatch.color % 10) * 16
            y: -(Math.floor(swatch.color / 10) * 16 + 48)
            source: root.systemImage
            smooth: false
        }
    }

    Swatch {
        x: root.horizontalPadding
        anchors.verticalCenter: parent.verticalCenter
        color: Math.max(0, root.currentIndex)
        visible: root.systemImage !== "" && root.currentIndex >= 0
    }

    delegate: Controls.ItemDelegate {
        id: line

        required property int index
        required property var modelData

        width: ListView.view ? ListView.view.width : implicitWidth
        highlighted: root.highlightedIndex === index
        font: Ez.Metrics.font
        contentItem: RowLayout {
            spacing: Ez.Metrics.columnSpacing
            Swatch {
                color: line.index
                visible: root.systemImage !== ""
            }
            Ez.Label {
                text: line.modelData.text
                font: Ez.Metrics.font
                color: line.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                Layout.fillWidth: true
            }
        }
    }
}
