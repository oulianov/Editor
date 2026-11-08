// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "EasyRpgCommandWidget"

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * Set Interpreter Flag (EasyRPG Player): the patch, named in the string of
 * the command, from the names EasyRPG Player knows (in any case,
 * Game_Interpreter::CommandEasyRpgSetInterpreterFlag); a name it does not
 * know is kept, and shown as written. Then the form of its schema: the
 * state, a constant or a variable.
 */
ColumnLayout {
    id: root

    required property Ez.CommandEditSession session
    readonly property string title: qsTranslate("EventCommandPalette", "Set Interpreter Flag")
    readonly property int revision: session.revision
    readonly property string patch: revision < 0 ? "" : String(session.value("/string") ?? "")

    /** The names EasyRPG Player knows, its alias rpg2k3-commands shown as rpg2k3-cmds */
    readonly property var patches: [
        { value: "maniac", text: qsTranslate("GamePatches", "Maniac Patch") },
        { value: "dynrpg", text: qsTranslate("GamePatches", "DynRPG") },
        { value: "key-patch", text: qsTranslate("GamePatches", "Ineluki Key Patch") },
        { value: "common-this", text: qsTranslate("GamePatches", "Common This Event") },
        { value: "pic-unlock", text: qsTranslate("GamePatches", "Unlock Pictures") },
        { value: "rpg2k3-cmds", text: qsTranslate("GamePatches", "RPG Maker 2003 Commands") },
        { value: "rpg2k-battle", text: qsTr("RPG Maker 2000 Battle System") },
        { value: "destiny", text: qsTr("Destiny Patch") }]

    /** The patch of a name, in any case; -1 for none */
    function indexOf(name) {
        const lower = name.toLowerCase() === "rpg2k3-commands" ? "rpg2k3-cmds" : name.toLowerCase()
        return patches.findIndex(one => one.value === lower)
    }
    readonly property var choices: {
        const list = patches.map(one => ({ value: one.value, text: "%1 (%2)".arg(one.text).arg(one.value) }))
        if (indexOf(patch) < 0) {
            list.push({ value: patch, text: patch === "" ? qsTr("(None)") : qsTr("Other: %1").arg(patch) })
        }
        return list
    }

    spacing: Ez.Metrics.sectionSpacing
    Layout.fillWidth: true
    Layout.fillHeight: true

    Ez.Section {
        title: qsTr("Patch")
        Layout.fillWidth: true

        Ez.ComboBox {
            objectName: "comboPatch"
            textRole: "text"
            valueRole: "value"
            model: root.choices
            Layout.fillWidth: true
            Binding on currentIndex {
                value: { const known = root.indexOf(root.patch); return known >= 0 ? known : root.choices.length - 1 }
            }
            // the name as written stays while the patch is the same
            onActivated: if (root.indexOf(root.patch) !== root.indexOf(currentValue) || root.indexOf(currentValue) < 0) root.session.set("/string", currentValue)
        }
    }

    SchemaForm {
        session: root.session
        only: [1]
        Layout.fillHeight: false
    }

    Item {
        Layout.fillHeight: true
    }
}
