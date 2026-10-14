// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * A push button as high as the fields beside it. Its text is centred: an
 * icon named but missing from the icon themes takes no room beside it
 * (Breeze keeps the room of any icon name it is given). An accent button
 * stands out: the one action its place suggests (the toolbar's Update).
 */
Controls.Button {
    id: root

    /** Drawn in the accent color of the theme (the colors of a selection:
     *  the highlight, its text readable on it), light and dark */
    property bool accent: false

    // Breeze draws a button with the colors of the button set of the theme
    Kirigami.Theme.colorSet: accent ? Kirigami.Theme.Selection : Kirigami.Theme.Button
    Kirigami.Theme.inherit: false

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
