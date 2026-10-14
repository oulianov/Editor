// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "EnemyGroupWidget"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * A monster group: its name, its members on the battle screen (dragged
 * there; the right button hides one at the start of the battle), the
 * monsters added from their list, the terrains where it is met, and its
 * battle events: pages of commands, each with its trigger. The test battle
 * against it opens from here (TestBattleDialog.qml).
 */
RichPage {
    id: root

    /** Its index in the troops */
    readonly property int troopIndex: entry ? Number(entry.pathPrefix.split("/")[2] ?? -1) : -1
    readonly property alias troopEditor: troop
    readonly property alias pageEditor: pageEditor

    /** What the window remembers of it: the page and the line of its commands */
    function viewState() {
        return { page: tabPages.currentIndex, commands: pageEditor.commandList.viewState() }
    }
    function restoreViewState(state) {
        if (state && state.page >= 0) {
            tabPages.currentIndex = state.page
            pageEditor.commandList.restoreViewState(state.commands)
        }
    }


    Ez.TroopEditor {
        id: troop
        database: root.database
        troop: root.troopIndex
        // the commands of the pages are found again where they went
        onPagesMoved: Qt.callLater(pageEditor.refresh)
        onChanged: Qt.callLater(pageEditor.battlePage.refresh)
    }
    Ez.JsonField {
        id: backdrop
        source: root.database
        path: "system/battletest_background"
    }
    onTroopIndexChanged: tabPages.currentIndex = 0

    ColumnLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: Ez.Metrics.sectionSpacing

        Ez.Section {
            objectName: "groupName"
            title: qsTr("Name")
            RowLayout {
                spacing: Ez.Metrics.columnSpacing
                Layout.fillWidth: true
                Ez.TextField {
                    id: name
                    objectName: "lineName"
                    jsonData: root.entry
                    key: "name"
                    limit: 20
                    Layout.fillWidth: true
                }
                Ez.Button {
                    objectName: "pushAutoName"
                    text: qsTr("Autoname Group")
                    onClicked: root.entry.set("name", troop.autoName())
                }
            }
        }

        Controls.SplitView {
            orientation: Qt.Vertical
            Layout.fillWidth: true
            Layout.fillHeight: true

            // the members, the buttons, the monsters and the terrains
            RowLayout {
                objectName: "widgetMembers"
                spacing: Ez.Metrics.sectionSpacing
                // its buttons are never cut
                Controls.SplitView.minimumHeight: implicitHeight
                Controls.SplitView.preferredHeight: implicitHeight + 40

                Item {
                    id: screenFrame
                    implicitWidth: 240
                    implicitHeight: 180
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumWidth: 160

                    Ez.TroopCanvas {
                        id: canvas
                        objectName: "graphicsTroop"
                        database: root.database
                        troop: root.troopIndex
                        backdrop: backdrop.value ?? ""
                        transformOrigin: Item.TopLeft
                        scale: Math.min(screenFrame.width / 320, screenFrame.height / 240)
                        x: (screenFrame.width - 320 * scale) / 2
                        y: (screenFrame.height - 240 * scale) / 2
                        onContextMenuRequested: (member, x, y) => {
                            memberMenu.member = member
                            memberMenu.popupAt(canvas, x, y)
                        }
                    }
                    // over the frame: the canvas is scaled
                    Controls.ToolTip.visible: canvasHover.hovered && !memberMenu.visible
                    Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                    Controls.ToolTip.text: qsTr("Drag the monsters to place them. Right-click a monster to hide it at the start of the battle.")
                    HoverHandler {
                        id: canvasHover
                    }
                    Ez.ContextMenu {
                        id: memberMenu
                        property int member: -1
                        font: Ez.Metrics.font
                        Controls.MenuItem {
                            objectName: "actionInvisible"
                            text: qsTranslate("EnemyGroupView", "Invisible")
                            checkable: true
                            checked: memberMenu.visible && troop.memberInvisible(memberMenu.member)
                            onTriggered: troop.setMemberInvisible(memberMenu.member, !troop.memberInvisible(memberMenu.member))
                        }
                    }
                }

                ColumnLayout {
                    spacing: Ez.Metrics.rowSpacing
                    Layout.alignment: Qt.AlignTop

                    Ez.Button {
                        objectName: "pushAdd"
                        text: qsTr("< Add")
                        Layout.fillWidth: true
                        onClicked: root.addMember()
                    }
                    Ez.Button {
                        objectName: "pushRemove"
                        text: qsTr("Remove >")
                        Layout.fillWidth: true
                        // the selected monster, else the last one
                        onClicked: troop.removeMember(canvas.selectedMember)
                    }
                    Ez.Button {
                        objectName: "pushRemoveAll"
                        text: qsTr("Remove All")
                        Layout.fillWidth: true
                        onClicked: troop.removeAllMembers()
                    }
                    Ez.Button {
                        objectName: "pushAutoAlign"
                        text: qsTr("Auto-Align")
                        Layout.fillWidth: true
                        onClicked: troop.autoAlign()
                    }
                    // automatic alignment and random appearance only exist in RPG Maker 2003
                    Ez.Section {
                        objectName: "groupAlignment"
                        title: qsTr("Alignment")
                        enabled: root.rpg2k3
                        Ez.RadioButton {
                            objectName: "radioManual"
                            text: qsTr("Manual")
                            jsonData: root.entry
                            key: "auto_alignment"
                            value: 0
                        }
                        Ez.RadioButton {
                            objectName: "radioAutomatic"
                            text: qsTr("Automatic")
                            jsonData: root.entry
                            key: "auto_alignment"
                            value: 1
                        }
                    }
                    Ez.CheckBox {
                        objectName: "checkRandom"
                        text: qsTr("Random")
                        jsonData: root.entry
                        key: "appear_randomly"
                        enabled: root.rpg2k3
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                        Controls.ToolTip.text: qsTr("Some monsters of the group are randomly left out of the battle")
                    }
                    Ez.Button {
                        objectName: "pushChangeBackdrop"
                        text: qsTr("Change BG")
                        Layout.fillWidth: true
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                        Controls.ToolTip.text: qsTr("Background of this view and of the test battles")
                        // RPG Maker keeps it in the database: the background of the test battles
                        onClicked: {
                            const choice = Ez.Pickers.pickFile({ folder: "Backdrop", preview: "Image", name: backdrop.value ?? "" }, this)
                            if (choice) {
                                backdrop.value = choice.name
                            }
                        }
                    }
                    Ez.Button {
                        objectName: "pushTestBattle"
                        text: qsTr("Test Battle")
                        Layout.fillWidth: true
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                        Controls.ToolTip.text: qsTr("Fights this monster group in the EasyRPG Player")
                        onClicked: Ez.Editor.exec("TestBattleDialog", { database: root.database, troopId: root.troopIndex + 1 }, this)
                    }
                }

                ColumnLayout {
                    spacing: Ez.Metrics.sectionSpacing
                    Layout.fillHeight: true
                    Layout.preferredWidth: 150

                    Ez.ColumnList {
                        id: enemies
                        listName: "listEnemies"
                        model: root.database ? root.database.list("enemies") : null
                        columns: [{ role: "text" }]
                        currentRow: 0
                        visibleLines: 6
                        onPicked: row => currentRow = row
                        onActivated: row => root.addMember()
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                    }
                    Ez.Section {
                        objectName: "groupTerrain"
                        title: qsTr("Habitat")
                        Layout.fillHeight: true

                        Ez.ArrayTable {
                            id: terrains
                            objectName: "listTerrains"
                            readonly property var names: root.database ? root.database.list("terrains") : null
                            rows: names ? names.count : 0
                            source: root.entry
                            showHeader: false
                            visibleRows: 4
                            columns: [
                                { name: "checkTerrain", checked: row => troop.terrainAllowed(row),
                                  toggle: (row, on) => troop.setTerrainAllowed(row, on),
                                  text: row => terrains.names.str(row + "/name") },
                            ]
                            implicitWidth: 130
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                        }
                    }
                }
            }

            // the battle events: a tab per page
            Ez.Section {
                objectName: "groupEvents"
                title: qsTr("Battle Events")
                Controls.SplitView.fillHeight: true
                Controls.SplitView.minimumHeight: 150

                RowLayout {
                    spacing: Ez.Metrics.sectionSpacing
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ColumnLayout {
                        spacing: Ez.Metrics.rowSpacing
                        Layout.alignment: Qt.AlignTop

                        Ez.Button {
                            objectName: "pushNewPage"
                            text: qsTr("New Page")
                            Layout.fillWidth: true
                            onClicked: root.showPage(troop.newPage())
                        }
                        Ez.Button {
                            objectName: "pushCopyPage"
                            text: qsTr("Copy Page")
                            Layout.fillWidth: true
                            onClicked: troop.copyPage(tabPages.currentIndex)
                        }
                        Ez.Button {
                            objectName: "pushPastePage"
                            text: qsTr("Paste Page")
                            enabled: troop.canPastePage
                            Layout.fillWidth: true
                            // after the current page
                            onClicked: root.showPage(troop.pastePage(tabPages.currentIndex))
                        }
                        Ez.Button {
                            objectName: "pushDeletePage"
                            text: qsTr("Delete Page")
                            Layout.fillWidth: true
                            onClicked: root.showPage(troop.deletePage(tabPages.currentIndex))
                        }
                    }

                    ColumnLayout {
                        spacing: 0
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        Controls.TabBar {
                            id: tabPages
                            objectName: "tabPages"
                            font: Ez.Metrics.font
                            Layout.fillWidth: true
                            Repeater {
                                model: troop.pageCount
                                Controls.TabButton {
                                    required property int index
                                    text: String(troop.pageId(index))
                                    width: implicitWidth
                                    font: Ez.Metrics.font
                                }
                            }
                        }
                        BattlePageEditor {
                            id: pageEditor
                            session: troop.session
                            troopIndex: root.troopIndex
                            pageIndex: Math.min(tabPages.currentIndex, troop.pageCount - 1)
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.topMargin: Ez.Metrics.rowSpacing
                        }
                    }
                }
            }
        }
    }

    /**
     * The battle page of an ID and a line of its commands selected (the
     * Search's results: start.battlePage, start.line; -1: no line)
     */
    function showLine(start) {
        for (let i = 0; i < troop.pageCount; ++i) {
            if (troop.pageId(i) === start.battlePage) {
                showPage(i)
                if (start.line >= 0) {
                    Qt.callLater(() => pageEditor.commandList.selectIndex(start.line))
                    Qt.callLater(() => pageEditor.commandList.view.forceActiveFocus())
                }
                return
            }
        }
    }
    /** Shows a page (after it was added, pasted or deleted) */
    function showPage(index) {
        if (index >= 0) {
            tabPages.currentIndex = index
        }
        pageEditor.refresh()
    }
    /** The monster chosen in the list, in the middle of the screen, selected */
    function addMember() {
        const index = troop.addMember(enemies.currentRow + 1)
        if (index >= 0) {
            canvas.selectedMember = index
        }
    }
}
