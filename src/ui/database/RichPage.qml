// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * A page of the database whose controls fill the window (a canvas, a
 * command list): its content in a column as large as the page, scrolled
 * when the window is smaller than the controls need.
 * Its properties are those of an Ez.EntryPage (entry, database, rpg2k3);
 * it is greyed while there is no entry to show.
 */
Flickable {
    id: root

    /** The data of the entry shown (a JsonView), null for none */
    property var entry: null
    /** The root of the database edited (a JsonView) */
    property var database: null
    /** The game is made with RPG Maker 2003 (else 2000) */
    property bool rpg2k3: true
    default property alias content: column.data
    readonly property alias column: column

    enabled: !!entry && entry.valid
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    contentWidth: column.width + 2 * Ez.Metrics.pagePadding
    contentHeight: column.height + 2 * Ez.Metrics.pagePadding
    Controls.ScrollBar.vertical: Controls.ScrollBar {}
    Controls.ScrollBar.horizontal: Controls.ScrollBar {}

    ColumnLayout {
        id: column
        x: Ez.Metrics.pagePadding
        y: Ez.Metrics.pagePadding
        width: Math.max(implicitWidth, root.width - 2 * Ez.Metrics.pagePadding)
        height: Math.max(implicitHeight, root.height - 2 * Ez.Metrics.pagePadding)
        spacing: Ez.Metrics.sectionSpacing
    }
}
