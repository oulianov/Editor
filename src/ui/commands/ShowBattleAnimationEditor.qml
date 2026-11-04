// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "ShowBattleAnimation_BWidget"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * Show Battle Animation (battle), as RPG Maker 2003 writes it: the
 * animation, one list for the target and its side, and the wait. The
 * target (p1) is a monster (from 0), or on the side of the party (p3, RPG
 * Maker 2003 reads it from 4 parameters on) a hero ID; -1 all of them. RPG
 * Maker 2003 games have the 5 parameters of RPG Maker 2003: p4 is the line
 * of the target in its list (All Enemies -1, the monsters of the troop,
 * Entire Party, then the heroes by ID), 8 monsters outside a troop.
 * Maniac Patch: the target's mode in nibble 1 of p5, a target from a
 * variable or a variable pointer with its side (p3) of its own.
 */
ColumnLayout {
    id: root

    required property Ez.CommandEditSession session
    readonly property string title: qsTr("Show Battle Animation")
    readonly property int revision: session.revision
    readonly property var parameters: revision < 0 ? [] : (session.value("/parameters") ?? [])
    readonly property int target: Number(parameters[1] ?? 0)
    readonly property int side: parameters.length > 3 ? Number(parameters[3]) : 0
    /** Maniac Patch: 0 the target of the list, 1 the one in a variable, 2 in a variable pointer */
    readonly property int targetMode: parameters.length > 5 ? (Number(parameters[5]) >> 4) & 0xF : 0
    /** The monsters before Entire Party in RPG Maker 2003's list: those of the troop */
    readonly property int monsters: session.extra("troopMembers") ?? 8

    /** The targets: the monsters, then (RPG Maker 2003) the party and its heroes; the command's when unknown */
    readonly property var targets: {
        const list = [{ text: qsTr("All Enemies"), target: -1, side: 0 }]
        for (let i = 0; i < 8; ++i) {
            list.push({ text: qsTr("Monster %1").arg(i + 1), target: i, side: 0 })
        }
        if (session.rpg2k3) {
            list.push({ text: qsTr("Entire Party"), target: -1, side: 1 })
            const heroes = session.database ? session.database.size("actors") : 0
            for (let id = 1; id <= heroes; ++id) {
                list.push({ text: session.entryName("actors", id), target: id, side: 1 })
            }
        }
        if (!list.some(one => one.target === root.target && one.side === root.side)) {
            list.push({ text: qsTr("Unknown (%1)").arg(root.target), target: root.target, side: root.side })
        }
        return list
    }

    /** Writes a target: in RPG Maker 2003 games with its side and its line, as it does */
    function writeTarget(chosen) {
        if (!session.rpg2k3) {
            session.set("/parameters/1", chosen.target)
            return
        }
        // the whole list: a parameter written to the value it reads missing
        // would not be added (the Maniac Patch's after them kept)
        const line = chosen.side === 0 ? chosen.target : monsters + Math.max(chosen.target, 0)
        session.set("/parameters", [Number(parameters[0] ?? 0), chosen.target, Number(parameters[2] ?? 0), chosen.side, line]
                    .concat(parameters.slice(5).map(Number)))
    }
    /** OK: in RPG Maker 2003 games, its 5 parameters (a command of RPG Maker 2000 or of an older Editor gets them) */
    function accept() {
        if (targetMode === 0) {
            writeTarget({ target: root.target, side: root.side })
        }
        return true
    }

    spacing: Ez.Metrics.sectionSpacing
    Layout.fillWidth: true

    Ez.Section {
        title: qsTr("Animation")
        CommandField {
            session: root.session
            fieldId: "animation"
            name: "battleAnimation"
            Layout.fillWidth: true
            Layout.preferredWidth: 240
        }
    }
    Ez.Section {
        title: qsTr("Target")
        RowLayout {
            spacing: Ez.Metrics.rowSpacing
            Layout.fillWidth: true

            Ez.ComboBox {
                objectName: "battleAnimationTargetMode"
                visible: root.session.maniac || root.targetMode !== 0
                enabled: root.session.maniac
                model: [{ value: 0, text: qsTranslate("ManiacCommandWidget", "Constant") },
                        { value: 1, text: qsTranslate("ManiacCommandWidget", "Variable") },
                        { value: 2, text: qsTranslate("ManiacCommandWidget", "Variable pointer") }]
                textRole: "text"
                valueRole: "value"
                currentIndex: Math.max(0, indexOfValue(root.targetMode))
                onActivated: {
                    if (currentValue === root.targetMode) {
                        return
                    }
                    // another kind of target: All Enemies, the first variable
                    if ((currentValue !== 0) !== (root.targetMode !== 0)) {
                        root.session.set("/parameters/1", currentValue === 0 ? -1 : 1)
                    }
                    root.session.set("/parameters/5@4:4", currentValue)
                }
            }
            Ez.ComboBox {
                objectName: "battleAnimationTarget"
                visible: root.targetMode === 0
                model: root.targets
                textRole: "text"
                valueRole: ""
                currentIndex: root.targets.findIndex(one => one.target === root.target && one.side === root.side)
                Layout.fillWidth: true
                onActivated: (index) => root.writeTarget(root.targets[index])
            }
            Ez.RefComboBox {
                objectName: "battleAnimationTargetVariable"
                visible: root.targetMode !== 0
                database: root.session.database
                listKey: "variables"
                jsonData: root.session
                key: "/parameters/1"
                Layout.fillWidth: true
            }
            // a target from a variable: a monster or (RPG Maker 2003) a hero
            Ez.ComboBox {
                objectName: "battleAnimationTargetSide"
                visible: root.targetMode !== 0 && root.session.rpg2k3
                model: [qsTranslate("ManiacCommandWidget", "Enemy"), qsTranslate("ManiacCommandWidget", "Hero")]
                currentIndex: root.side === 1 ? 1 : 0
                onActivated: (index) => root.session.set("/parameters/3", index)
            }
        }
    }
    Ez.Section {
        title: qsTr("Option")
        CommandField {
            session: root.session
            fieldId: "wait"
            name: "battleAnimationWait"
        }
    }
    Item {
        Layout.fillHeight: true
    }
}
