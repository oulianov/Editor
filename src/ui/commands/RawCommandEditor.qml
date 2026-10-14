// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "EventRawWidget"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * The raw data of an event command: its code, its string and its numbers,
 * for the commands without an editor of their own (the EasyRPG Player
 * commands...). Numbers are added and removed at the end.
 */
ColumnLayout {
    id: root

    required property Ez.CommandEditSession session
    readonly property string title: qsTr("Raw Event Editor")
    readonly property int preferredWidth: 360
    /** The numbers of the command */
    readonly property int count: session.revision < 0 ? 0 : Math.max(0, session.size("/parameters"))

    /** Adds a number at the end (0) or removes the last one */
    function resize(count) {
        const parameters = session.value("/parameters") ?? []
        while (parameters.length < count) {
            parameters.push(0)
        }
        parameters.length = Math.max(0, count)
        session.set("/parameters", parameters)
    }

    spacing: Ez.Metrics.sectionSpacing
    Layout.fillWidth: true
    Layout.fillHeight: true

    Ez.Note {
        objectName: "labelWarn"
        visible: !!root.session.extra("warn")
        text: qsTr("This Command is not supported. The raw event data is displayed.")
    }

    Ez.Form {
        Ez.FormLabel {
            text: qsTr("Event Code:")
        }
        Ez.SpinBox {
            objectName: "spinCode"
            jsonData: root.session
            key: "/code"
            from: 0
            to: 99999
        }
        Ez.FormLabel {
            text: qsTr("String parameter:")
        }
        Ez.TextField {
            objectName: "lineString"
            jsonData: root.session
            key: "/string"
            Layout.fillWidth: true
        }
    }

    Ez.Section {
        objectName: "groupBox"
        title: qsTr("Numeric Parameters")
        Layout.fillHeight: true

        Controls.ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: Math.min(numbers.implicitHeight, 160)
            implicitHeight: Math.min(numbers.implicitHeight, 320)
            contentWidth: availableWidth

            GridLayout {
                id: numbers
                width: parent.width
                columns: 2
                rowSpacing: Ez.Metrics.rowSpacing
                columnSpacing: Ez.Metrics.columnSpacing

                Repeater {
                    model: root.count * 2
                    delegate: Loader {
                        id: cell
                        required property int index
                        readonly property int parameter: Math.floor(index / 2)
                        Layout.fillWidth: index % 2 === 1
                        sourceComponent: index % 2 === 0 ? number : spin

                        Component {
                            id: number
                            Ez.FormLabel {
                                text: (cell.parameter + 1) + ":"
                            }
                        }
                        Component {
                            id: spin
                            Ez.SpinBox {
                                objectName: "spinParameter" + cell.parameter
                                jsonData: root.session
                                key: "/parameters/" + cell.parameter
                                from: -2147483648
                                to: 2147483647
                            }
                        }
                    }
                }
            }
        }
    }

    Ez.Button {
        objectName: "buttonAddNum"
        text: qsTr("Add Numeric Parameter")
        Layout.fillWidth: true
        onClicked: root.resize(root.count + 1)
    }
    Ez.Button {
        objectName: "buttonRemoveNum"
        text: qsTr("Remove Numeric Parameter")
        Layout.fillWidth: true
        enabled: root.count > 0
        onClicked: root.resize(root.count - 1)
    }
}
