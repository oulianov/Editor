// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "SearchDialog"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import QtQml.Models
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * Edit > Search: where the events of the project use a variable, a switch,
 * an item (by number or name), the events and common events named so, the
 * texts of their commands (ProjectSearch). The entries of the database are
 * suggested while typing. A result opens its map at its event, page and
 * line (or its common event, its monster group). Not modal: the search and
 * its results stay for the next time it is opened.
 */
Ez.DialogWindow {
    id: root

    /** The search of the project, kept by the main window */
    required property Ez.ProjectSearch search

    objectName: "SearchDialog"
    title: qsTr("Search")
    preferredWidth: 850
    preferredHeight: 560
    // its Close button is in its footer, by the count of results
    standardButtons: Controls.DialogButtonBox.NoButton

    function runSearch() {
        const text = searchText.text.trim()
        if (text === "") {
            return
        }
        suggestions.close()
        root.search.text = text
        root.search.search()
        table.selectionModel.clear()
        table.positionViewAtRow(0, TableView.AlignTop)
    }
    /** A suggestion chosen: its text, and its type to search */
    function pick(row) {
        const label = root.search.suggestions.labelAt(row)
        const type = root.search.suggestions.typeAt(row)
        if (type < 0) {
            return
        }
        searchText.text = label
        root.search.suggestions.setQuery(label)
        suggestions.close()
        root.search.type = type
        // a common event: where it is called, in the common events
        if (type === Ez.ProjectSearch.EventNames) {
            root.search.scope = Ez.ProjectSearch.CommonEvents
        }
    }
    function openRow(row) {
        if (row >= 0) {
            root.search.open(row)
        }
    }

    Component.onCompleted: {
        searchText.text = root.search.text
        searchText.forceActiveFocus()
    }
    // a new search (the local state reset): the text goes too
    Connections {
        target: root.search
        function onTextChanged() {
            if (root.search.text === "") {
                searchText.text = ""
            }
        }
    }

    RowLayout {
        spacing: Ez.Metrics.columnSpacing
        Layout.fillWidth: true

        Ez.TextField {
            id: searchText
            objectName: "search_text"
            placeholderText: qsTr("Search by ID, name or event text…")
            Accessible.name: qsTr("Search query")
            Layout.fillWidth: true

            onTextEdited: {
                root.search.suggestions.setQuery(text)
                suggestionList.currentIndex = -1
                if (text.trim() !== "" && root.search.suggestions.count > 0) {
                    suggestions.open()
                } else {
                    suggestions.close()
                }
            }
            // the arrows go through the suggestions, Return chooses one or searches
            Keys.onDownPressed: {
                if (!suggestions.visible && text.trim() !== "" && root.search.suggestions.count > 0) {
                    suggestions.open()
                } else if (suggestions.visible) {
                    suggestionList.incrementCurrentIndex()
                }
            }
            Keys.onUpPressed: {
                if (suggestions.visible) {
                    suggestionList.decrementCurrentIndex()
                }
            }
            Keys.onReturnPressed: event => {
                if (suggestions.visible && suggestionList.currentIndex >= 0) {
                    root.pick(suggestionList.currentIndex)
                } else {
                    root.runSearch()
                }
            }
            Keys.onEnterPressed: event => Keys.returnPressed(event)
            // Escape closes the suggestions before the window
            Keys.onShortcutOverride: event => {
                event.accepted = event.key === Qt.Key_Escape && suggestions.visible
            }
            Keys.onEscapePressed: suggestions.close()

            Controls.Popup {
                id: suggestions
                y: searchText.height
                width: searchText.width
                height: Math.min(suggestionList.contentHeight, 260) + topPadding + bottomPadding
                padding: 1
                // the field keeps the keys
                focus: false
                closePolicy: Controls.Popup.CloseOnPressOutsideParent

                contentItem: ListView {
                    id: suggestionList
                    objectName: "searchSuggestions"
                    clip: true
                    currentIndex: -1
                    model: root.search.suggestions
                    highlightMoveDuration: 0
                    Controls.ScrollBar.vertical: Controls.ScrollBar {}

                    delegate: Controls.ItemDelegate {
                        id: suggestion
                        required property int index
                        required property string label
                        required property string iconName

                        width: ListView.view.width
                        font: Ez.Metrics.font
                        verticalPadding: Ez.Metrics.optionPadding
                        highlighted: ListView.isCurrentItem
                        text: label
                        contentItem: RowLayout {
                            spacing: Ez.Metrics.columnSpacing
                            Ez.EditorIcon {
                                name: suggestion.iconName
                                Layout.preferredWidth: 16
                                Layout.preferredHeight: 16
                            }
                            Ez.Label {
                                text: suggestion.label
                                font: Ez.Metrics.font
                                elide: Text.ElideRight
                                color: suggestion.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                                Layout.fillWidth: true
                            }
                        }
                        onClicked: root.pick(index)
                    }
                }
            }
        }
        Ez.Button {
            objectName: "button_search"
            text: qsTr("Search")
            enabled: searchText.text.trim() !== ""
            onClicked: root.runSearch()
        }
    }

    RowLayout {
        spacing: Ez.Metrics.columnSpacing
        Layout.fillWidth: true

        Ez.FormLabel {
            text: qsTr("Type")
        }
        Ez.ComboBox {
            id: typeCombo
            objectName: "filter_type"
            textRole: "name"
            valueRole: "value"
            model: root.search.typeChoices()
            onActivated: root.search.type = currentValue
            Binding on currentIndex {
                value: root.search.type
            }
        }
        Ez.FormLabel {
            text: qsTr("Scope")
        }
        Ez.ComboBox {
            objectName: "filter_scope"
            textRole: "name"
            valueRole: "value"
            model: root.search.scopeChoices()
            onActivated: root.search.scope = currentValue
            Binding on currentIndex {
                value: root.search.scope
            }
        }
        Item {
            Layout.fillWidth: true
        }
    }

    // the results: a line each, the columns of RPG Maker's search
    Rectangle {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 120
        color: Kirigami.Theme.backgroundColor
        border.color: Ez.Metrics.sectionBorderColor
        Kirigami.Theme.colorSet: Kirigami.Theme.View
        Kirigami.Theme.inherit: false

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 1
            spacing: 0

            Ez.TableHeader {
                syncView: table
                Layout.fillWidth: true
            }
            TableView {
                id: table
                objectName: "list_result"
                model: root.search.results
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                resizableColumns: true
                selectionBehavior: TableView.SelectRows
                selectionMode: TableView.SingleSelection
                selectionModel: ItemSelectionModel {}
                keyNavigationEnabled: true
                pointerNavigationEnabled: false
                Layout.fillWidth: true
                Layout.fillHeight: true

                // map, event, page, type; the match takes the rest
                readonly property list<int> widths: [220, 180, 55, 125]
                columnWidthProvider: column => {
                    const set = explicitColumnWidth(column)
                    if (set >= 0) {
                        return set
                    }
                    if (column < widths.length) {
                        return widths[column]
                    }
                    let used = 0
                    for (let i = 0; i < widths.length; ++i) {
                        used += table.columnWidth(i) > 0 ? table.columnWidth(i) : widths[i]
                    }
                    return Math.max(120, table.width - used)
                }
                rowHeightProvider: () => 36
                onWidthChanged: forceLayout()

                Controls.ScrollBar.vertical: Controls.ScrollBar {}
                Controls.ScrollBar.horizontal: Controls.ScrollBar {}

                Keys.onReturnPressed: root.openRow(table.currentRow)
                Keys.onEnterPressed: root.openRow(table.currentRow)

                delegate: Rectangle {
                    id: cell
                    required property int row
                    required property int column
                    required property bool selected
                    required property string display
                    required property string locationIcon
                    required property string eventIcon
                    required property string typeIcon
                    required property string charsetName
                    required property rect charsetFrame

                    // the line chosen: by the arrows too
                    readonly property bool marked: selected || row === table.currentRow
                    readonly property bool graphic: column === 1 && charsetName !== ""
                    readonly property string iconName: column === 0 ? locationIcon
                        : column === 1 && !graphic ? eventIcon
                        : column === 3 ? typeIcon : ""

                    implicitHeight: 36
                    color: marked ? Kirigami.Theme.highlightColor
                        : row % 2 ? Kirigami.Theme.alternateBackgroundColor : Kirigami.Theme.backgroundColor

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 4
                        anchors.rightMargin: 4
                        spacing: 4

                        // the event as the map shows it: its frame of the CharSet
                        Item {
                            visible: cell.graphic
                            clip: true
                            Layout.preferredWidth: 24
                            Layout.preferredHeight: 32
                            Image {
                                x: -cell.charsetFrame.x
                                y: -cell.charsetFrame.y
                                source: cell.graphic ? "image://project/CharSet/" + cell.charsetName : ""
                                smooth: false
                            }
                        }
                        Ez.EditorIcon {
                            visible: cell.iconName !== ""
                            name: cell.iconName
                            Layout.preferredWidth: 24
                            Layout.preferredHeight: 24
                        }
                        Ez.Label {
                            text: cell.display
                            font: Ez.Metrics.font
                            elide: Text.ElideRight
                            color: cell.marked ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                            Layout.fillWidth: true
                        }
                    }
                    // a click opens it
                    TapHandler {
                        onTapped: {
                            table.forceActiveFocus()
                            table.selectionModel.setCurrentIndex(table.index(cell.row, cell.column),
                                ItemSelectionModel.ClearAndSelect | ItemSelectionModel.Rows)
                            root.openRow(cell.row)
                        }
                    }
                }
            }
        }
    }

    RowLayout {
        spacing: Ez.Metrics.columnSpacing
        Layout.fillWidth: true

        Ez.Label {
            objectName: "label_status"
            font: Ez.Metrics.font
            elide: Text.ElideRight
            text: root.search.text === "" ? qsTr("Enter a search query to find references in events.")
                : qsTr("%n result(s)", "", root.search.results.count)
            Layout.fillWidth: true
        }
        Ez.DialogButton {
            objectName: "button_close"
            text: qsTr("Close")
            onClicked: root.reject()
        }
    }
}
