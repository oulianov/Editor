// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * The lines a task reports, read-only, scrolled to the last one. Its long
 * lines scroll: they do not widen the window (it asks for fieldWidth).
 * Automation finds the text by textArea.objectName.
 */
Controls.ScrollView {
    id: root

    property list<string> lines
    property alias placeholderText: text.placeholderText
    readonly property alias textArea: text

    implicitWidth: Ez.Metrics.fieldWidth
    implicitHeight: 140

    Ez.TextArea {
        id: text
        readOnly: true
        wrapMode: TextEdit.NoWrap
        text: root.lines.join("\n")
        // the last line in view
        onTextChanged: cursorPosition = length
    }
}
