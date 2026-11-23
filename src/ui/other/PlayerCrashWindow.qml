// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "PlayerCrashDialog"

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * EasyRPG Player crashed in a test play (GameRunner, PlayerCrash): how, its
 * dump, and its report (the Player, the system, the end of its log), to
 * copy or to open as a GitHub issue. The buttons of the side leave it open.
 */
Ez.DialogWindow {
    id: root

    /** The crash (PlayerCrash) */
    required property var crash
    /** The report was put in the clipboard */
    property bool copied: false

    objectName: "playerCrashWindow"
    title: qsTr("EasyRPG Player Crashed")
    preferredWidth: 620
    preferredHeight: 460
    modality: Qt.NonModal
    standardButtons: Controls.DialogButtonBox.Close

    buttons: [
        Ez.Button {
            objectName: "pushRevealDump"
            text: Qt.platform.os === "osx" ? qsTr("Show in Finder")
                : Qt.platform.os === "windows" ? qsTr("Show in Explorer") : qsTr("Show in Folder")
            enabled: root.crash.dumpPath !== ""
            Controls.DialogButtonBox.buttonRole: Controls.DialogButtonBox.ActionRole
            onClicked: root.crash.reveal()
        },
        Ez.Button {
            objectName: "pushCopyReport"
            text: root.copied ? qsTr("Copied") : qsTr("Copy Report")
            Controls.DialogButtonBox.buttonRole: Controls.DialogButtonBox.ActionRole
            onClicked: {
                root.crash.copyReport()
                root.copied = true
            }
        },
        Ez.Button {
            objectName: "pushReportIssue"
            text: qsTr("Report on GitHub...")
            Controls.DialogButtonBox.buttonRole: Controls.DialogButtonBox.ActionRole
            onClicked: root.crash.openIssue()
        }
    ]

    RowLayout {
        spacing: Ez.Metrics.dialogMargin
        Layout.fillWidth: true

        Kirigami.Icon {
            source: "dialog-error"
            implicitWidth: Kirigami.Units.iconSizes.medium
            implicitHeight: Kirigami.Units.iconSizes.medium
            Layout.alignment: Qt.AlignTop
        }
        Ez.Label {
            objectName: "labelCrash"
            wrapMode: Text.Wrap
            // its lines wrap: the window does not widen to them
            Layout.preferredWidth: 540
            text: (root.crash.dumpName !== ""
                ? qsTr("EasyRPG Player crashed during the test play (%1). Crash dump: %2.")
                    .arg(root.crash.cause).arg(root.crash.dumpName)
                : qsTr("EasyRPG Player crashed during the test play (%1).").arg(root.crash.cause))
                + " " + qsTr("Report on GitHub opens an issue with this report; attach the dump to it.")
            Layout.fillWidth: true
        }
    }

    Controls.ScrollView {
        // its long lines scroll: they do not widen the window
        implicitWidth: 580
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredHeight: 280

        Ez.TextArea {
            objectName: "textReport"
            readOnly: true
            wrapMode: TextEdit.NoWrap
            selectByMouse: true
            font: Ez.Metrics.monospaceFont
            text: root.crash.report
        }
    }
}
