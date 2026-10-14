// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "QObject"

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * The guide of the Editor (QmlHost::help), scrolled to a topic: an anchor
 * of it. Its links to other topics scroll it, the others open in the browser.
 */
Controls.ApplicationWindow {
    id: root

    /** The anchor shown at the top */
    property string topic

    objectName: "helpWindow"
    title: qsTr("EasyRPG Editor Help")
    flags: Qt.Dialog | Qt.WindowTitleHint | Qt.WindowCloseButtonHint | Qt.WindowMinMaxButtonsHint
    width: 640
    minimumWidth: 320
    minimumHeight: 240
    height: 720

    /** The anchor to scroll to once the text is laid out that far ("": none) */
    property string pendingAnchor

    /** Scrolls the guide to an anchor (nothing when it has none) */
    function scrollTo(anchor) {
        pendingAnchor = anchor
        scrollToPending()
    }
    function scrollToPending() {
        const flickable = scroll.contentItem as Flickable
        if (pendingAnchor === "" || !flickable || browser.length === 0) {
            return
        }
        const position = Ez.Editor.anchorPosition(browser.textDocument, pendingAnchor)
        if (position < 0) {
            pendingAnchor = ""
            return
        }
        const y = browser.positionToRectangle(position).y
        const end = flickable.contentHeight - flickable.height
        flickable.contentY = Math.max(0, Math.min(y, end))
        // the rich text is laid out again at the width of the window once
        // shown, bit by bit: again then, until the anchor can be at the top
        if (flickable.height > 0 && browser.width >= flickable.width && y <= end) {
            pendingAnchor = ""
        }
    }

    onTopicChanged: scrollTo(topic)
    Component.onCompleted: scrollTo(topic)

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Controls.ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            onHeightChanged: Qt.callLater(root.scrollToPending)

            Controls.TextArea {
                id: browser
                objectName: "helpBrowser"
                readOnly: true
                selectByMouse: true
                textFormat: TextEdit.RichText
                wrapMode: TextEdit.Wrap
                text: Ez.Editor.helpText()
                focus: true
                onContentHeightChanged: Qt.callLater(root.scrollToPending)
                onWidthChanged: Qt.callLater(root.scrollToPending)
                onLinkActivated: link => {
                    if (link.startsWith("#")) {
                        root.scrollTo(link.substring(1))
                    } else {
                        Qt.openUrlExternally(link)
                    }
                }

                HoverHandler {
                    cursorShape: browser.hoveredLink !== "" ? Qt.PointingHandCursor : Qt.IBeamCursor
                }
            }
        }
        Ez.ButtonBox {
            Layout.fillWidth: true
            Layout.margins: Ez.Metrics.dialogMargin
            standardButtons: Controls.DialogButtonBox.Close
            onRejected: root.close()
        }
    }

    // the user scrolls: the topic is left alone
    Connections {
        target: scroll.contentItem
        function onMovementStarted() {
            root.pendingAnchor = ""
        }
    }

    Shortcut {
        sequences: [StandardKey.Cancel]
        onActivated: root.close()
    }
}
