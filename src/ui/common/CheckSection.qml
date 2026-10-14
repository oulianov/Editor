// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * A section that can be turned off (the checkable group box): its title is
 * a check box (bound as an Ez.CheckBox to jsonData and key), and its
 * content is greyed while it is unchecked. The check box has the name
 * (checkName) scripts set it by.
 */
ColumnLayout {
    id: root

    property alias title: check.text
    property alias checked: check.checked
    property alias jsonData: check.jsonData
    property alias key: check.key
    property alias checkName: check.objectName
    default property alias content: frame.content
    /** The column of the content (its spacing...) */
    readonly property alias body: frame.body
    readonly property alias checkBox: check

    /** Checked or unchecked by the user */
    signal toggled()

    spacing: Ez.Metrics.sectionTitleGap
    Layout.fillWidth: true
    // as high as its content, as a group box: Layout.fillHeight makes it take the room left
    Layout.fillHeight: false

    Ez.CheckBox {
        id: check
        // the title of a section, with its box
        font: Ez.Metrics.sectionTitleFont
        verticalPadding: 0
        onToggled: root.toggled()
    }
    Ez.Section {
        id: frame
        enabled: check.checked
        Layout.fillHeight: true
    }
}
