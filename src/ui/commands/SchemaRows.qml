// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * Rows of fields of the command's schema, for the hand-written editors: a
 * label and the controls of a field per row (SchemaField), the fields
 * marked inline on the row of the one before. A row whose rules fail is
 * hidden (the page of another mode), or greyed with its default value
 * (hides: false). A check box or a field without a label takes the whole row.
 * A field of an add-on (its origin, Ez.Origins) has its tag after its label
 * (objectName: the field's and "Tag"), a warning one when the command has
 * a value of it and the game does not run it.
 *
 *   SchemaRows { session: root.session; ids: ["switch", "switchState"] }
 */
GridLayout {
    id: root

    required property Ez.CommandEditSession session
    /** The ids of the fields, in order */
    property var ids: []
    /** A field whose rules fail is hidden, else greyed */
    property bool hides: true
    /** The objectNames of the controls of fields, by id (those its editor gives); else the id */
    property var names: ({})
    /** A row of a field is shown (besides its rules): a function of the field id */
    property var showRow: null

    /** A text of the schema in the language of the interface */
    function tr(text) {
        if (!text || !text.text) {
            return ""
        }
        const shown = text.context === "" ? text.text : qsTranslate(text.context, text.text)
        return text.number >= 0 ? shown.arg(text.number) : shown
    }

    readonly property var fieldRows: {
        const list = []
        for (const id of ids) {
            const field = session.schema.field(id)
            if (!field.id) {
                console.warn("SchemaRows: no field", id)
                continue
            }
            if (field.inline && list.length > 0) {
                list[list.length - 1].fields.push(field)
            } else {
                list.push({ field: field, fields: [field],
                            labelled: !!(field.label && field.label.text) && field.kind !== "Check" && field.kind !== "Note" })
            }
        }
        return list
    }

    columns: 2
    rowSpacing: Ez.Metrics.rowSpacing
    columnSpacing: Ez.Metrics.columnSpacing
    Layout.fillWidth: true

    component RowRules: QtObject {
        required property var row
        readonly property int revision: root.session.revision
        readonly property bool visible: {
            if (revision < 0) {
                return false
            }
            // of an add-on the game does not run: hidden unless the command has it (Editor Settings)
            return (!root.hides || root.session.isVisible(row.field.id)) && (!root.showRow || root.showRow(row.field.id))
                   && root.session.originShown(row.field.id)
        }
        readonly property bool enabled: revision < 0 ? false : root.session.isVisible(row.field.id) && root.session.isEnabled(row.field.id)
    }

    Repeater {
        model: root.fieldRows
        delegate: Ez.FormLabel {
            id: label
            required property var modelData
            required property int index
            readonly property RowRules rowState: RowRules { row: label.modelData }
            /** The add-on of the field, else of its item chosen (a list) */
            readonly property string origin: {
                const field = modelData.field
                if (field.origin || rowState.revision < 0) {
                    return field.origin ?? ""
                }
                const value = root.session.read(field.id)
                const item = (field.items ?? []).find(one => one.value === value)
                return item ? item.origin ?? "" : ""
            }
            text: root.tr(modelData.field.label)
            visible: modelData.labelled && rowState.visible
            enabled: rowState.enabled
            Layout.row: index
            Layout.column: 0
            // its tag after the text
            rightPadding: labelTag.width > 0 ? labelTag.width + Ez.Metrics.columnSpacing : 0
            Ez.OriginTag {
                id: labelTag
                readonly property string text: shown ? Ez.Origins.letter(origin) + (warning ? "!" : "") : ""
                objectName: (root.names[label.modelData.field.id] ?? label.modelData.field.id) + "Tag"
                origin: label.origin
                warning: !Ez.Origins.available(origin) && label.rowState.revision >= 0
                         && (!label.modelData.field.origin || root.session.isSet(label.modelData.field.id))
                x: label.leftPadding + label.contentWidth + Ez.Metrics.columnSpacing
                anchors.verticalCenter: parent.verticalCenter
            }
            Ez.OriginTip {
                origin: label.origin
            }
        }
    }
    Repeater {
        model: root.fieldRows
        delegate: RowLayout {
            id: controls
            required property var modelData
            required property int index
            readonly property RowRules rowState: RowRules { row: controls.modelData }
            spacing: Ez.Metrics.columnSpacing
            visible: rowState.visible
            Layout.row: index
            Layout.column: modelData.labelled ? 1 : 0
            Layout.columnSpan: modelData.labelled ? 1 : 2
            Layout.fillWidth: true

            Repeater {
                model: controls.modelData.fields
                SchemaField {
                    required property var modelData
                    required property int index
                    readonly property bool fieldVisible: controls.rowState.revision < 0 ? false : root.session.isVisible(modelData.id)
                    session: root.session
                    field: modelData
                    name: root.names[modelData.id] ?? modelData.id
                    bound: fieldVisible
                    // the first is the row's; the next ones: hidden when their rules fail
                    visible: (index === 0 || fieldVisible) && (controls.rowState.revision < 0 || root.session.originShown(modelData.id))
                    enabled: controls.rowState.revision < 0 ? false : fieldVisible && root.session.isEnabled(modelData.id)
                }
            }
            Item {
                // the controls keep their width, the row reaches the end of the cell
                Layout.fillWidth: !controls.modelData.fields.some(one => one.list !== ""
                    || ["File", "String", "Map", "Character", "Choice", "Tile"].includes(one.kind))
            }
        }
    }
}
