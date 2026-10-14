// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * The page of a database entry (beside its Ez.EntryListPane, or alone for
 * a page of the database such as the Vocabulary): its controls in a
 * column, bound to the entry (jsonData: page.entry, key: "name"...),
 * scrolled when the window is smaller than them. It is greyed while there is
 * no entry to show.
 *
 *   Ez.EntryPage {
 *       id: page
 *       Ez.TextField { jsonData: page.entry; key: "name" }
 *   }
 *
 * The column is as wide as RPG Maker's pages (Ez.Metrics.pageColumnWidth),
 * the rest of a wider window left empty; fillWidth gives it all the width
 * (the pages with lists and pictures).
 */
Flickable {
    id: root

    /** The data of the entry shown (a JsonView), null for none */
    property var entry: null
    /** The root of the database edited (a JsonView: the other lists, the system...) */
    property var database: null
    /** The game is made with RPG Maker 2003 (else 2000) */
    property bool rpg2k3: true
    /** The page takes the whole width */
    property bool fillWidth: false
    /** The page shows the database itself, not an entry of a list */
    property bool single: false
    default property alias content: column.data
    /** The column of the controls (its spacing...) */
    readonly property alias column: column

    enabled: single || (!!entry && entry.valid)
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    contentWidth: column.width + 2 * Ez.Metrics.pagePadding
    contentHeight: column.implicitHeight + 2 * Ez.Metrics.pagePadding
    // the scroll bars over the page, beside the controls
    Controls.ScrollBar.vertical: Controls.ScrollBar {
        id: verticalBar
    }
    Controls.ScrollBar.horizontal: Controls.ScrollBar {}

    ColumnLayout {
        id: column

        // the room the window gives (less the scroll bar when the page is
        // higher), never less than the controls need: a narrower window
        // scrolls the page
        readonly property real room: root.width - 2 * Ez.Metrics.pagePadding
                                     - (root.contentHeight > root.height ? verticalBar.width : 0)

        x: Ez.Metrics.pagePadding
        y: Ez.Metrics.pagePadding
        width: Math.max(implicitWidth, root.fillWidth ? room : Math.min(room, Ez.Metrics.pageColumnWidth))
        spacing: Ez.Metrics.sectionSpacing
    }
}
