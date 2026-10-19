// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "RunGameDialog"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * Game > Test Play Options: how the EasyRPG Player starts on the project
 * (normal, new game, a savegame, a battle test), where the party starts
 * and who is in it, the audio. The values are a TestPlayOptions'; OK
 * starts the Player with them, and the window stays open when it does not
 * start. A battle test saves its party in the database first, from which
 * the Player reads it.
 */
Ez.DialogWindow {
    id: root

    required property Ez.TestPlayOptions options
    readonly property bool battle: options.mode === Ez.TestPlayOptions.TestBattle
    /** The game starts from its beginning: the start position and party can be replaced */
    readonly property bool gameStart: options.mode === Ez.TestPlayOptions.Normal
                                      || options.mode === Ez.TestPlayOptions.NewGame

    objectName: "RunGameDialog"
    title: qsTr("Test Play Options")

    onAccepting: check => {
        if (root.battle && !testPlay.saveBattleParty(battleParty)) {
            check.ok = false
            return
        }
        check.ok = testPlay.launch(root.options.arguments(), false)
    }

    Ez.TestPlay {
        id: testPlay
        window: root
    }
    Ez.TestBattlePartyModel {
        id: battleParty
        projectData: Ez.ProjectData
    }

    GridLayout {
        columns: 2
        rowSpacing: Ez.Metrics.sectionSpacing
        columnSpacing: Ez.Metrics.dialogMargin
        Layout.fillWidth: true
        Layout.fillHeight: true

        ColumnLayout {
            spacing: Ez.Metrics.sectionSpacing
            Layout.alignment: Qt.AlignTop

            Ez.Section {
                title: qsTr("Play Mode")
                Ez.ComboBox {
                    id: mode
                    objectName: "comboMode"
                    textRole: ""
                    valueRole: ""
                    // TestPlayOptions.Mode
                    model: [qsTr("Normal"), qsTr("New Game"), qsTr("Load savegame"), qsTr("Test Battle")]
                    Layout.fillWidth: true
                    onActivated: index => root.options.mode = index
                    Binding on currentIndex {
                        value: mode.count > 0 ? root.options.mode : -1
                    }
                }
            }
            Ez.Section {
                title: qsTr("Display Mode")
                Ez.ComboBox {
                    id: display
                    objectName: "comboDisplayMode"
                    textRole: ""
                    valueRole: ""
                    model: [qsTr("Window"), qsTr("Fullscreen")]
                    Layout.fillWidth: true
                    onActivated: index => root.options.fullscreen = index === 1
                    Binding on currentIndex {
                        value: display.count > 0 ? (root.options.fullscreen ? 1 : 0) : -1
                    }
                }
            }
            Ez.Section {
                title: qsTr("Options")
                GridLayout {
                    columns: 2
                    rowSpacing: Ez.Metrics.rowSpacing
                    columnSpacing: 2 * Ez.Metrics.columnSpacing

                    Ez.CheckBox {
                        objectName: "checkSfx"
                        text: qsTr("Disable Sfx")
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.text: qsTr("Plays the sound effects at volume 0")
                        onToggled: root.options.noSound = checked
                        Binding on checked {
                            value: root.options.noSound
                        }
                    }
                    Ez.CheckBox {
                        objectName: "checkRTP"
                        text: qsTr("Disable RTP")
                        onToggled: root.options.noRtp = checked
                        Binding on checked {
                            value: root.options.noRtp
                        }
                    }
                    Ez.CheckBox {
                        objectName: "checkMusic"
                        text: qsTr("Disable Music")
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.text: qsTr("Plays the music at volume 0")
                        onToggled: root.options.noMusic = checked
                        Binding on checked {
                            value: root.options.noMusic
                        }
                    }
                }
            }
        }

        Ez.Section {
            objectName: "groupBattleOptions"
            title: qsTr("Battle options")
            enabled: root.battle
            Layout.alignment: Qt.AlignTop

            Ez.Form {
                Ez.FormLabel {
                    text: qsTr("Initial Condition")
                }
                Ez.ComboBox {
                    id: condition
                    objectName: "comboBattleCondition"
                    textRole: ""
                    valueRole: ""
                    model: root.options.conditions
                    Layout.fillWidth: true
                    onActivated: index => root.options.condition = index
                    Binding on currentIndex {
                        value: condition.count > 0 ? root.options.condition : -1
                    }
                }
                Ez.FormLabel {
                    text: qsTr("Enemy Troop")
                }
                Ez.ComboBox {
                    id: troop
                    objectName: "comboTroop"
                    textRole: ""
                    valueRole: ""
                    // the test troop of the database first
                    model: root.options.troops
                    Layout.fillWidth: true
                    onActivated: index => root.options.troopIndex = index
                    Binding on currentIndex {
                        value: troop.count > 0 ? root.options.troopIndex : -1
                    }
                }
            }
            // the formation, condition and terrain are given when the heroes are placed automatically
            Ez.Section {
                objectName: "groupPartyFormation"
                title: qsTr("Party formation")
                enabled: root.options.formationEnabled

                GridLayout {
                    columns: 2
                    rowSpacing: Ez.Metrics.rowSpacing
                    columnSpacing: Ez.Metrics.columnSpacing

                    // lcf::rpg::System::BattleFormation
                    Ez.RadioButton {
                        objectName: "radioLooseFormation"
                        text: qsTr("Use a Loose Formation")
                        value: 1
                        Layout.columnSpan: 2
                        onToggled: if (checked) root.options.formation = value
                        Binding on checked {
                            value: root.options.formation === 1
                        }
                    }
                    Ez.RadioButton {
                        objectName: "radioTightFormation"
                        text: qsTr("Use a Tight Formation")
                        value: 2
                        Layout.columnSpan: 2
                        onToggled: if (checked) root.options.formation = value
                        Binding on checked {
                            value: root.options.formation === 2
                        }
                    }
                    Ez.RadioButton {
                        objectName: "radioCustomFormation"
                        text: qsTr("Use this Formation")
                        value: 0
                        onToggled: if (checked) root.options.formation = value
                        Binding on checked {
                            value: root.options.formation === 0
                        }
                    }
                    Ez.ComboBox {
                        id: terrain
                        objectName: "comboCustomFormation"
                        textRole: ""
                        valueRole: ""
                        model: root.options.terrains
                        Layout.fillWidth: true
                        onActivated: index => root.options.terrainIndex = index
                        Binding on currentIndex {
                            value: terrain.count > 0 ? root.options.terrainIndex : -1
                        }
                    }
                }
            }
        }

        RowLayout {
            spacing: Ez.Metrics.dialogMargin
            Layout.columnSpan: 2
            Layout.fillWidth: true

            Ez.Section {
                checkName: "groupInitialPosition"
                title: qsTr("Replace party starting position")
                checkable: true
                checked: root.options.replacePosition
                enabled: root.gameStart
                onToggled: root.options.replacePosition = checked

                RowLayout {
                    spacing: Ez.Metrics.columnSpacing

                    Ez.ComboBox {
                        id: map
                        objectName: "comboMapId"
                        textRole: ""
                        valueRole: ""
                        model: root.options.maps
                        Layout.fillWidth: true
                        onActivated: index => root.options.mapIndex = index
                        Binding on currentIndex {
                            value: map.count > 0 ? root.options.mapIndex : -1
                        }
                    }
                    Ez.SpinBox {
                        objectName: "spinX"
                        prefix: qsTr("X: ")
                        from: 0
                        to: 499
                        onValueModified: root.options.x = value
                        Binding on value {
                            value: root.options.x
                        }
                    }
                    Ez.SpinBox {
                        objectName: "spinY"
                        prefix: qsTr("Y: ")
                        from: 0
                        to: 499
                        onValueModified: root.options.y = value
                        Binding on value {
                            value: root.options.y
                        }
                    }
                }
            }
            Ez.Section {
                objectName: "groupSave"
                title: qsTr("Save Game")
                enabled: root.options.mode === Ez.TestPlayOptions.LoadGame
                Layout.fillWidth: false

                Ez.SpinBox {
                    objectName: "spinSave"
                    prefix: qsTr("Slot ")
                    from: 1
                    to: 99
                    onValueModified: root.options.saveSlot = value
                    Binding on value {
                        value: root.options.saveSlot
                    }
                }
            }
        }

        Ez.Section {
            checkName: "groupInitialParty"
            title: qsTr("Replace initial party")
            checkable: true
            checked: root.options.replaceParty
            enabled: root.gameStart
            visible: !root.battle
            Layout.columnSpan: 2
            onToggled: root.options.replaceParty = checked

            Repeater {
                model: 4
                delegate: Ez.ComboBox {
                    id: hero
                    required property int index

                    objectName: "comboInitialParty" + (index + 1)
                    textRole: ""
                    valueRole: ""
                    model: root.options.heroes
                    Layout.fillWidth: true
                    onActivated: choice => root.options.setPartyMember(index, choice)
                    Binding on currentIndex {
                        value: hero.count > 0 ? root.options.party[hero.index] : -1
                    }
                }
            }
        }

        // the levels and equipment of the battle test, saved in the database
        Ez.Section {
            objectName: "groupBattleParty"
            title: qsTr("Battle Test Party")
            visible: root.battle
            Layout.columnSpan: 2
            Layout.fillHeight: true
            Controls.ToolTip.visible: partyHover.hovered
            Controls.ToolTip.text: qsTr("Saved in the database when the battle test starts")

            HoverHandler {
                id: partyHover
            }
            Ez.TestBattlePartyView {
                party: battleParty
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }
    }
}
