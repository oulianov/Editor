// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "ClassEditors"

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * The skills a hero or a class learns (tableSkills): a line per skill and
 * its level, sorted by level, an empty line to add one. As in RPG Maker, a
 * double-click (or Return) edits a line in a small dialog (spinSkillLevel,
 * comboSkill), on the empty line or below the lines adds one; Delete
 * removes it.
 *
 *   SkillTable { entry: page.entry; database: page.database; maxLevel: 99 }
 */
Ez.ArrayTable {
    id: root

    /** The hero or class (a JsonView) */
    property var entry: null
    property var database: null
    /** The highest level of the game (99, 50 in RPG Maker 2000) */
    property int maxLevel: 99

    readonly property var skills: entry ? entry.list("skills") : null
    readonly property var skillList: database ? database.list("skills") : null

    /** The name of a skill by its ID ("<12?>" for none) */
    function skillName(id) {
        return skillList && id >= 1 && id <= skillList.count ? skillList.str((id - 1) + "/name") : qsTr("<%1?>").arg(id)
    }
    /** Edits a line, or adds one past the end */
    function edit(row) {
        if (!skills || row < 0) {
            return
        }
        editor.row = Math.min(row, skills.count)
        const adding = editor.row >= skills.count
        level.value = adding ? 1 : skills.num(editor.row + "/level")
        skill.currentIndex = adding ? 0 : skill.indexOfValue(skills.num(editor.row + "/skill_id"))
        editor.open()
    }
    function remove(row) {
        if (skills && row >= 0 && row < skills.count) {
            skills.removeEntries(row, 1)
            currentIndex = Math.min(row, rows)
        }
    }
    /** The lines in the order of their levels, as RPG Maker keeps them */
    function sortByLevel() {
        const lines = []
        for (let i = 0; i < skills.count; ++i) {
            lines.push({ level: skills.num(i + "/level"), skill: skills.num(i + "/skill_id") })
        }
        // stable: the lines of a level keep their order
        const sorted = lines.map((line, i) => ({ line, i }))
                            .sort((a, b) => a.line.level - b.line.level || a.i - b.i)
                            .map(item => item.line)
        for (let i = 0; i < sorted.length; ++i) {
            skills.set(i + "/level", sorted[i].level)
            skills.set(i + "/skill_id", sorted[i].skill)
        }
    }

    rows: skills ? skills.count : 0
    source: skills
    addLine: true
    columns: [
        { title: qsTranslate("ClassWidget", "Level"), width: 50, text: row => String(root.skills.num(row + "/level")) },
        { title: qsTranslate("ClassWidget", "Skill Learned"), text: row => root.skillName(root.skills.num(row + "/skill_id")) },
    ]
    onActivated: (row) => edit(row)
    onRemoveRequested: (row) => remove(row)

    // the names change on the Skills page
    Connections {
        target: root.skillList
        function onDataChanged() { root.revision++ }
        function onCountChanged() { root.revision++ }
    }

    Ez.Dialog {
        id: editor

        property int row: 0

        parent: Controls.Overlay.overlay
        anchors.centerIn: parent
        modal: true
        title: root.skills && row >= root.skills.count ? qsTr("Add Skill") : qsTr("Edit Skill")
        standardButtons: Controls.Dialog.Ok | Controls.Dialog.Cancel
        font: Ez.Metrics.font
        padding: Ez.Metrics.dialogMargin

        onOpened: level.forceActiveFocus()
        onAccepted: {
            if (skill.currentIndex < 0) {
                return
            }
            if (row >= root.skills.count && !root.skills.insertEntries(root.skills.count, 1)) {
                return
            }
            root.skills.set(row + "/level", level.value)
            root.skills.set(row + "/skill_id", skill.currentValue)
            root.sortByLevel()
        }

        contentItem: Ez.Form {
            Ez.FormLabel {
                text: qsTr("Level")
            }
            Ez.SpinBox {
                id: level
                objectName: "spinSkillLevel"
                from: 1
                to: root.maxLevel
            }
            Ez.FormLabel {
                text: qsTr("Skill")
            }
            Ez.RefComboBox {
                id: skill
                objectName: "comboSkill"
                database: root.database
                listKey: "skills"
                Layout.fillWidth: true
                Layout.preferredWidth: 220
            }
        }
    }
}
