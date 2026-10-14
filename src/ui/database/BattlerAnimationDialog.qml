// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "BattlerAnimationDataEditor"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * How each hero moves when using a skill, or attacks with a weapon, in the
 * battles of RPG Maker 2003 (Set... on the Skills and Items pages): the
 * hero (comboHero), its movement and afterimage, then the pose of the skill
 * (comboPose), or for a weapon the weapon drawn or a battle animation, the
 * number of attacks and its ranged attack (groupRanged, whose trajectory
 * and targets belong to the weapon). The poses and weapons are those of the
 * hero's battle graphics. Edited on a copy with an entry for every hero,
 * written on OK.
 *
 *   Ez.Editor.exec("BattlerAnimationDialog", { database: page.database, entry: page.entry, weapon: false }, button)
 */
Ez.DialogWindow {
    id: root

    property var database: null
    /** The skill or item (a JsonView) */
    property var entry: null
    /** A weapon (animation_data), else a skill (battler_animation_data) */
    property bool weapon: false

    readonly property string key: weapon ? "animation_data" : "battler_animation_data"
    readonly property var fields: ["type", "weapon_animation_id", "movement", "after_image", "attacks", "ranged",
                                   "ranged_animation_id", "ranged_speed", "battle_animation_id", "pose"]
    /** The settings of each hero (the copy edited) */
    property var settings: []
    /** The weapon's own: trajectory and targets of its ranged attack */
    property int trajectory: 0
    property int rangedTarget: 0
    readonly property int hero: heroBox.currentIndex
    readonly property var current: settings[hero] ?? null
    readonly property var heroes: database ? database.list("actors") : null

    /** A setting of the hero shown changed */
    function setField(field, value) {
        if (!current) {
            return
        }
        const all = settings.slice()
        const changed = Object.assign({}, current)
        changed[field] = value
        all[hero] = changed
        settings = all
    }
    /** The battler animation of a hero (a JsonView), null for none */
    function battlerOf(row) {
        if (!database || !heroes || row < 0 || row >= heroes.count) {
            return null
        }
        const id = heroes.num(row + "/battler_animation")
        const battlers = database.list("battleranimations")
        return id >= 1 && id <= battlers.count ? battlers.entry(id - 1) : null
    }
    /** The names of a list of the hero's battle graphics, as choices */
    function names(list) {
        const choices = []
        for (let i = 0; list && i < list.count; ++i) {
            choices.push({ text: list.str(i + "/name"), value: i })
        }
        return choices
    }
    function choices(texts) {
        return texts.map((text, i) => ({ text: text, value: i }))
    }

    // the section's check box follows the hero shown (the section sets its checked itself)
    onCurrentChanged: rangedSection.checked = !!current && !!current.ranged

    readonly property var battler: battlerOf(hero)
    readonly property var poses: {
        const defaults = [qsTr("Idle Stance"), qsTr("Right Hand"), qsTr("Left Hand"), qsTr("Skill Use"),
                          qsTr("Dead"), qsTr("Damage"), qsTr("Bad Status"), qsTr("Defending"), qsTr("Walking Left"),
                          qsTr("Walking Right"), qsTr("Victory"), qsTr("Item")]
        const list = battler ? battler.list("poses") : null
        return list && list.count > 0 ? names(list) : choices(defaults)
    }
    readonly property var weapons: names(battler ? battler.list("weapons") : null)

    objectName: "BattlerAnimationDialog"
    title: weapon ? qsTr("Weapon Animation") : qsTr("Skill Animation")
    preferredWidth: 360

    Component.onCompleted: {
        const all = []
        const list = entry ? entry.list(key) : null
        const count = heroes ? heroes.count : 0
        for (let row = 0; row < count; ++row) {
            // a hero past the end of the list: the settings of a new one
            const shown = { type: 0, weapon_animation_id: 0, movement: 0, after_image: 0, attacks: 0, ranged: false,
                            ranged_animation_id: 0, ranged_speed: 0, battle_animation_id: 1, pose: 3 }
            if (list && row < list.count) {
                for (const field of fields) {
                    shown[field] = field === "ranged" ? list.boolean(row + "/" + field) : list.num(row + "/" + field)
                }
            }
            all.push(shown)
        }
        settings = all
        if (entry && weapon) {
            trajectory = entry.num("ranged_trajectory")
            rangedTarget = entry.num("ranged_target")
        }
    }
    onAccepting: (check) => {
        if (!entry) {
            return
        }
        const list = entry.list(key)
        entry.beginBatch()
        if (list.count < settings.length) {
            list.resize(settings.length)
        }
        for (let row = 0; row < settings.length; ++row) {
            for (const field of fields) {
                list.set(row + "/" + field, settings[row][field])
            }
        }
        if (weapon) {
            entry.set("ranged_trajectory", trajectory)
            entry.set("ranged_target", rangedTarget)
        }
        entry.endBatch()
    }

    // no hero to set: all greyed
    ColumnLayout {
        spacing: Ez.Metrics.sectionSpacing
        enabled: root.settings.length > 0
        Layout.fillWidth: true

        Ez.Form {
            Ez.FormLabel {
                text: qsTr("Hero")
            }
            Ez.ComboBox {
                id: heroBox
                objectName: "comboHero"
                model: root.heroes
                currentIndex: 0
                Layout.fillWidth: true
            }
            Ez.FormLabel {
                text: root.weapon ? qsTr("Movement Before Attack") : qsTr("Movement")
            }
            Ez.ComboBox {
                objectName: "comboMovement"
                model: root.choices(root.weapon ? [qsTr("None"), qsTr("Step Forward"), qsTr("Jump Forward"), qsTr("Move to Target")]
                                                : [qsTr("None"), qsTr("Step Forward"), qsTr("Jump Forward")])
                currentIndex: root.current && count > 0 ? indexOfValue(root.current.movement) : -1
                onActivated: root.setField("movement", currentValue)
                Layout.fillWidth: true
            }
            Ez.FormLabel {
                text: qsTr("Afterimage")
            }
            RowLayout {
                spacing: 2 * Ez.Metrics.columnSpacing
                // liblcf's values: 1 adds one, 0 none
                Ez.RadioButton {
                    objectName: "radioAfterimageAdd"
                    text: qsTr("Add")
                    checked: !!root.current && root.current.after_image === 1
                    onToggled: root.setField("after_image", 1)
                }
                Ez.RadioButton {
                    objectName: "radioAfterimageNone"
                    text: qsTr("None")
                    checked: !!root.current && root.current.after_image === 0
                    onToggled: root.setField("after_image", 0)
                }
            }
            Ez.FormLabel {
                visible: !root.weapon
                text: qsTr("Pose")
            }
            Ez.ComboBox {
                objectName: "comboPose"
                visible: !root.weapon
                model: root.poses
                currentIndex: root.current && count > 0 ? indexOfValue(root.current.pose) : -1
                onActivated: root.setField("pose", currentValue)
                Layout.fillWidth: true
            }
        }

        // a weapon: what it draws, how many times it attacks, its ranged attack
        Ez.Section {
            visible: root.weapon
            title: qsTr("Animation Type")
            Ez.Form {
                Ez.RadioButton {
                    objectName: "radioWeaponAnimation"
                    text: qsTr("Weapon")
                    checked: !!root.current && root.current.type === 0
                    onToggled: root.setField("type", 0)
                }
                Ez.ComboBox {
                    objectName: "comboWeaponAnimation"
                    model: root.weapons
                    enabled: !!root.current && root.current.type === 0
                    currentIndex: root.current && count > 0 ? indexOfValue(root.current.weapon_animation_id) : -1
                    onActivated: root.setField("weapon_animation_id", currentValue)
                    Layout.fillWidth: true
                }
                Ez.RadioButton {
                    objectName: "radioBattleAnimation"
                    text: qsTr("Battle Animation")
                    checked: !!root.current && root.current.type === 1
                    onToggled: root.setField("type", 1)
                }
                Ez.RefComboBox {
                    id: battleAnimation
                    objectName: "comboBattleAnimation"
                    database: root.database
                    listKey: "animations"
                    enabled: !!root.current && root.current.type === 1
                    onActivated: root.setField("battle_animation_id", currentValue)
                    Layout.fillWidth: true

                    /** Shows the hero's (after the combo box's own choice of a new list) */
                    function show() {
                        currentIndex = root.current && count > 0 ? indexOfValue(root.current.battle_animation_id) : -1
                    }
                    onCountChanged: Qt.callLater(show)
                    Connections {
                        target: root
                        function onCurrentChanged() { battleAnimation.show() }
                    }
                }
            }
        }
        Ez.Form {
            visible: root.weapon
            Ez.FormLabel {
                text: qsTr("Number of Attacks")
            }
            Ez.ComboBox {
                objectName: "comboAttacks"
                model: root.choices([qsTr("Once"), qsTr("Twice"), qsTr("Three Times")])
                currentIndex: root.current && count > 0 ? indexOfValue(root.current.attacks) : -1
                onActivated: root.setField("attacks", currentValue)
                Layout.fillWidth: true
            }
        }
        Ez.Section {
            id: rangedSection
            visible: root.weapon
            title: qsTr("Ranged Weapon")
            checkable: true
            checkName: "groupRanged"
            onToggled: root.setField("ranged", checked)
            Ez.Form {
                Ez.FormLabel {
                    text: qsTr("Weapon Animation")
                }
                Ez.ComboBox {
                    objectName: "comboRangedAnimation"
                    model: root.weapons
                    currentIndex: root.current && count > 0 ? indexOfValue(root.current.ranged_animation_id) : -1
                    onActivated: root.setField("ranged_animation_id", currentValue)
                    Layout.fillWidth: true
                }
                // the weapon's own, not each hero's
                Ez.FormLabel {
                    text: qsTr("Trajectory")
                }
                Ez.ComboBox {
                    objectName: "comboTrajectory"
                    model: root.choices([qsTr("Fly Straight to Target"), qsTr("Return to User After Striking")])
                    currentIndex: count > 0 ? indexOfValue(root.trajectory) : -1
                    onActivated: root.trajectory = currentValue
                    Layout.fillWidth: true
                }
                Ez.FormLabel {
                    text: qsTr("Targeting")
                }
                Ez.ComboBox {
                    objectName: "comboTargeting"
                    model: root.choices([qsTr("Single Target"), qsTr("Fly Down the Center of the Screen"),
                                         qsTr("Strike Each Enemy Simultaneously"), qsTr("Strike Each Enemy in Turn")])
                    currentIndex: count > 0 ? indexOfValue(root.rangedTarget) : -1
                    onActivated: root.rangedTarget = currentValue
                    Layout.fillWidth: true
                }
                Ez.FormLabel {
                    text: qsTr("Animation Speed")
                }
                Ez.ComboBox {
                    objectName: "comboSpeed"
                    model: root.choices([qsTr("Fast"), qsTr("Medium"), qsTr("Slow")])
                    currentIndex: root.current && count > 0 ? indexOfValue(root.current.ranged_speed) : -1
                    onActivated: root.setField("ranged_speed", currentValue)
                    Layout.fillWidth: true
                }
            }
        }
    }
}
