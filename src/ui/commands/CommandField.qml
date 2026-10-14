// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick

/**
 * The control of one field of the command's schema in a hand-written
 * editor (SchemaField), by its id, following the rules of the field: greyed
 * with its default value when they fail (or hidden: hides), greyed when it
 * cannot be changed.
 *
 *   CommandField { session: root.session; fieldId: "troop"; name: "encounterTroop" }
 */
SchemaField {
    id: root

    required property string fieldId
    /** Hidden when its rules fail, else greyed */
    property bool hides: false
    readonly property int revision: session.revision
    readonly property bool ruleVisible: revision < 0 ? false : session.isVisible(fieldId)

    field: session.schema.field(fieldId)
    bound: ruleVisible
    visible: !hides || ruleVisible
    enabled: revision < 0 ? false : ruleVisible && session.isEnabled(fieldId)
}
