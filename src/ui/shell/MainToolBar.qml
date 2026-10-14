// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "MainWindow"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * A toolbar of the main window as the catalog lists it: the window's
 * (mainToolBar) or the palette's (toolBar). Buttons show the icons of
 * their actions, their texts as tips and their hints in the status bar,
 * as RPG Maker's small buttons. Those that do not fit go to the menu of
 * the "»" button.
 */
Controls.ToolBar {
    id: root

    /** The actions of the window (Actions.qml) */
    required property Actions actions
    /** The toolbar of the catalog: "mainToolBar" or "toolBar" */
    required property string toolbarName
    /** The size of the icons (RPG Maker's 16 pixels, a quarter larger) */
    property int iconSize: Ez.Metrics.toolIconSize
    readonly property var items: {
        const list = actions.shell.actions.toolBar(toolbarName)
        // no separator at the end
        while (list.length > 0 && list[list.length - 1] === "") {
            list.pop()
        }
        return list
    }

    /** The width showing all its buttons and separators (known before the
     *  actions are: the width of a dock made with the window) */
    function fullWidth() {
        let width = leftPadding + rightPadding
        for (const name of items) {
            width += name === "" ? measureSeparator.implicitWidth : iconSize + 2 * Ez.Metrics.toolButtonPadding
        }
        return width
    }
    // the actions whose buttons do not fit, in their order
    function hiddenActions() {
        const hidden = []
        const limit = content.width - (more.visible ? more.width : 0)
        for (let i = 0; i < buttons.count; i++) {
            const item = buttons.itemAt(i)
            // the action of the slot (none: a separator)
            const action = root.items[i] === "" ? null : root.actions.action(root.items[i])
            if (item && item.width > 0 && item.x + item.width > limit) {
                if (action) {
                    hidden.push(action)
                } else if (hidden.length > 0 && hidden[hidden.length - 1] !== null) {
                    hidden.push(null)
                }
            }
        }
        while (hidden.length > 0 && hidden[hidden.length - 1] === null) {
            hidden.pop()
        }
        return hidden
    }

    objectName: toolbarName
    padding: 1
    leftPadding: 2
    rightPadding: 2

    contentItem: Item {
        id: content

        implicitWidth: row.implicitWidth
        implicitHeight: row.implicitHeight
        clip: true

        Row {
            id: row

            Repeater {
                id: buttons
                model: root.items

                delegate: Item {
                    id: slot

                    required property string modelData
                    readonly property var action: modelData === "" ? null : root.actions.action(modelData)

                    visible: action === null || action.shown
                    width: !visible ? 0 : action ? button.implicitWidth : separator.implicitWidth
                    height: Math.max(button.implicitHeight, separator.implicitHeight)

                    Controls.ToolSeparator {
                        id: separator
                        visible: slot.action === null
                        anchors.verticalCenter: parent.verticalCenter
                        topPadding: 2
                        bottomPadding: 2
                    }
                    Controls.ToolButton {
                        id: button
                        visible: slot.action !== null
                        action: slot.action
                        display: Controls.AbstractButton.IconOnly
                        icon.width: root.iconSize
                        icon.height: root.iconSize
                        padding: Ez.Metrics.toolButtonPadding
                        focusPolicy: Qt.NoFocus
                        Controls.ToolTip.text: slot.action ? (slot.action.toolTip !== "" ? slot.action.toolTip : slot.action.text.replace("&", "")) : ""
                        Controls.ToolTip.visible: hovered && Controls.ToolTip.text !== ""
                        Controls.ToolTip.delay: 600
                        onHoveredChanged: root.actions.hint = hovered && slot.action ? slot.action.statusTip : ""
                    }
                }
            }
        }

        Controls.ToolButton {
            id: more
            objectName: root.toolbarName + "More"
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            visible: row.implicitWidth > content.width
            text: "»"
            padding: Ez.Metrics.toolButtonPadding
            focusPolicy: Qt.NoFocus
            background: Rectangle {
                color: root.palette.window
            }
            onClicked: {
                // removeItem() destroys them (the items of addAction() are the
                // menu's: destroy() refuses them, the menu would not open)
                while (overflow.count > 0) {
                    overflow.removeItem(overflow.itemAt(0))
                }
                for (const action of root.hiddenActions()) {
                    if (action) {
                        overflow.addAction(action)
                    } else {
                        overflow.addItem(separatorComponent.createObject(overflow.contentItem))
                    }
                }
                overflow.popup(more, 0, more.height)
            }
        }
        Controls.Menu {
            id: overflow
        }
        // as wide as the separators of the toolbar (fullWidth())
        Controls.ToolSeparator {
            id: measureSeparator
            visible: false
            topPadding: 2
            bottomPadding: 2
        }
        Component {
            id: separatorComponent
            Controls.MenuSeparator {}
        }
    }
}
