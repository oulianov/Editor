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
 * written), or hide it in the editors of the Maniac Patch.
 *
 * A field or an option of an add-on (its origin: the Maniac Patch, RPG
 * Maker 2003's commands in a 2000 game, Ez.Origins) has its tag after its
 * label or text; while the game does not run it, it is greyed, its tooltip
 * saying what the game needs, or hidden (Editor Settings: Ez.Origins.shown,
 * the session's originShown). One the command already uses stays as it
 * is, its tag a warning one. The tags are named after their field ("idTag", an
 * option's "id" and value and "Tag"), their text the letter ("M", "M!").
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

    /** The items of the Maniac Patch: hidden in other games unless the command has them (else greyed, with their tag) */
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
        if (!item || session.read(field.id) === item.value) {
            return false
        }
        // of an add-on the game does not run, not offered (Editor Settings)
        return (hideManiacItems && item.maniac && !session.maniac) || !Ez.Origins.shown(item.origin ?? "")
    }
    /** An item of a row of radio buttons shown: of the engine, or the command's */
    function itemShown(field, item) {
        return (!item.kept || session.read(field.id) === item.value) && !itemHidden(field, item)
    }
    /** The origin a field shows the tag of: its own, else the one of its item chosen (a list) */
    function originOf(field) {
        if (field.origin) {
            return field.origin
        }
        if (!field.radio && (field.items ?? []).length > 0 && session.revision >= 0) {
            const value = session.read(field.id)
            const item = field.items.find(one => one.value === value)
            return item ? item.origin ?? "" : ""
        }
        return ""
    }
    /** The command uses what comes from origin in the field, and the game does not run it */
    function warns(field, origin) {
        if (!origin || Ez.Origins.available(origin) || session.revision < 0) {
            return false
        }
        return field.origin ? session.isSet(field.id) : true
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

    /** A sample beside the fields of its section (RPG Maker's sample of a flash) */
    function isSide(field) {
        return field.kind === "Sample" && field.link === "flash"
    }

    /** The cells of a section's grid: a label and its controls per row (or one cell over both columns) */
    function cellsOf(section) {
        const cells = []
        for (const row of section.rows) {
            if (hides.includes(row.field.id) || isSide(row.field)) {
                continue
            }
            const field = row.field
            const options = row.options ?? []
            const labelled = !!(field.label && field.label.text) && field.kind !== "Check" && field.kind !== "Note"
            // (the options of the Maniac Patch only in its games)
            if (field.radio && options.some(option => option.inline.length > 0 && (!option.item.maniac || !hideManiacItems || session.maniac)
                                                && Ez.Origins.shown(option.item.origin ?? ""))) {
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

    /** The sections shown, in rows: one marked beside goes at the right of the one before it */
    readonly property var sectionRows: {
        const shown = only ? session.schema.sections.filter((section, index) => only.includes(index)) : session.schema.sections
        const rows = []
        for (const section of shown) {
            if (section.beside && rows.length > 0) {
                rows[rows.length - 1].push(section)
            } else {
                rows.push([section])
            }
        }
        return rows
    }

    Repeater {
        model: root.sectionRows

        delegate: RowLayout {
            id: sectionRow

            required property var modelData
            /** Sections side by side: the width of the widest for each, so that a
             *  value shown (a longer name, another radio button chosen) does not
             *  move the line between them */
            readonly property real share: {
                let widest = 0
                for (const child of children) {
                    if (child.visible && child.isSection) {
                        widest = Math.max(widest, child.implicitWidth)
                    }
                }
                return widest
            }

            visible: root.session.revision < 0 ? false
                     : modelData.some(one => one.rows.some(row => root.session.isShown(row.field.id)))
            spacing: Ez.Metrics.sectionSpacing
            Layout.fillWidth: true

        Repeater {
            model: sectionRow.modelData

        delegate: ColumnLayout {
            id: section

            required property var modelData
            readonly property string title: root.tr(modelData.title)
            /** One of its fields is shown */
            readonly property bool shown: root.session.revision < 0 ? false
                                         : modelData.rows.some(row => root.session.isShown(row.field.id))

            readonly property bool isSection: true

            visible: shown
            spacing: 0
            Layout.fillWidth: true
            // side by side: the same width, their tops aligned
            Layout.preferredWidth: sectionRow.modelData.length > 1 ? sectionRow.share : -1
            Layout.alignment: Qt.AlignTop

            Ez.Section {
                id: box
                visible: section.title !== ""
                title: section.title
            }

            RowLayout {
                id: content
                // in the frame of the section, else on its own
                parent: section.title !== "" ? box.body : section
                spacing: Ez.Metrics.sectionSpacing
                Layout.fillWidth: true

                // the sample at the left of the fields
                Repeater {
                    model: section.modelData.rows.filter(row => root.isSide(row.field))
                    SchemaField {
                        required property var modelData
                        session: root.session
                        field: modelData.field
                        enabled: root.session.revision < 0 ? false : root.session.isEnabled(modelData.field.id)
                        Layout.alignment: Qt.AlignTop
                    }
                }

            GridLayout {
                id: grid
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
                        // the controls of the fields hidden (the other modes, operands...) made once shown
                        active: visible
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
                                id: fieldLabel
                                readonly property string origin: cell.revision < 0 ? "" : root.originOf(cell.field)
                                text: root.tr(cell.field.label)
                                // its tag after the text
                                rightPadding: labelTag.width > 0 ? labelTag.width + Ez.Metrics.columnSpacing : 0
                                Ez.OriginTag {
                                    id: labelTag
                                    readonly property string text: shown ? Ez.Origins.letter(origin) + (warning ? "!" : "") : ""
                                    objectName: root.nameOf(cell.field.id) + "Tag"
                                    origin: fieldLabel.origin
                                    warning: root.warns(cell.field, fieldLabel.origin)
                                    x: fieldLabel.leftPadding + fieldLabel.contentWidth + Ez.Metrics.columnSpacing
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Ez.OriginTip {
                                    origin: fieldLabel.origin
                                }
                            }
                        }

                        Component {
                            id: radio
                            Ez.RadioButton {
                                id: optionRadio
                                readonly property var item: cell.modelData.item
                                readonly property string origin: item.origin ?? ""
                                objectName: root.radioName(cell.field, item)
                                // "Constant:", the value after it
                                text: root.itemText(cell.field, item) + (cell.modelData.fields.length > 0 ? ":" : "")
                                enabled: Ez.Origins.available(origin)
                                jsonData: root.session
                                key: cell.fieldVisible ? cell.field.path : "/defaults/" + cell.field.id
                                value: item.value
                                // its tag after the text
                                rightPadding: horizontalPadding + (optionTag.width > 0 ? optionTag.width + Ez.Metrics.columnSpacing : 0)
                                Ez.OriginTag {
                                    id: optionTag
                                    readonly property string text: shown ? Ez.Origins.letter(origin) + (warning ? "!" : "") : ""
                                    objectName: optionRadio.objectName + "Tag"
                                    origin: optionRadio.origin
                                    warning: optionRadio.checked && !Ez.Origins.available(origin)
                                    x: optionRadio.leftPadding + optionRadio.contentItem.implicitWidth + Ez.Metrics.columnSpacing
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Ez.OriginTip {
                                    origin: optionRadio.origin
                                }
                            }
                        }

                        Component {
                            id: radios
                            RowLayout {
                                spacing: 2 * Ez.Metrics.columnSpacing
                                // on one line; in columns, top to bottom; four directions as a cross (RPG Maker's)
                                GridLayout {
                                    id: radioGrid
                                    // the values of the items shown: their places follow
                                    readonly property var shownValues: cell.revision < 0 ? []
                                        : cell.field.items.filter(item => root.itemShown(cell.field, item)).map(item => item.value)
                                    // the items of the Maniac Patch shown: its columns
                                    readonly property int columnCount: cell.field.maniacColumns > 0
                                                                       && cell.field.items.some(item => item.maniac && shownValues.includes(item.value))
                                                                       ? cell.field.maniacColumns : cell.field.columns
                                    readonly property int lines: columnCount > 0 ? Math.ceil(shownValues.length / columnCount) : 1
                                    rowSpacing: cell.field.cross ? 0 : Ez.Metrics.rowSpacing
                                    columnSpacing: cell.field.cross ? 0 : 2 * Ez.Metrics.columnSpacing
                                Repeater {
                                    model: cell.field.items
                                    Ez.RadioButton {
                                        id: itemRadio
                                        required property var modelData
                                        required property int index
                                        readonly property string origin: modelData.origin ?? ""
                                        readonly property int place: Math.max(0, radioGrid.shownValues.indexOf(modelData.value))
                                        // the cross: Up, Left, Right, Down
                                        Layout.row: cell.field.cross ? [0, 1, 1, 2][index] ?? 0 : place % radioGrid.lines
                                        Layout.column: cell.field.cross ? [1, 0, 2, 1][index] ?? 0 : Math.floor(place / radioGrid.lines)
                                        objectName: root.radioName(cell.field, modelData)
                                        text: root.itemText(cell.field, modelData)
                                        visible: cell.revision < 0 ? false : root.itemShown(cell.field, modelData)
                                        enabled: Ez.Origins.available(origin)
                                        jsonData: root.session
                                        key: cell.fieldVisible ? cell.field.path : "/defaults/" + cell.field.id
                                        value: modelData.value
                                        // its tag after the text
                                        rightPadding: horizontalPadding + (itemTag.width > 0 ? itemTag.width + Ez.Metrics.columnSpacing : 0)
                                        Ez.OriginTag {
                                            id: itemTag
                                            readonly property string text: shown ? Ez.Origins.letter(origin) + (warning ? "!" : "") : ""
                                            objectName: itemRadio.objectName + "Tag"
                                            origin: itemRadio.origin
                                            warning: itemRadio.checked && !Ez.Origins.available(origin)
                                            x: itemRadio.leftPadding + itemRadio.contentItem.implicitWidth + Ez.Metrics.columnSpacing
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                        Ez.OriginTip {
                                            origin: itemRadio.origin
                                        }
                                    }
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
                                        visible: fieldVisible && root.session.originShown(modelData.id)
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
                                        // button not chosen. In the editors that hide what does not
                                        // apply (Maniac Patch, EasyRPG Player), the values of a radio
                                        // button's line that depend on other fields too show when those hold
                                        visible: ((index === 0 && !cell.modelData.option)
                                                  || (cell.modelData.option && (!root.session.schema.hides || cell.revision < 0
                                                                                || root.session.holdsExcept(modelData.id, cell.field.id)))
                                                  || fieldVisible)
                                                 && (cell.revision < 0 || root.session.originShown(modelData.id))
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
        }
        }
    }

    // a command of an add-on the game does not run: what the game does with
    // it (RPG Maker 2003's in a 2000 game: kept, not editable)
    RowLayout {
        visible: !root.only && !Ez.Origins.available(root.session.schema.origin)
        spacing: Ez.Metrics.columnSpacing
        Ez.OriginTag {
            origin: root.session.schema.origin
            warning: true
        }
        Ez.Note {
            objectName: "commandWarning"
            text: Ez.Origins.warning(root.session.schema.origin)
            Layout.fillWidth: true
        }
    }

    // the sections at the top, the buttons at the bottom
    Item {
        Layout.fillHeight: true
    }
}
