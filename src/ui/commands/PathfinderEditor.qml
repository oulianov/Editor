// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "EasyRpgCommandWidget"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * Pathfinder (EasyRPG Player): the form of its schema (the event, the
 * destination, the route, what to do while the event moves), then what
 * follows parameter 10, of a length of its own, as EasyRPG Player reads it
 * (Game_Interpreter_Map::CommandEasyRpgPathfinder):
 *
 * - the events the search goes through: parameter 11 their mode; 0: their
 *   count in parameter 12, their IDs from parameter 13; 1 and 2: the value
 *   of the variable (or the variable pointer) in parameter 12 is the first
 *   variable of the list, holding their count, the next variables their IDs;
 * - the move frequency after them: its mode, then its value (missing: 3).
 */
ColumnLayout {
    id: root

    required property Ez.CommandEditSession session
    readonly property string title: qsTranslate("EventCommandPalette", "Pathfinder")
    readonly property int revision: session.revision
    readonly property var parameters: revision < 0 ? [] : (session.value("/parameters") ?? []).map(Number)
    readonly property int ignoreMode: at(11)
    readonly property var ignored: ignoreMode === 0 ? parameters.slice(13, 13 + Math.max(0, at(12))) : []
    /** Where the move frequency is: after the events listed */
    readonly property int frequencyAt: ignoreMode === 0 ? 13 + Math.max(0, at(12)) : 13
    readonly property bool hasFrequency: parameters.length > frequencyAt + 1
    readonly property int frequencyMode: hasFrequency ? at(frequencyAt) : 0
    readonly property int frequency: hasFrequency ? at(frequencyAt + 1) : 3

    readonly property var modes: [{ value: 0, text: qsTranslate("ManiacCommandWidget", "Constant") },
                                  { value: 1, text: qsTranslate("ManiacCommandWidget", "Variable") },
                                  { value: 2, text: qsTranslate("ManiacCommandWidget", "Variable pointer") }]

    function at(index) {
        return index < parameters.length ? parameters[index] : 0
    }
    /** Writes the parameters after 10: the events ignored (mode 0: their
     *  IDs; else the variable), then the move frequency */
    function write(mode, list, variable, freqMode, freqValue) {
        const written = parameters.slice(0, 11)
        while (written.length < 11) {
            written.push(0)
        }
        written.push(mode)
        if (mode === 0) {
            written.push(list.length)
            for (const id of list) {
                written.push(id)
            }
        } else {
            written.push(variable)
        }
        written.push(freqMode, freqValue)
        session.set("/parameters", written)
    }
    function setIgnoreMode(mode) {
        if (mode !== ignoreMode) {
            // a variable from the first one, not V[0]
            write(mode, [], mode === 0 ? 0 : Math.max(1, at(12)), frequencyMode, frequency)
        }
    }
    function setIgnored(list) {
        write(0, list, 0, frequencyMode, frequency)
    }
    function setFrequency(mode, value) {
        write(ignoreMode, ignored, at(12), mode, value)
    }
    /** The events of the map to choose from, the one of an ID not there added */
    function eventChoices(id) {
        const events = root.session.events
        if (events.some(one => one.value === id)) {
            return events
        }
        return events.concat([{ value: id, text: qsTranslate("LocationCommandWidget", "Unknown Event (%1)").arg(id) }])
    }

    spacing: Ez.Metrics.sectionSpacing
    Layout.fillWidth: true
    Layout.fillHeight: true

    SchemaForm {
        session: root.session
        Layout.fillHeight: false
    }

    RowLayout {
        spacing: Ez.Metrics.sectionSpacing
        Layout.fillWidth: true

        // the events the route goes through as if they were not there
        Ez.Section {
            title: qsTr("Events the Search Ignores")
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop

            RowLayout {
                spacing: Ez.Metrics.columnSpacing
                Layout.fillWidth: true

                Ez.ComboBox {
                    id: ignoreModeBox
                    objectName: "ignoreMode"
                    textRole: "text"
                    valueRole: "value"
                    model: [{ value: 0, text: qsTr("Listed here") },
                            { value: 1, text: qsTranslate("ManiacCommandWidget", "Variable") },
                            { value: 2, text: qsTranslate("ManiacCommandWidget", "Variable pointer") }]
                    Binding on currentIndex {
                        value: Math.max(0, ignoreModeBox.indexOfValue(root.ignoreMode))
                    }
                    onActivated: root.setIgnoreMode(currentValue)
                }
                Ez.RefComboBox {
                    objectName: "ignoreVariable"
                    visible: root.ignoreMode !== 0
                    database: root.session.database
                    listKey: "variables"
                    jsonData: root.session
                    key: visible ? "/parameters/12" : ""
                    Layout.fillWidth: true
                    Layout.preferredWidth: 200
                }
                Item {
                    visible: root.ignoreMode === 0
                    Layout.fillWidth: true
                }
            }
            Ez.Note {
                visible: root.ignoreMode !== 0
                // EasyRPG Player reads the value first (as for the other values), so
                // the list starts at the variable whose number this one holds
                text: root.ignoreMode === 1
                      ? qsTr("This variable holds the number of the variable where the list starts: that one holds how many events, the variables after it their IDs.")
                      : qsTr("This variable holds the number of a variable, which holds the number of the variable where the list starts: that one holds how many events, the variables after it their IDs.")
                Layout.fillWidth: true
            }
            Repeater {
                model: root.ignoreMode === 0 ? root.ignored : []
                delegate: RowLayout {
                    id: line
                    required property int index
                    required property var modelData

                    spacing: Ez.Metrics.columnSpacing
                    Layout.fillWidth: true

                    Ez.ComboBox {
                        id: eventBox
                        objectName: "ignored" + line.index
                        textRole: "text"
                        valueRole: "value"
                        model: root.eventChoices(Number(line.modelData))
                        Binding on currentIndex {
                            value: Math.max(0, eventBox.indexOfValue(Number(line.modelData)))
                        }
                        Layout.fillWidth: true
                        onActivated: {
                            const list = root.ignored.slice()
                            list[line.index] = currentValue
                            root.setIgnored(list)
                        }
                    }
                    Ez.Button {
                        objectName: "ignored" + line.index + "Remove"
                        text: qsTranslate("ManiacCommandWidget", "Remove")
                        onClicked: {
                            const list = root.ignored.slice()
                            list.splice(line.index, 1)
                            root.setIgnored(list)
                        }
                    }
                }
            }
            RowLayout {
                visible: root.ignoreMode === 0
                spacing: Ez.Metrics.columnSpacing
                Ez.Button {
                    objectName: "addIgnored"
                    text: qsTr("Add Event")
                    enabled: root.session.events.length > 0
                    onClicked: root.setIgnored(root.ignored.concat([root.session.events[0].value]))
                }
                Item {
                    Layout.fillWidth: true
                }
            }
        }

        // the speed of the event's steps on the route (1 to 8)
        Ez.Section {
            title: qsTr("Move Frequency")
            Layout.alignment: Qt.AlignTop

            RowLayout {
                spacing: Ez.Metrics.columnSpacing

                Ez.ComboBox {
                    id: frequencyModeBox
                    objectName: "frequencyMode"
                    textRole: "text"
                    valueRole: "value"
                    model: root.modes
                    Binding on currentIndex {
                        value: Math.max(0, frequencyModeBox.indexOfValue(root.frequencyMode))
                    }
                    onActivated: root.setFrequency(currentValue, currentValue === 0 ? 3 : Math.max(1, root.frequency))
                }
                Ez.SpinBox {
                    objectName: "frequency"
                    visible: root.frequencyMode === 0
                    from: 1
                    to: 8
                    value: root.frequency
                    onValueModified: root.setFrequency(0, value)
                }
                Ez.RefComboBox {
                    objectName: "frequencyVariable"
                    visible: root.frequencyMode !== 0
                    database: root.session.database
                    listKey: "variables"
                    jsonData: root.session
                    key: visible ? "/parameters/" + (root.frequencyAt + 1) : ""
                    Layout.preferredWidth: 200
                }
            }
        }
    }

    Item {
        Layout.fillHeight: true
    }
}
