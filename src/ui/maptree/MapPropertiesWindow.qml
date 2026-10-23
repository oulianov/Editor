// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "MapPropertiesDialog"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * Map Properties (RPG Maker's TFORMEDMAP), of a new map too: the name, the
 * tileset, the size and the wrapping, the monster groups met, the panorama,
 * the music and battle background, teleport, escape and save (as the parent
 * map, or not), and the settings of the Random Dungeon Generator. It edits
 * copies (MapPropertiesForm); OK gives them to the map as one undo step.
 */
Ez.DialogWindow {
    id: root

    required property Ez.MapPropertiesForm form
    /** Grows when a box of the dungeon generator gets other tiles */
    property int generatorRevision: 0
    /** The map gets smaller: asked once */
    property bool shrinkConfirmed: false

    objectName: "MapPropertiesDialog"
    title: qsTr("Map properties")

    onAccepting: (accepting) => {
        if (!shrinkConfirmed && (mapWidth.value < form.oldWidth || mapHeight.value < form.oldHeight)) {
            accepting.ok = false
            shrink.open()
        }
    }

    Connections {
        target: root.form
        function onGeneratorChanged() {
            root.generatorRevision++
        }
    }

    Ez.JsonField {
        id: chipset
        source: root.form.map
        path: "chipset_id"
    }
    Ez.JsonField {
        id: panoramaName
        source: root.form.map
        path: "parallax_name"
    }
    Ez.JsonField {
        id: generatorMode
        source: root.form.map
        path: "generator_mode"
    }
    // how the music and the battle background are chosen (2: specified)
    Ez.JsonField {
        id: musicType
        source: root.form.info
        path: "music_type"
    }
    Ez.JsonField {
        id: backgroundType
        source: root.form.info
        path: "background_type"
    }

    function pickPanorama() {
        const choice = Ez.Pickers.pickFile({ folder: "Panorama", preview: "Image", name: panoramaName.value ?? "" },
                                           panorama)
        if (choice) {
            panoramaName.value = choice.name
        }
    }

    /** A box of the dungeon generator: its tiles, a click picks others */
    component TileBox: Controls.ToolButton {
        id: tileBox

        required property Ez.MapPropertiesForm form
        required property int box
        /** The chipset chosen, and the boxes' changes (tiles read again) */
        property int chipsetId: -1
        property int revision: 0
        readonly property bool obstacle: box >= 6

        /** Its palette cells, for the scripts */
        function automationText() {
            return form.generatorCellsText(box)
        }

        padding: 3
        // greyed with the generator's settings, as the Ez controls
        opacity: enabled ? 1 : Ez.Metrics.disabledOpacity
        Controls.ToolTip.visible: hovered
        Controls.ToolTip.text: qsTranslate("MapPropertiesDialog", "Click to choose the tiles")
        Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
        onClicked: form.pickGeneratorTiles(box, tileBox)

        contentItem: Item {
            implicitWidth: tiles.width
            implicitHeight: tiles.height

            Ez.MapLayerView {
                id: tiles
                width: implicitWidth
                height: implicitHeight
                chipsetId: tileBox.chipsetId
                gridColumns: tileBox.obstacle ? 2 : 1
                gridTiles: {
                    tileBox.revision
                    return tileBox.form.generatorTiles(tileBox.box)
                }
                zoom: 2
            }
        }
    }

    RowLayout {
        spacing: Ez.Metrics.sectionSpacing
        Layout.fillHeight: true

        // name, tileset, size, wrapping, encounters
        ColumnLayout {
            spacing: Ez.Metrics.sectionSpacing
            Layout.fillHeight: true
            Layout.alignment: Qt.AlignTop

            Ez.Section {
                objectName: "groupName"
                title: qsTr("Name")
                Layout.fillHeight: false
                Ez.TextField {
                    objectName: "lineName"
                    jsonData: root.form.info
                    key: "name"
                    // the length RPG Maker allows
                    maximumLength: 24
                    Layout.fillWidth: true
                }
                // the ID of the map: its file, events and teleports name it so
                Ez.Label {
                    objectName: "labelMapId"
                    readonly property string number: String(root.form.mapId).padStart(4, "0")
                    text: qsTr("ID %1 · %2").arg(number).arg("Map" + number + ".lmu")
                    color: Ez.Metrics.mutedTextColor
                    font: Ez.Metrics.font
                }
            }
            Ez.Section {
                objectName: "groupTileset"
                title: qsTr("Tile set")
                Layout.fillHeight: false
                Ez.ComboBox {
                    objectName: "comboTileset"
                    model: Ez.ProjectData.database().list("chipsets")
                    textRole: "name"
                    valueRole: "value"
                    jsonData: root.form.map
                    key: "chipset_id"
                    Layout.fillWidth: true
                }
            }
            Ez.Section {
                objectName: "groupDimensions"
                title: qsTr("Dimensions")
                Layout.fillHeight: false
                RowLayout {
                    spacing: Ez.Metrics.columnSpacing
                    Ez.SpinBox {
                        id: mapWidth
                        objectName: "spinWidth"
                        from: 20
                        to: 500
                        jsonData: root.form.map
                        key: "width"
                        Layout.fillWidth: true
                    }
                    Ez.SpinBox {
                        id: mapHeight
                        objectName: "spinHeight"
                        from: 15
                        to: 500
                        jsonData: root.form.map
                        key: "height"
                        Layout.fillWidth: true
                    }
                }
            }
            Ez.Section {
                objectName: "groupWrapping"
                title: qsTr("Wrapping")
                Layout.fillHeight: false
                Ez.ComboBox {
                    objectName: "comboWrapping"
                    textRole: "text"
                    valueRole: "value"
                    model: [
                        { text: qsTr("None"), value: 0 },
                        { text: qsTr("Vertical"), value: 1 },
                        { text: qsTr("Horizontal"), value: 2 },
                        { text: qsTr("Both"), value: 3 },
                    ]
                    jsonData: root.form.map
                    key: "scroll_type"
                    Layout.fillWidth: true
                }
            }
            Ez.Section {
                objectName: "groupEncounters"
                title: qsTr("Enemy Encounters")
                Layout.fillHeight: true

                EncounterTable {
                    id: encounters
                    encounters: root.form.encounters
                    visibleRows: 4
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
                RowLayout {
                    spacing: Ez.Metrics.columnSpacing
                    Ez.Button {
                        objectName: "pushAddEncounter"
                        text: qsTr("Add")
                        Layout.fillWidth: true
                        onClicked: encounters.add()
                    }
                    Ez.Button {
                        objectName: "pushRemoveEncounter"
                        text: qsTr("Delete")
                        Layout.fillWidth: true
                        onClicked: encounters.remove()
                    }
                }
                RowLayout {
                    spacing: Ez.Metrics.columnSpacing
                    Ez.Label {
                        objectName: "labelEncounterRate"
                        text: qsTr("Rate")
                        font: Ez.Metrics.font
                        Layout.fillWidth: true
                    }
                    Ez.SpinBox {
                        objectName: "spinEncounterRate"
                        from: 0
                        to: 999
                        jsonData: root.form.info
                        key: "encounter_steps"
                    }
                }
            }
        }

        // panorama, music, battle background, teleport, escape, save
        ColumnLayout {
            spacing: Ez.Metrics.sectionSpacing
            Layout.fillHeight: true
            Layout.alignment: Qt.AlignTop

            Ez.CheckSection {
                checkName: "groupPanorama"
                title: qsTr("Panorama")
                jsonData: root.form.map
                key: "parallax_flag"
                Layout.fillHeight: false

                // the picture; a click on it (or where it would be) chooses it
                Rectangle {
                    id: panorama
                    objectName: "graphicsPanorama"
                    implicitWidth: 324
                    implicitHeight: 244
                    Layout.alignment: Qt.AlignHCenter
                    Kirigami.Theme.colorSet: Kirigami.Theme.View
                    Kirigami.Theme.inherit: false
                    color: Kirigami.Theme.backgroundColor
                    border.color: Ez.Metrics.sectionBorderColor
                    clip: true

                    Image {
                        anchors.fill: parent
                        anchors.margins: 1
                        visible: parent.enabled && (panoramaName.value ?? "") !== ""
                        source: visible ? "image://project/Panorama/" + panoramaName.value : ""
                        fillMode: Image.PreserveAspectFit
                        horizontalAlignment: Image.AlignLeft
                        verticalAlignment: Image.AlignTop
                        smooth: false
                        asynchronous: true
                    }
                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.pickPanorama()
                        Controls.ToolTip.visible: containsMouse
                        Controls.ToolTip.text: qsTr("Click to choose the panorama")
                        Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                    }
                }
                Ez.Button {
                    objectName: "pushSetPanorama"
                    text: qsTr("Set")
                    Layout.fillWidth: true
                    onClicked: root.pickPanorama()
                }
                RowLayout {
                    spacing: Ez.Metrics.columnSpacing
                    Ez.CheckSection {
                        checkName: "groupHorizontalScroll"
                        title: qsTr("Horizontal Scroll")
                        jsonData: root.form.map
                        key: "parallax_loop_x"
                        Layout.preferredWidth: 1
                        Layout.fillHeight: false

                        Ez.CheckBox {
                            id: horizontalAutoscroll
                            objectName: "checkHorizontalAutoscroll"
                            text: qsTr("Autoscroll")
                            jsonData: root.form.map
                            key: "parallax_auto_loop_x"
                        }
                        RowLayout {
                            spacing: Ez.Metrics.columnSpacing
                            Ez.Label {
                                text: qsTr("Speed")
                                font: Ez.Metrics.font
                                enabled: horizontalAutoscroll.checked
                                Layout.fillWidth: true
                            }
                            Ez.SpinBox {
                                objectName: "spinHorizontalScrollSpeed"
                                from: -8
                                to: 8
                                enabled: horizontalAutoscroll.checked
                                jsonData: root.form.map
                                key: "parallax_sx"
                            }
                        }
                    }
                    Ez.CheckSection {
                        checkName: "groupVerticalScroll"
                        title: qsTr("Vertical Scroll")
                        jsonData: root.form.map
                        key: "parallax_loop_y"
                        Layout.preferredWidth: 1
                        Layout.fillHeight: false

                        Ez.CheckBox {
                            id: verticalAutoscroll
                            objectName: "checkVerticalAutoscroll"
                            text: qsTr("Autoscroll")
                            jsonData: root.form.map
                            key: "parallax_auto_loop_y"
                        }
                        RowLayout {
                            spacing: Ez.Metrics.columnSpacing
                            Ez.Label {
                                text: qsTr("Speed")
                                font: Ez.Metrics.font
                                enabled: verticalAutoscroll.checked
                                Layout.fillWidth: true
                            }
                            Ez.SpinBox {
                                objectName: "spinVerticalScrollSpeed"
                                from: -8
                                to: 8
                                enabled: verticalAutoscroll.checked
                                jsonData: root.form.map
                                key: "parallax_sy"
                            }
                        }
                    }
                }
            }

            RowLayout {
                spacing: Ez.Metrics.sectionSpacing
                Layout.fillHeight: false

                // as wide as each other, as their options need (French: the
                // window grows; preferred widths of 1 cut them)
                Ez.Section {
                    id: bgmSection
                    objectName: "groupBGM"
                    title: qsTr("BGM")
                    Layout.preferredWidth: Math.max(implicitWidth, backdropSection.implicitWidth)
                    Layout.fillHeight: true

                    Ez.RadioButton {
                        objectName: "radioBGMparent"
                        text: qsTr("Same as parent")
                        jsonData: root.form.info
                        key: "music_type"
                        value: 0
                        enabled: root.form.hasParent
                    }
                    Ez.RadioButton {
                        objectName: "radioBGMevent"
                        text: qsTr("Entrust Event")
                        jsonData: root.form.info
                        key: "music_type"
                        value: 1
                    }
                    Ez.RadioButton {
                        objectName: "radioBGMspecify"
                        text: qsTr("Specify")
                        jsonData: root.form.info
                        key: "music_type"
                        value: 2
                    }
                    Ez.AudioField {
                        jsonData: root.form.info
                        key: "music"
                        music: true
                        fieldName: "lineBGMname"
                        buttonName: "toolSetBGM"
                        // a music picked is the map's: Specify
                        onPicked: (choice) => {
                            if (choice.name) {
                                musicType.value = 2
                            }
                        }
                    }
                }
                Ez.Section {
                    id: backdropSection
                    objectName: "groupBattleBackground"
                    title: qsTr("Battle Background")
                    Layout.preferredWidth: Math.max(implicitWidth, bgmSection.implicitWidth)
                    Layout.fillHeight: true

                    Ez.RadioButton {
                        objectName: "radioBackdropParent"
                        text: qsTr("Same as parent")
                        jsonData: root.form.info
                        key: "background_type"
                        value: 0
                        enabled: root.form.hasParent
                    }
                    Ez.RadioButton {
                        objectName: "radioBackdropTerrain"
                        text: qsTr("Use terrain settings")
                        jsonData: root.form.info
                        key: "background_type"
                        value: 1
                    }
                    Ez.RadioButton {
                        objectName: "radioBackdropSpecify"
                        text: qsTr("Specify")
                        jsonData: root.form.info
                        key: "background_type"
                        value: 2
                    }
                    Ez.FileField {
                        jsonData: root.form.info
                        key: "background_name"
                        folder: "Backdrop"
                        fieldName: "lineBackdropName"
                        buttonName: "toolSetBackdrop"
                        // a background picked is the map's: Specify
                        onPicked: (choice) => {
                            if (choice.name) {
                                backgroundType.value = 2
                            }
                        }
                    }
                }
            }

            // allowed here, forbidden, or as the parent map
            RowLayout {
                spacing: Ez.Metrics.sectionSpacing
                Layout.fillHeight: true

                Repeater {
                    model: [
                        { name: "Teleport", title: qsTr("Teleport"), key: "teleport" },
                        { name: "Escape", title: qsTr("Escape"), key: "escape" },
                        { name: "Save", title: qsTr("Save"), key: "save" },
                    ]
                    Ez.Section {
                        id: triState
                        required property var modelData
                        objectName: "group" + modelData.name
                        title: modelData.title
                        // as wide as its options need (the three alike)
                        Layout.preferredWidth: implicitWidth
                        Layout.fillHeight: true

                        Ez.RadioButton {
                            objectName: "radio" + triState.modelData.name + "Parent"
                            text: qsTr("As Parent")
                            jsonData: root.form.info
                            key: triState.modelData.key
                            value: 0
                            enabled: root.form.hasParent
                        }
                        Ez.RadioButton {
                            objectName: "radio" + triState.modelData.name + "Allow"
                            text: qsTr("Allow")
                            jsonData: root.form.info
                            key: triState.modelData.key
                            value: 1
                        }
                        Ez.RadioButton {
                            objectName: "radio" + triState.modelData.name + "Forbid"
                            text: qsTr("Forbid")
                            jsonData: root.form.info
                            key: triState.modelData.key
                            value: 2
                        }
                    }
                }
            }
        }

        // the Random Dungeon Generator
        Ez.CheckSection {
            checkName: "groupUseGenerator"
            title: qsTr("Use Random Dungeon Generator")
            jsonData: root.form.map
            key: "generator_flag"
            Layout.fillHeight: true
            Layout.alignment: Qt.AlignTop
            // its texts wrap from 310 pixels; its options are never cut
            Layout.preferredWidth: Math.max(310, implicitWidth)

            Ez.Note {
                objectName: "labelGeneratorHelp"
                text: qsTr("OK applies these settings. To create a dungeon map, right-click this map and choose Generate Dungeon. Choose different ceiling and floor tiles so the layout is visible.")
            }
            Ez.Section {
                objectName: "groupGeneratorGuidelines"
                title: qsTr("Dungeon Generation guidelines")
                Layout.fillHeight: false

                Ez.RadioGroup {
                    jsonData: root.form.map
                    key: "generator_mode"
                    choices: [
                        { name: "radioDungeonSinglePassage", text: qsTr("Single Winding Passage") },
                        { name: "radioDungeonLinkedRooms", text: qsTr("Rooms Linked with Passages") },
                        { name: "radioDungeonMaze", text: qsTr("Maze Like Passage structures") },
                        { name: "radioDungeonOpenRoom", text: qsTr("Open Room with obstacles") },
                    ]
                }
                RowLayout {
                    spacing: Ez.Metrics.columnSpacing
                    Ez.Section {
                        id: passageSection
                        objectName: "groupDungeonPassage"
                        title: qsTr("Passage Granularity")
                        Layout.fillHeight: true
                        Layout.preferredWidth: Math.max(implicitWidth, roomSection.implicitWidth)

                        Ez.RadioGroup {
                            jsonData: root.form.map
                            key: "generator_tiles"
                            choices: [
                                { name: "radioDungeonPassage1_1", text: qsTr("1 x 1") },
                                { name: "radioDungeonPassage2_2", text: qsTr("2 x 2") },
                            ]
                        }
                    }
                    Ez.Section {
                        id: roomSection
                        objectName: "groupRoomDimensions"
                        title: qsTr("Room Dimensions")
                        // only "Rooms Linked with Passages" has rooms
                        enabled: generatorMode.value === 1
                        Layout.fillHeight: true
                        Layout.preferredWidth: Math.max(implicitWidth, passageSection.implicitWidth)

                        Ez.Form {
                            Ez.FormLabel {
                                text: qsTr("Width")
                            }
                            Ez.SpinBox {
                                objectName: "spinDungeonRoomWidth"
                                from: 2
                                to: 20
                                jsonData: root.form.map
                                key: "generator_width"
                                Layout.fillWidth: true
                            }
                            Ez.FormLabel {
                                text: qsTr("Height")
                            }
                            Ez.SpinBox {
                                objectName: "spinDungeonRoomHeight"
                                from: 2
                                to: 15
                                jsonData: root.form.map
                                key: "generator_height"
                                Layout.fillWidth: true
                            }
                        }
                    }
                }
                Ez.CheckBox {
                    objectName: "checkDungeonSurroundWithWalls"
                    text: qsTr("Surround Map with Wall Tiles")
                    jsonData: root.form.map
                    key: "generator_surround"
                }
            }

            // the tiles: lower tiles, then obstacles of up to 2 x 2 upper tiles
            GridLayout {
                columns: 3
                rowSpacing: Ez.Metrics.rowSpacing
                columnSpacing: Ez.Metrics.columnSpacing
                Layout.fillHeight: false

                Ez.Section {
                    objectName: "groupCeiling"
                    title: qsTr("Ceiling")
                    Layout.alignment: Qt.AlignBottom
                    Layout.fillHeight: false
                    TileBox {
                        objectName: "toolCeiling"
                        box: 0
                        form: root.form
                        chipsetId: chipset.value ?? -1
                        revision: root.generatorRevision
                    }
                }
                Ez.Section {
                    objectName: "groupLowerWall"
                    title: qsTr("Lower Wall")
                    Layout.alignment: Qt.AlignBottom
                    Layout.fillHeight: false
                    TileBox {
                        objectName: "toolLowerWall"
                        box: 1
                        form: root.form
                        chipsetId: chipset.value ?? -1
                        revision: root.generatorRevision
                    }
                }
                Ez.CheckSection {
                    checkName: "groupUpperWall"
                    title: qsTr("Upper Wall")
                    jsonData: root.form.map
                    key: "generator_upper_wall"
                    Layout.alignment: Qt.AlignBottom
                    Layout.fillHeight: false
                    TileBox {
                        objectName: "toolUpperWall"
                        box: 2
                        form: root.form
                        chipsetId: chipset.value ?? -1
                        revision: root.generatorRevision
                    }
                }
                Ez.Section {
                    objectName: "groupFloorA"
                    title: qsTr("Floor A")
                    Layout.alignment: Qt.AlignBottom
                    Layout.fillHeight: false
                    TileBox {
                        objectName: "toolFloorA"
                        box: 3
                        form: root.form
                        chipsetId: chipset.value ?? -1
                        revision: root.generatorRevision
                    }
                }
                Ez.CheckSection {
                    checkName: "groupFloorB"
                    title: qsTr("Floor B")
                    jsonData: root.form.map
                    key: "generator_floor_b"
                    Layout.alignment: Qt.AlignBottom
                    Layout.fillHeight: false
                    TileBox {
                        objectName: "toolFloorB"
                        box: 4
                        form: root.form
                        chipsetId: chipset.value ?? -1
                        revision: root.generatorRevision
                    }
                }
                Ez.CheckSection {
                    checkName: "groupFloorC"
                    title: qsTr("Floor C")
                    jsonData: root.form.map
                    key: "generator_floor_c"
                    Layout.alignment: Qt.AlignBottom
                    Layout.fillHeight: false
                    TileBox {
                        objectName: "toolFloorC"
                        box: 5
                        form: root.form
                        chipsetId: chipset.value ?? -1
                        revision: root.generatorRevision
                    }
                }
                Ez.Section {
                    objectName: "groupObstacleA"
                    title: qsTr("Obstacle A")
                    Layout.alignment: Qt.AlignBottom
                    Layout.fillHeight: false
                    TileBox {
                        objectName: "toolObstacleA"
                        box: 6
                        form: root.form
                        chipsetId: chipset.value ?? -1
                        revision: root.generatorRevision
                    }
                }
                Ez.CheckSection {
                    checkName: "groupObstacleB"
                    title: qsTr("Obstacle B")
                    jsonData: root.form.map
                    key: "generator_extra_b"
                    Layout.alignment: Qt.AlignBottom
                    Layout.fillHeight: false
                    TileBox {
                        objectName: "toolObstacleB"
                        box: 7
                        form: root.form
                        chipsetId: chipset.value ?? -1
                        revision: root.generatorRevision
                    }
                }
                Ez.CheckSection {
                    checkName: "groupObstacleC"
                    title: qsTr("Obstacle C")
                    jsonData: root.form.map
                    key: "generator_extra_c"
                    Layout.alignment: Qt.AlignBottom
                    Layout.fillHeight: false
                    TileBox {
                        objectName: "toolObstacleC"
                        box: 8
                        form: root.form
                        chipsetId: chipset.value ?? -1
                        revision: root.generatorRevision
                    }
                }
            }
            Item {
                Layout.fillHeight: true
            }
        }
    }

    // a smaller map loses what is past its new edges
    Ez.MessageDialog {
        id: shrink
        objectName: "shrinkQuestion"
        parent: Controls.Overlay.overlay
        title: qsTr("Shrink map")
        text: qsTr("You are about to shrink the current map. All out of bounds map data and events will be deleted. This cannot be undone. Do you want to continue?")
        standardButtons: Controls.Dialog.Yes | Controls.Dialog.No
        onAccepted: {
            root.shrinkConfirmed = true
            root.accept()
        }
    }
}
