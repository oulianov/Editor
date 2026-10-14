// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "ShowBattleAnimation_BWidget"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * Show Battle Animation (battle): the animation, one list for the target
 * and its side (p1, and p3 the party in RPG Maker 2003: written when it is
 * or when the command has it; monsters count from 0, party members from 1),
 * and the wait.
 */
ColumnLayout {
    id: root

    required property Ez.CommandEditSession session
    readonly property string title: qsTr("Show Battle Animation")
    readonly property int revision: session.revision
    readonly property int target: revision < 0 ? 0 : Number(session.value("/parameters/1") ?? 0)
    readonly property int side: revision < 0 ? 0 : Number(session.value("/parameters/3") ?? 0)

    /** The targets: of the monsters, then (RPG Maker 2003) of the party; the command's when unknown */
    readonly property var targets: {
        const list = [{ text: qsTr("All Enemies"), target: -1, side: 0 }]
        for (let i = 0; i < 8; ++i) {
            list.push({ text: qsTr("Monster %1").arg(i + 1), target: i, side: 0 })
        }
        if (session.rpg2k3) {
            list.push({ text: qsTr("Entire Party"), target: -1, side: 1 })
            for (let i = 1; i <= 4; ++i) {
                list.push({ text: qsTr("Party Member %1").arg(i), target: i, side: 1 })
            }
        }
        if (!list.some(one => one.target === root.target && one.side === root.side)) {
            list.push({ text: qsTr("Unknown (%1)").arg(root.target), target: root.target, side: root.side })
        }
        return list
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
        Ez.ComboBox {
            objectName: "battleAnimationTarget"
            model: root.targets
            textRole: "text"
            valueRole: ""
            currentIndex: root.targets.findIndex(one => one.target === root.target && one.side === root.side)
            Layout.fillWidth: true
            onActivated: (index) => {
                const chosen = root.targets[index]
                root.session.set("/parameters/1", chosen.target)
                if (chosen.side !== 0 || (root.session.value("/parameters") ?? []).length > 3) {
                    root.session.set("/parameters/3", chosen.side)
                }
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
