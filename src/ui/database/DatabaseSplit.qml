// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "DatabaseDialog"

import QtQuick
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * A page of the database with a list: its entries beside the page of the
 * entry shown, the page's properties (entry, database, rpg2k3) given by
 * this view. Without a page a note stands beside the list.
 */
Controls.SplitView {
    id: root

    /** The database edited (a JsonView) */
    property var database: null
    property bool rpg2k3: true
    /** The pointer of the list in the database ("attributes") */
    property string listKey
    /** The page of an entry (an Ez.EntryPage); null: not in QML yet */
    property Component page: null
    readonly property alias pane: pane
    /** The page of the entry shown (null: none) */
    readonly property var entryPage: page ? pageLoader.item : null
    readonly property var list: database && listKey !== "" ? database.list(listKey) : null

    /** What the window remembers of it */
    function viewState() {
        return { entry: pane.currentId }
    }
    function restoreViewState(state) {
        if (state && state.entry > 0) {
            pane.selectId(state.entry)
        }
    }

    orientation: Qt.Horizontal

    Ez.EntryListPane {
        id: pane
        list: root.list
        focus: true
        Controls.SplitView.preferredWidth: implicitWidth
        Controls.SplitView.minimumWidth: implicitWidth * 2 / 3
    }

    Loader {
        id: pageLoader
        sourceComponent: root.page ?? placeholder
        Controls.SplitView.fillWidth: true

        Binding {
            target: pageLoader.item
            property: "entry"
            value: pane.currentEntry
            when: !!root.page && !!pageLoader.item
        }
        Binding {
            target: pageLoader.item
            property: "database"
            value: root.database
            when: !!root.page && !!pageLoader.item
        }
        Binding {
            target: pageLoader.item
            property: "rpg2k3"
            value: root.rpg2k3
            when: !!root.page && !!pageLoader.item
        }
    }

    Component {
        id: placeholder
        Item {
            Ez.Note {
                anchors.centerIn: parent
                width: Math.min(parent.width - 2 * Ez.Metrics.dialogMargin, 360)
                horizontalAlignment: Text.AlignHCenter
                color: Ez.Metrics.mutedTextColor
                text: qsTr("This page is not available in the QML database yet.")
            }
        }
    }
}
