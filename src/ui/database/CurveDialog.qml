// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "CurveDialog"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * RPG Maker's Parameter Progression window (a double-click on a curve of a
 * hero or a class): the six curves on their tabs (tabParameters), a graph
 * to draw them (graphParameter, a bar per level), the level (spinLevel)
 * and its value (spinValue), the values of every level (tableValues),
 * QuickCurve (pushOutstanding... pushBelowAverage) and Specify Endpoints
 * (pushEndpoints). The curves are edited on a copy, written to parameters
 * on OK.
 *
 *   Ez.Editor.exec("CurveDialog", { parameters: hero.subtree("parameters"), maxLevel: 99, maxHp: 9999, curve: 2 }, item)
 */
Ez.DialogWindow {
    id: root

    /** The parameters of the hero or class (a JsonView) */
    property var parameters: null
    property int maxLevel: 99
    property int maxHp: 9999
    /** The curve shown first (0 Max HP ... 5 Agility) */
    property int curve: 0

    readonly property var keys: ["maxhp", "maxsp", "attack", "defense", "spirit", "agility"]
    readonly property var names: [qsTr("Max HP"), qsTr("Max MP"), qsTr("Attack"), qsTr("Defense"),
                                  qsTr("Intelligence"), qsTr("Agility")]
    /** The values of the six curves (the copy edited), a level without one read as the game does */
    property var curves: []
    /** The level shown (1 to maxLevel) */
    property int level: 1
    readonly property int current: tabs.currentIndex
    readonly property int minimum: Ez.Curves.minimum(current)
    readonly property int maximum: Ez.Curves.maximum(current, maxHp)
    readonly property var values: curves[current] ?? []

    /** The curve shown changed: its graph, table and value follow */
    function setValues(values) {
        const all = curves.slice()
        all[current] = values
        curves = all
    }
    function setLevel(value) {
        level = Math.max(1, Math.min(maxLevel, value))
    }

    objectName: "CurveDialog"
    stateKey: "CurveDialog"
    title: qsTr("Parameter Progression")
    preferredWidth: 680
    height: Math.max(minimumHeight, 480)

    Component.onCompleted: {
        const all = []
        for (let i = 0; i < keys.length; ++i) {
            all.push(Ez.Curves.padded(parameters ? Array.from(parameters.value(keys[i]) ?? []) : [], i, maxLevel))
        }
        curves = all
        tabs.currentIndex = Math.max(0, Math.min(5, curve))
    }
    onAccepting: (check) => {
        if (!parameters) {
            return
        }
        parameters.beginBatch()
        for (let i = 0; i < keys.length; ++i) {
            parameters.set(keys[i], curves[i])
        }
        parameters.endBatch()
    }

    Controls.TabBar {
        id: tabs
        objectName: "tabParameters"
        Layout.fillWidth: true
        font: Ez.Metrics.font

        Repeater {
            model: [qsTr("Maximum HP"), qsTr("Maximum MP"), qsTr("Attack"), qsTr("Defense"),
                    qsTr("Intelligence"), qsTr("Agility")]
            Controls.TabButton {
                required property string modelData
                text: modelData
                width: implicitWidth
                font: Ez.Metrics.font
            }
        }
    }

    RowLayout {
        spacing: Ez.Metrics.sectionSpacing
        Layout.fillWidth: true
        Layout.fillHeight: true

        Ez.CurveCanvas {
            id: graph
            objectName: "graphParameter"
            values: root.values
            curve: root.current
            levels: root.maxLevel
            minimum: root.minimum
            maximum: root.maximum
            currentLevel: root.level
            bars: true
            editable: true
            implicitWidth: 420
            implicitHeight: 300
            Layout.minimumWidth: 200
            Layout.minimumHeight: 120
            Layout.fillWidth: true
            Layout.fillHeight: true
            onEdited: (level) => {
                root.setValues(graph.values)
                root.setLevel(level)
            }

            Controls.ToolTip.visible: hover.hovered
            Controls.ToolTip.delay: 700
            Controls.ToolTip.text: qsTranslate("CurveEditor", "Click or drag to set the values of the levels")
            HoverHandler {
                id: hover
            }
        }

        ColumnLayout {
            spacing: Ez.Metrics.sectionSpacing
            Layout.fillHeight: true
            Layout.preferredWidth: 200

            Ez.Section {
                title: qsTr("Level")
                Ez.SpinBox {
                    objectName: "spinLevel"
                    from: 1
                    to: root.maxLevel
                    value: root.level
                    Layout.fillWidth: true
                    onValueModified: root.setLevel(value)
                }
            }
            Ez.Section {
                title: root.names[root.current] ?? ""
                Ez.SpinBox {
                    objectName: "spinValue"
                    from: root.minimum
                    to: root.maximum
                    value: Number(root.values[root.level - 1] ?? 0)
                    Layout.fillWidth: true
                    onValueModified: {
                        const values = root.values.slice()
                        values[root.level - 1] = value
                        root.setValues(values)
                    }
                }
            }
            Ez.ArrayTable {
                id: table
                objectName: "tableValues"
                rows: root.maxLevel
                source: root
                visibleRows: 4
                Layout.fillWidth: true
                Layout.fillHeight: true
                columns: [
                    { title: "", width: 34, text: row => String(row + 1) },
                    { title: root.names[root.current] ?? "", text: row => String(root.values[row] ?? "") },
                ]
                // the line of the level, and the level of the line chosen
                Binding {
                    target: table
                    property: "currentIndex"
                    value: root.level - 1
                    restoreMode: Binding.RestoreNone
                }
                onCurrentIndexChanged: {
                    if (currentIndex >= 0) {
                        root.setLevel(currentIndex + 1)
                    }
                }
                // a new curve: its cells again
                Connections {
                    target: root
                    function onCurvesChanged() { table.revision++ }
                    function onCurrentChanged() { table.revision++ }
                }
            }
            Ez.Section {
                title: qsTr("QuickCurve")
                GridLayout {
                    columns: 2
                    rowSpacing: Ez.Metrics.rowSpacing
                    columnSpacing: Ez.Metrics.columnSpacing
                    Layout.fillWidth: true

                    Repeater {
                        model: [
                            { name: "pushOutstanding", text: qsTr("Outstanding") },
                            { name: "pushAboveAverage", text: qsTr("Above Average") },
                            { name: "pushAverage", text: qsTr("Average") },
                            { name: "pushBelowAverage", text: qsTr("Below Average") },
                        ]
                        Ez.Button {
                            required property var modelData
                            required property int index
                            objectName: modelData.name
                            text: modelData.text
                            Layout.fillWidth: true
                            onClicked: root.setValues(Ez.Curves.quickCurve(root.values, root.maxLevel, root.current,
                                                                           index, root.maxHp))
                        }
                    }
                }
            }
            Ez.Button {
                objectName: "pushEndpoints"
                text: qsTr("Specify Endpoints...")
                Layout.fillWidth: true
                onClicked: {
                    first.value = Number(root.values[0] ?? root.minimum)
                    last.value = Number(root.values[root.maxLevel - 1] ?? root.minimum)
                    growth.value = 0
                    endpoints.open()
                }
            }
        }
    }

    // Specify Endpoints: the values of the first and last levels, and where the curve grows
    Ez.Dialog {
        id: endpoints

        parent: Controls.Overlay.overlay
        anchors.centerIn: parent
        modal: true
        title: qsTr("Specify Endpoints")
        standardButtons: Controls.Dialog.Ok | Controls.Dialog.Cancel
        font: Ez.Metrics.font
        padding: Ez.Metrics.dialogMargin

        onOpened: first.forceActiveFocus()
        onAccepted: root.setValues(Ez.Curves.endpoints(root.values, root.maxLevel, first.value, last.value, growth.value))

        contentItem: ColumnLayout {
            spacing: Ez.Metrics.sectionSpacing

            RowLayout {
                spacing: Ez.Metrics.sectionSpacing
                Ez.Section {
                    title: qsTr("L1 Endpoint")
                    Ez.SpinBox {
                        id: first
                        objectName: "spinFirstLevel"
                        from: root.minimum
                        to: root.maximum
                        Layout.fillWidth: true
                    }
                }
                Ez.Section {
                    title: qsTr("L%1 Endpoint").arg(root.maxLevel)
                    Ez.SpinBox {
                        id: last
                        objectName: "spinLastLevel"
                        from: root.minimum
                        to: root.maximum
                        Layout.fillWidth: true
                    }
                }
            }
            Ez.Section {
                title: qsTr("Accelerate Growth")
                Ez.Slider {
                    id: growth
                    objectName: "sliderGrowth"
                    from: -4
                    // a tick mark at each step, as in the Widgets editor
                    tickInterval: 1
                    to: 4
                    Layout.fillWidth: true
                }
                RowLayout {
                    Layout.fillWidth: true
                    Ez.Label {
                        text: qsTr("Early")
                        font: Ez.Metrics.font
                        Layout.fillWidth: true
                    }
                    Ez.Label {
                        text: qsTr("Equal")
                        font: Ez.Metrics.font
                        horizontalAlignment: Text.AlignHCenter
                        Layout.fillWidth: true
                    }
                    Ez.Label {
                        text: qsTr("Late")
                        font: Ez.Metrics.font
                        horizontalAlignment: Text.AlignRight
                        Layout.fillWidth: true
                    }
                }
            }
        }
    }
}
