// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "QEventWidget"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * A page of a map event, as RPG Maker 2003's event window shows it: on the
 * left its settings (the conditions, the graphic, the trigger, the layer,
 * the movement and the animation), on the right its commands. The
 * settings are the page shown's (EventEditorController.page), with the
 * rules of the window on what can be changed; the commands are those of
 * the page pageIndex.
 */
Item {
    id: root

    /** The event edited */
    required property Ez.EventEditorController controller
    /** The page whose commands are shown (from 0) */
    property int pageIndex: 0
    /** The chipset of the map: the tiles a page can show */
    property int chipsetId: 0
    /** The width of the settings (0: as they need), the splitter's */
    property real settingsWidth: 0

    readonly property Ez.EventPageSettings page: controller.page
    readonly property var database: Ez.ProjectData.database()
    /** The command list (its keys, its line selected) */
    readonly property alias commandList: commands
    readonly property alias commandEditor: editor

    implicitWidth: split.implicitWidth
    implicitHeight: split.implicitHeight

    Ez.CommandListEditor {
        id: editor
        projectData: Ez.ProjectData
    }
    // the commands of the page, again when the pages move
    onPageIndexChanged: editor.showEventPage(controller, pageIndex)
    Component.onCompleted: editor.showEventPage(controller, pageIndex)
    Connections {
        target: editor
        // the commands changed: the event did (not while the window goes,
        // its controller already gone)
        function onLinesChanged() {
            root.controller?.markEdited()
        }
    }

    /** A list of the window (EventPageSettings.items) */
    component Choice: Ez.ComboBox {
        id: choice

        property int list: 0
        property int value: 0
        signal chosen(int value)

        // the id is not "list": items() would be given the combo box
        model: root.page.items(choice.list)
        // as wide as its longest entry (a few): another one chosen moves nothing
        implicitContentWidthPolicy: Controls.ComboBox.WidestText
        Layout.fillWidth: true
        Binding on currentIndex {
            value: {
                choice.count
                return choice.indexOfValue(choice.value)
            }
            restoreMode: Binding.RestoreNone
        }
        onActivated: chosen(currentValue)
    }
    /** A number of the page */
    component NumberField: Ez.SpinBox {
        id: number

        property int shown: 0
        signal chosen(int value)

        Layout.fillWidth: true
        Binding on value {
            value: number.shown
            restoreMode: Binding.RestoreNone
        }
        onValueModified: chosen(value)
    }
    /** A timer: its minutes and its seconds, as RPG Maker. The seconds take
     *  any number (90...): when the field is left, whole minutes go to the
     *  minutes, as on a clock */
    component TimerFields: RowLayout {
        id: timer

        property alias minutesName: minutes.objectName
        property alias secondsName: seconds.objectName
        property int total: 0
        signal chosen(int total)

        spacing: Ez.Metrics.columnSpacing
        Layout.columnSpan: 2
        Layout.fillWidth: true

        NumberField {
            id: minutes
            from: 0
            to: 99
            suffix: qsTr(" Minutes")
            shown: Math.floor(timer.total / 60)
            onChosen: (value) => timer.chosen(value * 60 + seconds.value)
        }
        NumberField {
            id: seconds
            from: 0
            to: 99 * 60 + 59
            suffix: qsTr(" seconds or less")
            shown: timer.total - Math.floor(timer.total / 60) * 60
            onChosen: (value) => timer.chosen(minutes.value * 60 + value)
        }
    }

    /** The graphic of the page: a character, drawn as the event moves standing
     *  still (steps, turns), or a tile of the upper layer. A click chooses
     *  another of its kind: the tiles for a tile, else the characters */
    component EventGraphic: Item {
        id: graphic

        required property Ez.EventPageSettings page
        property int chipsetId: 0
        readonly property bool tileShown: page.tile && Ez.EventTools.hasTiles(chipsetId)

        /** Select Event Graphic: the character, its facing and its frame */
        function chooseCharacter() {
            sprite.pick()
        }
        /** A tile of the upper layer instead; its layer follows its passability */
        function chooseTile() {
            const tile = Ez.EventTools.pickUpperTile(chipsetId, page.hasCharacter ? -1 : page.characterIndex, graphic)
            if (tile >= 0) {
                page.setTile(tile, Ez.EventTools.upperTileFlags(chipsetId, tile))
            }
        }

        objectName: "graphicsSprite"
        implicitWidth: sprite.implicitWidth
        implicitHeight: sprite.implicitHeight

        // the animation of the Player, at 60 frames a second, from the pose chosen
        property int count: 0
        property int frame: page.pattern
        property int facing: page.direction
        readonly property string shown: [page.characterName, page.characterIndex, page.direction, page.pattern,
                                         page.animationType, page.moveSpeed].join("/")
        onShownChanged: {
            count = 0
            frame = page.pattern
            facing = page.direction
        }
        Timer {
            interval: 1000 / 60
            repeat: true
            running: graphic.page.animates && graphic.visible
            onRunningChanged: {
                graphic.count = 0
                graphic.frame = graphic.page.pattern
                graphic.facing = graphic.page.direction
            }
            onTriggered: {
                graphic.count++
                if (graphic.page.animationType === 5) {
                    // spin: a quarter turn
                    if (graphic.count >= Ez.EventTools.turnFrames(graphic.page.moveSpeed)) {
                        graphic.count = 0
                        graphic.facing = (graphic.facing + 1) % 4
                    }
                } else if (graphic.count >= Ez.EventTools.stepFrames(graphic.page.moveSpeed)) {
                    // left, middle, right, middle
                    graphic.count = 0
                    graphic.frame = (graphic.frame + 1) % 4
                }
            }
        }

        Ez.SpriteField {
            id: sprite
            anchors.fill: parent
            visible: !graphic.tileShown
            preview: "CharSetPose"
            title: qsTranslate("EventPageWidget", "Select Event Graphic")
            // no graphic yet: the picker starts on the first charset of the list
            fileName: graphic.page.characterName
            spriteIndex: graphic.page.hasCharacter ? graphic.page.characterIndex : 0
            direction: graphic.page.direction
            pattern: graphic.page.pattern
            shownDirection: graphic.facing
            shownPattern: graphic.frame === 3 ? 1 : graphic.frame
            transparent: graphic.page.translucent
            Controls.ToolTip.visible: spriteHover.hovered
            Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
            Controls.ToolTip.text: qsTranslate("EventPageWidget", "Click to choose the character")
            // the layer and the animation follow, as in RPG Maker
            onPicked: (choice) => graphic.page.setCharacter(choice.name, choice.index, choice.direction, choice.pattern)
            HoverHandler {
                id: spriteHover
                cursorShape: Qt.PointingHandCursor
            }
        }
        Rectangle {
            anchors.fill: parent
            visible: graphic.tileShown
            Kirigami.Theme.colorSet: Kirigami.Theme.View
            Kirigami.Theme.inherit: false
            color: Kirigami.Theme.backgroundColor
            border.color: Ez.Metrics.sectionBorderColor
            radius: 2

            Ez.MapLayerView {
                anchors.centerIn: parent
                chipsetId: graphic.chipsetId
                zoom: 2
                gridColumns: 1
                gridTiles: [10000 + graphic.page.characterIndex]
                width: implicitWidth
                height: implicitHeight
                opacity: graphic.page.translucent ? 0.5 : 1
            }
            TapHandler {
                onTapped: graphic.chooseTile()
            }
            HoverHandler {
                cursorShape: Qt.PointingHandCursor
            }
        }
    }

    Controls.SplitView {
        id: split
        anchors.fill: parent
        orientation: Qt.Horizontal

        // the settings keep their width and scroll when the window is short
        Controls.ScrollView {
            id: settingsScroll

            readonly property real needed: settings.implicitWidth + effectiveScrollBarWidth

            Controls.SplitView.minimumWidth: needed
            Controls.SplitView.preferredWidth: root.settingsWidth > 0 ? Math.max(needed, Math.min(root.settingsWidth, needed * 1.5)) : needed
            Controls.SplitView.maximumWidth: needed * 1.5
            implicitWidth: needed
            // short windows scroll the settings
            implicitHeight: 160
            contentWidth: availableWidth
            onWidthChanged: {
                if (split.resizing) {
                    root.settingsWidth = width
                }
            }

            ColumnLayout {
                id: settings
                width: settingsScroll.availableWidth
                spacing: Ez.Metrics.sectionSpacing

                Ez.Section {
                    objectName: "groupBox_3"
                    title: qsTr("Precondition")

                    GridLayout {
                        columns: 3
                        rowSpacing: Ez.Metrics.rowSpacing
                        columnSpacing: Ez.Metrics.columnSpacing
                        Layout.fillWidth: true

                        Ez.CheckBox {
                            objectName: "checkSwitchA"
                            text: qsTr("Switch")
                            checked: root.page.switchA
                            onToggled: root.page.switchA = checked
                        }
                        RefChoice {
                            database: root.database
                            objectName: "comboSwitchA"
                            listKey: "switches"
                            enabled: root.page.switchA
                            value: root.page.switchAId
                            onChosen: (id) => root.page.switchAId = id
                            Layout.columnSpan: 2
                        }
                        Ez.CheckBox {
                            objectName: "checkSwitchB"
                            text: qsTr("Switch")
                            checked: root.page.switchB
                            onToggled: root.page.switchB = checked
                        }
                        RefChoice {
                            database: root.database
                            objectName: "comboSwitchB"
                            listKey: "switches"
                            enabled: root.page.switchB
                            value: root.page.switchBId
                            onChosen: (id) => root.page.switchBId = id
                            Layout.columnSpan: 2
                        }
                        Ez.CheckBox {
                            objectName: "checkVar"
                            text: qsTr("Variable")
                            checked: root.page.variable
                            onToggled: root.page.variable = checked
                        }
                        RefChoice {
                            database: root.database
                            objectName: "comboVariable"
                            listKey: "variables"
                            enabled: root.page.variable
                            value: root.page.variableId
                            onChosen: (id) => root.page.variableId = id
                            Layout.columnSpan: 2
                        }
                        RowLayout {
                            spacing: Ez.Metrics.columnSpacing
                            Layout.columnSpan: 3
                            Layout.fillWidth: true

                            Choice {
                                objectName: "comboVarOperation"
                                list: Ez.EventPageSettings.CompareOperators
                                enabled: root.page.compareOperatorEnabled
                                value: root.page.compareOperator
                                onChosen: (value) => root.page.compareOperator = value
                            }
                            NumberField {
                                objectName: "spinVarValue"
                                enabled: root.page.variable
                                from: root.page.variableMinimum
                                to: root.page.variableMaximum
                                shown: root.page.variableValue
                                onChosen: (value) => root.page.variableValue = value
                            }
                        }
                        Ez.CheckBox {
                            objectName: "checkItem"
                            text: qsTr("Item")
                            checked: root.page.item
                            onToggled: root.page.item = checked
                        }
                        RefChoice {
                            database: root.database
                            objectName: "comboItem"
                            listKey: "items"
                            enabled: root.page.item
                            value: root.page.itemId
                            onChosen: (id) => root.page.itemId = id
                            Layout.columnSpan: 2
                        }
                        Ez.CheckBox {
                            objectName: "checkHero"
                            text: qsTr("Hero")
                            checked: root.page.actor
                            onToggled: root.page.actor = checked
                        }
                        RefChoice {
                            database: root.database
                            objectName: "comboHero"
                            listKey: "actors"
                            enabled: root.page.actor
                            value: root.page.actorId
                            onChosen: (id) => root.page.actorId = id
                            Layout.columnSpan: 2
                        }
                        Ez.CheckBox {
                            objectName: "checkTimerA"
                            text: qsTr("Timer 1")
                            checked: root.page.timer
                            onToggled: root.page.timer = checked
                        }
                        TimerFields {
                            minutesName: "spinTimerAMin"
                            secondsName: "spinTimerASec"
                            enabled: root.page.timer
                            total: root.page.timerSeconds
                            onChosen: (total) => root.page.timerSeconds = total
                        }
                        Ez.CheckBox {
                            objectName: "checkTimerB"
                            text: qsTr("Timer 2")
                            // RPG Maker 2003's
                            enabled: root.page.timer2Allowed
                            checked: root.page.timer2
                            onToggled: root.page.timer2 = checked
                        }
                        TimerFields {
                            minutesName: "spinTimerBMin"
                            secondsName: "spinTimerBSec"
                            enabled: root.page.timer2
                            total: root.page.timer2Seconds
                            onChosen: (total) => root.page.timer2Seconds = total
                        }
                    }
                }

                GridLayout {
                    columns: 2
                    rowSpacing: Ez.Metrics.sectionSpacing
                    columnSpacing: Ez.Metrics.sectionSpacing
                    Layout.fillWidth: true

                    Ez.Section {
                        objectName: "groupBox_5"
                        title: qsTr("Event Graphic")
                        Layout.rowSpan: 2
                        // no smaller than its content: a grid spreads a span over its rows
                        Layout.minimumHeight: implicitHeight
                        Layout.fillHeight: true
                        Layout.alignment: Qt.AlignTop

                        EventGraphic {
                            id: graphic
                            page: root.page
                            chipsetId: root.chipsetId
                            Layout.alignment: Qt.AlignHCenter
                        }
                        Ez.CheckBox {
                            objectName: "checkTransparent"
                            text: qsTr("Semi-Transparent")
                            checked: root.page.translucent
                            onToggled: root.page.translucent = checked
                            Controls.ToolTip.visible: hovered
                            Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                            Controls.ToolTip.text: qsTr("The graphic is drawn half see-through in the game")
                        }
                        Ez.FieldButton {
                            objectName: "pushSetSprite"
                            text: qsTr("Set")
                            menu: graphicMenu
                            Layout.fillWidth: true
                        }
                    }
                    Ez.Section {
                        objectName: "groupBox_7"
                        title: qsTr("Trigger Condition")
                        Layout.alignment: Qt.AlignTop

                        Choice {
                            objectName: "comboCondition"
                            list: Ez.EventPageSettings.Triggers
                            value: root.page.trigger
                            onChosen: (value) => root.page.trigger = value
                        }
                    }
                    Ez.Section {
                        objectName: "groupBox_8"
                        title: qsTr("Event Layer")
                        Layout.alignment: Qt.AlignTop

                        Choice {
                            objectName: "comboLayer"
                            list: Ez.EventPageSettings.Layers
                            value: root.page.layer
                            onChosen: (value) => root.page.layer = value
                        }
                        Ez.CheckBox {
                            objectName: "checkOverlapForbidden"
                            text: qsTr("Forbid Event Overlap")
                            checked: root.page.overlapForbidden
                            onToggled: root.page.overlapForbidden = checked
                            Controls.ToolTip.visible: hovered
                            Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                            Controls.ToolTip.text: qsTr("Other events cannot move onto this one, and it cannot move onto them, whatever their layers. Without it, only events on the same layer block each other. The hero is not concerned.")
                        }
                    }
                    Ez.Section {
                        objectName: "groupBox_6"
                        title: qsTr("Movement Type")
                        Layout.rowSpan: 2
                        // no smaller than its content: a grid spreads a span over its rows
                        Layout.minimumHeight: implicitHeight
                        Layout.fillHeight: true
                        Layout.alignment: Qt.AlignTop

                        Choice {
                            objectName: "comboMoveType"
                            list: Ez.EventPageSettings.MoveTypes
                            value: root.page.moveType
                            onChosen: (value) => root.page.moveType = value
                        }
                        RowLayout {
                            spacing: Ez.Metrics.columnSpacing
                            Layout.fillWidth: true

                            Ez.Label {
                                objectName: "label"
                                text: qsTr("Frequency")
                                enabled: root.page.moveFrequencyEnabled
                            }
                            Choice {
                                objectName: "comboMoveFrequency"
                                list: Ez.EventPageSettings.MoveFrequencies
                                enabled: root.page.moveFrequencyEnabled
                                value: root.page.moveFrequency
                                onChosen: (value) => root.page.moveFrequency = value
                            }
                        }
                        Ez.Button {
                            objectName: "pushMovePattern"
                            text: qsTr("Define Pattern")
                            enabled: root.page.moveRouteEnabled
                            Layout.fillWidth: true
                            onClicked: Ez.EventTools.editRoute(root.page, this)
                        }
                    }
                    Ez.Section {
                        objectName: "groupBox_9"
                        title: qsTr("Animation type")
                        Layout.alignment: Qt.AlignTop

                        Choice {
                            objectName: "comboAnimationType"
                            list: Ez.EventPageSettings.AnimationTypes
                            // a tile or nothing does not animate
                            enabled: root.page.animationTypeEnabled
                            value: root.page.animationType
                            onChosen: (value) => root.page.animationType = value
                        }
                    }
                    Ez.Section {
                        objectName: "groupBox_10"
                        title: qsTr("Move Speed")
                        Layout.alignment: Qt.AlignTop

                        Choice {
                            objectName: "comboMoveSpeed"
                            list: Ez.EventPageSettings.MoveSpeeds
                            value: root.page.moveSpeed
                            onChosen: (value) => root.page.moveSpeed = value
                        }
                    }
                }
                Item {
                    Layout.fillHeight: true
                }
            }
        }

        // the commands take the rest of the window
        Ez.Section {
            objectName: "groupBox_4"
            title: qsTr("Event Commands")
            Controls.SplitView.fillWidth: true
            Controls.SplitView.minimumWidth: 120

            CommandList {
                id: commands
                editor: editor
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }
    }

    // A character of a charset, a tile of the upper layer, or nothing
    Controls.Menu {
        id: graphicMenu
        font: Ez.Metrics.font

        Controls.MenuItem {
            text: qsTranslate("EventPageWidget", "Character...")
            onTriggered: graphic.chooseCharacter()
        }
        Controls.MenuItem {
            text: qsTranslate("EventPageWidget", "Tile...")
            enabled: Ez.EventTools.hasTiles(root.chipsetId)
            onTriggered: graphic.chooseTile()
        }
        Controls.MenuItem {
            text: qsTranslate("EventPageWidget", "No Graphic")
            onTriggered: root.page.clearGraphic()
        }
    }
}
