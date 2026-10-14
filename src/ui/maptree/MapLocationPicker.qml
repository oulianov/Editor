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
 * Select Location (RPG Maker's TFORMMAPSETPLACE, Pickers::pickLocation):
 * the maps of the project, searched by number or name, and the map chosen,
 * on which a cell is clicked (the location given marked first); a
 * double-click on a cell is OK. RPG Maker 2003 asks the Character Facing
 * after a teleport too.
 */
Ez.DialogWindow {
    id: root

    required property Ez.LocationPicker picker
    /** A cell of the map shown only: no list of the maps */
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

    /** Shows the map of a line of the list (areas and the project are no location) */
    function choose(row) {
        if (picker.maps.selectableAt(row)) {
            picker.showMap(picker.maps.mapIdAt(row))
        }
    }

    objectName: "MapLocationDialog"
    title: customTitle !== "" ? customTitle : qsTr("Select Location")
    preferredWidth: 860
    height: Math.max(minimumHeight, 560)
    acceptable: picker.mapId > 0 && view.hasSelection

    Component.onCompleted: {
        if (picker.mapId > 0) {
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
            Controls.SplitView.preferredWidth: 200
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
                    objectName: "treeMaps"
                    anchors.fill: parent
                    anchors.margins: 1
                    clip: true
                    activeFocusOnTab: true
                    boundsBehavior: Flickable.StopAtBounds
                    highlightMoveDuration: 0
                    // the line of the map shown (again after a search)
                    currentIndex: {
                        root.picker.maps.filter
                        return root.picker.maps.rowOf(root.picker.mapId)
                    }
                    model: root.picker.maps
                    Controls.ScrollBar.vertical: Controls.ScrollBar {}

                    // the arrows go from map to map, over the areas
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

                    delegate: Controls.ItemDelegate {
                        id: line

                        required property int index
                        required property string name
                        required property int depth
                        required property int type
                        required property bool selectable

                        width: ListView.view.width
                        text: name
                        font: Ez.Metrics.font
                        topPadding: Ez.Metrics.listRowPadding
                        bottomPadding: Ez.Metrics.listRowPadding
                        leftPadding: Ez.Metrics.listTextPadding + 14 * depth
                        highlighted: ListView.isCurrentItem
                        // the project and the areas, as the map tree shows them, in
                        // the variant for the background (dark mode)
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
                    }
                }
            }
        }

        ColumnLayout {
            spacing: Ez.Metrics.rowSpacing
            Controls.SplitView.fillWidth: true

            MapCellView {
                id: view
                objectName: "mapPreview"
                document: root.picker.document
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 240
                Layout.minimumHeight: 180
                onCellActivated: {
                    if (root.acceptable) {
                        root.accept()
                    }
                }
            }
            RowLayout {
                spacing: Ez.Metrics.columnSpacing
                MapCellView.ZoomButtons {
                    target: view
                }
                Ez.Label {
                    objectName: "labelLocation"
                    text: root.picker.mapId <= 0 ? qsTr("Choose a map")
                        : view.hasSelection ? qsTr("%1 (%2, %3)").arg(root.picker.mapName).arg(view.selection.x).arg(view.selection.y)
                        : root.picker.mapName
                    font: Ez.Metrics.font
                    elide: Text.ElideRight
                    Layout.fillWidth: true
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
                }
            }
            Item {
                Layout.fillWidth: true
            }
        }
    }
}
