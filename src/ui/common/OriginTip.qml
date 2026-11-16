// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * The tooltip of a control the game does not run (Ez.Origins): what the game
 * needs and where to turn it on. Put inside the control, it fills it: a
 * greyed (disabled) control shows no tooltip of its own, its hover handlers
 * still see the pointer. Says nothing while the game runs the origin (the
 * control's own tooltip then).
 *
 *     Ez.CheckBox {
 *         text: qsTr("Mouse wheel")
 *         enabled: Ez.Origins.available("maniac")
 *         Ez.OriginTip { origin: "maniac" }
 *     }
 *
 * Several origins (an option of two): the first the game does not run.
 */
Item {
    id: root

    /** An origin of Ez.Origins, or several */
    property var origin: ""
    /** The tooltip; by default what the first origin not run needs */
    property string toolTip: {
        const origins = Array.isArray(origin) ? origin : [origin]
        for (const o of origins)
            if (!Ez.Origins.available(o))
                return Ez.Origins.need(o)
        return ""
    }

    anchors.fill: parent

    HoverHandler {
        id: hover
    }

    Controls.ToolTip.visible: hover.hovered && root.toolTip !== ""
    Controls.ToolTip.text: root.toolTip
    Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
}
