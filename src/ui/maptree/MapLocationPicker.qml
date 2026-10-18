// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "MapLocationDialog"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * Select Location (RPG Maker 2003's TFORMMAPSETPLACE, Pickers::pickLocation):
 * at the left the map tree (as the main window's: the project, the maps,
 * their areas, which are no location), searched by number or name; at the
 * right the map chosen, on which a cell is clicked (the location given
 * marked first), a double-click is OK. Under them the Character Facing
 * after a teleport (when asked), then the location chosen and the zoom of
 * the map (1/2 first, as RPG Maker 2003).
 */
Ez.DialogWindow {
    id: root

    required property Ez.LocationPicker picker
    /** A cell of the map shown only: no map tree */
    property bool fixedMap: false
    /** The cell marked first */
    property int startX: 0
    property int startY: 0
    /** The Character Facing (0 retain, 1 up, 2 right, 3 down, 4 left); -1: not asked */
    property int facing: -1
    property string customTitle

    /** The cell chosen */
    readonly property int cellX: view.selection.x
    readonly property int cellY: view.selection.y

    /** Shows the map of a line of the tree (areas and the project are no location) */
    function choose(row) {
        if (picker.maps.selectableAt(row)) {
            picker.showMap(picker.maps.mapIdAt(row))
        }
    }

    objectName: "MapLocationDialog"
    title: customTitle !== "" ? customTitle : qsTr("Select Location")
    stateKey: "MapLocationDialog"
    preferredWidth: 760
    preferredHeight: 540
    acceptable: picker.mapId > 0 && view.hasSelection

    Component.onCompleted: {
        if (picker.mapId > 0) {
            // the map chosen shown in the tree, what it holds too
            picker.maps.reveal(picker.mapId)
            picker.maps.setExpanded(picker.maps.rowOf(picker.mapId), true)
            view.setCell(startX, startY)
            view.centerOn(startX, startY)
        }
        maps.positionViewAtIndex(Math.max(0, maps.currentIndex), ListView.Center)
        view.forceActiveFocus()
    }

    Controls.SplitView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 220

        ColumnLayout {
            visible: !root.fixedMap
            spacing: Ez.Metrics.rowSpacing
            Controls.SplitView.preferredWidth: 214
            Controls.SplitView.minimumWidth: 160

            // the maps searched by number or name, as the lists of maps
            Ez.TextField {
                id: search
                objectName: "searchMaps"
                placeholderText: qsTr("Search by number or name")
                Layout.fillWidth: true
                onTextChanged: {
                    root.picker.maps.filter = text
                    maps.positionViewAtIndex(Math.max(0, maps.currentIndex), ListView.Contain)
                }
                // the first map shown
                function chooseFirst() {
                    root.choose(root.picker.maps.firstSelectable())
                    maps.forceActiveFocus()
                }
                Keys.onReturnPressed: chooseFirst()
                Keys.onEnterPressed: chooseFirst()
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Kirigami.Theme.colorSet: Kirigami.Theme.View
                Kirigami.Theme.inherit: false
                color: Kirigami.Theme.backgroundColor
                border.color: maps.activeFocus ? Kirigami.Theme.focusColor : Ez.Metrics.sectionBorderColor
                radius: 2

                ListView {
                    id: maps

                    /** Changes when the lines change (a search, a node expanded) */
                    property int revision: 0

                    /** The line of a map by its text, shown first (the scripts' click-item) */
                    function automationItem(text) {
                        const map = root.picker.maps
                        let found = -1
                        for (const exact of [true, false]) {
                            for (let row = 0; row < count && found < 0; ++row) {
                                const name = map.data(map.index(row, 0))
                                if (exact ? name === text : name.indexOf(text) >= 0) {
                                    found = row
                                }
                            }
                        }
                        if (found < 0) {
                            return null
                        }
                        positionViewAtIndex(found, ListView.Contain)
                        return itemAtIndex(found)
                    }

                    objectName: "treeMaps"
                    anchors.fill: parent
                    anchors.margins: 1
                    clip: true
                    activeFocusOnTab: true
                    boundsBehavior: Flickable.StopAtBounds
                    highlightMoveDuration: 0
                    // the line of the map shown (again after a search, a node expanded)
                    currentIndex: revision >= 0 ? root.picker.maps.rowOf(root.picker.mapId) : -1
                    model: root.picker.maps
                    Controls.ScrollBar.vertical: Controls.ScrollBar {}

                    Connections {
                        target: root.picker.maps
                        function onModelReset() {
                            maps.revision++
                        }
                    }

                    // the arrows go from map to map, over the areas; right and
                    // left expand and collapse the map shown
                    Keys.onUpPressed: {
                        for (let row = currentIndex - 1; row >= 0; --row) {
                            if (root.picker.maps.selectableAt(row)) {
                                root.choose(row)
                                positionViewAtIndex(row, ListView.Contain)
                                return
                            }
                        }
                    }
                    Keys.onDownPressed: {
                        for (let row = currentIndex + 1; row < count; ++row) {
                            if (root.picker.maps.selectableAt(row)) {
                                root.choose(row)
                                positionViewAtIndex(row, ListView.Contain)
                                return
                            }
                        }
                    }
                    Keys.onRightPressed: root.picker.maps.setExpanded(currentIndex, true)
                    Keys.onLeftPressed: root.picker.maps.setExpanded(currentIndex, false)

                    // a node as the map tree of the main window draws it
                    delegate: Controls.ItemDelegate {
                        id: line

                        required property int index
                        required property string name
                        required property int depth
                        required property int type
                        required property bool selectable
                        required property bool hasChildren
                        required property bool expanded
                        readonly property int indent: 14

                        width: ListView.view.width
                        text: name
                        font: Ez.Metrics.font
                        topPadding: Ez.Metrics.listRowPadding
                        bottomPadding: Ez.Metrics.listRowPadding
                        leftPadding: (depth + 1) * indent
                        highlighted: ListView.isCurrentItem
                        // the project and the areas: their icons, in the variant
                        // for the background (dark mode)
                        icon.source: type === 0 || type === 2
                            ? (Kirigami.Theme.backgroundColor.hslLightness < 0.5 ? "qrc:/dark/" : "qrc:/bright/")
                              + (type === 0 ? "folder" : "select")
                            : ""
                        icon.width: 16
                        icon.height: 16
                        icon.color: "transparent"
                        display: type !== 1 ? Controls.AbstractButton.TextBesideIcon : Controls.AbstractButton.TextOnly
                        onClicked: {
                            maps.forceActiveFocus()
                            root.choose(index)
                        }

                        // the arrow of a node holding others: expands or collapses it
                        Ez.Label {
                            x: line.depth * line.indent
                            width: line.indent
                            height: parent.height
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: Text.AlignHCenter
                            visible: line.hasChildren
                            text: line.expanded ? "▾" : "▸"
                            color: line.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                            MouseArea {
                                anchors.fill: parent
                                onClicked: root.picker.maps.setExpanded(line.index, !line.expanded)
                            }
                        }
                    }
                }
            }
        }

        MapCellView {
            id: view
            objectName: "mapPreview"
            document: root.picker.document
            // 1/2 first, as RPG Maker 2003
            cellSize: 16
            Controls.SplitView.fillWidth: true
            Controls.SplitView.minimumWidth: 240
            onCellActivated: {
                if (root.acceptable) {
                    root.accept()
                }
            }
        }
    }

    Ez.Section {
        objectName: "groupFacing"
        visible: root.facing >= 0
        title: qsTr("Character Facing")
        Layout.fillHeight: false
        RowLayout {
            spacing: 2 * Ez.Metrics.columnSpacing
            Repeater {
                model: [qsTr("Retain Current Facing"), qsTr("Up"), qsTr("Right"), qsTr("Down"), qsTr("Left")]
                Ez.RadioButton {
                    required property string modelData
                    required property int index
                    objectName: "radioFacing" + index
                    text: modelData
                    checked: root.facing === index
                    onToggled: root.facing = index
                    Layout.fillWidth: true
                }
            }
        }
    }

    // the location chosen, then the zoom of the map
    RowLayout {
        spacing: Ez.Metrics.columnSpacing
        Layout.fillHeight: false

        Rectangle {
            Layout.preferredWidth: 225
            Layout.minimumWidth: 160
            implicitHeight: location.implicitHeight + 2 * Ez.Metrics.fieldPadding
            color: "transparent"
            border.color: Ez.Metrics.sectionBorderColor
            radius: 2

            Ez.Label {
                id: location
                objectName: "labelLocation"
                anchors.fill: parent
                anchors.leftMargin: Ez.Metrics.listTextPadding + 3
                anchors.rightMargin: Ez.Metrics.listTextPadding
                verticalAlignment: Text.AlignVCenter
                text: root.picker.mapId <= 0 ? qsTr("Choose a map")
                    : view.hasSelection ? qsTr("%1 (%2, %3)").arg(root.picker.mapName).arg(view.selection.x).arg(view.selection.y)
                    : root.picker.mapName
                font: Ez.Metrics.font
                elide: Text.ElideRight
            }
        }
        Controls.ToolSeparator {}
        MapZoomButtons {
            target: view
        }
        Item {
            Layout.fillWidth: true
        }
    }
}
