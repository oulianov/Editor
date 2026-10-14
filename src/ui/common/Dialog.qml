// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * A dialog drawn over a window (Array Size, a warning...), modal: its
 * buttons (standardButtons) are those of the windows, Ez.ButtonBox, in line
 * with its content.
 */
Controls.Dialog {
    id: root

    modal: true
    font: Ez.Metrics.font

    footer: Ez.ButtonBox {
        leftPadding: root.leftPadding
        rightPadding: root.rightPadding
        bottomPadding: root.bottomPadding
    }
}
