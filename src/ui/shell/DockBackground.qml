// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
// the dock widgets of KDDockWidgets, in its resources
// qmllint disable import unused-imports
import "qrc:/kddockwidgets/qtquick/views/qml/" as KDDW
// qmllint enable import unused-imports

/**
 * What is behind the contents of a dock of the main window (the palette,
 * the map tree): KDDockWidgets' own, in the color of the window, light or
 * dark (KDDockWidgets leaves it white: it showed around the search above
 * the map tree).
 */
// qmllint disable import unresolved-type
KDDW.DockWidget {
    color: colors.window

    SystemPalette {
        id: colors
    }
}
