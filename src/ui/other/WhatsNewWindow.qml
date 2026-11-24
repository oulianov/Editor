// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "WhatsNewDialog"

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * What is new in the version the Editor just updated itself to: the text of
 * its release (Markdown), shown once at its first start (QmlShell::showWhatsNew).
 */
Ez.DialogWindow {
    id: root

    /** "0.2.27" */
    property string version
    /** The text of the release, Markdown */
    property string notes
    /** Its page on GitHub */
    property url page
    /**
     * The notes, their HTML pictures (<img src>, as GitHub writes the
     * pictures dropped in a release) as Markdown ones: Qt places an HTML
     * picture of Markdown elsewhere (before its heading, centered)
     */
    readonly property string shownNotes: notes.replace(/<img\b[^>]*>/gi, (tag) => {
        const src = /\bsrc\s*=\s*["']([^"']+)["']/i.exec(tag)
        const alt = /\balt\s*=\s*["']([^"']*)["']/i.exec(tag)
        return src ? "![" + (alt ? alt[1] : "") + "](" + src[1] + ")" : ""
    })

    objectName: "whatsNewWindow"
    title: qsTr("What's New in EasyRPG Editor %1").arg(version)
    preferredWidth: 640
    preferredHeight: 520
    stateKey: "WhatsNewWindow"
    modality: Qt.NonModal
    standardButtons: Controls.DialogButtonBox.Close

    buttons: [
        Ez.Button {
            objectName: "pushReleasePage"
            text: qsTr("Release Page")
            visible: root.page.toString() !== ""
            Controls.DialogButtonBox.buttonRole: Controls.DialogButtonBox.ActionRole
            onClicked: Qt.openUrlExternally(root.page)
        }
    ]

    Controls.ScrollView {
        // its lines wrap to the window: they do not widen it
        implicitWidth: 600
        implicitHeight: 440
        Layout.fillWidth: true
        Layout.fillHeight: true

        Ez.TextArea {
            id: notesText
            objectName: "textNotes"
            readOnly: true
            textFormat: TextEdit.MarkdownText
            text: root.notes !== "" ? root.shownNotes : qsTr("EasyRPG Editor is now version %1.").arg(root.version)
            onLinkActivated: (link) => Qt.openUrlExternally(link)
            // the pictures (screenshots) at most as wide as the text: they
            // load after it, which makes it taller
            function fitImages() {
                Ez.Editor.fitImages(textDocument, width - leftPadding - rightPadding)
            }
            onContentHeightChanged: Qt.callLater(fitImages)
            onWidthChanged: Qt.callLater(fitImages)
            HoverHandler {
                cursorShape: parent.hoveredLink !== "" ? Qt.PointingHandCursor : Qt.IBeamCursor
            }
        }
    }
}
