// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * A question or a message of the Editor in a window of its own (the QML
 * DialogService): its text, its buttons (standard ones and named ones),
 * and in `answer` the one chosen: "ok", "yes", "no", "cancel", or the
 * answer of a named button ("" when the window was closed).
 */
Ez.DialogWindow {
    id: root

    property alias text: label.text
    /**
     * Buttons besides the standard ones: {answer, text, role (a
     * DialogButtonBox role)}, and optionally enabled (false: greyed),
     * toolTip (shown over it, greyed too) and origin (its tag, Ez.Origins:
     * what it needs)
     */
    property var extraButtons: []
    property string answer: ""
    /** question, information, warning or error: its icon beside the text ("": none) */
    property string kind: ""

    objectName: "messageWindow"
    preferredWidth: 360
    standardButtons: Controls.DialogButtonBox.Ok

    RowLayout {
        spacing: Ez.Metrics.dialogMargin
        Layout.fillWidth: true

        // the icons of the themes (ezbright, ezdark), as the message boxes show them
        Kirigami.Icon {
            objectName: "messageIcon"
            visible: root.kind !== ""
            source: root.kind !== "" ? "dialog-" + root.kind : ""
            implicitWidth: Kirigami.Units.iconSizes.medium
            implicitHeight: Kirigami.Units.iconSizes.medium
            Layout.alignment: Qt.AlignTop
        }
        Ez.Label {
            id: label
            objectName: "messageText"
            wrapMode: Text.Wrap
            Layout.fillWidth: true
            Layout.preferredWidth: 340
        }
    }

    Instantiator {
        model: root.extraButtons
        delegate: Ez.Button {
            id: extra
            required property var modelData
            readonly property string origin: modelData.origin ?? ""
            text: modelData.text
            objectName: "answer_" + modelData.answer
            enabled: modelData.enabled ?? true
            Controls.DialogButtonBox.buttonRole: modelData.role
            // a button of an origin: its text, then its tag (the box makes
            // it as wide as they are)
            Component.onCompleted: {
                if (origin !== "") {
                    contentItem = taggedContent.createObject(extra)
                }
            }
            Component {
                id: taggedContent
                Item {
                    implicitWidth: tagged.implicitWidth
                    implicitHeight: tagged.implicitHeight
                    Row {
                        id: tagged
                        anchors.centerIn: parent
                        spacing: Ez.Metrics.columnSpacing
                        Ez.Label {
                            text: extra.text
                            font: extra.font
                            // faded with the button
                            opacity: 1
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Ez.OriginTag {
                            origin: extra.origin
                            // the button's tooltip says it, when it has one
                            toolTip: (extra.modelData.toolTip ?? "") !== "" ? "" : Ez.Origins.tip(extra.origin)
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }
            }
            Ez.OriginTip {
                toolTip: extra.modelData.toolTip ?? ""
            }
            onClicked: {
                root.answer = modelData.answer
                // roles the box does not close on (Destructive...)
                if (modelData.role !== Controls.DialogButtonBox.AcceptRole
                        && modelData.role !== Controls.DialogButtonBox.YesRole
                        && modelData.role !== Controls.DialogButtonBox.RejectRole
                        && modelData.role !== Controls.DialogButtonBox.NoRole) {
                    root.accept()
                }
            }
        }
        onObjectAdded: (index, object) => root.buttonBox.addItem(object)
        onObjectRemoved: (index, object) => root.buttonBox.removeItem(object)
    }

    Connections {
        target: root.buttonBox
        function onClicked(button) {
            if (root.answer !== "") {
                return
            }
            // standardButton(kind) is the button of a kind: which one was it
            const answers = [[Controls.DialogButtonBox.Ok, "ok"], [Controls.DialogButtonBox.Yes, "yes"],
                             [Controls.DialogButtonBox.No, "no"], [Controls.DialogButtonBox.Cancel, "cancel"]]
            for (const [kind, answer] of answers) {
                if (button === root.buttonBox.standardButton(kind)) {
                    root.answer = answer
                    return
                }
            }
        }
    }
}
