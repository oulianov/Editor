// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import org.easyrpg.editor as Ez
// the frame of the docks of KDDockWidgets, in its resources
// qmllint disable import unused-imports
import "qrc:/kddockwidgets/qtquick/views/qml/" as KDDW
// qmllint enable import unused-imports

/**
 * The frame of a dock of the main window (the palette, the map tree, the
 * maps): KDDockWidgets' own, its line in the color of the lines of the
 * sections, light or dark (KDDockWidgets draws it in a fixed light grey).
 */
// qmllint disable import unresolved-type
KDDW.Group {
    border.color: Ez.Metrics.sectionBorderColor
}
