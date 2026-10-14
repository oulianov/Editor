// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "CommentWidget"

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/** Comment: its text, a line of the comment per Comment_2 command after the first one */
ColumnLayout {
    id: root

    required property Ez.CommandEditSession session
    readonly property string title: qsTr("Comment")

    spacing: Ez.Metrics.rowSpacing
    Layout.fillWidth: true
    Layout.fillHeight: true

    Component.onCompleted: {
        // typing at once, at the end
        comment.cursorPosition = comment.length
        comment.forceActiveFocus()
    }

    Controls.ScrollView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumWidth: 420
        Layout.minimumHeight: 160

        Ez.TextArea {
            id: comment
            objectName: "comment"
            jsonData: root.session
            key: "/lines"
        }
    }
}
