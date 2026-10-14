// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * Breeze's tool tip without its binding loop: its width came from the
 * implicit width of its wrapped text, which changes with the width it is
 * given ("Binding loop detected for property contentWidth" each time one was
 * shown). As wide as its longest line once wrapped at Ez.Metrics.toolTipWidth,
 * measured apart from the text shown.
 *
 * QmlEngine makes it the tool tip the windows share (Controls.ToolTip.text
 * and Controls.ToolTip.visible on an item): it is used everywhere.
 */
Controls.ToolTip {
    id: root

    contentWidth: Math.ceil(widthSource.contentWidth) + 1

    contentItem: Controls.Label {
        text: root.text
        font: root.font
        wrapMode: Text.WordWrap
    }

    // the text wrapped at the widest width of a tool tip: not shown
    readonly property Text widthSource: Text {
        visible: false
        width: Ez.Metrics.toolTipWidth
        text: root.text
        font: root.font
        wrapMode: Text.WordWrap
    }
}
