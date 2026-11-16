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
 * Ez.CheckBox). Automation finds the check box by its objectName, set with
 * checkName: only a checkable one makes its check box, after the section is
 * made, so checkBox is null while it is (checkBox.objectName cannot be set).
 *
 * A section of what comes from an add-on (origin: the Maniac Patch...) has
 * its tag after the title (Ez.OriginTag).
 */
ColumnLayout {
    id: root

    property string title
    default property alias content: body.data
    /** The column of the content (its spacing...) */
    readonly property alias body: body
    property bool checkable: false
    property bool checked: false
    property var jsonData: null
    property string key: ""
    property string checkName: ""
    /**
     * Where its content comes from when it is not the game's engine
     * (Ez.Origins: "maniac"...): its tag after the title, and over the
     * content what the game needs while it does not run it. The page greys
     * it (enabled).
     */
    property string origin: ""
    /** What is set in it is not run by the game: the tag says so (Ez.OriginTag.warning) */
    property bool originWarning: false
    /** The check box of a checkable one (made only for it), else null */
    readonly property Ez.CheckBox checkBox: check.item as Ez.CheckBox
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

    // the title (or its check box), then the tag of its origin
    RowLayout {
        spacing: Ez.Metrics.columnSpacing
        visible: root.title !== "" || root.checkable

        Ez.Label {
            text: root.title
            visible: text !== "" && !root.checkable
            color: Ez.Metrics.mutedTextColor
            font: Ez.Metrics.sectionTitleFont
            opacity: enabled ? 1 : Ez.Metrics.disabledTitleOpacity
        }
        Loader {
            id: check
            active: root.checkable
            visible: root.checkable
            sourceComponent: Ez.CheckBox {
                id: box
                objectName: root.checkName
                text: root.title
                padding: 0
                font: Ez.Metrics.sectionTitleFont
                jsonData: root.jsonData
                key: root.key
                onToggled: {
                    root.checked = checked
                    root.toggled()
                }
                // without data: the box shows the section's state; with data: the
                // section follows the box
                Binding on checked {
                    value: root.checked
                    when: !box.jsonData
                }
                onCheckedChanged: {
                    if (box.jsonData) {
                        root.checked = checked
                    }
                }
            }
        }
        // made only for a section of an origin
        Loader {
            active: root.origin !== ""
            visible: active
            sourceComponent: Ez.OriginTag {
                origin: root.origin
                warning: root.originWarning
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
        // what the game needs, over the content greyed
        Loader {
            anchors.fill: parent
            active: root.origin !== ""
            sourceComponent: Ez.OriginTip {
                origin: root.origin
            }
        }
    }
}
