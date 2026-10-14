// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * Shared code for the entry edit pages such as editing single Actors or Skills
 *
 * The page of a database entry (beside its DatabaseEntryListPage, or alone
 * for a page of the database such as the Vocabulary): its controls in a
 * column, bound to the entry (jsonData: root.jsonData, key: "name"...; the
 * pages call it entry), scrolled when the window is smaller than them. It is
 * greyed while there is no entry to show.
 *
 *   DatabaseEntryPage {
 *       id: root
 *       Ez.TextField { jsonData: root.entry; key: "name" }
 *   }
 *
 * The column is as wide as RPG Maker's pages (Ez.Metrics.pageColumnWidth),
 * the rest of a wider window left empty; fillWidth gives it all the width
 * (the pages with lists and pictures), fillHeight all the width and height
 * (the pages of a canvas or a command list).
 */
Flickable {
    id: root

    /** Database data rooted at the current item (a JsonView), null for none */
    property Ez.JsonView jsonData: null
    /** The same (the entry shown), as the pages read it */
    readonly property Ez.JsonView entry: jsonData
    /** When the object comes from a list, contains the index. Otherwise -1. */
    property int objIndex: -1
    /** The name of the entry shown (of an entry of a list) */
    property string title

    /** The root of the database edited (a JsonView: the other lists, the system...) */
    property var database: null
    /** The game is made with RPG Maker 2003 (else 2000) */
    property bool rpg2k3: true
    /** The page takes the whole width */
    property bool fillWidth: false
    /** The page takes the whole width and height (its column as large as the page) */
    property bool fillHeight: false
    /** The page shows the database itself, not an entry of a list */
    property bool single: false
    default property alias content: column.data
    /** The column of the controls (its spacing...) */
    readonly property alias column: column

    // Update the Page title when the name changes
    Connections {
        target: root.jsonData
        function onValueChanged(jsonPtr) {
            if (jsonPtr == "/name") {
                root.updateTitle();
            }
        }
    }

    onJsonDataChanged: updateTitle()
    onObjIndexChanged: updateTitle()
    function updateTitle() {
        if (objIndex >= 0 && jsonData) {
            root.title = jsonData.str("name");
        }
    }

    enabled: single || (!!jsonData && jsonData.valid)
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    contentWidth: column.width + 2 * Ez.Metrics.pagePadding
    contentHeight: column.height + 2 * Ez.Metrics.pagePadding
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
        width: root.fillHeight ? Math.max(implicitWidth, root.width - 2 * Ez.Metrics.pagePadding)
                               : Math.max(implicitWidth, root.fillWidth ? room : Math.min(room, Ez.Metrics.pageColumnWidth))
        height: root.fillHeight ? Math.max(implicitHeight, root.height - 2 * Ez.Metrics.pagePadding) : implicitHeight
        spacing: Ez.Metrics.sectionSpacing
    }
}
