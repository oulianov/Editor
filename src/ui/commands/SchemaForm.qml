// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * The editor of a command made from its schema (CommandSchema, through
 * session.schema): its sections, each a form of a label and the controls of
 * a field per row (SchemaField). The radio buttons of a mode carry on their
 * line the value of the mode ("Constant: [5]", "By Variable: [V0001]"), as
 * RPG Maker does.
 *
 * The rules of the fields grey what does not apply (its default shown, not
 * written), or hide it in the editors of the Maniac Patch; a field the
 * engine lacks (RPG Maker 2003, the Maniac Patch) is greyed.
 *
 * The controls are named after the fields (objectName: the id), a radio
 * button after its field and its value ("target1").
 */
ColumnLayout {
    id: root

    required property Ez.CommandEditSession session
    readonly property string title: tr(session.schema.title)
    /** The sections shown, by index (a column of a hand-written editor); null: all */
    property var only: null
    /** The objectNames of the controls of fields, by id (those a hand-written editor gives); else the id */
    property var names: ({})

    /** The items of the Maniac Patch: hidden in other games unless the command has them (as RPG Maker's windows) */
    property bool hideManiacItems: false
    /** The fields not shown, by id (a hand-written editor shows them its way) */
    property var hides: []

    function nameOf(id) {
        return names[id] ?? id
    }
    /** The objectName of the radio button of an item: names[id + value], else the field's + value */
    function radioName(field, item) {
        return names[field.id + item.value] ?? nameOf(field.id) + item.value
    }
    /** An item hidden: of the Maniac Patch in another game, not the command's */
    function itemHidden(field, item) {
        return hideManiacItems && !!item && item.maniac && !session.maniac && session.read(field.id) !== item.value
    }

    /** A text of the schema in the language of the interface */
    function tr(text) {
        if (!text || !text.text) {
            return ""
        }
        const shown = text.context === "" ? text.text : qsTranslate(text.context, text.text)
        return text.number >= 0 ? shown.arg(text.number) : shown
    }

    /** The text of a radio button of a choice (the pages of the keyboard: in the game's encoding) */
    function itemText(field, item) {
        if (field.kind === "CharacterSet") {
            const page = session.characterSets.find(one => one.value === item.value)
            return tr(page ? page.text : item.text)
        }
        return tr(item.text)
    }

    /** A field whose control takes the width of its row */
    function isWide(field) {
        return field.list !== "" || ["File", "String", "Map", "Character", "Choice", "Tile", "Location"].includes(field.kind)
    }

    /** The cells of a section's grid: a label and its controls per row (or one cell over both columns) */
    function cellsOf(section) {
        const cells = []
        for (const row of section.rows) {
            if (hides.includes(row.field.id)) {
                continue
            }
            const field = row.field
            const options = row.options ?? []
            const labelled = !!(field.label && field.label.text) && field.kind !== "Check" && field.kind !== "Note"
            if (field.radio && options.some(option => option.inline.length > 0)) {
                // a radio button per line, the values of its mode after it
                for (const option of options) {
                    cells.push({ type: "radio", field: field, item: option.item, fields: option.inline, option: true, span: 1 })
                    cells.push({ type: "controls", field: field, item: option.item, fields: option.inline, option: true, span: 1 })
                }
                if (row.inline.length > 0) {
                    cells.push({ type: "controls", field: field, fields: row.inline, option: false, span: 2 })
                }
            } else if (field.radio) {
                if (labelled) {
                    cells.push({ type: "label", field: field, fields: [], option: false, span: 1 })
                }
                cells.push({ type: "radios", field: field, fields: row.inline, option: false, span: labelled ? 1 : 2 })
            } else if (labelled) {
                cells.push({ type: "label", field: field, fields: [], option: false, span: 1 })
                cells.push({ type: "controls", field: field, fields: [field].concat(row.inline), option: false, span: 1 })
            } else {
                cells.push({ type: "controls", field: field, fields: [field].concat(row.inline), option: false, span: 2 })
            }
        }
        return cells
    }

    spacing: Ez.Metrics.sectionSpacing
    Layout.fillWidth: true
    Layout.fillHeight: true

    Repeater {
        model: root.only ? root.session.schema.sections.filter((section, index) => root.only.includes(index))
                         : root.session.schema.sections

        delegate: ColumnLayout {
            id: section

            required property var modelData
            readonly property string title: root.tr(modelData.title)
            /** One of its fields is shown */
            readonly property bool shown: root.session.revision < 0 ? false
                                         : modelData.rows.some(row => root.session.isShown(row.field.id))

            visible: shown
            spacing: 0
            Layout.fillWidth: true

            Ez.Section {
                id: box
                visible: section.title !== ""
                title: section.title
            }

            GridLayout {
                id: grid
                // in the frame of the section, else on its own
                parent: section.title !== "" ? box.body : section
                columns: 2
                rowSpacing: Ez.Metrics.rowSpacing
                columnSpacing: Ez.Metrics.columnSpacing
                Layout.fillWidth: true

                Repeater {
                    model: root.cellsOf(section.modelData)

                    delegate: Loader {
                        id: cell

                        required property var modelData
                        readonly property var field: modelData.field
                        // the rules, read again on every change
                        readonly property int revision: root.session.revision
                        readonly property bool fieldShown: revision < 0 ? false : root.session.isShown(field.id)
                        readonly property bool fieldVisible: revision < 0 ? false : root.session.isVisible(field.id)
                        readonly property bool fieldEnabled: revision < 0 ? false : fieldVisible && root.session.isEnabled(field.id)

                        visible: fieldShown && (revision < 0 || !root.itemHidden(field, modelData.item))
                        enabled: fieldEnabled
                        Layout.columnSpan: modelData.span
                        Layout.fillWidth: modelData.type === "controls" || modelData.type === "radios"
                        Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
                        sourceComponent: modelData.type === "label" ? label
                                       : modelData.type === "radio" ? radio
                                       : modelData.type === "radios" ? radios : controls

                        Component {
                            id: label
                            Ez.FormLabel {
                                text: root.tr(cell.field.label)
                            }
                        }

                        Component {
                            id: radio
                            Ez.RadioButton {
                                readonly property var item: cell.modelData.item
                                objectName: root.radioName(cell.field, item)
                                // "Constant:", the value after it
                                text: root.itemText(cell.field, item) + (cell.modelData.fields.length > 0 ? ":" : "")
                                enabled: (!item.rpg2003 || root.session.rpg2k3) && (!item.maniac || root.session.maniac)
                                jsonData: root.session
                                key: cell.fieldVisible ? cell.field.path : "/defaults/" + cell.field.id
                                value: item.value
                            }
                        }

                        Component {
                            id: radios
                            RowLayout {
                                spacing: 2 * Ez.Metrics.columnSpacing
                                Repeater {
                                    model: cell.field.items
                                    Ez.RadioButton {
                                        required property var modelData
                                        objectName: root.radioName(cell.field, modelData)
                                        text: root.itemText(cell.field, modelData)
                                        visible: cell.revision < 0 ? false : (!modelData.kept || root.session.read(cell.field.id) === modelData.value)
                                                                         && !root.itemHidden(cell.field, modelData)
                                        enabled: (!modelData.rpg2003 || root.session.rpg2k3) && (!modelData.maniac || root.session.maniac)
                                        jsonData: root.session
                                        key: cell.fieldVisible ? cell.field.path : "/defaults/" + cell.field.id
                                        value: modelData.value
                                    }
                                }
                                Repeater {
                                    model: cell.modelData.fields
                                    SchemaField {
                                        required property var modelData
                                        readonly property bool fieldVisible: cell.revision < 0 ? false : root.session.isVisible(modelData.id)
                                        session: root.session
                                        field: modelData
                                        name: root.nameOf(modelData.id)
                                        bound: fieldVisible
                                        visible: fieldVisible
                                        enabled: cell.revision < 0 ? false : fieldVisible && root.session.isEnabled(modelData.id)
                                    }
                                }
                                Item {
                                    Layout.fillWidth: true
                                }
                            }
                        }

                        Component {
                            id: controls
                            RowLayout {
                                spacing: Ez.Metrics.columnSpacing
                                Repeater {
                                    model: cell.modelData.fields
                                    SchemaField {
                                        required property var modelData
                                        required property int index
                                        readonly property bool fieldVisible: cell.revision < 0 ? false : root.session.isVisible(modelData.id)
                                        session: root.session
                                        field: modelData
                                        name: root.nameOf(modelData.id)
                                        bound: fieldVisible
                                        // the first is the row's (its cell shows and greys it); the
                                        // next ones: the value of a mode, hidden when another mode
                                        // shows its own in its place, greyed on the line of its radio
                                        // button not chosen
                                        visible: (index === 0 && !cell.modelData.option) || cell.modelData.option || fieldVisible
                                        enabled: cell.revision < 0 ? false : fieldVisible && root.session.isEnabled(modelData.id)
                                    }
                                }
                                Item {
                                    // the controls keep their width, the row reaches the end of the cell
                                    Layout.fillWidth: !cell.modelData.fields.some(one => root.isWide(one))
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Ez.Note {
        // RPG Maker 2000 has neither classes nor battle commands: kept, not editable
        visible: !root.only && !root.session.engineHasCommand()
        text: qsTranslate("HeroCommandWidget", "This command requires RPG Maker 2003.")
    }

    // the sections at the top, the buttons at the bottom
    Item {
        Layout.fillHeight: true
    }
}
