// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "ShowMessageWidget"

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * Show Message: the text of the message, a line of the message per
 * ShowMessage_2 command after the first one, in a monospaced font with the
 * width of a message with and without a face marked. Add Code types the
 * codes of messages (MessageCodePopup).
 */
ColumnLayout {
    id: root

    required property Ez.CommandEditSession session
    readonly property string title: qsTr("Show message")

    spacing: Ez.Metrics.rowSpacing
    Layout.fillWidth: true
    Layout.fillHeight: true

    Component.onCompleted: {
        // typing at once, at the end
        message.textArea.cursorPosition = message.textArea.length
        message.textArea.forceActiveFocus()
    }

    MessageTextArea {
        id: message
        fieldName: "message"
        jsonData: root.session
        key: "/lines"
        Layout.fillWidth: true
        Layout.fillHeight: true
    }

    RowLayout {
        spacing: Ez.Metrics.columnSpacing
        // the codes of messages, explained, typed at the cursor
        Ez.Button {
            id: addCode
            objectName: "addCode"
            text: qsTr("Add Code")
            hoverEnabled: true
            Controls.ToolTip.visible: hovered
            Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
            Controls.ToolTip.text: qsTr("Hero names, variables, colors, pauses, symbols...")
            onClicked: codes.showCodes(addCode)
        }
        Item {
            Layout.fillWidth: true
        }
    }

    MessageCodePopup {
        id: codes
        session: root.session
        message: message
        Layout.preferredWidth: 0
        Layout.preferredHeight: 0
    }
}
