// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/** Rows of a label and its field (Ez.FormLabel, then the field filling the rest) */
GridLayout {
    columns: 2
    rowSpacing: Ez.Metrics.rowSpacing
    columnSpacing: Ez.Metrics.columnSpacing
    Layout.fillWidth: true
}
