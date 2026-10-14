// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * A section of a form, the group box of the Editor: a title in a muted color
 * above a light rounded border, with little padding (the same on every
 * platform). Its content is laid out in a column.
 *
 * A checkable one (a checkable group box) has a check box as its title: its
 * content is enabled while checked, greyed otherwise. checked is set by the
 * page, or with jsonData and key it is a bool of the project's data (as
 * Ez.CheckBox). Automation finds the check box by its objectName (checkName,
 * or checkBox.objectName).
 */
ColumnLayout {
    id: root

    property string title
    default property alias content: body.data
    /** The column of the content (its spacing...) */
    readonly property alias body: body
    property bool checkable: false
    property bool checked: false
    property alias jsonData: check.jsonData
    property alias key: check.key
    property alias checkName: check.objectName
    readonly property alias checkBox: check
    /** checked changed by the user (not by the data) */
    signal toggled()

    /** An item of the content takes the height left (a layout: one of its items does) */
    function fills(item) {
        if (!item.visible || !item.Layout.fillHeight) {
            return false
        }
        if (item instanceof GridLayout || item instanceof RowLayout || item instanceof ColumnLayout
                || item instanceof StackLayout) {
            return Array.from(item.children).some(child => fills(child))
        }
        return true
    }

    spacing: Ez.Metrics.sectionTitleGap
    Layout.fillWidth: true
    // as high as its content, as a group box: Layout.fillHeight makes it take the room left
    Layout.fillHeight: false

    Ez.Label {
        text: root.title
        visible: text !== "" && !root.checkable
        color: Ez.Metrics.mutedTextColor
        font: Ez.Metrics.sectionTitleFont
        opacity: enabled ? 1 : Ez.Metrics.disabledTitleOpacity
    }
    Ez.CheckBox {
        id: check
        text: root.title
        visible: root.checkable
        padding: 0
        font: Ez.Metrics.sectionTitleFont
        onToggled: {
            root.checked = checked
            root.toggled()
        }
        // without data: the box shows the section's state; with data: the
        // section follows the box
        Binding on checked {
            value: root.checked
            when: !check.jsonData
        }
        onCheckedChanged: {
            if (check.jsonData) {
                root.checked = checked
            }
        }
    }
    Rectangle {
        Layout.fillWidth: true
        Layout.fillHeight: true
        implicitWidth: body.implicitWidth + 2 * Ez.Metrics.sectionPadding
        implicitHeight: body.implicitHeight + 2 * Ez.Metrics.sectionPadding
        color: "transparent"
        radius: Ez.Metrics.sectionRadius
        border.color: Ez.Metrics.sectionBorderColor

        ColumnLayout {
            id: body

            // a section higher than its content (Layout.fillHeight): the
            // content stays at the top as in a group box, unless an item of
            // it fills the height (a list)
            readonly property bool filled: Array.from(children).some(child => root.fills(child))

            x: Ez.Metrics.sectionPadding
            y: Ez.Metrics.sectionPadding
            width: parent.width - 2 * Ez.Metrics.sectionPadding
            height: filled ? parent.height - 2 * Ez.Metrics.sectionPadding : implicitHeight
            spacing: Ez.Metrics.rowSpacing
            enabled: !root.checkable || root.checked
        }
    }
}
