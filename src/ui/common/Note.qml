// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * A text explaining a part of a window, on as many lines as its width
 * allows: it never makes the window wider
 */
Ez.Label {
    wrapMode: Text.Wrap
    Layout.fillWidth: true
    // the width comes from the other controls
    Layout.preferredWidth: 1
}
