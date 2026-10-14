// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "KeyInputProcWidget"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * Key Input Processing. The place of each key depends on the engine and on
 * the number of parameters: RPG Maker 2000 (the four arrows in p2, or with 6
 * or 10 parameters from p6, Shift in p5), RPG Maker 2003 before 1.08 (the
 * arrows in p2, numbers and operators in p5 and p6, the time in p7 and p8)
 * and after (more than 10 parameters: the arrows from p10, Shift in p9).
 * RPG Maker 2003 packs the decision and cancel keys (and with the newer
 * layout the down, up and Shift keys) in bit 0. An older layout becomes the
 * newer one only when an option needs it.
 */
ColumnLayout {
    id: root

    required property Ez.CommandEditSession session
    readonly property string title: qsTr("Key Input Processing")
    readonly property int revision: session.revision
    readonly property bool rpg2003: session.rpg2k3
    // the layout of the command (as opened; an option can make it the newer one)
    property bool modern2000: false
    property bool modern2003: false

    Component.onCompleted: {
        const count = (session.value("/parameters") ?? []).length
        modern2000 = count === 10 || (!rpg2003 && count >= 6)
        modern2003 = rpg2003 && count > 10
    }

    readonly property var labels: [qsTr("Down Key (1)"), qsTr("Left Key (2)"), qsTr("Right Key (3)"), qsTr("Up Key (4)"),
                                   qsTr("Decision Key (5)"), qsTr("Cancel Key (6)"), qsTr("Shift Key (7)"),
                                   qsTr("Number Keys 0-9 (10-19)"), qsTr("+,-,*,/,. Keys (20,21,22,23,24)")]
    readonly property var names: ["keyDown", "keyLeft", "keyRight", "keyUp", "keyDecision", "keyCancel", "keyShift",
                                  "keyNumbers", "keyOperators"]

    function parameter(index) {
        return Number(session.value("/parameters/" + index) ?? 0)
    }
    function setParameter(index, value) {
        session.set("/parameters/" + index, value)
    }
    /** The parameter of a key, -1 when this layout has none */
    function keyIndex(key) {
        if (key < 4) {
            return modern2003 ? key + 10 : modern2000 ? key + 6 : 2
        }
        if (key < 6) {
            return key - 1
        }
        if (key === 6) {
            return modern2003 ? 9 : modern2000 ? 5 : -1
        }
        return rpg2003 && !modern2000 ? key - 2 : -1
    }
    /** The key is in bit 0 of its parameter */
    function packed(key) {
        return rpg2003 && (key === 4 || key === 5 || (modern2003 && (key === 0 || key === 3 || key === 6)))
    }
    function keyChecked(key) {
        const index = keyIndex(key)
        const value = index < 0 ? 0 : parameter(index)
        return packed(key) ? (value & 1) !== 0 : value !== 0
    }
    readonly property bool timed: revision < 0 ? false : rpg2003 && !modern2000 && parameter(8) !== 0

    /** The older layouts share the places of the parameters: the newer one, when an option needs it */
    function modernize() {
        if (modern2003 || (!rpg2003 && modern2000)) {
            return
        }
        const keys = []
        for (let i = 0; i < 9; ++i) {
            keys.push(keyChecked(i))
        }
        const time = parameter(7)
        const isTimed = timed
        if (rpg2003) {
            setParameter(13, 0)
            for (let i = 0; i < 4; ++i) {
                setParameter(i + 10, keys[i] ? 1 : 0)
            }
            setParameter(9, keys[6] ? 1 : 0)
            setParameter(5, keys[7] ? 1 : 0)
            setParameter(6, keys[8] ? 1 : 0)
            setParameter(7, Math.max(1, time))
            setParameter(8, isTimed ? 1 : 0)
            modern2003 = true
            modern2000 = false
        } else {
            setParameter(9, 0)
            for (let i = 0; i < 4; ++i) {
                setParameter(i + 6, keys[i] ? 1 : 0)
            }
            setParameter(5, keys[6] ? 1 : 0)
            modern2000 = true
        }
    }
    function setKey(key, checked) {
        if (key < 4 || key >= 6) {
            modernize()
        }
        const index = keyIndex(key)
        const value = packed(key) ? (parameter(index) & ~1) | (checked ? 1 : 0) : checked ? 1 : 0
        setParameter(index, value)
    }

    spacing: Ez.Metrics.sectionSpacing
    Layout.fillWidth: true

    Ez.Section {
        title: qsTr("Store Key Code in")
        Ez.Form {
            Ez.FormLabel {
                text: qsTr("Variable")
            }
            CommandField {
                session: root.session
                fieldId: "variable"
                name: "keyResultVariable"
                Layout.fillWidth: true
            }
        }
    }

    Ez.Section {
        title: qsTr("Option")
        CommandField {
            id: wait
            session: root.session
            fieldId: "wait"
            name: "keyWait"
        }
        RowLayout {
            spacing: Ez.Metrics.columnSpacing
            Ez.CheckBox {
                objectName: "keyStoreTime"
                text: qsTr("Store Amount of Time it Takes to Press Key")
                checked: root.timed
                enabled: root.rpg2003 && (root.revision < 0 ? false : root.parameter(1) !== 0)
                onToggled: {
                    root.modernize()
                    root.setParameter(8, checked ? 1 : 0)
                }
            }
            Ez.RefComboBox {
                objectName: "keyTimeVariable"
                database: root.session.database
                listKey: "variables"
                enabled: root.timed && (root.revision < 0 ? false : root.parameter(1) !== 0)
                // the variable of the older layout's p7 (the newer reads it there too)
                currentIndex: root.revision < 0 || !root.rpg2003 || root.modern2000 ? 0 : Math.max(0, root.parameter(7) - 1)
                onActivated: (index) => {
                    root.modernize()
                    root.setParameter(7, index + 1)
                }
                Layout.fillWidth: true
                Layout.preferredWidth: 200
            }
        }
    }

    Ez.Section {
        title: qsTr("Keys to Process")
        GridLayout {
            columns: 2
            rowSpacing: Ez.Metrics.rowSpacing
            columnSpacing: 2 * Ez.Metrics.columnSpacing
            Repeater {
                model: root.names.length
                Ez.CheckBox {
                    required property int index
                    objectName: root.names[index]
                    text: root.labels[index]
                    // the number and operator keys are RPG Maker 2003's
                    enabled: index < 7 || root.rpg2003
                    checked: root.revision < 0 ? false : root.keyChecked(index)
                    onToggled: root.setKey(index, checked)
                }
            }
        }
    }

    Item {
        Layout.fillHeight: true
    }
}
