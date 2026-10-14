// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "ChipsetWidget"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * A chipset: its name and graphic, its tiles edited in a mode (their
 * terrain, passability, the directions the hero can walk out of them as
 * RPG Maker 2003's arrows and dots, counters) on the lower or upper layer,
 * and the animation of its water.
 */
RichPage {
    id: root

    readonly property alias canvas: canvas

    /** What the window remembers of it: the layer and the mode */
    function viewState() {
        return { layer: layer.currentIndex, mode: canvas.mode }
    }
    function restoreViewState(state) {
        if (state) {
            layer.currentIndex = state.layer ?? 0
            canvas.mode = state.mode ?? 1
        }
    }


    RowLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: Ez.Metrics.sectionSpacing

        // the tiles of a layer
        ColumnLayout {
            spacing: 0
            Layout.fillHeight: true

            Controls.TabBar {
                id: layer
                objectName: "tabLayer"
                font: Ez.Metrics.font
                Layout.fillWidth: true
                Controls.TabButton {
                    text: qsTr("Lower Layer")
                    width: implicitWidth
                    font: Ez.Metrics.font
                }
                Controls.TabButton {
                    text: qsTr("Upper Layer")
                    width: implicitWidth
                    font: Ez.Metrics.font
                }
            }
            Rectangle {
                Kirigami.Theme.colorSet: Kirigami.Theme.View
                Kirigami.Theme.inherit: false
                color: Kirigami.Theme.backgroundColor
                border.color: Ez.Metrics.sectionBorderColor
                // as wide as the tiles: the whole chipset does not make the page as tall
                implicitWidth: 6 * 32 + scroll.effectiveScrollBarWidth + 2
                implicitHeight: 160
                Layout.fillHeight: true

                Controls.ScrollView {
                    id: scroll
                    anchors.fill: parent
                    anchors.margins: 1
                    contentWidth: canvas.implicitWidth
                    contentHeight: canvas.implicitHeight
                    Controls.ScrollBar.horizontal.policy: Controls.ScrollBar.AlwaysOff

                    Ez.ChipsetCanvas {
                        id: canvas
                        objectName: "viewTiles"
                        entry: root.entry
                        lowerLayer: layer.currentIndex === 0
                        mode: 1
                        terrainId: terrain.currentIndex >= 0 ? Number(terrain.currentValue) : 1
                        background: Kirigami.Theme.backgroundColor
                        width: implicitWidth
                        height: implicitHeight
                    }
                }
            }
        }

        // its settings
        ColumnLayout {
            spacing: Ez.Metrics.sectionSpacing
            Layout.alignment: Qt.AlignTop
            Layout.fillWidth: true

            Ez.Form {
                Ez.FormLabel {
                    text: qsTr("Name")
                }
                Ez.TextField {
                    objectName: "lineName"
                    jsonData: root.entry
                    key: "name"
                    limit: 20
                    Layout.fillWidth: true
                }
                Ez.FormLabel {
                    text: qsTr("Graphic")
                }
                Ez.FileField {
                    jsonData: root.entry
                    key: "chipset_name"
                    folder: "ChipSet"
                    fieldName: "lineChipsetFile"
                    buttonName: "pushChipsetFile"
                }
            }

            Ez.Section {
                title: qsTr("Editing Mode")

                Controls.ButtonGroup {
                    id: modes
                    objectName: "editingMode"
                }
                Ez.RadioButton {
                    objectName: "radioTerrain"
                    text: qsTr("Terrain")
                    checked: canvas.mode === 0
                    Controls.ButtonGroup.group: modes
                    onToggled: if (checked) canvas.mode = 0
                }
                Ez.RefComboBox {
                    id: terrain
                    objectName: "comboTerrain"
                    database: root.database
                    listKey: "terrains"
                    pickable: true
                    enabled: canvas.mode === 0
                    currentIndex: 0
                    Layout.leftMargin: 20
                    Layout.fillWidth: true
                }
                Ez.Button {
                    objectName: "pushUniformTerrain"
                    text: qsTr("Set Uniform Terrain")
                    enabled: canvas.lowerLayer
                    Controls.ToolTip.visible: hovered
                    Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                    Controls.ToolTip.text: qsTr("Gives one terrain to all the tiles of the lower layer")
                    onClicked: Ez.Editor.exec("SetUniformTerrainDialog", { database: root.database, canvas: canvas,
                                                                        terrain: terrain.currentIndex }, this)
                }
                Ez.RadioButton {
                    objectName: "radioPassability"
                    text: qsTr("Passability")
                    checked: canvas.mode === 1
                    Controls.ButtonGroup.group: modes
                    onToggled: if (checked) canvas.mode = 1
                }
                Ez.RadioButton {
                    objectName: "radioDirections"
                    text: qsTr("Directions")
                    checked: canvas.mode === 2
                    Controls.ButtonGroup.group: modes
                    onToggled: if (checked) canvas.mode = 2
                }
                Ez.RadioButton {
                    objectName: "radioCounter"
                    text: qsTr("Counter")
                    checked: canvas.mode === 3
                    Controls.ButtonGroup.group: modes
                    onToggled: if (checked) canvas.mode = 3
                }
                // what a click does in the current mode
                Ez.Note {
                    objectName: "labelHint"
                    color: Ez.Metrics.mutedTextColor
                    text: [qsTr("Click or drag over tiles to give them the terrain above."),
                           qsTr("Click a tile: passable (○), blocked (×), above the hero (☆). Right click goes back. Drag to paint the same."),
                           qsTr("An arrow: the hero can walk out of the tile that way; a dot: not. Click a side of a tile to change it. Drag to do the same on other tiles."),
                           qsTr("Click a tile to make it a counter: events can be talked to across it (□). Drag to paint the same.")][canvas.mode] ?? ""
                    Layout.fillWidth: true
                }
            }

            Ez.Section {
                title: qsTr("Water Animation")
                enabled: canvas.lowerLayer

                Ez.Form {
                    Ez.FormLabel {
                        text: qsTr("Sequence")
                    }
                    Ez.RadioGroup {
                        jsonData: root.entry
                        key: "animation_type"
                        columns: 2
                        choices: [
                            { name: "radioSequence0", text: "1-2-3-2" },
                            { name: "radioSequence1", text: "1-2-3" },
                        ]
                    }
                    Ez.FormLabel {
                        text: qsTr("Speed")
                    }
                    Ez.RadioGroup {
                        jsonData: root.entry
                        key: "animation_speed"
                        columns: 2
                        choices: [
                            { name: "radioSpeedSlow", text: qsTr("Slow") },
                            { name: "radioSpeedFast", text: qsTr("Fast") },
                        ]
                    }
                }
            }
        }
    }
}
