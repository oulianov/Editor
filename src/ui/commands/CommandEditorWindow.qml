// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * The window of an editor of an event command (CommandEditorRegistry): the
 * editor of the type editorType (a hand-written one, SchemaForm or
 * RawCommandEditor) on the session, then OK, Cancel and Help.
 *
 * An editor is an item with a required property session; it may have a
 * title (the window's), a preferredWidth, and a function accept() saying
 * whether OK can close the window (the check of the session follows).
 */
Ez.DialogWindow {
    id: root

    required property Ez.CommandEditSession session
    required property string editorType
    /** The editor shown (an Item, with the properties above it may have) */
    property var editor: null

    objectName: "CommandEditor"
    title: editor && editor.title ? editor.title : ""
    preferredWidth: editor && editor.preferredWidth ? editor.preferredWidth : 0
    standardButtons: Controls.DialogButtonBox.Ok | Controls.DialogButtonBox.Cancel | Controls.DialogButtonBox.Help

    onAccepting: (check) => {
        if (editor && typeof editor.accept === "function" && !editor.accept()) {
            check.ok = false
            return
        }
        check.ok = session.accept()
    }

    Component.onCompleted: {
        const component = Qt.createComponent("org.easyrpg.editor", editorType)
        if (component.status !== Component.Ready) {
            console.warn("The command editor", editorType, "could not be made:", component.errorString())
            return
        }
        editor = component.createObject(root.body, { session: root.session })
        if (!editor) {
            console.warn("The command editor", editorType, "could not be made")
        }
    }

    Connections {
        target: root.buttonBox
        function onHelpRequested() {
            root.session.help(root.contentItem)
        }
    }
}
