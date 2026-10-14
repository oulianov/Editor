// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "GenerateDungeonDialog"

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * Generate Dungeon (RPG Maker's TFORMEDDUNGEON): a new map with the settings
 * of a map using the Random Dungeon Generator, its own name, tileset and
 * size, an entrance and an exit kept on the floor, and the dungeon shown as
 * it is generated (another one on demand). OK adds the map shown.
 */
Ez.DialogWindow {
    id: root

    required property Ez.DungeonForm form
    /** The name of the new map */
    property string dungeonName

    function generate() {
        form.generate(tileset.currentValue ?? form.sourceChipset, mapWidth.value, mapHeight.value,
                      entrance.cellX, entrance.cellY, exit.cellX, exit.cellY)
    }

    objectName: "GenerateDungeonDialog"
    title: qsTr("Generate Dungeon")

    Component.onCompleted: {
        name.text = dungeonName
        tileset.currentIndex = Math.max(0, tileset.indexOfValue(form.sourceChipset))
        mapWidth.value = Math.max(20, Math.min(500, form.sourceWidth))
        mapHeight.value = Math.max(15, Math.min(500, form.sourceHeight))
        generate()
    }
    // the dungeon of the settings shown, a change not drawn yet too
    onAccepted: {
        dungeonName = name.text
        later.stop()
        generate()
    }

    /** A cell of the map: its X and Y */
    component PointSection: Ez.Section {
        id: point

        required property string name
        /** The size of the map */
        property int columns: 20
        property int rows: 15
        readonly property int cellX: pointX.value
        readonly property int cellY: pointY.value

        /** Changed by the user */
        signal modified()

        Controls.ToolTip.visible: hover.hovered
        Controls.ToolTip.text: qsTr("A cell kept on the floor and reachable from the whole dungeon. 0, 0: none.")
        Controls.ToolTip.delay: Kirigami.Units.toolTipDelay

        HoverHandler {
            id: hover
        }
        RowLayout {
            spacing: Ez.Metrics.columnSpacing
            Ez.FormLabel {
                text: qsTr("X")
            }
            Ez.SpinBox {
                id: pointX
                objectName: "spin" + point.name + "X"
                from: 0
                // RPG Maker stops at 100; a cell of a larger map is allowed too
                to: point.columns - 1
                Layout.fillWidth: true
                onValueModified: point.modified()
            }
            Ez.FormLabel {
                text: qsTr("Y")
            }
            Ez.SpinBox {
                id: pointY
                objectName: "spin" + point.name + "Y"
                from: 0
                to: point.rows - 1
                Layout.fillWidth: true
                onValueModified: point.modified()
            }
        }
    }

    Timer {
        id: later
        interval: 150
        onTriggered: root.generate()
    }

    RowLayout {
        spacing: Ez.Metrics.sectionSpacing
        Layout.fillHeight: true

        ColumnLayout {
            spacing: Ez.Metrics.sectionSpacing
            Layout.alignment: Qt.AlignTop

            Ez.Section {
                title: qsTr("Name")
                Layout.fillHeight: false
                Ez.TextField {
                    id: name
                    objectName: "editName"
                    maximumLength: 24
                    Layout.fillWidth: true
                }
            }
            Ez.Section {
                title: qsTr("Tileset")
                Layout.fillHeight: false
                Ez.ComboBox {
                    id: tileset
                    objectName: "comboTileset"
                    model: Ez.ProjectData.database().list("chipsets")
                    textRole: "name"
                    valueRole: "value"
                    Layout.fillWidth: true
                    onActivated: later.restart()
                }
            }
            Ez.Section {
                title: qsTr("Dimensions")
                Layout.fillHeight: false
                RowLayout {
                    spacing: Ez.Metrics.columnSpacing
                    Ez.SpinBox {
                        id: mapWidth
                        objectName: "spinWidth"
                        from: 20
                        to: 500
                        Layout.fillWidth: true
                        onValueModified: later.restart()
                    }
                    Ez.Label {
                        text: "×"
                        font: Ez.Metrics.font
                    }
                    Ez.SpinBox {
                        id: mapHeight
                        objectName: "spinHeight"
                        from: 15
                        to: 500
                        Layout.fillWidth: true
                        onValueModified: later.restart()
                    }
                }
            }
            // cells the dungeon keeps on the floor and joined to the rest, to
            // put the teleports in and out there afterwards; 0, 0 (in the
            // border): none
            PointSection {
                id: entrance
                columns: mapWidth.value
                rows: mapHeight.value
                onModified: later.restart()
                name: "Entrance"
                title: qsTr("Entrance")
                Layout.fillHeight: false
            }
            PointSection {
                id: exit
                columns: mapWidth.value
                rows: mapHeight.value
                onModified: later.restart()
                name: "Exit"
                title: qsTr("Exit")
                Layout.fillHeight: false
            }
            Ez.Button {
                objectName: "pushAnotherDungeon"
                text: qsTr("Another Dungeon")
                Layout.fillWidth: true
                onClicked: {
                    later.stop()
                    root.form.reseed()
                }
            }
        }

        // the dungeon, the whole map in the frame
        Rectangle {
            id: frame
            objectName: "dungeonPreview"
            implicitWidth: 360
            implicitHeight: 280
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Qt.darker(Kirigami.Theme.backgroundColor, Kirigami.Theme.backgroundColor.hslLightness > 0.5 ? 1.5 : 1.3)
            border.color: Ez.Metrics.sectionBorderColor
            clip: true

            MapPreview {
                id: preview
                document: root.form.preview
                snap: false
                zoom: {
                    const cells = lowerView.cells
                    const tile = Math.max(1, lowerView.tileSize)
                    if (cells.width <= 0 || cells.height <= 0) {
                        return 1
                    }
                    return Math.min((frame.width - 2) / (cells.width * tile), (frame.height - 2) / (cells.height * tile))
                }
                anchors.centerIn: parent
            }
        }
    }

}
