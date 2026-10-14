// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * A push button as high as the fields beside it. Its text is centred: an
 * icon named but missing from the icon themes takes no room beside it
 * (Breeze keeps the room of any icon name it is given).
 */
Controls.Button {
    id: root

    font: Ez.Metrics.font
    verticalPadding: Ez.Metrics.fieldPadding
    display: icon.name !== "" && icon.source.toString() === "" && !Ez.Editor.hasIcon(icon.name)
        ? Controls.AbstractButton.TextOnly : Controls.AbstractButton.TextBesideIcon

    // disabled: greyed (Ez.Metrics.disabledOpacity), on the face of an
    // enabled button (Breeze darkens it and drops its shadow: it looked pressed)
    opacity: enabled ? 1 : Ez.Metrics.disabledOpacity
    Binding {
        target: root.background
        property: "color"
        value: Ez.Metrics.palette.button
        when: !root.enabled && !root.flat && root.background !== null
    }
}
