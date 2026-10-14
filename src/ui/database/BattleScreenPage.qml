// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "BattleScreenWidget"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * The battle layout and the battle commands of RPG Maker 2003 (the
 * database's battlecommands): the type of battle, its windows, where the
 * heroes stand (placed with the mouse on the battle screen, behind a
 * monster group and a background shown here only), the heroes' own
 * commands, the commands themselves, and what a lost random battle does.
 */
DatabaseEntryPage {
    id: root

    /** The hero shown on the battle screen (an index), with their commands */
    property int hero: 0
    readonly property string battle: "battlecommands/"
    readonly property var commands: database ? database.list("battlecommands/commands") : null
    readonly property var commandChoices: database ? database.choices("battlecommands/commands", qsTr("<none>"), 0) : null
    readonly property var heroes: database ? database.list("actors") : null
    /** The command whose name and archetype are shown */
    property int command: 0

    /** What the window remembers of it */
    function viewState() {
        return { hero: hero, command: command }
    }
    function restoreViewState(state) {
        if (state) {
            hero = Math.max(0, state.hero ?? 0)
            command = Math.max(0, state.command ?? 0)
        }
    }

    single: true
    fillWidth: true

    // the commands of the hero before the Row command (0), as the Classes page
    property int heroRevision: 0
    function heroCommands() {
        const ids = []
        if (!database || !heroes || hero < 0 || hero >= heroes.count) {
            return ids
        }
        const all = database.value("actors/" + hero + "/battle_commands") ?? []
        for (let i = 0; i < all.length; ++i) {
            if (all[i] > 0) {
                ids.push(all[i])
            }
        }
        return ids
    }
    function writeHeroCommands() {
        if (!database || hero < 0 || !heroes || hero >= heroes.count) {
            return
        }
        const ids = []
        for (let slot = 0; slot < heroCommandRepeater.count; ++slot) {
            const combo = heroCommandRepeater.itemAt(slot) as Ez.ComboBox
            const id = combo ? Number(combo.currentValue) : 0
            if (id > 0) {
                ids.push(id)
            }
        }
        // like RPG Maker: the Row command (0) ends the list, padded to 7 entries with -1
        ids.push(0)
        while (ids.length < 7) {
            ids.push(-1)
        }
        database.set("actors/" + hero + "/battle_commands", ids)
    }
    Connections {
        target: root.database
        function onValueChanged(jsonPtr) {
            if (jsonPtr === "" || jsonPtr.startsWith("/actors") || jsonPtr.startsWith("/battlecommands/commands")) {
                root.heroRevision++
            }
        }
    }

    Ez.Note {
        objectName: "labelRpg2003Only"
        visible: !root.rpg2k3
        text: qsTr("These settings only exist in RPG Maker 2003 games.")
        Layout.fillWidth: true
    }

    RowLayout {
        objectName: "widgetSettings"
        enabled: root.rpg2k3
        spacing: Ez.Metrics.sectionSpacing
        Layout.fillWidth: true

        // the hero on the battle screen, their row and commands, the placement
        ColumnLayout {
            spacing: Ez.Metrics.sectionSpacing
            Layout.alignment: Qt.AlignTop
            Layout.fillWidth: true

            Ez.Section {
                objectName: "groupCharacter"
                title: qsTr("Character")

                GridLayout {
                    columns: 2
                    rowSpacing: Ez.Metrics.rowSpacing
                    columnSpacing: Ez.Metrics.columnSpacing
                    Layout.fillWidth: true

                    Ez.ColumnList {
                        listName: "listHeroes"
                        model: root.heroes
                        columns: [{ role: "text" }]
                        currentRow: root.hero
                        visibleLines: 12
                        onPicked: row => root.hero = row
                        Layout.preferredWidth: 110
                        Layout.fillHeight: true
                    }

                    // the battle screen at its size, larger when there is room
                    Item {
                        id: screenFrame
                        // half its size in a small window, whole in a large one
                        implicitWidth: 160
                        implicitHeight: 120
                        Layout.fillWidth: true
                        Layout.preferredHeight: width * 3 / 4

                        // over the frame: the canvas is scaled
                        Controls.ToolTip.visible: layout.heroHovered
                        Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                        Controls.ToolTip.text: layout.heroMovable
                            ? qsTr("Drag %1 to their battle position").arg(root.heroes && root.hero < root.heroes.count ? root.database.str("actors/" + root.hero + "/name") : "")
                            : qsTr("The automatic placement puts the heroes on the grid of the terrain")

                        Ez.TroopCanvas {
                            id: layout
                            objectName: "graphicsLayout"
                            database: root.database
                            troop: troopShown.currentIndex
                            readOnly: true
                            backdrop: backdrop.value ?? ""
                            hero: root.hero
                            heroMovable: placement.value === 0
                            heroFrontRow: row.value !== 1
                            transformOrigin: Item.TopLeft
                            scale: Math.min(screenFrame.width / 320, screenFrame.height / 240)
                            x: (screenFrame.width - 320 * scale) / 2
                        }
                        // the background of the test battles, until another is shown here
                        Ez.JsonField {
                            id: backdrop
                            source: root.database
                            path: "system/battletest_background"
                        }
                    }

                    // a monster group and a background shown behind, as in RPG Maker
                    RowLayout {
                        spacing: Ez.Metrics.columnSpacing
                        Layout.columnSpan: 2
                        Layout.fillWidth: true

                        Ez.Label {
                            objectName: "labelTroopShown"
                            text: qsTr("Monster Group Shown")
                            font: Ez.Metrics.font
                        }
                        Ez.ComboBox {
                            id: troopShown
                            objectName: "comboTroopShown"
                            model: root.database ? root.database.list("troops") : null
                            currentIndex: 0
                            Layout.fillWidth: true
                            Controls.ToolTip.visible: hovered
                            Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                            Controls.ToolTip.text: qsTr("Shown here only, to place the heroes")
                        }
                        Ez.Button {
                            objectName: "pushChangeBackdrop"
                            text: qsTr("Change BG")
                            Layout.fillWidth: true
                            Controls.ToolTip.visible: hovered
                            Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                            Controls.ToolTip.text: qsTr("Background shown here only, to place the heroes")
                            onClicked: {
                                const choice = Ez.Pickers.pickFile({ folder: "Backdrop", preview: "Image", name: layout.backdrop }, this)
                                if (choice) {
                                    layout.backdrop = choice.name
                                }
                            }
                        }
                    }

                    RowLayout {
                        spacing: Ez.Metrics.sectionSpacing
                        Layout.columnSpan: 2
                        Layout.fillWidth: true

                        Ez.Section {
                            objectName: "groupRow"
                            title: qsTr("Row Shown")
                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            // the row moves the hero on the screen
                            Ez.RadioGroup {
                                id: row
                                jsonData: root.database
                                key: root.battle + "row"
                                choices: [
                                    { name: "radioRowFront", text: qsTr("Front") },
                                    { name: "radioRowBack", text: qsTr("Back") },
                                ]
                            }
                        }

                        Ez.Section {
                            objectName: "groupHeroCommands"
                            title: qsTr("Default Battle Commands")
                            enabled: !!root.heroes && root.hero >= 0 && root.hero < root.heroes.count
                            Layout.fillWidth: true

                            GridLayout {
                                columns: 2
                                rowSpacing: Ez.Metrics.rowSpacing
                                columnSpacing: Ez.Metrics.columnSpacing
                                Layout.fillWidth: true

                                Repeater {
                                    id: heroCommandRepeater
                                    model: 6

                                    Ez.ComboBox {
                                        id: heroCommand
                                        required property int index
                                        objectName: "comboHeroCommand" + (index + 1)
                                        model: root.commandChoices
                                        Layout.fillWidth: true
                                        // room for 8 characters and more, not the widest command
                                        implicitWidth: 130
                                        // the hero's command in this slot, <none> after their last
                                        readonly property int commandId: {
                                            root.heroRevision
                                            const ids = root.heroCommands()
                                            return index < ids.length ? ids[index] : 0
                                        }
                                        function show() {
                                            currentIndex = Math.max(0, indexOfValue(commandId))
                                        }
                                        onCommandIdChanged: show()
                                        onCountChanged: show()
                                        Component.onCompleted: show()
                                        onActivated: root.writeHeroCommands()
                                    }
                                }
                                Ez.Label {
                                    objectName: "labelHeroRowCommand"
                                    text: qsTr("Row")
                                    font: Ez.Metrics.font
                                    Controls.ToolTip.visible: rowHover.hovered
                                    Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                                    Controls.ToolTip.text: qsTr("The last command is always Row")
                                    HoverHandler {
                                        id: rowHover
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Ez.Section {
                objectName: "groupPlacement"
                title: qsTr("Character Placement")

                Ez.RadioGroup {
                    id: placement
                    jsonData: root.database
                    key: root.battle + "placement"
                    choices: [
                        { name: "radioPlacementManual", text: qsTr("Manual") },
                        { name: "radioPlacementAutomatic", text: qsTr("Automatic") },
                    ]
                }
            }
        }

        // the battle and its commands
        ColumnLayout {
            spacing: Ez.Metrics.sectionSpacing
            Layout.alignment: Qt.AlignTop
            Layout.fillHeight: true

            Ez.Section {
                objectName: "groupBattleType"
                title: qsTr("Battle Type")
                Ez.RadioGroup {
                    jsonData: root.database
                    key: root.battle + "battle_type"
                    choices: [
                        { name: "radioTraditional", text: qsTr("Traditional") },
                        { name: "radioAlternative", text: qsTr("Alternative") },
                        { name: "radioGauge", text: qsTr("Gauge") },
                    ]
                }
            }

            Ez.Section {
                objectName: "groupWindow"
                title: qsTr("Window Characteristics")
                Ez.RadioGroup {
                    jsonData: root.database
                    key: root.battle + "window_size"
                    choices: [
                        { name: "radioWindowLarge", text: qsTr("Large Window") },
                        { name: "radioWindowSmall", text: qsTr("Small Window") },
                    ]
                }
                Ez.CheckBox {
                    objectName: "checkTransparent"
                    text: qsTr("Transparent")
                    jsonData: root.database
                    key: root.battle + "transparency"
                }
            }

            Ez.Section {
                objectName: "groupCommands"
                title: qsTr("Battle Commands")
                Layout.fillHeight: true

                Ez.ColumnList {
                    listName: "listCommands"
                    model: root.commands
                    columns: [{ role: "text" }]
                    currentRow: root.commands ? Math.min(root.command, root.commands.count - 1) : -1
                    visibleLines: 10
                    onPicked: row => root.command = row
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredWidth: 180
                }
                RowLayout {
                    spacing: Ez.Metrics.columnSpacing
                    Layout.fillWidth: true

                    // commands are referenced by their ID: only the last one
                    // can go, like shrinking the list in RPG Maker
                    Ez.Button {
                        objectName: "pushAddCommand"
                        text: qsTr("Add")
                        Layout.fillWidth: true
                        onClicked: {
                            root.commands.resize(root.commands.count + 1)
                            root.command = root.commands.count - 1
                        }
                    }
                    Ez.Button {
                        objectName: "pushRemoveCommand"
                        text: qsTr("Remove")
                        enabled: !!root.commands && root.commands.count > 1
                        Layout.fillWidth: true
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                        Controls.ToolTip.text: qsTr("Removes the last command")
                        onClicked: {
                            root.commands.resize(root.commands.count - 1)
                            root.command = Math.min(root.command, root.commands.count - 1)
                        }
                    }
                }
                Ez.Form {
                    readonly property string prefix: root.battle + "commands/" + root.command + "/"
                    readonly property bool valid: !!root.commands && root.command >= 0 && root.command < root.commands.count

                    Ez.FormLabel {
                        objectName: "labelCommandName"
                        text: qsTr("Name")
                    }
                    Ez.TextField {
                        objectName: "lineCommandName"
                        jsonData: root.database
                        key: parent.prefix + "name"
                        enabled: parent.valid
                        limit: 10
                        Layout.fillWidth: true
                    }
                    Ez.FormLabel {
                        objectName: "labelCommandType"
                        text: qsTr("Archetype")
                    }
                    Ez.ComboBox {
                        objectName: "comboCommandType"
                        jsonData: root.database
                        key: parent.prefix + "type"
                        enabled: parent.valid
                        model: [
                            { text: qsTr("Attack"), value: 0 },
                            { text: qsTr("Skill"), value: 1 },
                            { text: qsTr("Skill Subset"), value: 2 },
                            { text: qsTr("Defend"), value: 3 },
                            { text: qsTr("Item"), value: 4 },
                            { text: qsTr("Escape"), value: 5 },
                            { text: qsTr("Link to Event"), value: 6 },
                        ]
                    }
                }
            }
        }

        // a random battle lost
        ColumnLayout {
            spacing: Ez.Metrics.sectionSpacing
            Layout.alignment: Qt.AlignTop

            Ez.Section {
                objectName: "groupDeathHandler"
                title: qsTr("Random Encounter Death Handler")

                Ez.RadioGroup {
                    id: deathHandler
                    jsonData: root.database
                    key: root.battle + "death_handler"
                    choices: [
                        { name: "radioDeathGameOver", text: qsTr("Game Over") },
                        { name: "radioDeathEvent", text: qsTr("Call Common Event") },
                    ]
                    // RPG Maker writes both chunks; RPG_RT only reads death_handler
                    onActivated: value => root.database.set(root.battle + "death_handler_unused", value === 1)
                }
                Ez.RefComboBox {
                    objectName: "comboDeathEvent"
                    database: root.database
                    listKey: "commonevents"
                    jsonData: root.database
                    key: root.battle + "death_event"
                    pickable: true
                    enabled: deathHandler.value === 1
                    Layout.fillWidth: true
                }

                Ez.Section {
                    objectName: "groupDeathTeleport"
                    title: qsTr("Teleport Before Executing Handler")
                    enabled: deathHandler.value === 1

                    Ez.Form {
                        Ez.CheckBox {
                            id: deathTeleport
                            objectName: "checkDeathTeleport"
                            text: qsTr("Enable")
                            jsonData: root.database
                            key: root.battle + "death_teleport"
                            Layout.columnSpan: 2
                        }
                        Ez.FormLabel {
                            objectName: "labelDeathMap"
                            text: qsTr("Map")
                            enabled: deathTeleport.checked
                        }
                        Ez.ComboBox {
                            objectName: "comboDeathMap"
                            jsonData: root.database
                            key: root.battle + "death_teleport_id"
                            enabled: deathTeleport.checked
                            model: root.mapChoices
                            Layout.fillWidth: true
                        }
                        Ez.FormLabel {
                            objectName: "labelDeathX"
                            text: qsTr("X Coordinate")
                            enabled: deathTeleport.checked
                        }
                        Ez.SpinBox {
                            objectName: "spinDeathX"
                            jsonData: root.database
                            key: root.battle + "death_teleport_x"
                            from: 0
                            to: 9999
                            enabled: deathTeleport.checked
                        }
                        Ez.FormLabel {
                            objectName: "labelDeathY"
                            text: qsTr("Y Coordinate")
                            enabled: deathTeleport.checked
                        }
                        Ez.SpinBox {
                            objectName: "spinDeathY"
                            jsonData: root.database
                            key: root.battle + "death_teleport_y"
                            from: 0
                            to: 9999
                            enabled: deathTeleport.checked
                        }
                        Ez.FormLabel {
                            objectName: "labelDeathFacing"
                            text: qsTr("Facing")
                            enabled: deathTeleport.checked
                        }
                        Ez.ComboBox {
                            objectName: "comboDeathFacing"
                            jsonData: root.database
                            key: root.battle + "death_teleport_face"
                            enabled: deathTeleport.checked
                            model: [
                                { text: qsTr("Retain Facing"), value: 0 },
                                { text: qsTr("Up"), value: 1 },
                                { text: qsTr("Right"), value: 2 },
                                { text: qsTr("Down"), value: 3 },
                                { text: qsTr("Left"), value: 4 },
                            ]
                        }
                    }
                }
            }
        }
    }

    /** The maps of the project ("0001: Field"), the one chosen kept when it is gone */
    readonly property var mapChoices: {
        const maps = []
        const tree = Ez.ProjectData.treeMap()
        const list = tree ? tree.list("maps") : null
        for (let i = 0; list && i < list.count; ++i) {
            const id = list.num(i + "/ID")
            if (id > 0) {
                maps.push({ text: String(id).padStart(4, "0") + ": " + list.str(i + "/name"), value: id })
            }
        }
        const current = database ? database.num(battle + "death_teleport_id") : 0
        if (!maps.some(map => map.value === current)) {
            maps.push({ text: qsTr("Unknown Map (%1)").arg(current), value: current })
        }
        return maps
    }
}
