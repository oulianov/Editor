// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import org.easyrpg.editor as Ez

/**
 * A radio button of a choice of the command's schema in a hand-written
 * editor: checked when the field (by its id) has value, its text the item's.
 * Greyed when the field cannot be changed, or the item comes from an add-on
 * the game does not run (its origin, Ez.Origins: RPG Maker 2003's commands
 * in a 2000 game, the Maniac Patch): then its tooltip says what the game
 * needs, or it is hidden (Editor Settings) unless the command has it. An
 * item of an add-on has its tag after its text (objectName: the button's
 * and "Tag"), a warning one when the command has it and the game does not
 * run it.
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
    /** The add-on of the item, else of the field (Ez.Origins), "" for none */
    readonly property string origin: (item && item.origin) ? item.origin : field.origin ?? ""

    text: {
        const label = item ? item.text : null
        if (!label || !label.text) {
            return ""
        }
        const shown = label.context === "" ? label.text : qsTranslate(label.context, label.text)
        return label.number >= 0 ? shown.arg(label.number) : shown
    }
    enabled: revision < 0 ? false : session.isVisible(fieldId) && session.isEnabled(fieldId) && Ez.Origins.available(origin)
    // not offered (Editor Settings) unless the command has it: the item, or a value of the field's add-on
    visible: Ez.Origins.shown(origin) || checked || (revision >= 0 && !(item && item.origin) && session.isSet(fieldId))
    jsonData: session
    key: field.path ?? ""

    // its tag after the text
    rightPadding: horizontalPadding + (tag.width > 0 ? tag.width + Ez.Metrics.columnSpacing : 0)
    Ez.OriginTag {
        id: tag
        readonly property string text: shown ? Ez.Origins.letter(origin) + (warning ? "!" : "") : ""
        objectName: root.objectName + "Tag"
        origin: root.origin
        warning: root.checked && !Ez.Origins.available(origin)
        x: root.leftPadding + root.contentItem.implicitWidth + Ez.Metrics.columnSpacing
        anchors.verticalCenter: parent.verticalCenter
    }
    Ez.OriginTip {
        origin: root.origin
    }
}
