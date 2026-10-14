// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * A button at the bottom of a window (OK, Cancel, Close...): larger than the
 * buttons of the forms (Ez.Metrics.dialogButtonHeight, at least
 * dialogButtonMinimumWidth wide), its text only. Ez.ButtonBox makes its
 * buttons with it; a window with a row of its own at the bottom (the
 * export, the search) uses it for the buttons of the row.
 */
Ez.Button {
    display: Controls.AbstractButton.TextOnly
    implicitWidth: Math.max(Ez.Metrics.dialogButtonMinimumWidth,
                            Math.ceil(implicitContentWidth) + 2 * Ez.Metrics.dialogButtonPadding)
    implicitHeight: Math.max(Ez.Metrics.dialogButtonHeight,
                             Math.ceil(implicitContentHeight) + topPadding + bottomPadding)
}
