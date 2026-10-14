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
 * in the font and with the length mark of the game's windows.
 */
Item {
    id: root

    /** The database edited (a JsonView) */
    property var database: null
    property bool rpg2k3: true
    readonly property var terms: database ? database.subtree("terms") : null
    /** The widest label of a term (longer ones wrap) */
    readonly property int labelWidth: 220
    /** The widest text the game puts before a term (longer ones wrap) */
    readonly property int hintWidth: 140
    /** The page of terms shown */
    property alias currentIndex: tabs.currentIndex

    Ez.VocabularyTable {
        id: table
        rpg2k3: root.rpg2k3
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

                Ez.EntryPage {
                    id: page

                    required property var modelData

                    single: true
                    fillWidth: true
                    database: root.database
                    rpg2k3: root.rpg2k3

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
                        RowLayout {
                            spacing: Ez.Metrics.columnSpacing

                            // what the game puts before the term
                            Ez.Label {
                                visible: cell.row.hint !== "" && cell.row.second === ""
                                text: visible ? qsTranslate("VocabularyWidget", cell.row.hint) : ""
                                font: Ez.Metrics.font
                                wrapMode: Text.Wrap
                                Layout.maximumWidth: root.hintWidth
                            }
                            TermField {
                                term: cell.row.term
                                limit: cell.row.limit
                            }
                            // or between the term and the second one
                            Ez.Label {
                                visible: cell.row.second !== ""
                                text: visible ? qsTranslate("VocabularyWidget", cell.row.hint) : ""
                                font: Ez.Metrics.font
                            }
                            TermField {
                                visible: cell.row.second !== ""
                                term: cell.row.second
                                limit: cell.row.secondLimit
                            }
                        }
                    }
                }
            }
        }
    }

    /** The field of a term: the pages fit the window, not the lengths */
    component TermField: Ez.TextField {
        property string term
        objectName: term
        jsonData: term !== "" ? root.terms : null
        key: term
        Layout.fillWidth: true
        Layout.minimumWidth: 40
        Layout.preferredWidth: 80
    }
}
