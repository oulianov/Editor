// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "ClassWidget"

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * The experience curve of a hero or a class: its three settings in a few
 * words (labelExpCurve, as the game's engine names them) and Edit...
 * (pushExpCurveEdit), opening the Experience Curve window on them.
 *
 *   ExperienceSummary { entry: page.entry; database: page.database }
 */
Ez.Section {
    id: root

    /** The hero or class (a JsonView) */
    property var entry: null
    property var database: null
    /** The button beside the text (the Characters page), else under it */
    property bool horizontal: false

    title: qsTr("Experience Curve")

    Ez.JsonField {
        id: base
        source: root.entry
        path: "exp_base"
    }
    Ez.JsonField {
        id: inflation
        source: root.entry
        path: "exp_inflation"
    }
    Ez.JsonField {
        id: correction
        source: root.entry
        path: "exp_correction"
    }

    GridLayout {
        columns: root.horizontal ? 2 : 1
        rowSpacing: Ez.Metrics.rowSpacing
        columnSpacing: Ez.Metrics.columnSpacing
        Layout.fillWidth: true

        Ez.Note {
            objectName: "labelExpCurve"
            text: base.valid ? Ez.Curves.experience(root.database, Number(base.value), Number(inflation.value),
                                                    Number(correction.value)).summary : ""
        }
        Ez.Button {
            id: edit
            objectName: "pushExpCurveEdit"
            text: qsTr("Edit...")
            Layout.fillWidth: !root.horizontal
            onClicked: Ez.Editor.exec("ExperienceDialog", { database: root.database, entry: root.entry }, edit)
        }
    }
}
