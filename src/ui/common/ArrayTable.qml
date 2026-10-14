// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * A table of an array of the project's data, a line per element: the
 * skills a class learns (a list of objects), the attributes an item has
 * (the bits of a bit array, a line per attribute)... Each column says what
 * its cells show:
 *
 *   Ez.ArrayTable {
 *       rows: skills.count
 *       source: skills
 *       addLine: true
 *       columns: [
 *           {title: qsTr("Level"), width: 50, text: row => skills.num(row + "/level")},
 *           {title: qsTr("Skill"), text: row => page.skillName(skills.num(row + "/skill_id"))},
 *       ]
 *       onActivated: (row) => edit(row)
 *   }
 *
 * A column: title, width (none: a share of the rest), and text(row) for a
 * text, checked(row) and toggle(row, on) for a check box (named name +
 * row), icon(row) for a picture before the text; alignment (Text.AlignRight
 * for numbers) and dimmed(row) (a value the game cannot reach...) for the
 * text. The cells are read again
 * when source (a Json, a JsonView or a list model) says its data changed.
 *
 * addLine: an empty line after the elements, to add one. Double-click and
 * Return on a line: activated(row) (rows on the empty line, or below the
 * lines); Delete (Backspace on macOS): removeRequested(row); a click on a
 * line, or Space: clicked(row) (a rank changed by a click...).
 *
 * contextMenu: the menu of the right button, opened on its press, on the
 * line pressed selected first (Ez.ContextMenuHandler).
 */
FocusScope {
    id: root

    property var columns: []
    /** The elements of the array */
    property int rows: 0
    /** The data the cells show: its changes repaint them */
    property var source: null
    /** An empty last line to add an element */
    property bool addLine: false
    property alias currentIndex: view.currentIndex
    /** Grows when the data changes: the cells read it again */
    property int revision: 0
    /** The lines shown before scrolling (the height the table asks for) */
    property int visibleRows: 6
    /** The titles of the columns above the lines (false: a plain list) */
    property bool showHeader: true
    /** The menu of the right button (none: no menu) */
    property Ez.ContextMenu contextMenu: null

    signal activated(int row)
    signal removeRequested(int row)
    signal clicked(int row)

    /**
     * What the table shows, for the scripts: the cells of a line joined by
     * " | " (a picture by its file name: "rank2"), the current line marked "> "
     */
    function automationText() {
        const lines = []
        for (let row = 0; row < rows; ++row) {
            const cells = []
            for (const column of columns) {
                if (column.checked) {
                    cells.push(column.checked(row) ? "[x]" : "[ ]")
                }
                if (column.icon) {
                    cells.push(String(column.icon(row)).split("/").pop())
                }
                if (column.text) {
                    cells.push(column.text(row))
                }
            }
            lines.push((row === currentIndex ? "> " : "  ") + cells.join(" | "))
        }
        return lines.join("\n")
    }

    /** What a cell shows: read again when revision grows (the bindings passing it depend on it) */
    function cell(read, row, revision) {
        return read(row)
    }

    // as wide as a field at least, and as its titles
    // (measured: the titles are as wide as the columns, which follow the table)
    implicitWidth: Math.max(Ez.Metrics.fieldWidth, 2 + (!showHeader ? 0 : columns.reduce(
        (sum, column) => sum + (column.width > 0 ? column.width
                                : Math.ceil(lineMetrics.advanceWidth(column.title ?? "")) + 2 * Ez.Metrics.listTextPadding), 0)))
    implicitHeight: (header.visible ? header.height + 1 : 0) + visibleRows * lineHeight + 2
    activeFocusOnTab: true

    readonly property real lineHeight: Math.max(lineMetrics.height + 2 * Ez.Metrics.listRowPadding, root.hasChecks ? 18 : 0)
    readonly property bool hasChecks: columns.some(column => !!column.checked)
    /** The width of each column */
    readonly property var widths: {
        const total = Math.max(0, view.width)
        let fixed = 0
        let shares = 0
        for (const column of columns) {
            if (column.width > 0) {
                fixed += column.width
            } else {
                shares += 1
            }
        }
        const share = shares > 0 ? Math.max(0, total - fixed) / shares : 0
        return columns.map(column => column.width > 0 ? column.width : share)
    }

    // the lines as a list model: added and removed at the end, the current
    // line stays where it is (a number as the model would start over)
    readonly property int lineCount: rows + (addLine ? 1 : 0)
    onLineCountChanged: syncLines()
    Component.onCompleted: syncLines()
    function syncLines() {
        while (lines.count < lineCount) {
            lines.append({ line: lines.count })
        }
        if (lines.count > lineCount) {
            lines.remove(lineCount, lines.count - lineCount)
        }
    }
    ListModel {
        id: lines
    }

    Connections {
        target: root.source
        ignoreUnknownSignals: true
        function onValueChanged() { root.revision++ }
        function onDataChanged() { root.revision++ }
        function onCountChanged() { root.revision++ }
    }
    FontMetrics {
        id: lineMetrics
        font: Ez.Metrics.font
    }

    // the menu on the right press, on the line pressed (below the lines and
    // on the titles: on the current one)
    Ez.ContextMenuHandler {
        enabled: !!root.contextMenu
        contextMenu: root.contextMenu
        onRequested: (x, y) => {
            view.forceActiveFocus()
            const point = view.contentItem.mapFromItem(root, x, y)
            const line = view.indexAt(point.x, point.y)
            if (line >= 0) {
                view.currentIndex = line
            }
        }
    }

    Rectangle {
        id: frame
        anchors.fill: parent
        // the colors of the views (dark mode too)
        Kirigami.Theme.colorSet: Kirigami.Theme.View
        Kirigami.Theme.inherit: false
        color: Kirigami.Theme.backgroundColor
        border.color: Ez.Metrics.sectionBorderColor
        border.width: 1

        Rectangle {
            id: header
            visible: root.showHeader
            x: 1
            y: 1
            width: parent.width - 2
            height: lineMetrics.height + 2 * Ez.Metrics.listRowPadding + 2
            implicitWidth: titles.implicitWidth
            Kirigami.Theme.colorSet: Kirigami.Theme.Button
            Kirigami.Theme.inherit: false
            color: Kirigami.Theme.backgroundColor

            Row {
                id: titles
                Repeater {
                    model: root.columns
                    Ez.Label {
                        required property var modelData
                        required property int index
                        width: root.widths[index] ?? implicitWidth
                        height: header.height
                        leftPadding: Ez.Metrics.listTextPadding
                        verticalAlignment: Text.AlignVCenter
                        text: modelData.title ?? ""
                        elide: Text.ElideRight
                        font: Ez.Metrics.font
                    }
                }
            }
        }

        ListView {
            id: view
            anchors.fill: parent
            anchors.topMargin: header.visible ? header.height + 1 : 1
            anchors.margins: 1
            clip: true
            focus: true
            model: lines
            boundsBehavior: Flickable.StopAtBounds
            keyNavigationEnabled: true
            highlightMoveDuration: 0
            currentIndex: -1
            Controls.ScrollBar.vertical: Controls.ScrollBar {}

            Keys.onPressed: (event) => {
                if (event.key === Qt.Key_Space && currentIndex >= 0 && currentIndex < root.rows) {
                    root.clicked(currentIndex)
                } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                    root.activated(Math.max(0, currentIndex))
                } else if ((event.key === Qt.Key_Delete || (event.key === Qt.Key_Backspace && Qt.platform.os === "osx"))
                           && currentIndex >= 0 && currentIndex < root.rows) {
                    root.removeRequested(currentIndex)
                } else if ((event.key === Qt.Key_Home || event.key === Qt.Key_End) && count > 0) {
                    currentIndex = event.key === Qt.Key_Home ? 0 : count - 1
                    positionViewAtIndex(currentIndex, ListView.Contain)
                } else {
                    return
                }
                event.accepted = true
            }

            // below the lines: a new element
            MouseArea {
                z: -1
                anchors.fill: parent
                onPressed: view.forceActiveFocus()
                onDoubleClicked: {
                    if (root.addLine) {
                        root.activated(root.rows)
                    }
                }
            }

            delegate: Item {
                id: line

                required property int index
                readonly property bool current: ListView.isCurrentItem

                width: ListView.view.width
                height: root.lineHeight

                Rectangle {
                    anchors.fill: parent
                    visible: line.current
                    color: view.activeFocus ? Kirigami.Theme.highlightColor
                                            : Qt.rgba(Kirigami.Theme.highlightColor.r, Kirigami.Theme.highlightColor.g,
                                                      Kirigami.Theme.highlightColor.b, 0.35)
                }
                MouseArea {
                    anchors.fill: parent
                    onPressed: {
                        view.forceActiveFocus()
                        view.currentIndex = line.index
                    }
                    onClicked: {
                        if (line.index < root.rows) {
                            root.clicked(line.index)
                        }
                    }
                    onDoubleClicked: root.activated(line.index)
                }
                Row {
                    height: parent.height
                    // the empty line has no cells
                    visible: line.index < root.rows

                    Repeater {
                        model: root.columns

                        Item {
                            id: cell

                            required property var modelData
                            required property int index

                            width: root.widths[index] ?? 0
                            height: line.height
                            clip: true

                            Row {
                                x: Ez.Metrics.listTextPadding
                                height: parent.height
                                spacing: Ez.Metrics.columnSpacing

                                Controls.CheckBox {
                                    visible: !!cell.modelData.checked
                                    objectName: visible && cell.modelData.name ? cell.modelData.name + line.index : ""
                                    anchors.verticalCenter: parent.verticalCenter
                                    padding: 0
                                    // no text: no room for one
                                    spacing: 0
                                    font: Ez.Metrics.font
                                    checked: visible && line.index < root.rows && root.cell(cell.modelData.checked, line.index, root.revision)
                                    onToggled: cell.modelData.toggle(line.index, checked)
                                }
                                Image {
                                    visible: !!cell.modelData.icon
                                    anchors.verticalCenter: parent.verticalCenter
                                    source: visible && line.index < root.rows ? root.cell(cell.modelData.icon, line.index, root.revision) : ""
                                    sourceSize.height: Math.min(16, line.height)
                                }
                                Ez.Label {
                                    visible: !!cell.modelData.text
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: cell.width - x - 2 * Ez.Metrics.listTextPadding
                                    text: visible && line.index < root.rows ? (root.cell(cell.modelData.text, line.index, root.revision) ?? "") : ""
                                    horizontalAlignment: cell.modelData.alignment ?? Text.AlignLeft
                                    opacity: !!cell.modelData.dimmed && line.index < root.rows
                                             && root.cell(cell.modelData.dimmed, line.index, root.revision) ? 0.5 : 1
                                    elide: Text.ElideRight
                                    font: Ez.Metrics.font
                                    color: line.current && view.activeFocus ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
