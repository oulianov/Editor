// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "ActorWidget"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * A hero of the database (Characters): its
 * name, title and levels, its initial equipment (the items it can use),
 * options and critical hits, its class (Apply copies the class's settings
 * to it), its graphics, parameter curves, skills, resistances, experience
 * curve, EasyRPG settings and battle commands. Classes, battle graphics and
 * battle commands are RPG Maker 2003's: greyed in a 2000 game.
 */
DatabaseEntryPage {
    id: root

    readonly property int maxLevel: rpg2k3 ? 99 : 50
    readonly property int maxHp: rpg2k3 ? 9999 : 999
    readonly property int entryId: entry ? entry.num("ID") : 0

    // the items read again when they change (a new weapon, who can use it...)
    property int itemsRevision: 0
    readonly property var items: database ? database.list("items") : null
    Connections {
        target: root.items
        ignoreUnknownSignals: true
        function onDataChanged() { root.itemsRevision++ }
        function onCountChanged() { root.itemsRevision++ }
    }

    /**
     * The items of a type the hero can use, for its initial equipment:
     * "(None)", then those whose list of heroes has it (an array shorter
     * than the hero lets it use the item)
     */
    function equipment(type) {
        const choices = [{ text: qsTranslate("DatabaseSplitWidget", "(None)"), value: 0 }]
        if (!items || itemsRevision < 0) {
            return choices
        }
        const index = entryId - 1
        for (let row = 0; row < items.count; ++row) {
            if (items.num(row + "/type") !== type) {
                continue
            }
            const set = items.value(row + "/actor_set")
            if (index >= 0 && set && index < set.length && !set[index]) {
                continue
            }
            choices.push({ text: String(row + 1).padStart(4, "0") + ": " + items.str(row + "/name"), value: row + 1 })
        }
        return choices
    }

    /**
     * Apply: what RPG Maker's help says the class gives the hero, its basic
     * statistics, experience curve, skills, resistances and battle commands
     */
    function applyClass() {
        const id = classField.valid ? Number(classField.value) : 0
        const classes = database ? database.list("classes") : null
        if (!entry || !classes || id < 1 || id > classes.count) {
            return
        }
        const cls = classes.entry(id - 1)
        entry.beginBatch()
        for (const key of ["maxhp", "maxsp", "attack", "defense", "spirit", "agility"]) {
            entry.set("parameters/" + key, cls.value("parameters/" + key))
        }
        for (const key of ["exp_base", "exp_inflation", "exp_correction", "state_ranks", "attribute_ranks",
                           "battle_commands"]) {
            entry.set(key, cls.value(key))
        }
        const skills = entry.list("skills")
        const learned = cls.list("skills")
        skills.removeEntries(0, skills.count)
        skills.insertEntries(0, learned.count)
        for (let i = 0; i < learned.count; ++i) {
            skills.set(i + "/level", learned.num(i + "/level"))
            skills.set(i + "/skill_id", learned.num(i + "/skill_id"))
        }
        entry.endBatch()
    }

    fillWidth: true

    Ez.JsonField {
        id: classField
        source: root.entry
        path: "class_id"
    }
    Ez.JsonField {
        id: battlerField
        source: root.entry
        path: "battler_animation"
    }

    /**
     * An item of the initial equipment: those of its type the hero can
     * equip, searched by number or name; "..." picks it from all the items
     */
    component Equipment: Ez.RefComboBox {
        /** Its field in initial_equipment ("weapon_id") */
        property string slot
        /** The type of the items (1: weapons...) */
        property int type

        database: root.database
        listKey: "items"
        noneText: qsTranslate("DatabaseSplitWidget", "(None)")
        choices: root.equipment(type)
        jsonData: root.entry
        key: "initial_equipment/" + slot
        // an item the hero cannot equip (or none of this type) reads "(None)"
        fallbackValue: 0
        pickable: true
        Layout.fillWidth: true
    }

    GridLayout {
        columns: 5
        rowSpacing: Ez.Metrics.sectionSpacing
        columnSpacing: Ez.Metrics.sectionSpacing
        Layout.fillWidth: true

        Ez.Section {
            objectName: "groupBox_16"
            title: qsTr("Name")
            Layout.row: 0
            Layout.column: 0
            Ez.TextField {
                objectName: "lineName"
                jsonData: root.entry
                key: "name"
                limit: 12
                Layout.fillWidth: true
            }
        }
        Ez.Section {
            objectName: "groupBox_366"
            title: qsTr("Title")
            Layout.row: 0
            Layout.column: 1
            Ez.TextField {
                objectName: "lineTitle"
                jsonData: root.entry
                key: "title"
                limit: 12
                Layout.fillWidth: true
            }
        }
        Ez.Section {
            objectName: "groupBox_367"
            title: qsTr("Min Lv")
            Layout.row: 0
            Layout.column: 2
            Ez.SpinBox {
                id: minLevel
                objectName: "spinMinLv"
                jsonData: root.entry
                key: "initial_level"
                from: 1
                to: root.maxLevel
                Layout.fillWidth: true
                // the final level is never below it
                onValueModified: {
                    if (root.entry && root.entry.num("final_level") < value) {
                        root.entry.set("final_level", value)
                    }
                }
            }
        }
        Ez.Section {
            objectName: "groupBox_368"
            title: qsTr("Initial Equipment")
            Layout.row: 0
            Layout.column: 3
            Layout.columnSpan: 2
            Layout.rowSpan: 4
            Layout.fillHeight: true
            Ez.Form {
                Ez.FormLabel {
                    text: qsTr("Weapon:")
                }
                Equipment {
                    objectName: "comboInitialWeapon"
                    slot: "weapon_id"
                    type: 1
                }
                Ez.FormLabel {
                    text: qsTr("Shield:")
                }
                Equipment {
                    objectName: "comboInitialShield"
                    slot: "shield_id"
                    type: 2
                }
                Ez.FormLabel {
                    text: qsTr("Armor:")
                }
                Equipment {
                    objectName: "comboInitialArmor"
                    slot: "armor_id"
                    type: 3
                }
                Ez.FormLabel {
                    text: qsTr("Helmet")
                }
                Equipment {
                    objectName: "comboInitialHelmet"
                    slot: "helmet_id"
                    type: 4
                }
                Ez.FormLabel {
                    text: qsTr("Other:")
                }
                Equipment {
                    objectName: "comboInitialMisc"
                    slot: "accessory_id"
                    type: 5
                }
            }
        }

        Ez.Section {
            objectName: "groupBox_369"
            title: qsTr("Options")
            Layout.row: 1
            Layout.column: 0
            Layout.rowSpan: 2
            Layout.fillHeight: true
            GridLayout {
                columns: 2
                rowSpacing: Ez.Metrics.rowSpacing
                columnSpacing: Ez.Metrics.columnSpacing
                Ez.CheckBox {
                    objectName: "checkDualWeapon"
                    text: qsTr("Dual Arms")
                    jsonData: root.entry
                    key: "two_weapon"
                }
                Ez.CheckBox {
                    objectName: "checkAI"
                    text: qsTr("AI")
                    jsonData: root.entry
                    key: "auto_battle"
                }
                Ez.CheckBox {
                    objectName: "checkFixedEquip"
                    text: qsTr("Fixed Equip")
                    jsonData: root.entry
                    key: "lock_equipment"
                }
                Ez.CheckBox {
                    objectName: "checkStrongDefense"
                    text: qsTr("Strg Def")
                    jsonData: root.entry
                    key: "super_guard"
                }
            }
        }
        Ez.Section {
            title: qsTr("Critical Chance")
            checkable: true
            checkName: "groupCritChance"
            jsonData: root.entry
            key: "critical_hit"
            Layout.row: 1
            Layout.column: 1
            Ez.SpinBox {
                objectName: "spinCritChance"
                jsonData: root.entry
                key: "critical_hit_chance"
                from: 1
                to: 100
                prefix: qsTr("1 in ")
                suffix: qsTr(" turns")
                Layout.fillWidth: true
            }
        }
        Ez.Section {
            objectName: "groupBox_370"
            title: qsTr("Max Lv")
            Layout.row: 1
            Layout.column: 2
            Ez.SpinBox {
                objectName: "spinMaxLv"
                jsonData: root.entry
                key: "final_level"
                from: 1
                to: root.maxLevel
                Layout.fillWidth: true
                // the initial level is never above it
                onValueModified: {
                    if (root.entry && root.entry.num("initial_level") > value) {
                        root.entry.set("initial_level", value)
                    }
                }
            }
        }

        Ez.Section {
            objectName: "groupBoxClass"
            title: qsTr("Class")
            Layout.row: 2
            Layout.column: 1
            Layout.columnSpan: 2
            Layout.rowSpan: 2
            Layout.fillHeight: true
            enabled: root.rpg2k3
            RowLayout {
                spacing: Ez.Metrics.columnSpacing
                Ez.ComboBox {
                    objectName: "comboProfession"
                    jsonData: root.entry
                    key: "class_id"
                    model: root.database ? root.database.choices("classes", qsTr("<none>"), 0) : null
                    Layout.fillWidth: true
                }
                Ez.Button {
                    objectName: "pushApplyProfession"
                    text: qsTr("Apply")
                    enabled: classField.valid && Number(classField.value) > 0
                    onClicked: root.applyClass()
                }
            }
        }

        Ez.Section {
            objectName: "groupBox_372"
            title: qsTr("Hero Graphics")
            Layout.row: 3
            Layout.column: 0
            Layout.rowSpan: 3
            Layout.fillHeight: true

            Ez.Label {
                objectName: "label_46"
                text: qsTr("Face")
                font: Ez.Metrics.font
            }
            Ez.SpriteField {
                objectName: "graphicsFaceset"
                preview: "FaceSet"
                jsonData: root.entry
                key: "face_name"
                indexKey: "face_index"
                zoom: 2
            }
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 1
                color: Ez.Metrics.sectionBorderColor
            }
            Ez.Label {
                objectName: "label_47"
                text: qsTr("Charset")
                font: Ez.Metrics.font
            }
            Ez.SpriteField {
                objectName: "graphicsCharset"
                preview: "CharSet"
                jsonData: root.entry
                key: "character_name"
                indexKey: "character_index"
                transparent: translucent.checked
            }
            Ez.CheckBox {
                id: translucent
                objectName: "checkTranslucent"
                text: qsTr("Translucent")
                jsonData: root.entry
                key: "transparent"
            }
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 1
                color: Ez.Metrics.sectionBorderColor
            }
            Ez.Label {
                objectName: "label_48"
                text: qsTr("Battleset")
                font: Ez.Metrics.font
                enabled: root.rpg2k3
            }
            BattlerView {
                objectName: "graphicsBattleset"
                database: root.database
                battler: battlerField.valid ? Number(battlerField.value) : 0
                enabled: root.rpg2k3
            }
            Ez.ComboBox {
                objectName: "comboBattleset"
                jsonData: root.entry
                key: "battler_animation"
                model: root.database ? root.database.choices("battleranimations", qsTr("<none>"), 0) : null
                enabled: root.rpg2k3
                Layout.fillWidth: true
            }
        }
        Ez.Section {
            objectName: "groupBox_242"
            title: qsTr("Curves")
            Layout.row: 4
            Layout.column: 1
            Layout.columnSpan: 2
            Layout.rowSpan: 2
            Layout.fillHeight: true
            ParameterGraphs {
                entry: root.entry
                maxLevel: root.maxLevel
                maxHp: root.maxHp
                graphHeight: 60
                names: ["graphicsHp", "graphicsMp", "graphicsAtt", "graphicsDef", "graphicsInt", "graphicsAgy"]
                titles: [qsTr("Hit Points"), qsTr("Magic Points"), qsTr("Attack"), qsTr("Defense"),
                         qsTr("Intelligence"), qsTr("Agility")]
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }
        Ez.Section {
            objectName: "groupBox_373"
            title: qsTr("Unnarmed animation")
            Layout.row: 4
            Layout.column: 3
            Layout.columnSpan: 2
            Ez.RefComboBox {
                objectName: "comboUnarmedAnimation"
                database: root.database
                listKey: "animations"
                jsonData: root.entry
                key: "unarmed_animation"
                pickable: true
                Layout.fillWidth: true
            }
        }
        Ez.Section {
            objectName: "groupBox_375"
            title: qsTr("Skill Progression")
            Layout.row: 5
            Layout.column: 3
            Layout.columnSpan: 2
            Layout.fillHeight: true
            SkillTable {
                objectName: "tableSkills"
                entry: root.entry
                database: root.database
                maxLevel: root.maxLevel
                visibleRows: 5
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }

        ExperienceSummary {
            objectName: "groupBox_378"
            entry: root.entry
            database: root.database
            horizontal: true
            Layout.row: 6
            Layout.column: 0
            Layout.columnSpan: 3
        }
        Ez.Section {
            objectName: "groupBox_376"
            title: qsTr("Status Ranks")
            Layout.row: 6
            Layout.column: 3
            Layout.rowSpan: 2
            Layout.fillHeight: true
            RankTable {
                objectName: "listStatusRanks"
                entry: root.entry
                key: "state_ranks"
                names: root.database ? root.database.list("states") : null
                visibleRows: 4
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }
        Ez.Section {
            objectName: "groupBox_377"
            title: qsTr("Attribute Ranks")
            Layout.row: 6
            Layout.column: 4
            Layout.rowSpan: 2
            Layout.fillHeight: true
            RankTable {
                objectName: "listAttributeRanks"
                entry: root.entry
                key: "attribute_ranks"
                names: root.database ? root.database.list("attributes") : null
                visibleRows: 4
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }
        Ez.Section {
            objectName: "groupBox_379"
            title: qsTr("EasyRPG Properties")
            Layout.row: 7
            Layout.column: 0
            Layout.columnSpan: 3
            RowLayout {
                spacing: Ez.Metrics.columnSpacing
                Ez.Note {
                    objectName: "labelEasyRpg"
                    text: qsTr("Settings only EasyRPG Player reads")
                }
                Ez.Button {
                    id: editCustom
                    objectName: "pushEditCustom"
                    text: qsTr("Edit...")
                    onClicked: Ez.Editor.exec("ActorEasyRpgDialog", { entry: root.entry }, editCustom)
                }
            }
        }

        BattleCommandBoxes {
            entry: root.entry
            database: root.database
            columns: 3
            enabled: root.rpg2k3
            Layout.row: 8
            Layout.column: 0
            Layout.columnSpan: 5
        }
    }
}
