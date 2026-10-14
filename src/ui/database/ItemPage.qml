// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "ItemWidget"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * An item of the database, as RPG Maker 2003 shows it: its name, type,
 * description, price and uses above, then the settings of its type (a
 * weapon's attack and options, the statistics equipment changes, what
 * medicine heals, what a seed raises, the skill a book teaches or a scroll
 * invokes, the switch of a switch item), the attributes and conditions it
 * gives or resists, and who can use it: the heroes, or (RPG Maker 2003)
 * the classes, a choice of the whole database.
 */
DatabaseEntryPage {
    id: root

    readonly property int type: typeField.valid ? Number(typeField.value) : 0
    readonly property bool weapon: type === 1
    readonly property bool armor: type >= 2 && type <= 5
    readonly property bool equipment: weapon || armor
    readonly property bool medicine: type === 6
    readonly property bool book: type === 7
    readonly property bool special: type === 9
    readonly property bool switchItem: type === 10
    /** Equipment is used up only by its skill: it invokes one when told to */
    readonly property bool invokes: !equipment || (useSkillField.valid && !!useSkillField.value)
    /** RPG Maker 2003 can let classes use the items, instead of heroes */
    readonly property bool byClass: rpg2k3 && userTypeField.valid && Number(userTypeField.value) === 1

    fillWidth: true

    Ez.JsonField {
        id: typeField
        source: root.entry
        path: "type"
    }
    Ez.JsonField {
        id: useSkillField
        source: root.entry
        path: "use_skill"
    }
    Ez.JsonField {
        id: userTypeField
        source: root.database
        path: "system/equipment_setting"
    }

    /** A number of the item, with its label in a form */
    component Amount: Ez.SpinBox {
        jsonData: root.entry
        Layout.fillWidth: true
    }
    /** An option of the item */
    component Option: Ez.CheckBox {
        jsonData: root.entry
    }

    // the name, type, description, price and uses
    GridLayout {
        columns: 4
        rowSpacing: Ez.Metrics.rowSpacing
        columnSpacing: Ez.Metrics.columnSpacing
        Layout.fillWidth: true

        Ez.FormLabel {
            text: qsTr("Name")
        }
        Ez.TextField {
            objectName: "itemName"
            jsonData: root.entry
            key: "name"
            limit: 20
            Layout.fillWidth: true
            Layout.horizontalStretchFactor: 1
        }
        Ez.FormLabel {
            text: qsTr("Type")
        }
        Ez.ComboBox {
            objectName: "itemType"
            jsonData: root.entry
            key: "type"
            model: [qsTr("Common Item"), qsTr("Weapon"), qsTr("Shield"), qsTr("Armor"), qsTr("Helmet"), qsTr("Accessory"),
                    qsTr("Medicine"), qsTr("Skill Book"), qsTr("Seed"), qsTr("Skill Scroll"), qsTr("Switch")]
                .map((text, i) => ({ text: text, value: i }))
            Layout.fillWidth: true
            Layout.horizontalStretchFactor: 1
        }
        Ez.FormLabel {
            text: qsTr("Description")
        }
        Ez.TextField {
            objectName: "itemDescription"
            jsonData: root.entry
            key: "description"
            limit: 50
            Layout.columnSpan: 3
            Layout.fillWidth: true
        }
        Ez.FormLabel {
            text: qsTr("Price")
        }
        Amount {
            objectName: "itemPrice"
            key: "price"
            from: 0
            to: 999999
            Layout.horizontalStretchFactor: 1
        }
        // equipment is used up only by its skill
        Ez.FormLabel {
            text: qsTr("Number of Uses")
            visible: root.type >= 6
        }
        Ez.ComboBox {
            objectName: "itemUses"
            jsonData: root.entry
            key: "uses"
            visible: root.type >= 6
            model: [qsTr("Unlimited Use"), qsTr("One Use (Normal)"), qsTr("Two Uses"), qsTr("Three Uses"),
                    qsTr("Four Uses"), qsTr("Five Uses")].map((text, i) => ({ text: text, value: i }))
            Layout.fillWidth: true
        }
    }

    RowLayout {
        id: columns

        /** The two columns of lists share what the settings leave */
        readonly property real listWidth: Math.max(0, (root.column.room - first.implicitWidth - second.implicitWidth
                                                       - 3 * spacing) / 2)

        spacing: Ez.Metrics.sectionSpacing
        Layout.fillWidth: true
        Layout.fillHeight: true

        // the settings of the type
        ColumnLayout {
            id: first
            spacing: Ez.Metrics.sectionSpacing
            Layout.alignment: Qt.AlignTop

            Ez.Section {
                title: qsTr("Weapon")
                visible: root.weapon
                Ez.Form {
                    Ez.FormLabel {
                        text: qsTr("Hands Required")
                    }
                    Ez.RadioGroup {
                        jsonData: root.entry
                        key: "two_handed"
                        columns: 2
                        choices: [{ name: "radioOneHand", text: qsTr("One") }, { name: "radioBothHands", text: qsTr("Both") }]
                    }
                    Ez.FormLabel {
                        text: qsTr("MP Consumed")
                    }
                    Amount {
                        objectName: "spinMpCost"
                        key: "sp_cost"
                        from: 0
                        to: 999
                    }
                    Ez.FormLabel {
                        text: qsTr("Hit Probability")
                    }
                    Amount {
                        objectName: "spinHit"
                        key: "hit"
                        from: 0
                        to: 100
                        suffix: "%"
                    }
                    Ez.FormLabel {
                        text: qsTr("Critical Bonus")
                    }
                    Amount {
                        objectName: "spinCriticalBonus"
                        key: "critical_hit"
                        from: 0
                        to: 100
                        suffix: "%"
                    }
                }
            }
            Ez.Section {
                title: qsTr("Attack Animation")
                visible: root.weapon
                Ez.RefComboBox {
                    objectName: "comboAnimation"
                    database: root.database
                    listKey: "animations"
                    jsonData: root.entry
                    key: "animation_id"
                    pickable: true
                    Layout.fillWidth: true
                }
                Ez.Button {
                    id: battlerAnimation
                    objectName: "pushBattlerAnimation"
                    text: qsTr("Set...")
                    enabled: root.rpg2k3
                    Controls.ToolTip.visible: hovered
                    Controls.ToolTip.delay: 700
                    Controls.ToolTip.text: qsTr("How each hero attacks with the weapon (RPG Maker 2003 battles)")
                    onClicked: Ez.Editor.exec("BattlerAnimationDialog", { database: root.database, entry: root.entry,
                                                                          weapon: true }, battlerAnimation)
                }
            }

            // medicine
            Ez.Section {
                title: qsTr("Target")
                visible: root.medicine
                Ez.RadioGroup {
                    jsonData: root.entry
                    key: "entire_party"
                    columns: 2
                    choices: [{ name: "radioSingleAlly", text: qsTr("Single Ally") },
                              { name: "radioAllAllies", text: qsTr("All Allies") }]
                }
            }
            Ez.Section {
                title: qsTr("HP Recovery")
                visible: root.medicine
                Ez.Form {
                    Ez.FormLabel {
                        text: qsTr("Percentage of Max HP")
                    }
                    Amount {
                        objectName: "spinHpRate"
                        key: "recover_hp_rate"
                        from: 0
                        to: 100
                        suffix: "%"
                    }
                    Ez.FormLabel {
                        text: qsTr("Additional HP")
                    }
                    Amount {
                        objectName: "spinHp"
                        key: "recover_hp"
                        from: 0
                        to: 9999
                    }
                }
            }
            Ez.Section {
                title: qsTr("MP Recovery")
                visible: root.medicine
                Ez.Form {
                    Ez.FormLabel {
                        text: qsTr("Percentage of Max MP")
                    }
                    Amount {
                        objectName: "spinMpRate"
                        key: "recover_sp_rate"
                        from: 0
                        to: 100
                        suffix: "%"
                    }
                    Ez.FormLabel {
                        text: qsTr("Additional MP")
                    }
                    Amount {
                        objectName: "spinMp"
                        key: "recover_sp"
                        from: 0
                        to: 999
                    }
                }
            }
            Ez.Section {
                title: qsTr("Options")
                visible: root.medicine
                Option {
                    objectName: "checkFieldOnly"
                    text: qsTr("Only Usable on the Field Map")
                    key: "occasion_field1"
                }
                Option {
                    objectName: "checkDeadOnly"
                    text: qsTr("Only Usable on Dead Characters")
                    key: "ko_only"
                }
            }

            // seeds
            Ez.Section {
                title: qsTr("Base Statistic Change")
                visible: root.type === 8
                Ez.Form {
                    Ez.FormLabel {
                        text: qsTr("Maximum HP")
                    }
                    Amount {
                        objectName: "spinMaxHp"
                        key: "max_hp_points"
                        from: -9999
                        to: 9999
                    }
                    Ez.FormLabel {
                        text: qsTr("Maximum MP")
                    }
                    Amount {
                        objectName: "spinMaxMp"
                        key: "max_sp_points"
                        from: -999
                        to: 999
                    }
                    Ez.FormLabel {
                        text: qsTr("Attack")
                    }
                    Amount {
                        objectName: "spinSeedAttack"
                        key: "atk_points2"
                        from: -999
                        to: 999
                    }
                    Ez.FormLabel {
                        text: qsTr("Defense")
                    }
                    Amount {
                        objectName: "spinSeedDefense"
                        key: "def_points2"
                        from: -999
                        to: 999
                    }
                    Ez.FormLabel {
                        text: qsTr("Intelligence")
                    }
                    Amount {
                        objectName: "spinSeedIntelligence"
                        key: "spi_points2"
                        from: -999
                        to: 999
                    }
                    Ez.FormLabel {
                        text: qsTr("Agility")
                    }
                    Amount {
                        objectName: "spinSeedAgility"
                        key: "agi_points2"
                        from: -999
                        to: 999
                    }
                }
            }

            // switches
            Ez.Section {
                title: qsTr("Turn ON Switch")
                visible: root.switchItem
                Ez.RefComboBox {
                    objectName: "comboSwitch"
                    database: root.database
                    listKey: "switches"
                    jsonData: root.entry
                    key: "switch_id"
                    pickable: true
                    Layout.fillWidth: true
                }
            }
            Ez.Section {
                title: qsTr("Activation Allowed")
                visible: root.switchItem
                RowLayout {
                    spacing: 2 * Ez.Metrics.columnSpacing
                    Option {
                        objectName: "checkOccasionField"
                        text: qsTr("Field")
                        key: "occasion_field2"
                    }
                    Option {
                        objectName: "checkOccasionBattle"
                        text: qsTr("Battle")
                        key: "occasion_battle"
                    }
                }
            }
        }

        // what equipment changes, its options, its skill
        ColumnLayout {
            id: second
            spacing: Ez.Metrics.sectionSpacing
            Layout.alignment: Qt.AlignTop

            Ez.Section {
                title: qsTr("Statistic Change")
                visible: root.equipment
                Ez.Form {
                    Ez.FormLabel {
                        text: qsTr("Attack")
                    }
                    Amount {
                        objectName: "spinAttack"
                        key: "atk_points1"
                        from: -999
                        to: 999
                    }
                    Ez.FormLabel {
                        text: qsTr("Defense")
                    }
                    Amount {
                        objectName: "spinDefense"
                        key: "def_points1"
                        from: -999
                        to: 999
                    }
                    Ez.FormLabel {
                        text: qsTr("Intelligence")
                    }
                    Amount {
                        objectName: "spinIntelligence"
                        key: "spi_points1"
                        from: -999
                        to: 999
                    }
                    Ez.FormLabel {
                        text: qsTr("Agility")
                    }
                    Amount {
                        objectName: "spinAgility"
                        key: "agi_points1"
                        from: -999
                        to: 999
                    }
                }
            }
            Ez.Section {
                title: qsTr("Options")
                visible: root.weapon
                Option {
                    objectName: "checkPreemptive"
                    text: qsTr("Always Attack First")
                    key: "preemptive"
                }
                Option {
                    objectName: "checkDualAttack"
                    text: qsTr("Attack Twice")
                    key: "dual_attack"
                }
                Option {
                    objectName: "checkAttackAll"
                    text: qsTr("Attack All Enemies")
                    key: "attack_all"
                }
                Option {
                    objectName: "checkIgnoreEvasion"
                    text: qsTr("Ignore Enemy Evasion")
                    key: "ignore_evasion"
                }
                Option {
                    objectName: "checkWeaponCursed"
                    text: qsTr("Cursed")
                    key: "cursed"
                    enabled: root.rpg2k3
                }
            }
            Ez.Section {
                title: qsTr("Options")
                visible: root.armor
                Option {
                    objectName: "checkPreventCritical"
                    text: qsTr("Prevent Critical Hits")
                    key: "prevent_critical"
                }
                Option {
                    objectName: "checkRaiseEvasion"
                    text: qsTr("Increased Evasion")
                    key: "raise_evasion"
                }
                Option {
                    objectName: "checkHalfMp"
                    text: qsTr("Half MP Consumption")
                    key: "half_sp_cost"
                }
                Option {
                    objectName: "checkNoTerrainDamage"
                    text: qsTr("No Terrain Damage")
                    key: "no_terrain_damage"
                }
                Option {
                    objectName: "checkArmorCursed"
                    text: qsTr("Cursed")
                    key: "cursed"
                    enabled: root.rpg2k3
                }
            }
            // skill books, skill scrolls, and equipment invoking a skill (RPG Maker 2003)
            Ez.Section {
                title: root.book ? qsTr("Skill Learned") : root.special ? qsTr("Invoke Skill") : qsTr("Skill")
                visible: root.equipment || root.book || root.special
                Option {
                    objectName: "checkUseSkill"
                    text: qsTr("Invoke Skill When Used")
                    key: "use_skill"
                    visible: root.equipment
                    enabled: root.rpg2k3
                }
                Ez.RefComboBox {
                    objectName: "comboSkill"
                    database: root.database
                    listKey: "skills"
                    jsonData: root.entry
                    key: "skill_id"
                    pickable: true
                    enabled: root.invokes
                    Layout.fillWidth: true
                }
                ColumnLayout {
                    spacing: Ez.Metrics.rowSpacing
                    visible: root.special || root.equipment
                    enabled: root.invokes
                    Ez.Label {
                        text: qsTr("Message Shown When Used")
                        font: Ez.Metrics.font
                    }
                    Ez.RadioGroup {
                        jsonData: root.entry
                        key: "using_message"
                        choices: [{ name: "radioMessageItem", text: qsTr("Item Name") },
                                  { name: "radioMessageSkill", text: qsTr("Name of Associated Skill") }]
                    }
                }
            }
        }

        // the attributes and conditions: weapons give them, armors protect from them
        ColumnLayout {
            spacing: Ez.Metrics.sectionSpacing
            visible: root.equipment || root.medicine
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: columns.listWidth
            Layout.minimumWidth: implicitWidth

            Ez.Section {
                title: root.weapon ? qsTr("Attack Attribute") : qsTr("Resistance")
                visible: root.equipment
                Layout.fillHeight: true
                BitTable {
                    objectName: "listAttributes"
                    name: "checkAttribute"
                    entry: root.entry
                    key: "attribute_set"
                    names: root.database ? root.database.list("attributes") : null
                    visibleRows: 5
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
            }
            Ez.Section {
                title: root.medicine ? qsTr("Cure Condition") : qsTr("Conditions")
                Layout.fillHeight: true
                // equipment: whether it inflicts or resists them, and how often (RPG Maker 2003 can reverse it)
                GridLayout {
                    columns: 2
                    rowSpacing: Ez.Metrics.rowSpacing
                    columnSpacing: Ez.Metrics.columnSpacing
                    visible: root.equipment
                    Ez.RadioGroup {
                        jsonData: root.entry
                        key: "reverse_state_effect"
                        enabled: root.rpg2k3
                        Layout.columnSpan: 2
                        choices: [{ name: "radioStateNormal", text: root.weapon ? qsTr("Inflict") : qsTr("Resist") },
                                  { name: "radioStateReverse", text: root.weapon ? qsTr("Remove") : qsTr("Inflict") }]
                    }
                    Ez.FormLabel {
                        text: root.weapon ? qsTr("Probability") : qsTr("Resistance")
                    }
                    Amount {
                        objectName: "spinStateChance"
                        key: "state_chance"
                        from: 0
                        to: 100
                        suffix: "%"
                    }
                }
                BitTable {
                    objectName: "listStates"
                    name: "checkState"
                    entry: root.entry
                    key: "state_set"
                    names: root.database ? root.database.list("states") : null
                    visibleRows: 5
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
            }
        }

        // who can use it
        ColumnLayout {
            spacing: Ez.Metrics.sectionSpacing
            visible: root.type !== 0 && !root.switchItem
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: columns.listWidth
            Layout.minimumWidth: implicitWidth

            Ez.Section {
                title: qsTr("Usable by:")
                Layout.fillHeight: true
                // a choice of the whole database (RPG Maker 2003)
                Repeater {
                    model: [{ name: "radioUserHero", text: qsTr("Hero") }, { name: "radioUserClass", text: qsTr("Class") }]
                    Ez.RadioButton {
                        required property var modelData
                        required property int index
                        objectName: modelData.name
                        text: modelData.text
                        jsonData: root.database
                        key: "system/equipment_setting"
                        value: index
                        enabled: root.rpg2k3
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.delay: 700
                        Controls.ToolTip.text: qsTr("Applies to all the items")
                    }
                }
                // the entries past the end of the arrays can use the item; the
                // first class is for the heroes without one
                BitTable {
                    objectName: "listUsers"
                    name: "checkUser"
                    entry: root.entry
                    key: root.byClass ? "class_set" : "actor_set"
                    missing: true
                    names: {
                        const list = root.database ? root.database.list(root.byClass ? "classes" : "actors") : null
                        const names = root.byClass ? [qsTr("(No Class)")] : []
                        for (let i = 0; list && i < list.count; ++i) {
                            names.push(list.str(i + "/name"))
                        }
                        return names
                    }
                    visibleRows: 5
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
            }
        }
    }
}
