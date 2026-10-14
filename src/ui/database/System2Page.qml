// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "System2Widget"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * The System 2 page of the database: the commands of the main menu and the
 * frame, battle graphics and title screen of RPG Maker 2003; the battle
 * tests, their party (up to four heroes with their level and equipment;
 * the members without a hero are left out), formation, condition and
 * background.
 */
DatabaseEntryPage {
    id: root

    readonly property var system: database ? database.subtree("system") : null
    readonly property int maxLevel: rpg2k3 ? 99 : 50

    single: true
    fillWidth: true

    /** The commands of the main menu: as many as were chosen, in order */
    readonly property var menuCommands: menuField.valid && menuField.value ? Array.from(menuField.value) : []
    readonly property var commandChoices: [qsTr("<none>"), qsTr("Item"), qsTr("Skill"), qsTr("Equipment"), qsTr("Save"),
                                           qsTr("Status"), qsTr("Row"), qsTr("Order"), qsTr("Wait")]
        .map((text, i) => ({ text: text, value: i }))
    function setCommand(slot, id) {
        const ids = []
        for (let i = 0; i < 8; ++i) {
            const command = i === slot ? id : Number(menuCommands[i] ?? 0)
            if (command > 0) {
                ids.push(command)
            }
        }
        menuField.value = ids
    }
    Ez.JsonField {
        id: menuField
        source: root.system
        path: "menu_commands"
    }

    /** The party of the battle tests (a TestBattler each), as the rows show it */
    readonly property var battlers: system ? system.list("battletest_data") : null
    readonly property var equipmentKeys: ["weapon_id", "shield_id", "armor_id", "helmet_id", "accessory_id"]
    readonly property var actors: database ? database.choices("actors", qsTranslate("SystemFields", "<none>"), 0) : null
    // the rows read again when the party changes
    property int partyRevision: 0
    Connections {
        target: root.battlers
        ignoreUnknownSignals: true
        function onDataChanged() { root.partyRevision++ }
        function onCountChanged() { root.partyRevision++ }
    }
    /** A member of the party: {hero, level, weapon_id...}, a member without a hero past its end */
    function member(row) {
        const shown = { hero: 0, level: 1 }
        for (const key of equipmentKeys) {
            shown[key] = 0
        }
        if (battlers && partyRevision >= 0 && row < battlers.count) {
            shown.hero = battlers.num(row + "/actor_id")
            shown.level = battlers.num(row + "/level")
            for (const key of equipmentKeys) {
                shown[key] = battlers.num(row + "/" + key)
            }
        }
        return shown
    }
    /** A field of a member changed: the party again, without the members without a hero */
    function setMember(row, field, value) {
        const members = []
        for (let i = 0; i < 4; ++i) {
            const shown = member(i)
            if (i === row) {
                shown[field] = value
            }
            if (shown.hero > 0) {
                members.push(shown)
            }
        }
        system.beginBatch()
        battlers.resize(members.length)
        for (let i = 0; i < members.length; ++i) {
            battlers.set(i + "/actor_id", members[i].hero)
            battlers.set(i + "/level", members[i].level)
            for (const key of equipmentKeys) {
                battlers.set(i + "/" + key, members[i][key])
            }
        }
        system.endBatch()
    }
    /** The items of a type (1 weapon ... 5 accessory), "<none>" first */
    function equipment(type) {
        const choices = [{ text: qsTranslate("SystemFields", "<none>"), value: 0 }]
        const items = database ? database.list("items") : null
        for (let row = 0; items && row < items.count; ++row) {
            if (items.num(row + "/type") === type) {
                choices.push({ text: String(row + 1).padStart(4, "0") + ": " + items.str(row + "/name"), value: row + 1 })
            }
        }
        return choices
    }
    readonly property var equipmentChoices: [1, 2, 3, 4, 5].map(type => equipment(type))

    /**
     * A combo box of an entry of a list (a menu command, a member of the
     * party): shown again once its choices are read (after the combo box's
     * own choice of a new list) or the entry changes; chosen(value) by the user
     */
    component ListBox: Ez.ComboBox {
        property int shownValue
        signal chosen(int value)
        function show() {
            currentIndex = count > 0 ? Math.max(0, indexOfValue(shownValue)) : -1
        }
        onCountChanged: Qt.callLater(show)
        onShownValueChanged: show()
        onActivated: chosen(Number(currentValue))
    }
    /** A combo box of a member of the party */
    component PartyBox: ListBox {
        property int row
        property string field
        shownValue: Number(root.member(row)[field] ?? 0)
        onChosen: (value) => root.setMember(row, field, value)
        // the columns share the row: a long name is cut
        implicitWidth: 60
        Layout.fillWidth: true
        Layout.preferredWidth: 1
        Layout.minimumWidth: 60
    }

    RowLayout {
        id: top
        spacing: Ez.Metrics.sectionSpacing
        Layout.fillWidth: true

        // RPG Maker 2003: the commands of the main menu, in order, then "Quit Game"
        Ez.Section {
            title: qsTr("Menu Commands")
            enabled: root.rpg2k3
            Layout.alignment: Qt.AlignTop
            Layout.fillHeight: true
            // half of the row each, wider when its fields need it (the page then scrolls)
            Layout.preferredWidth: Math.max(implicitWidth, (root.column.room - top.spacing) / 2)
            GridLayout {
                // read down the first column, then the second
                flow: GridLayout.TopToBottom
                rows: 5
                rowSpacing: Ez.Metrics.rowSpacing
                columnSpacing: Ez.Metrics.columnSpacing
                Layout.fillWidth: true

                Repeater {
                    model: 8
                    ListBox {
                        id: command
                        required property int index
                        objectName: "comboMenuCommand" + (index + 1)
                        model: root.commandChoices
                        shownValue: Number(root.menuCommands[index] ?? 0)
                        onChosen: (value) => root.setCommand(command.index, value)
                        Layout.row: index % 4
                        Layout.column: Math.floor(index / 4)
                        Layout.fillWidth: true
                    }
                }
                Ez.Label {
                    text: qsTr("Quit Game")
                    font: Ez.Metrics.font
                    leftPadding: Ez.Metrics.columnSpacing
                    Layout.row: 4
                    Layout.column: 1
                }
            }
        }

        // RPG Maker 2003: frame, battle graphics and title
        Ez.Section {
            title: qsTr("Graphics")
            enabled: root.rpg2k3
            Layout.alignment: Qt.AlignTop
            Layout.fillHeight: true
            // half of the row each, wider when its fields need it (the page then scrolls)
            Layout.preferredWidth: Math.max(implicitWidth, (root.column.room - top.spacing) / 2)
            Ez.Form {
                Ez.CheckBox {
                    objectName: "checkShowFrame"
                    text: qsTr("Use Frame")
                    jsonData: root.system
                    key: "show_frame"
                    Layout.columnSpan: 2
                }
                Ez.FormLabel {
                    text: qsTr("Frame")
                }
                Ez.FileField {
                    fieldName: "lineFrame"
                    jsonData: root.system
                    key: "frame_name"
                    folder: "Frame"
                }
                Ez.FormLabel {
                    text: qsTr("Battle Graphics")
                }
                Ez.FileField {
                    fieldName: "lineSystem2"
                    jsonData: root.system
                    key: "system2_name"
                    folder: "System2"
                }
                Ez.CheckBox {
                    objectName: "checkInvertAnimations"
                    text: qsTr("Reverse Battle Animations when Attacked from Behind")
                    jsonData: root.system
                    key: "invert_animations"
                    Layout.columnSpan: 2
                }
                // RPG Maker 2003 1.11: without title screen, the game starts on the first map
                Ez.CheckBox {
                    objectName: "checkShowTitle"
                    text: qsTr("Show Title Screen")
                    jsonData: root.system
                    key: "show_title"
                    Layout.columnSpan: 2
                }
            }
        }
    }

    // the battle tests: the party, then the battle
    Ez.Section {
        title: qsTr("Test Battle")
        GridLayout {
            columns: 7
            rowSpacing: Ez.Metrics.rowSpacing
            columnSpacing: Ez.Metrics.columnSpacing
            Layout.fillWidth: true

            Repeater {
                model: [qsTr("Hero"), qsTr("Level"), qsTr("Weapon"), qsTr("Shield"), qsTr("Armor"), qsTr("Helmet"),
                        qsTr("Accessory")]
                Ez.FormLabel {
                    required property string modelData
                    required property int index
                    text: modelData
                    // the columns of the combo boxes are as wide as each other, whatever their title
                    Layout.fillWidth: index !== 1
                    Layout.preferredWidth: index !== 1 ? 1 : -1
                }
            }
            Repeater {
                model: 4 * 7
                Loader {
                    id: cell
                    required property int index
                    readonly property int row: Math.floor(index / 7)
                    readonly property int column: index % 7
                    Layout.fillWidth: column !== 1
                    Layout.preferredWidth: column !== 1 ? 1 : -1
                    sourceComponent: column === 0 ? heroBox : column === 1 ? levelBox : equipmentBox

                    Component {
                        id: heroBox
                        PartyBox {
                            objectName: "comboTestHero" + (cell.row + 1)
                            row: cell.row
                            field: "hero"
                            model: root.actors
                        }
                    }
                    Component {
                        id: levelBox
                        Ez.SpinBox {
                            objectName: "spinTestLevel" + (cell.row + 1)
                            from: 1
                            to: root.maxLevel
                            value: root.member(cell.row).level
                            onValueModified: root.setMember(cell.row, "level", value)
                        }
                    }
                    Component {
                        id: equipmentBox
                        PartyBox {
                            objectName: "comboTest" + ["Weapon", "Shield", "Armor", "Helmet", "Accessory"][cell.column - 2]
                                        + (cell.row + 1)
                            row: cell.row
                            field: root.equipmentKeys[cell.column - 2]
                            model: root.equipmentChoices[cell.column - 2]
                        }
                    }
                }
            }
        }
        Ez.Form {
            // formations and conditions are RPG Maker 2003 battles
            Ez.FormLabel {
                text: qsTr("Party Formation")
                enabled: root.rpg2k3
            }
            RowLayout {
                spacing: Ez.Metrics.columnSpacing
                Ez.ComboBox {
                    id: formation
                    objectName: "comboFormation"
                    jsonData: root.system
                    key: "battletest_formation"
                    enabled: root.rpg2k3
                    // liblcf's values: 0 the terrain, 1 loose, 2 tight
                    model: [{ text: qsTr("Use a Loose Formation"), value: 1 }, { text: qsTr("Use a Tight Formation"), value: 2 },
                            { text: qsTr("Use the following Terrain:"), value: 0 }]
                }
                Ez.RefComboBox {
                    objectName: "comboTerrain"
                    database: root.database
                    listKey: "terrains"
                    jsonData: root.system
                    key: "battletest_terrain"
                    pickable: true
                    enabled: root.rpg2k3 && formation.currentIndex >= 0 && Number(formation.currentValue) === 0
                    Layout.fillWidth: true
                }
            }
            Ez.FormLabel {
                text: qsTr("Special Battle Condition")
                enabled: root.rpg2k3
            }
            Ez.ComboBox {
                objectName: "comboCondition"
                jsonData: root.system
                key: "battletest_condition"
                enabled: root.rpg2k3
                // liblcf's values: 0 none, 1 initiative, 2 back, 3 surround, 4 pincers
                model: [{ text: qsTr("No Special Conditions"), value: 0 }, { text: qsTr("Back Attack"), value: 2 },
                        { text: qsTr("Surround Attack"), value: 3 }, { text: qsTr("Pincers Attack"), value: 4 },
                        { text: qsTr("Initiative"), value: 1 }]
                Layout.fillWidth: true
            }
            Ez.FormLabel {
                text: qsTr("Background")
            }
            Ez.FileField {
                fieldName: "lineTestBackground"
                jsonData: root.system
                key: "battletest_background"
                folder: "Backdrop"
            }
        }
    }
}
