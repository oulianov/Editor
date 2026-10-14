// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "ExperienceDialog"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * RPG Maker's Experience Curve window (Edit... of a hero or a class): the
 * experience of each level, in total (tableTotalExperience) or to the next
 * level (tableExperiencePerLevel), computed as the game does from three
 * settings (sliderExpBase and spinExpBase, ...Inflation, ...Correction),
 * named and bounded as in the game's engine. The levels the game cannot
 * reach (at the maximum experience) are greyed, as RPG Maker dots them.
 * OK writes the settings to the entry.
 *
 *   Ez.Editor.exec("ExperienceDialog", { database: page.database, entry: page.entry }, button)
 */
Ez.DialogWindow {
    id: root

    /** The database edited (a JsonView: its engine and EasyRPG settings) */
    property var database: null
    /** The hero or class (a JsonView: exp_base, exp_inflation, exp_correction) */
    property var entry: null

    readonly property var keys: ["exp_base", "exp_inflation", "exp_correction"]
    readonly property var names: ["ExpBase", "ExpInflation", "ExpCorrection"]
    /** The settings shown (the copy edited) */
    property var values: [0, 0, 0]
    readonly property var curve: Ez.Curves.experience(database, values[0], values[1], values[2])
    readonly property int maxLevel: curve.maxLevel ?? 99

    function setValue(index, value) {
        if (values[index] === value) {
            return
        }
        const all = values.slice()
        all[index] = value
        values = all
    }

    objectName: "ExperienceDialog"
    stateKey: "ExperienceDialog"
    title: qsTr("Experience Curve")
    preferredWidth: 520
    height: Math.max(minimumHeight, 420)

    Component.onCompleted: {
        if (entry) {
            values = keys.map(key => entry.num(key))
        }
    }
    onAccepting: (check) => {
        if (!entry) {
            return
        }
        entry.beginBatch()
        for (let i = 0; i < keys.length; ++i) {
            entry.set(keys[i], values[i])
        }
        entry.endBatch()
    }

    // new settings: the tables again
    onCurveChanged: {
        totalTable.revision++
        perLevelTable.revision++
    }

    RowLayout {
        spacing: Ez.Metrics.sectionSpacing
        Layout.fillWidth: true
        Layout.fillHeight: true

        ColumnLayout {
            spacing: 0
            Layout.fillWidth: true
            Layout.fillHeight: true

            Controls.TabBar {
                id: tabs
                objectName: "tabExperience"
                Layout.fillWidth: true
                font: Ez.Metrics.font

                Controls.TabButton {
                    text: qsTr("Total Experience")
                    width: implicitWidth
                    font: Ez.Metrics.font
                }
                Controls.TabButton {
                    text: qsTr("Experience Per Level")
                    width: implicitWidth
                    font: Ez.Metrics.font
                }
            }
            StackLayout {
                currentIndex: tabs.currentIndex
                Layout.fillWidth: true
                Layout.fillHeight: true

                Ez.ArrayTable {
                    id: totalTable
                    objectName: "tableTotalExperience"
                    rows: root.maxLevel
                    source: root
                    visibleRows: 8
                    columns: [
                        { title: "", width: 34, text: row => String(row + 1) },
                        { title: qsTr("Experience"), alignment: Text.AlignRight,
                          text: row => String(root.curve.total[row] ?? ""),
                          dimmed: row => !!root.curve.capped[row] },
                    ]
                }
                Ez.ArrayTable {
                    id: perLevelTable
                    objectName: "tableExperiencePerLevel"
                    rows: root.maxLevel
                    source: root
                    visibleRows: 8
                    columns: [
                        { title: "", width: 34, text: row => String(row + 1) },
                        // the last level has no next one
                        { title: qsTr("To the next level"), alignment: Text.AlignRight,
                          text: row => row + 1 < root.maxLevel
                                       ? String(root.curve.total[row + 1] - root.curve.total[row]) : "-",
                          dimmed: row => row + 1 < root.maxLevel && !!root.curve.capped[row + 1] },
                    ]
                }
            }
        }

        // the three settings, named and bounded as in the game's engine
        ColumnLayout {
            spacing: Ez.Metrics.sectionSpacing
            Layout.alignment: Qt.AlignTop

            Repeater {
                model: 3

                Ez.Section {
                    id: setting

                    required property int index
                    readonly property var info: root.curve.settings ? root.curve.settings[index] : ({})

                    title: info.name ?? ""
                    RowLayout {
                        spacing: Ez.Metrics.columnSpacing
                        Ez.Slider {
                            objectName: "slider" + root.names[setting.index]
                            from: setting.info.minimum ?? 0
                            to: setting.info.maximum ?? 0
                            value: root.values[setting.index]
                            Layout.fillWidth: true
                            Layout.minimumWidth: 160
                            onMoved: root.setValue(setting.index, Math.round(value))
                        }
                        Ez.SpinBox {
                            objectName: "spin" + root.names[setting.index]
                            from: setting.info.minimum ?? 0
                            to: setting.info.maximum ?? 0
                            value: root.values[setting.index]
                            onValueModified: root.setValue(setting.index, value)
                        }
                    }
                }
            }
        }
    }
}
