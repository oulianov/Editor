// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * A list of lines in columns (a name, its format, where it comes from) in
 * its frame, dense as RPG Maker's lists: the titles of the columns above
 * them, the current line highlighted, the titles of the sections between
 * the lines (sectionRole).
 *
 * The current line is the model's (currentRow): a click or the keys on a
 * line tell it (picked), the list does not choose one by itself. A
 * double-click or Return on a line: activated.
 *
 * columns: one object per column, from left to right:
 *   title: above the column ("" or none: no titles)
 *   role: the role shown, or text: a function of the line's model giving it
 *   sample: the widest text expected; the columns without one share the rest
 *   alignment: Text.AlignLeft (default) or Text.AlignRight
 *   dimmed, italic: functions of the line's model (a file of the RTP...)
 *   icon: the name of an icon of the Editor (resources/icons) before the text
 *   sort: the column's key for sortColumn
 *
 * Sorted by the model, on request: with a sortColumn, a click on the title
 * of a column that has a sort key asks for it (sortClicked); the title of
 * the column sorted shows its direction (▲ ▼).
 */
Rectangle {
    id: root

    /** The name of the ListView, for the scripts */
    property alias listName: list.objectName
    property alias model: list.model
    readonly property alias view: list
    property var columns: [{ role: "display" }]
    /** The role whose changes start a section (its value is the section's title); "": none */
    property string sectionRole: ""
    /** The line of the model shown as current, -1 for none */
    property int currentRow: -1
    /** Lines shown without scrolling, for the size of the list */
    property int visibleLines: 8
    /** The sort key of the column the model sorts the lines by; "": the titles are not clicked */
    property string sortColumn: ""
    property bool sortAscending: true

    signal picked(int row)
    signal activated(int row)
    /** A click on the title of a column with a sort key (another one, or the one sorted to reverse it) */
    signal sortClicked(string column)
    /** Delete or Backspace on the list */
    signal deletePressed()

    /** The space between a column's text and its edges */
    readonly property int cellPadding: 4
    readonly property bool hasTitles: columns.some(column => (column.title ?? "") !== "")
    /** The width of each column, -1 for the one that fills */
    readonly property var columnWidths: columns.map(column => column.sample === undefined ? -1
        : Math.ceil(Math.max(metrics.advanceWidth((column.title ?? "") + (sortKey(column) !== "" ? " ▼" : "")),
            metrics.advanceWidth(column.sample))
            + (column.icon ? iconSize + Ez.Metrics.columnSpacing : 0)) + 2 * cellPadding + 2)
    readonly property int fixedWidth: columnWidths.reduce((sum, width) => sum + Math.max(0, width), 0)
    readonly property int iconSize: Kirigami.Units.iconSizes.small

    /** The columns without a sample share the room the others leave */
    readonly property int fillColumns: Math.max(1, columnWidths.filter(width => width < 0).length)
    function cellWidth(column) {
        const width = columnWidths[column]
        return width >= 0 ? width
            : Math.max(0, Math.floor((list.width - (bar.visible ? bar.width : 0) - fixedWidth) / fillColumns))
    }
    /** The sort key of a column, "" when its title is not clicked */
    function sortKey(column) {
        return sortColumn !== "" ? column.sort ?? "" : ""
    }
    /** The title of a column, with the direction of the sort when it is the column sorted */
    function titleText(column) {
        const title = column.title ?? ""
        return sortKey(column) !== "" && column.sort === sortColumn ? title + (sortAscending ? " ▲" : " ▼") : title
    }
    function cellText(column, line) {
        if (column.text) {
            return column.text(line) ?? ""
        }
        return line[column.role ?? "display"] ?? ""
    }
    /** The current line on the model's, shown */
    function sync() {
        if (list.currentIndex !== currentRow) {
            list.currentIndex = currentRow
        }
        if (currentRow >= 0) {
            list.positionViewAtIndex(currentRow, ListView.Contain)
        }
    }
    function move(row) {
        if (list.count > 0) {
            root.picked(Math.max(0, Math.min(list.count - 1, row)))
        }
    }

    onCurrentRowChanged: sync()

    Kirigami.Theme.colorSet: Kirigami.Theme.View
    Kirigami.Theme.inherit: false
    color: Kirigami.Theme.backgroundColor
    border.color: Ez.Metrics.sectionBorderColor
    radius: 2
    implicitWidth: (columnWidths.includes(-1) ? 120 : fixedWidth) + bar.implicitWidth + 2
    implicitHeight: visibleLines * lineMetrics.height + (hasTitles ? lineMetrics.height : 0) + 2

    FontMetrics {
        id: metrics
        font: Ez.Metrics.font
    }
    // the height of a line: its text and its padding
    QtObject {
        id: lineMetrics
        readonly property real height: Math.ceil(metrics.height) + 2
    }

    ListView {
        id: list
        anchors.fill: parent
        anchors.margins: 1
        clip: true
        focus: true
        activeFocusOnTab: true
        // the keys tell the model, which tells the list (sync)
        keyNavigationEnabled: false
        highlightFollowsCurrentItem: false
        boundsBehavior: Flickable.StopAtBounds
        currentIndex: -1
        headerPositioning: ListView.OverlayHeader
        Controls.ScrollBar.vertical: Controls.ScrollBar {
            id: bar
        }
        // a model listed again may move the current line: the model's is the one
        onCurrentIndexChanged: {
            if (currentIndex !== root.currentRow) {
                Qt.callLater(root.sync)
            }
        }
        onCountChanged: Qt.callLater(root.sync)

        Keys.onUpPressed: root.move(root.currentRow - 1)
        Keys.onDownPressed: root.move(root.currentRow < 0 ? 0 : root.currentRow + 1)
        Keys.onPressed: event => {
            const page = Math.max(1, Math.floor(height / lineMetrics.height) - 1)
            switch (event.key) {
            case Qt.Key_PageUp:
                root.move(root.currentRow - page)
                break
            case Qt.Key_PageDown:
                root.move(root.currentRow + page)
                break
            case Qt.Key_Home:
                root.move(0)
                break
            case Qt.Key_End:
                root.move(count - 1)
                break
            case Qt.Key_Return:
            case Qt.Key_Enter:
                if (root.currentRow >= 0) {
                    root.activated(root.currentRow)
                }
                break
            case Qt.Key_Delete:
            case Qt.Key_Backspace:
                root.deletePressed()
                break
            default:
                return
            }
            event.accepted = true
        }

        header: Rectangle {
            visible: root.hasTitles
            z: 2
            width: ListView.view.width
            height: visible ? lineMetrics.height + 2 : 0
            color: Kirigami.Theme.alternateBackgroundColor
            Kirigami.Theme.colorSet: Kirigami.Theme.Button
            Kirigami.Theme.inherit: false

            Row {
                anchors.fill: parent
                Repeater {
                    model: root.columns
                    delegate: Text {
                        id: title
                        required property var modelData
                        required property int index
                        readonly property string sortKey: root.sortKey(modelData)
                        // for the scripts: listFilesSortName...
                        objectName: sortKey !== "" ? root.listName + "Sort" + sortKey.charAt(0).toUpperCase() + sortKey.slice(1) : ""
                        width: root.cellWidth(index)
                        height: parent.height
                        leftPadding: root.cellPadding
                        rightPadding: root.cellPadding
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: modelData.alignment ?? Text.AlignLeft
                        text: root.titleText(modelData)
                        font: Ez.Metrics.font
                        color: Kirigami.Theme.textColor
                        elide: Text.ElideRight

                        // a title clicked sorts by its column: lit under the pointer
                        Rectangle {
                            z: -1
                            anchors.fill: parent
                            visible: sortArea.containsMouse
                            color: Kirigami.Theme.hoverColor
                            opacity: sortArea.pressed ? 0.6 : 0.3
                        }
                        MouseArea {
                            id: sortArea
                            anchors.fill: parent
                            enabled: title.sortKey !== ""
                            hoverEnabled: enabled
                            onClicked: root.sortClicked(title.sortKey)
                        }

                        // the line between the titles
                        Rectangle {
                            anchors.right: parent.right
                            width: 1
                            height: parent.height
                            color: Ez.Metrics.sectionBorderColor
                        }
                    }
                }
            }
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Ez.Metrics.sectionBorderColor
            }
        }

        section.property: root.sectionRole
        section.criteria: ViewSection.FullString
        section.delegate: Text {
            required property string section
            // without sections, ListView still makes them (from the first role):
            // empty (it shows its section items itself)
            height: root.sectionRole !== "" ? implicitHeight : 0
            width: ListView.view.width
            leftPadding: root.cellPadding
            topPadding: 1
            bottomPadding: 1
            text: root.sectionRole !== "" ? section : ""
            font.family: Ez.Metrics.font.family
            font.pointSize: Ez.Metrics.font.pointSize
            font.bold: true
            color: Kirigami.Theme.textColor
            elide: Text.ElideRight
        }

        delegate: Controls.ItemDelegate {
            id: line

            required property int index
            required property var model

            width: ListView.view.width - (bar.visible ? bar.width : 0)
            // the text of the first column: the scripts click a line by it
            text: root.cellText(root.columns[0], model)
            font: Ez.Metrics.font
            topPadding: 1
            bottomPadding: 1
            leftPadding: 0
            rightPadding: 0
            highlighted: index === root.currentRow

            contentItem: Row {
                Repeater {
                    model: root.columns
                    delegate: Item {
                        id: cell
                        required property var modelData
                        required property int index
                        width: root.cellWidth(index)
                        implicitHeight: label.implicitHeight

                        Kirigami.Icon {
                            id: icon
                            visible: (cell.modelData.icon ?? "") !== ""
                            // an icon of the Editor, drawn for the background of the list
                            source: visible ? "qrc:/" + (Kirigami.Theme.backgroundColor.hslLightness >= 0.5 ? "bright" : "dark")
                                + "/" + cell.modelData.icon : ""
                            x: root.cellPadding
                            width: visible ? root.iconSize : 0
                            height: root.iconSize
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Ez.Label {
                            id: label
                            x: icon.visible ? icon.x + icon.width + Ez.Metrics.columnSpacing : 0
                            width: parent.width - x
                            leftPadding: icon.visible ? 0 : root.cellPadding
                            rightPadding: root.cellPadding
                            text: root.cellText(cell.modelData, line.model)
                            horizontalAlignment: cell.modelData.alignment ?? Text.AlignLeft
                            elide: Text.ElideRight
                            font.family: line.font.family
                            font.pointSize: line.font.pointSize
                            font.italic: cell.modelData.italic ? cell.modelData.italic(line.model) : false
                            color: line.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                            opacity: !line.highlighted && cell.modelData.dimmed && cell.modelData.dimmed(line.model) ? 0.6 : 1
                        }
                    }
                }
            }

            Controls.ToolTip.visible: hovered && (model.toolTip ?? "") !== ""
            Controls.ToolTip.text: model.toolTip ?? ""
            Controls.ToolTip.delay: Kirigami.Units.toolTipDelay

            onClicked: {
                list.forceActiveFocus()
                root.picked(index)
            }
            onDoubleClicked: root.activated(index)
        }
    }
}
