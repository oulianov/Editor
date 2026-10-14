// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "SkillWidget"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * A skill of the database, as RPG Maker 2003 shows it: its name, type,
 * description, MP cost and target above; for a normal skill its battle
 * animation (and how heroes move, Set...), power, the statistics it raises
 * or lowers, the conditions and attributes it gives; for a teleport, escape
 * or switch skill its sound (and switch); the messages of RPG Maker 2000's
 * battles. The texts follow the target: a skill on allies increases and
 * removes, on enemies it decreases and inflicts.
 */
DatabaseEntryPage {
    id: root

    /** 2003 games can use the battles of RPG Maker 2000, and their messages */
    readonly property bool has2kMessages: !rpg2k3 || (database ? database.boolean("system/easyrpg_use_rpg2k_battle_system") : false)
    readonly property int type: typeField.valid ? Number(typeField.value) : 0
    /** The subset types of RPG Maker 2003 are normal skills */
    readonly property bool normal: type === 0 || type >= 4
    /** Skills on allies heal and raise, skills on enemies hurt and lower */
    readonly property bool allies: scopeField.valid && Number(scopeField.value) >= 2
    readonly property bool percent: spTypeField.valid && Number(spTypeField.value) === 1

    /** The types: RPG Maker's four, then (2003) one per battle command listing a subset of the skills */
    readonly property var types: {
        const choices = [qsTr("Normal"), qsTr("Teleport"), qsTr("Escape"), qsTr("Switch")].map((text, i) => ({ text: text, value: i }))
        const commands = database && rpg2k3 ? database.list("battlecommands/commands") : null
        let type = 4
        for (let i = 0; commands && i < commands.count; ++i) {
            if (commands.num(i + "/type") === 2) {
                choices.push({ text: commands.str(i + "/name"), value: type++ })
            }
        }
        return choices
    }
    /** The failure messages of the vocabulary */
    readonly property var failures: {
        const keys = ["skill_failure_a", "skill_failure_b", "skill_failure_c", "dodge"]
        return keys.map((key, i) => {
            const text = database ? database.str("terms/" + key) : ""
            return { text: text !== "" ? text : qsTr("Message %1").arg(i + 1), value: i }
        })
    }

    fillWidth: true

    Ez.JsonField {
        id: typeField
        source: root.entry
        path: "type"
    }
    Ez.JsonField {
        id: scopeField
        source: root.entry
        path: "scope"
    }
    Ez.JsonField {
        id: spTypeField
        source: root.entry
        path: "sp_type"
    }

    /** A rate of 0 to 10 and its value */
    /** A label of the power: a long one (French...) on two lines, the page fits the window */
    component PowerLabel: Ez.FormLabel {
        wrapMode: Text.Wrap
        Layout.maximumWidth: 150
    }
    component Rate: RowLayout {
        id: rate
        property alias name: slider.objectName
        property string key
        spacing: Ez.Metrics.columnSpacing
        Layout.fillWidth: true
        Ez.Slider {
            id: slider
            jsonData: root.entry
            key: rate.key
            from: 0
            // a tick mark at each step, as in the Widgets editor
            tickInterval: 1
            to: 10
            Layout.fillWidth: true
            Layout.preferredWidth: 80
        }
        Ez.Label {
            text: String(Math.round(slider.value))
            horizontalAlignment: Text.AlignRight
            font: Ez.Metrics.font
            Layout.minimumWidth: digits.advanceWidth
            TextMetrics {
                id: digits
                font: Ez.Metrics.font
                text: "10"
            }
        }
    }

    // the name, type, description, cost and target
    GridLayout {
        columns: 4
        rowSpacing: Ez.Metrics.rowSpacing
        columnSpacing: Ez.Metrics.columnSpacing
        Layout.fillWidth: true

        Ez.FormLabel {
            text: qsTr("Name")
        }
        Ez.TextField {
            objectName: "skillName"
            jsonData: root.entry
            key: "name"
            limit: 20
            Layout.fillWidth: true
        }
        Ez.FormLabel {
            text: qsTr("Type")
        }
        Ez.ComboBox {
            objectName: "skillType"
            jsonData: root.entry
            key: "type"
            model: root.types
            Layout.fillWidth: true
        }
        Ez.FormLabel {
            text: qsTr("Description")
        }
        Ez.TextField {
            objectName: "skillDescription"
            jsonData: root.entry
            key: "description"
            limit: 50
            Layout.columnSpan: 3
            Layout.fillWidth: true
        }
        Ez.FormLabel {
            text: qsTr("MP Consumed")
        }
        RowLayout {
            spacing: Ez.Metrics.columnSpacing
            // a number, or a percentage of the maximum MP (RPG Maker 2003)
            Ez.SpinBox {
                objectName: "skillMpCost"
                jsonData: root.entry
                key: root.percent ? "sp_percent" : "sp_cost"
                from: 0
                to: root.percent ? 100 : 999
                suffix: root.percent ? "%" : ""
            }
            Ez.RadioButton {
                objectName: "radioMpNumber"
                text: qsTr("Number")
                jsonData: root.entry
                key: "sp_type"
                value: 0
            }
            Ez.RadioButton {
                objectName: "radioMpPercent"
                text: qsTr("% of Max MP")
                jsonData: root.entry
                key: "sp_type"
                value: 1
                enabled: root.rpg2k3
            }
            Item {
                Layout.fillWidth: true
            }
        }
        Ez.FormLabel {
            text: qsTr("Target")
            visible: root.normal
        }
        Ez.ComboBox {
            objectName: "skillScope"
            jsonData: root.entry
            key: "scope"
            visible: root.normal
            model: [qsTr("Single Enemy"), qsTr("All Enemies"), qsTr("Self"), qsTr("Single Ally"), qsTr("All Allies")]
                .map((text, i) => ({ text: text, value: i }))
            Layout.fillWidth: true
        }
    }

    RowLayout {
        id: columns
        spacing: Ez.Metrics.sectionSpacing
        Layout.fillWidth: true
        Layout.fillHeight: true

        // the animation and power of a normal skill; the sound, switch and
        // occasions of the others
        ColumnLayout {
            spacing: Ez.Metrics.sectionSpacing
            Layout.alignment: Qt.AlignTop
            Layout.fillWidth: true
            Layout.preferredWidth: (root.column.room - 2 * columns.spacing) / 3
            Layout.minimumWidth: implicitWidth

            Ez.Section {
                title: qsTr("Battle Animation")
                visible: root.normal
                RowLayout {
                    spacing: Ez.Metrics.columnSpacing
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
                        Controls.ToolTip.text: qsTr("How each hero moves when using the skill (RPG Maker 2003 battles)")
                        onClicked: Ez.Editor.exec("BattlerAnimationDialog", { database: root.database, entry: root.entry,
                                                                              weapon: false }, battlerAnimation)
                    }
                }
            }
            Ez.Section {
                title: qsTr("Power")
                visible: root.normal
                Ez.Form {
                    PowerLabel {
                        text: qsTr("Attack Influence")
                    }
                    Rate {
                        name: "sliderAttackInfluence"
                        key: "physical_rate"
                    }
                    PowerLabel {
                        text: qsTr("Intelligence Influence")
                    }
                    Rate {
                        name: "sliderIntelligenceInfluence"
                        key: "magical_rate"
                    }
                    PowerLabel {
                        text: qsTr("Variance")
                    }
                    Rate {
                        name: "sliderVariance"
                        key: "variance"
                    }
                    PowerLabel {
                        text: qsTr("Effect Rating")
                    }
                    Ez.SpinBox {
                        objectName: "spinEffectRating"
                        jsonData: root.entry
                        key: "power"
                        from: 0
                        to: 9999
                    }
                    PowerLabel {
                        text: qsTr("Success Rate")
                    }
                    Ez.SpinBox {
                        objectName: "spinSuccessRate"
                        jsonData: root.entry
                        key: "hit"
                        from: 0
                        to: 100
                        suffix: "%"
                    }
                }
            }
            Ez.Section {
                title: qsTr("Sound Effect")
                visible: !root.normal
                Ez.AudioField {
                    fieldName: "lineSound"
                    jsonData: root.entry
                    key: "sound_effect"
                }
            }
            Ez.Section {
                title: qsTr("Turn ON Switch")
                visible: root.type === 3
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
                visible: root.type === 3
                RowLayout {
                    spacing: 2 * Ez.Metrics.columnSpacing
                    Ez.CheckBox {
                        objectName: "checkOccasionField"
                        text: qsTr("Field")
                        jsonData: root.entry
                        key: "occasion_field"
                    }
                    Ez.CheckBox {
                        objectName: "checkOccasionBattle"
                        text: qsTr("Battle")
                        jsonData: root.entry
                        key: "occasion_battle"
                    }
                }
            }
        }

        // the statistics changed; the messages of RPG Maker 2000's battles
        ColumnLayout {
            spacing: Ez.Metrics.sectionSpacing
            Layout.alignment: Qt.AlignTop
            Layout.fillWidth: true
            Layout.preferredWidth: (root.column.room - 2 * columns.spacing) / 3
            Layout.minimumWidth: implicitWidth

            Ez.Section {
                objectName: "groupEffects"
                title: root.allies ? qsTr("Increase") : qsTr("Decrease")
                visible: root.normal
                Repeater {
                    model: [
                        { name: "checkAffectHp", text: qsTr("HP"), key: "affect_hp" },
                        { name: "checkAffectMp", text: qsTr("MP"), key: "affect_sp" },
                        { name: "checkAffectAttack", text: qsTr("Attack"), key: "affect_attack" },
                        { name: "checkAffectDefense", text: qsTr("Defense"), key: "affect_defense" },
                        { name: "checkAffectIntelligence", text: qsTr("Intelligence"), key: "affect_spirit" },
                        { name: "checkAffectAgility", text: qsTr("Agility"), key: "affect_agility" },
                        { name: "checkAbsorb", text: qsTr("Absorb"), key: "absorb_damage" },
                        { name: "checkIgnoreDefense", text: qsTr("Ignore Defense"), key: "ignore_defense" },
                    ]
                    Ez.CheckBox {
                        required property var modelData
                        objectName: modelData.name
                        text: modelData.text
                        jsonData: root.entry
                        key: modelData.key
                    }
                }
            }
            Ez.Section {
                title: qsTr("Using Message")
                enabled: root.has2kMessages
                Ez.Form {
                    Ez.FormLabel {
                        text: qsTr("Line 1")
                    }
                    Ez.TextField {
                        objectName: "lineMessage1"
                        jsonData: root.entry
                        key: "using_message1"
                        Layout.fillWidth: true
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.delay: 700
                        Controls.ToolTip.text: qsTr("Shown after the name of the user, in the RPG Maker 2000 battle system")
                    }
                    Ez.FormLabel {
                        text: qsTr("Line 2")
                    }
                    Ez.TextField {
                        objectName: "lineMessage2"
                        jsonData: root.entry
                        key: "using_message2"
                        Layout.fillWidth: true
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.delay: 700
                        Controls.ToolTip.text: qsTr("Shown after the name of the user, in the RPG Maker 2000 battle system")
                    }
                    Ez.FormLabel {
                        text: qsTr("Failure")
                    }
                    Ez.ComboBox {
                        objectName: "comboFailure"
                        jsonData: root.entry
                        key: "failure_message"
                        model: root.failures
                        Layout.fillWidth: true
                    }
                }
            }
        }

        // the conditions and attributes it gives
        ColumnLayout {
            spacing: Ez.Metrics.sectionSpacing
            visible: root.normal
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: (root.column.room - 2 * columns.spacing) / 3
            Layout.minimumWidth: implicitWidth

            Ez.Section {
                title: qsTr("Condition")
                Layout.fillHeight: true
                // RPG Maker 2003 can make a skill remove the conditions on enemies, inflict them on allies
                RowLayout {
                    spacing: 2 * Ez.Metrics.columnSpacing
                    enabled: root.rpg2k3
                    Ez.RadioButton {
                        objectName: "radioStateNormal"
                        text: root.allies ? qsTr("Remove") : qsTr("Inflict")
                        jsonData: root.entry
                        key: "reverse_state_effect"
                        value: 0
                    }
                    Ez.RadioButton {
                        objectName: "radioStateReverse"
                        text: root.allies ? qsTr("Inflict") : qsTr("Remove")
                        jsonData: root.entry
                        key: "reverse_state_effect"
                        value: 1
                    }
                }
                BitTable {
                    objectName: "listStates"
                    name: "checkState"
                    entry: root.entry
                    key: "state_effects"
                    names: root.database ? root.database.list("states") : null
                    visibleRows: 6
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
            }
            Ez.Section {
                title: qsTr("Attack Attribute")
                Layout.fillHeight: true
                BitTable {
                    objectName: "listAttributes"
                    name: "checkAttribute"
                    entry: root.entry
                    key: "attribute_effects"
                    names: root.database ? root.database.list("attributes") : null
                    visibleRows: 6
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
                Ez.CheckBox {
                    objectName: "checkAttributeDefense"
                    text: root.allies ? qsTr("Increase Resistance") : qsTr("Reduce Resistance")
                    jsonData: root.entry
                    key: "affect_attr_defence"
                }
            }
        }
    }
}
