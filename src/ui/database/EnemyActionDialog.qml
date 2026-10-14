// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "EnemyActionDialog"

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * One action of a monster's behavior: its precondition, priority, the
 * action itself (a basic one, a skill or a transformation) and the
 * switches it turns on and off (TFORMEDMONSTERACT, "Monster Behavior", of
 * RPG Maker). OK writes them to the action.
 *
 *   Ez.Editor.exec("EnemyActionDialog", { database: page.database, action: enemy.subtree("actions/0") }, item)
 */
Ez.DialogWindow {
    id: root

    /** The root of the database (the switches, skills and monsters listed) */
    property var database: null
    /** The action edited (a JsonView) */
    property var action: null

    /** The range of the two parameters of a precondition type */
    function paramRange(type) {
        switch (type) {
        case 2:
            return [0, 9999]
        case 3:
            return [0, 8]
        case 6:
            return [1, 99]
        default:
            return [0, 100]
        }
    }
    /** The entry chosen in a list, else the one the action had */
    function chosenId(combo, fallback) {
        const id = combo.currentIndex >= 0 ? Number(combo.currentValue) : 0
        return id > 0 ? id : fallback
    }
    function show(combo, id) {
        combo.currentIndex = combo.indexOfValue(id)
    }

    objectName: "EnemyActionDialog"
    title: qsTr("Monster Behavior")

    // shown once the lists are made; the changes wait for OK
    property bool ready: false
    Component.onCompleted: {
        if (!action) {
            return
        }
        condition.currentIndex = action.num("condition_type")
        param1.value = action.num("condition_param1")
        param2.value = action.num("condition_param2")
        show(conditionSwitch, action.num("switch_id"))
        rating.value = action.num("rating")
        const kind = action.num("kind")
        radioBasic.checked = kind === 0
        radioSkill.checked = kind === 1
        radioTransform.checked = kind === 2
        basic.currentIndex = action.num("basic")
        show(skill, action.num("skill_id"))
        show(transform, action.num("enemy_id"))
        checkSwitchOn.checked = action.boolean("switch_on")
        show(switchOn, action.num("switch_on_id"))
        checkSwitchOff.checked = action.boolean("switch_off")
        show(switchOff, action.num("switch_off_id"))
        ready = true
    }
    onAccepting: (check) => {
        if (!action) {
            return
        }
        action.beginBatch()
        const type = condition.currentIndex
        action.set("condition_type", type)
        if (type > 1) {
            action.set("condition_param1", param1.value)
            action.set("condition_param2", param2.value)
        }
        action.set("switch_id", chosenId(conditionSwitch, action.num("switch_id")))
        action.set("rating", rating.value)
        action.set("kind", radioSkill.checked ? 1 : radioTransform.checked ? 2 : 0)
        action.set("basic", basic.currentIndex)
        action.set("skill_id", chosenId(skill, action.num("skill_id")))
        action.set("enemy_id", chosenId(transform, action.num("enemy_id")))
        action.set("switch_on", checkSwitchOn.checked)
        action.set("switch_on_id", chosenId(switchOn, action.num("switch_on_id")))
        action.set("switch_off", checkSwitchOff.checked)
        action.set("switch_off_id", chosenId(switchOff, action.num("switch_off_id")))
        action.endBatch()
    }

    // what triggers it: the precondition and the priority
    Ez.Section {
        title: qsTr("Behavior Trigger")

        RowLayout {
            spacing: Ez.Metrics.sectionSpacing
            Layout.fillWidth: true

            Ez.Section {
                title: qsTr("Preconditions")
                Layout.fillWidth: true
                Layout.fillHeight: true

                Ez.ComboBox {
                    id: condition
                    objectName: "comboCondition"
                    textRole: ""
                    valueRole: ""
                    model: [qsTr("None"), qsTr("Switch"), qsTr("Turns Elapsed"), qsTr("Monsters Present"),
                            qsTr("Monster HP"), qsTr("Monster MP"), qsTr("Party Level Average"), qsTr("Party Exhaustion")]
                    Layout.fillWidth: true
                    // a new type starts with its whole range (turns: every turn)
                    onActivated: {
                        const range = currentIndex === 2 ? [1, 0] : root.paramRange(currentIndex)
                        param1.value = range[0]
                        param2.value = range[1]
                    }
                }
                RowLayout {
                    readonly property int type: condition.currentIndex
                    readonly property bool percent: type === 4 || type === 5 || type === 7
                    spacing: Ez.Metrics.columnSpacing
                    Layout.fillWidth: true

                    Ez.RefComboBox {
                        id: conditionSwitch
                        objectName: "comboConditionSwitch"
                        database: root.database
                        listKey: "switches"
                        visible: parent.type === 1
                        Layout.fillWidth: true
                    }
                    Ez.Label {
                        text: qsTr("is ON")
                        font: Ez.Metrics.font
                        visible: parent.type === 1
                    }
                    Ez.SpinBox {
                        id: param1
                        objectName: "spinParam1"
                        from: root.paramRange(parent.type)[0]
                        to: root.paramRange(parent.type)[1]
                        visible: parent.type > 1
                    }
                    Ez.Label {
                        // every param1 turns, from turn param2
                        text: parent.type === 2 ? qsTr("x +") : parent.percent ? qsTr("% -") : qsTr("-")
                        font: Ez.Metrics.font
                        visible: parent.type > 1
                    }
                    Ez.SpinBox {
                        id: param2
                        objectName: "spinParam2"
                        from: root.paramRange(parent.type)[0]
                        to: root.paramRange(parent.type)[1]
                        visible: parent.type > 1
                    }
                    Ez.Label {
                        text: parent.percent ? qsTr("%") : ""
                        font: Ez.Metrics.font
                        visible: parent.type > 1 && text !== ""
                    }
                    Item {
                        Layout.fillWidth: parent.type !== 1
                    }
                }
            }

            Ez.Section {
                title: qsTr("Priority")
                Layout.fillHeight: true
                Ez.SpinBox {
                    id: rating
                    objectName: "spinRating"
                    from: 1
                    to: 100
                }
                Item {
                    Layout.fillHeight: true
                }
            }
        }
    }

    // the action: a basic one, a skill or a transformation
    Ez.Section {
        title: qsTr("Action")

        GridLayout {
            columns: 2
            rowSpacing: Ez.Metrics.rowSpacing
            columnSpacing: Ez.Metrics.columnSpacing
            Layout.fillWidth: true

            Controls.ButtonGroup {
                id: kinds
            }
            Ez.RadioButton {
                id: radioBasic
                objectName: "radioBasic"
                text: qsTr("Basic")
                Controls.ButtonGroup.group: kinds
            }
            Ez.ComboBox {
                id: basic
                objectName: "comboBasic"
                textRole: ""
                valueRole: ""
                enabled: radioBasic.checked
                model: [qsTr("Attack"), qsTr("Double Attack"), qsTr("Defend"), qsTr("Observe Battle"),
                        qsTr("Charge Up"), qsTr("Self-Destruct"), qsTr("Escape"), qsTr("No Action")]
                Layout.fillWidth: true
            }
            Ez.RadioButton {
                id: radioSkill
                objectName: "radioSkill"
                text: qsTr("Skill")
                Controls.ButtonGroup.group: kinds
            }
            Ez.RefComboBox {
                id: skill
                objectName: "comboSkill"
                database: root.database
                listKey: "skills"
                enabled: radioSkill.checked
                Layout.fillWidth: true
            }
            Ez.RadioButton {
                id: radioTransform
                objectName: "radioTransform"
                text: qsTr("Transform")
                Controls.ButtonGroup.group: kinds
            }
            Ez.RefComboBox {
                id: transform
                objectName: "comboTransform"
                database: root.database
                listKey: "enemies"
                enabled: radioTransform.checked
                Layout.fillWidth: true
            }
        }
    }

    // the switches turned on and off when the action is taken
    Ez.Section {
        title: qsTr("Action Turns Switch ON")
        RowLayout {
            spacing: Ez.Metrics.columnSpacing
            Layout.fillWidth: true
            Ez.CheckBox {
                id: checkSwitchOn
                objectName: "checkSwitchOn"
                text: qsTr("Use")
            }
            Ez.RefComboBox {
                id: switchOn
                objectName: "comboSwitchOn"
                database: root.database
                listKey: "switches"
                enabled: checkSwitchOn.checked
                Layout.fillWidth: true
            }
        }
    }
    Ez.Section {
        title: qsTr("Action Turns Switch OFF")
        RowLayout {
            spacing: Ez.Metrics.columnSpacing
            Layout.fillWidth: true
            Ez.CheckBox {
                id: checkSwitchOff
                objectName: "checkSwitchOff"
                text: qsTr("Use")
            }
            Ez.RefComboBox {
                id: switchOff
                objectName: "comboSwitchOff"
                database: root.database
                listKey: "switches"
                enabled: checkSwitchOff.checked
                Layout.fillWidth: true
            }
        }
    }
}
