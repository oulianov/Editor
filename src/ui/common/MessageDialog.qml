// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * A message over a window (a warning, a question): its text, then its
 * buttons (OK unless told otherwise). Drawn in the window, so scripts and
 * screenshots see it.
 *
 * Its width comes from the lines of the text (at most maximumTextWidth,
 * and what the window leaves) and its buttons, not from the label: the
 * label wraps at the width the dialog gives it, a long text makes it
 * taller, never wider.
 */
Ez.Dialog {
    id: root

    property alias text: label.text
    /** Where a long line wraps */
    property int maximumTextWidth: 360

    /** The room the window gives */
    readonly property real overlayWidth: Controls.Overlay.overlay ? Controls.Overlay.overlay.width : 400
    /** The widest line of the text, unwrapped */
    readonly property real widestLine: {
        let widest = 0
        for (const line of label.text.split("\n")) {
            widest = Math.max(widest, metrics.advanceWidth(line))
        }
        return Math.ceil(widest) + 1
    }

    anchors.centerIn: Controls.Overlay.overlay
    standardButtons: Controls.Dialog.Ok
    font: Ez.Metrics.font
    // the width from the text, the title and the buttons, measured, not
    // from the implicit widths of the label and the title: they follow the
    // width the dialog gives them (wrapped, elided), a loop
    width: Math.max(0, Math.min(overlayWidth - 2 * Ez.Metrics.dialogMargin,
                                Math.max(Math.min(maximumTextWidth, widestLine) + leftPadding + rightPadding,
                                         titleWidth, implicitFooterWidth)))

    /** The title as the style's header shows it */
    readonly property real titleWidth: {
        const label = header as Controls.Label
        return title === "" || !label ? 0 : Math.ceil(titleMetrics.advanceWidth) + label.leftPadding + label.rightPadding + 1
    }
    TextMetrics {
        id: titleMetrics
        font: (root.header as Controls.Label)?.font ?? root.font
        text: root.title
    }

    FontMetrics {
        id: metrics
        font: label.font
    }

    // laid out at the width given (the layout reads the height of the
    // wrapped text once it is laid out, not while the dialog's is computed)
    contentItem: ColumnLayout {
        Ez.Label {
            id: label
            objectName: "messageText"
            wrapMode: Text.Wrap
            font: Ez.Metrics.font
            Layout.fillWidth: true
        }
    }
}
