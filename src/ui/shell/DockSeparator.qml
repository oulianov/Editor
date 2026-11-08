// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
// the separator of the docks of KDDockWidgets, in its resources
// qmllint disable import unused-imports
import "qrc:/kddockwidgets/qtquick/views/qml/" as KDDW
// qmllint enable import unused-imports

/**
 * The space between two docks of the main window, dragged to resize them:
 * KDDockWidgets' own, in the color of the window, light or dark
 * (KDDockWidgets paints it in a fixed light grey).
 */
// qmllint disable import unresolved-type
KDDW.Separator {
    color: colors.window

    SystemPalette {
        id: colors
    }
}
