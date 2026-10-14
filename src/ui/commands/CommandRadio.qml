// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import org.easyrpg.editor as Ez

/**
 * A radio button of a choice of the command's schema in a hand-written
 * editor: checked when the field (by its id) has value, its text the item's.
 * Greyed when the field cannot be changed, or the item needs another engine
 * (RPG Maker 2003, the Maniac Patch) and is not the command's.
 *
 *   CommandRadio { session: root.session; fieldId: "escape"; value: 2; objectName: "encounterEscape2" }
 */
Ez.RadioButton {
    id: root

    required property Ez.CommandEditSession session
    required property string fieldId
    readonly property var field: session.schema.field(fieldId)
    readonly property var item: (field.items ?? []).find(one => one.value === root.value) ?? null
    readonly property int revision: session.revision

    text: {
        const label = item ? item.text : null
        if (!label || !label.text) {
            return ""
        }
        const shown = label.context === "" ? label.text : qsTranslate(label.context, label.text)
        return label.number >= 0 ? shown.arg(label.number) : shown
    }
    enabled: {
        if (revision < 0) {
            return false
        }
        const engine = !item || ((!item.rpg2003 || session.rpg2k3) && (!item.maniac || session.maniac))
        return session.isVisible(fieldId) && session.isEnabled(fieldId) && (engine || checked)
    }
    jsonData: session
    key: field.path ?? ""
}
