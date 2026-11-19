// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "VocabularyWidget"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * The terms of the game (the Vocabulary of the database) on the pages of
 * RPG Maker, from the VocabularyFields table: only the terms of the game's
 * engine (2000 or 2003), each field named after its term (battle_start...),
 * in the font and with the length mark of the game's windows. A message is
 * shown in its order: its terms and what the game puts between them (with
 * the Maniac Patch, its own texts around RPG Maker's: greyed in a game
 * without it, each with its tag M; when the Editor Settings hide what the
 * game does not run, RPG Maker's message unless one has a text). The groups of EasyRPG Player's terms
 * have its tag E after their title (M for those the Maniac Patch reads too,
 * in its games); such a term left empty is EasyRPG Player's text, shown
 * greyed.
 */
Item {
    id: root

    /** The database edited (a JsonView) */
    property var database: null
    property bool rpg2k3: true
    /** The game uses the Maniac Patch: the parts it adds to the messages are enabled */
    property bool maniac: false
    readonly property var terms: database ? database.subtree("terms") : null
    /** The widest label of a term (longer ones wrap) */
    readonly property int labelWidth: 220
    /** The widest text the game puts before a term (longer ones wrap) */
    readonly property int hintWidth: 140
    /** The page of terms shown */
    property alias currentIndex: tabs.currentIndex
    /** The parts the Maniac Patch adds are shown: the game runs it, else
     *  greyed (or hidden, as the Editor Settings say) */
    readonly property bool maniacShown: Ez.Origins.shown("maniac")
    // read again when a term changes (one the Maniac Patch adds, set by another program)
    property int termsRevision: 0
    Connections {
        target: root.terms
        ignoreUnknownSignals: true
        function onValueChanged() { root.termsRevision++ }
    }
    /** A row shows the Maniac Patch's message: its parts are shown, or one of
     *  them has a text the game has (kept, greyed) */
    function maniacLayout(row) {
        if (!row || !row.maniacLines) {
            return false
        }
        return maniacShown || (termsRevision >= 0 && !!terms && row.maniacTerms.some(term => terms.str(term) !== ""))
    }

    // RPG Maker 2000's battles in an RPG Maker 2003 game (EasyRPG Player's
    // option, System page): their terms are shown too
    Ez.JsonField {
        id: battle2k
        source: root.database
        path: "system/easyrpg_use_rpg2k_battle_system"
    }

    Ez.VocabularyTable {
        id: table
        rpg2k3: root.rpg2k3
        battle2k: battle2k.value === true
        maniac: root.maniac
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Controls.TabBar {
            id: tabs
            objectName: "tabVocabulary"
            Layout.fillWidth: true
            font: Ez.Metrics.font

            Repeater {
                model: table.pages
                Controls.TabButton {
                    required property var modelData
                    text: qsTranslate("VocabularyWidget", modelData.title)
                    width: implicitWidth
                    font: Ez.Metrics.font
                }
            }
        }

        StackLayout {
            currentIndex: tabs.currentIndex
            Layout.fillWidth: true
            Layout.fillHeight: true

            Repeater {
                model: table.pages

                DatabaseEntryPage {
                    id: page

                    required property var modelData

                    single: true
                    fillWidth: true
                    database: root.database
                    rpg2k3: root.rpg2k3
                    maniac: root.maniac

                    // the two columns of the page, each as wide
                    RowLayout {
                        spacing: Ez.Metrics.sectionSpacing
                        Layout.fillWidth: true

                        Repeater {
                            model: [0, 1]

                            ColumnLayout {
                                id: column
                                required property int modelData
                                spacing: Ez.Metrics.sectionSpacing
                                Layout.fillWidth: true
                                // as wide as the other column (not narrower than its
                                // terms need: a layout's own minimum)
                                Layout.preferredWidth: 1
                                Layout.alignment: Qt.AlignTop

                                Repeater {
                                    model: page.modelData.groups.filter(group => group.column === column.modelData)
                                    TermGroup {}
                                }
                                // the groups at the top
                                Item {
                                    Layout.fillHeight: true
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    /** A group of terms: a label and its field (and what the game shows around it) on each line */
    component TermGroup: Ez.Section {
        id: group

        required property var modelData

        title: qsTranslate("VocabularyWidget", modelData.title)
        // the terms EasyRPG Player reads (and the Maniac Patch, in its games): their tag
        origin: modelData.origin
        // as high as its terms: the column keeps the rest below
        Layout.fillHeight: false

        GridLayout {
            columns: group.modelData.pairs * 2
            rowSpacing: Ez.Metrics.rowSpacing
            columnSpacing: Ez.Metrics.columnSpacing
            Layout.fillWidth: true

            Repeater {
                model: group.modelData.rows.length * 2

                // the label, then the field
                Loader {
                    id: cell

                    required property int index
                    readonly property var row: group.modelData.rows[Math.floor(index / 2)]
                    /** The row shows the Maniac Patch's message (only changes of it lay it out again) */
                    readonly property bool maniacLayout: root.maniacLayout(row)

                    Layout.fillWidth: index % 2 === 1
                    // a long label goes on two lines (French...): the page fits the window
                    Layout.maximumWidth: index % 2 === 0 ? root.labelWidth : Number.POSITIVE_INFINITY
                    sourceComponent: index % 2 === 0 ? labelCell : fieldCell

                    Component {
                        id: labelCell
                        Ez.Label {
                            text: cell.row.label !== "" ? qsTranslate("VocabularyWidget", cell.row.label) : ""
                            font: Ez.Metrics.font
                            wrapMode: Text.Wrap
                        }
                    }
                    Component {
                        id: fieldCell
                        // the lines of the message: its terms and what the game puts between them
                        ColumnLayout {
                            spacing: Ez.Metrics.rowSpacing

                            Repeater {
                                model: cell.maniacLayout ? cell.row.maniacLines : cell.row.lines

                                RowLayout {
                                    id: line
                                    required property var modelData
                                    spacing: Ez.Metrics.columnSpacing
                                    Layout.fillWidth: true

                                    Repeater {
                                        model: line.modelData

                                        // a field (the pages fit the window, not the lengths; the
                                        // parts the Maniac Patch adds narrower), else a text (a long
                                        // one wraps)
                                        Loader {
                                            id: part
                                            required property var modelData
                                            readonly property bool isTerm: modelData.term !== undefined
                                            Layout.fillWidth: isTerm
                                            Layout.horizontalStretchFactor: !isTerm ? -1 : modelData.own ? 2 : 1
                                            Layout.minimumWidth: isTerm ? 40 : 0
                                            Layout.preferredWidth: isTerm ? (modelData.own ? 80 : 40) : -1
                                            Layout.maximumWidth: isTerm ? Number.POSITIVE_INFINITY : root.hintWidth
                                            sourceComponent: isTerm ? termPart : hintPart

                                            Component {
                                                id: termPart
                                                TermField {
                                                    term: part.modelData.term
                                                    origin: part.modelData.origin
                                                    limit: part.modelData.limit
                                                    fallback: part.modelData.fallback === undefined ? ""
                                                              : part.modelData.fallbackHint ? qsTranslate("VocabularyWidget", part.modelData.fallback)
                                                              : part.modelData.fallback
                                                    defaulted: part.modelData.fallback !== undefined
                                                }
                                            }
                                            Component {
                                                id: hintPart
                                                Ez.Label {
                                                    text: qsTranslate("VocabularyWidget", part.modelData.hint)
                                                    font: Ez.Metrics.font
                                                    wrapMode: Text.Wrap
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
    }

    /**
     * The field of a term. A term of EasyRPG Player (defaulted) shows the
     * Player's text greyed while it is left to it ("default_term"), and goes
     * back to it once emptied. A part the Maniac Patch adds (origin) has
     * its tag inside, on the right; it is greyed in a game without it, its
     * text kept (the tag then warns that the game does not show it)
     */
    component TermField: Ez.TextField {
        id: termField
        property string term
        /** Ez.Origins: "maniac" for a part the Maniac Patch adds, else empty */
        property string origin: ""
        property bool defaulted: false
        /** What the game shows while it is left to EasyRPG Player's text */
        property string fallback
        objectName: term
        jsonData: term !== "" && !defaulted ? root.terms : null
        key: term
        enabled: origin === "" || Ez.Origins.available(origin)
        // the game's text, whatever its length
        font: Ez.Metrics.monospaceFont
        placeholderText: defaulted && defaultValue.value === "default_term" ? fallback : ""

        // the tag of a part the Maniac Patch adds, inside on the right (the
        // columns have no room beside the fields); the text stops before it.
        // Made only for such a part
        Binding {
            target: termField
            property: "rightPadding"
            value: termField.leftPadding + tag.width + Ez.Metrics.fieldPadding
            when: termField.origin !== "" && tag.width > 0
        }
        Loader {
            id: tag
            active: termField.origin !== ""
            anchors.right: parent.right
            anchors.rightMargin: Ez.Metrics.fieldPadding
            anchors.verticalCenter: parent.verticalCenter
            sourceComponent: Ez.OriginTag {
                origin: termField.origin
                // a text the game does not show
                warning: !termField.enabled && termField.text !== ""
            }
        }
        Loader {
            anchors.fill: parent
            active: termField.origin !== ""
            sourceComponent: Ez.OriginTip {
                origin: termField.origin
            }
        }

        Ez.JsonField {
            id: defaultValue
            source: termField.defaulted ? root.terms : null
            path: termField.term
        }
        Binding {
            target: termField
            property: "text"
            value: defaultValue.value === "default_term" ? "" : (defaultValue.value ?? "")
            when: termField.defaulted && defaultValue.valid
            restoreMode: Binding.RestoreNone
        }
        onTextEdited: {
            if (defaulted && defaultValue.valid) {
                defaultValue.value = text === "" ? "default_term" : text
            }
        }
    }
}
