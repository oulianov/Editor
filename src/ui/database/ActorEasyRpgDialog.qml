// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "ActorWidget"
pragma ComponentBehavior: Bound

import QtQuick
import org.easyrpg.editor as Ez

/**
 * The EasyRPG Properties of a hero (Edit... on the Characters page): the
 * settings of liblcf's easyrpg_* fields that only EasyRPG Player reads, as
 * check boxes (checkPreventCritical...) and the hit rate of its unarmed
 * attacks (spinUnarmedHit, -1: RPG Maker's 90%). OK writes them.
 *
 *   Ez.Editor.exec("ActorEasyRpgDialog", { entry: page.entry }, button)
 */
Ez.DialogWindow {
    id: root

    /** The hero (a JsonView) */
    property var entry: null

    /** The options: check box, text, field; the first three are defenses */
    readonly property var options: [
        { name: "checkPreventCritical", text: qsTr("Prevent critical hits"), key: "easyrpg_prevent_critical" },
        { name: "checkRaiseEvasion", text: qsTr("Raise evasion"), key: "easyrpg_raise_evasion" },
        { name: "checkImmuneAttributeDownshifts", text: qsTr("Immune to attribute reductions"),
          key: "easyrpg_immune_to_attribute_downshifts" },
        { name: "checkIgnoreEvasion", text: qsTr("Ignore evasion"), key: "easyrpg_ignore_evasion" },
        { name: "checkDualAttack", text: qsTr("Attack twice"), key: "easyrpg_dual_attack" },
        { name: "checkAttackAll", text: qsTr("Attack all enemies"), key: "easyrpg_attack_all" },
    ]
    /** The values shown (written on OK) */
    property var values: ({})

    objectName: "ActorEasyRpgDialog"
    title: qsTr("EasyRPG Properties")

    Component.onCompleted: {
        const shown = {}
        if (entry) {
            for (const flag of options) {
                shown[flag.key] = entry.boolean(flag.key)
            }
            shown.easyrpg_unarmed_hit = entry.num("easyrpg_unarmed_hit")
        }
        values = shown
    }
    onAccepting: (check) => {
        if (!entry) {
            return
        }
        entry.beginBatch()
        for (const key in values) {
            entry.set(key, values[key])
        }
        entry.endBatch()
    }

    /** A check box of a flag of the copy */
    component Flag: Ez.CheckBox {
        id: flag
        required property var modelData
        objectName: modelData.name
        text: modelData.text
        checked: !!root.values[modelData.key]
        onToggled: {
            const changed = Object.assign({}, root.values)
            changed[flag.modelData.key] = checked
            root.values = changed
        }
    }

    Ez.Note {
        text: qsTr("EasyRPG Player reads these settings; RPG Maker ignores them.")
    }
    Ez.Section {
        title: qsTr("Defense")
        Repeater {
            model: root.options.slice(0, 3)
            Flag {}
        }
    }
    Ez.Section {
        title: qsTr("Unarmed Attacks")
        Repeater {
            model: root.options.slice(3)
            Flag {}
        }
        Ez.Form {
            Ez.FormLabel {
                text: qsTr("Hit rate")
            }
            Ez.SpinBox {
                objectName: "spinUnarmedHit"
                // -1: the hit rate of RPG Maker, 90%
                from: -1
                to: 100
                value: root.values.easyrpg_unarmed_hit ?? -1
                textFromValue: function(value, locale) {
                    return value < 0 ? qsTr("Default (90%)") : value + qsTr("%")
                }
                valueFromText: function(text, locale) {
                    if (text === qsTr("Default (90%)")) {
                        return -1
                    }
                    return Number.fromLocaleString(locale, text.replace(qsTr("%"), "").trim())
                }
                onValueModified: {
                    const changed = Object.assign({}, root.values)
                    changed.easyrpg_unarmed_hit = value
                    root.values = changed
                }
            }
        }
    }
}
