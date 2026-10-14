// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "ImportImageDialog"

import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * The transparent color of an image imported into the game, as the Preview
 * window of RPG Maker's Resource Manager: a click on the image picks the
 * color, shown transparent at once. Small game graphics (charsets,
 * faces...) open enlarged, they are easier to click.
 */
Ez.DialogWindow {
    id: root

    /** The image and its transparent color (Ez.TransparentColorChoice) */
    property Ez.TransparentColorChoice choice: null

    /** The color of a pixel of the image becomes the transparent one */
    function pick(x, y) {
        if (choice) {
            choice.pick(x, y)
        }
    }

    objectName: "ImportImageDialog"
    title: qsTr("Select transparent color")
    preferredWidth: 680
    preferredHeight: 520

    Component.onCompleted: {
        if (choice && choice.size.width <= 320 && choice.size.height <= 256) {
            view.zoom = 2
        }
    }

    Ez.Note {
        objectName: "labelHelp"
        text: qsTr("Click the color that is transparent in the game.")
    }
    Ez.ZoomView {
        id: view
        objectName: "graphicsView"
        image: root.choice ? root.choice.shown : undefined
        picking: true
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 160
        onPicked: (x, y) => root.pick(x, y)
    }
    // the color picked on the left, the zoom buttons on the right (as in the Resource Manager)
    RowLayout {
        spacing: Ez.Metrics.columnSpacing
        Layout.fillWidth: true

        Rectangle {
            objectName: "labelSwatch"
            implicitWidth: 16
            implicitHeight: 16
            color: root.choice && root.choice.hasColor ? root.choice.color : "transparent"
            border.color: Kirigami.Theme.textColor
            Layout.alignment: Qt.AlignTop
        }
        Ez.Label {
            objectName: "labelColor"
            text: root.choice ? root.choice.text : ""
            font: Ez.Metrics.font
            wrapMode: Text.Wrap
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Layout.alignment: Qt.AlignTop
        }
        Ez.ZoomButtons {
            view: view
            Layout.alignment: Qt.AlignTop
        }
    }
}
