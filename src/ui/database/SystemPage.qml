// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "SystemWidget"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * The System page of the database, in RPG Maker 2003's three columns: the
 * graphics of the vehicles, the title, game over and system, the text
 * font and the starting party; the music of the game and its screen
 * transitions; its sound effects. Each file has a "..." button opening its
 * picker (a vehicle's: its character in the sheet too).
 */
Ez.EntryPage {
    id: root

    readonly property var system: database ? database.subtree("system") : null
    readonly property var actors: database ? database.choices("actors", qsTranslate("SystemFields", "<none>"), 0) : null

    single: true
    fillWidth: true

    /** A file of the system (an image), a label before it */
    component Picture: Ez.FileField {
        jsonData: root.system
        preview: "Image"
    }
    /** A vehicle's graphic: its CharSet and its character in it, "Vehicles (2)" */
    // the screen transitions: long texts (French...) on two lines, the page fits the window
    component TransitionLabel: Ez.FormLabel {
        wrapMode: Text.Wrap
        Layout.maximumWidth: 110
    }
    component TransitionHeader: Ez.FormLabel {
        wrapMode: Text.Wrap
        Layout.fillWidth: true
        Layout.preferredWidth: 1
    }
    component Vehicle: RowLayout {
        id: vehicle

        property string key
        property string fieldName

        readonly property string fileName: nameField.valid ? String(nameField.value ?? "") : ""
        readonly property int index: indexField.valid ? Number(indexField.value) : 0

        spacing: Ez.Metrics.columnSpacing
        Layout.fillWidth: true

        Ez.JsonField {
            id: nameField
            source: root.system
            path: vehicle.key + "_name"
        }
        Ez.JsonField {
            id: indexField
            source: root.system
            path: vehicle.key + "_index"
        }
        Ez.TextField {
            objectName: vehicle.fieldName
            text: vehicle.fileName === "" ? "" : vehicle.fileName + " (" + (vehicle.index + 1) + ")"
            readOnly: true
            Layout.fillWidth: true
            Layout.minimumWidth: 100
        }
        Ez.FieldButton {
            id: pick
            objectName: vehicle.fieldName + "Picker"
            onClicked: {
                const choice = Ez.Pickers.pickFile({ folder: "CharSet", preview: "CharSet", name: vehicle.fileName,
                                                     index: vehicle.index }, pick)
                if (choice) {
                    nameField.value = choice.name
                    indexField.value = choice.index
                }
            }
        }
    }
    /** A music or a sound effect of the system */
    component Audio: Ez.AudioField {
        jsonData: root.system
    }
    /** A choice of a number of the system */
    component Choice: Ez.ComboBox {
        jsonData: root.system
        Layout.fillWidth: true
    }

    /** The heroes of the starting party: as many as were chosen, in order */
    readonly property var party: partyField.valid && partyField.value ? Array.from(partyField.value) : []
    function setMember(slot, id) {
        const ids = []
        for (let i = 0; i < 4; ++i) {
            const member = i === slot ? id : Number(party[i] ?? 0)
            if (member > 0) {
                ids.push(member)
            }
        }
        partyField.value = ids
    }
    Ez.JsonField {
        id: partyField
        source: root.system
        path: "party"
    }

    /** Erasing the screen; showing it has no "No Transition", as in RPG Maker */
    readonly property var eraseChoices: [qsTr("Fade Out"), qsTr("Remove Blocks"), qsTr("Wipe Downward"), qsTr("Wipe Upward"),
        qsTr("Venetian Blinds"), qsTr("Vertical Blinds"), qsTr("Horizontal Blinds"),
        qsTr("Receding Square"), qsTr("Expanding Square"), qsTr("Screen Moves Up"),
        qsTr("Screen Moves Down"), qsTr("Screen Moves Left"), qsTr("Screen Moves Right"),
        qsTr("Vertical Division"), qsTr("Horizontal Division"), qsTr("Quadrasection"),
        qsTr("Zoom In"), qsTr("Mosaic"), qsTr("Waver Screen"), qsTr("Instantaneous"), qsTr("No Transition")]
        .map((text, i) => ({ text: text, value: i }))
    readonly property var showChoices: [qsTr("Fade In"), qsTr("Reconstitute Blocks"), qsTr("Unwipe Downward"), qsTr("Unwipe Upward"),
        qsTr("Venetian Blinds"), qsTr("Vertical Blinds"), qsTr("Horizontal Blinds"),
        qsTr("Receding Square"), qsTr("Expanding Square"), qsTr("Screen Moves Down"),
        qsTr("Screen Moves Up"), qsTr("Screen Moves Right"), qsTr("Screen Moves Left"),
        qsTr("Vertical Unify"), qsTr("Horizontal Unify"), qsTr("Unify Quadrants"),
        qsTr("Zoom Out"), qsTr("Mosaic"), qsTr("Waver Screen"), qsTr("Instantaneous")]
        .map((text, i) => ({ text: text, value: i }))

    RowLayout {
        id: columns
        spacing: Ez.Metrics.sectionSpacing
        Layout.fillWidth: true

        // the graphics and the starting party
        ColumnLayout {
            id: left
            spacing: Ez.Metrics.sectionSpacing
            Layout.alignment: Qt.AlignTop
            Layout.fillWidth: true
            // the rest of the row, halved (its combo boxes keep their width)
            Layout.preferredWidth: Math.max(implicitWidth, (root.column.room - 2 * columns.spacing - middle.Layout.preferredWidth) / 2)
            Layout.minimumWidth: implicitWidth

            Ez.Section {
                title: qsTr("Vehicles")
                Ez.Form {
                    Ez.FormLabel {
                        text: qsTr("Skiff")
                    }
                    Vehicle {
                        key: "boat"
                        fieldName: "lineBoat"
                    }
                    Ez.FormLabel {
                        text: qsTr("Ship")
                    }
                    Vehicle {
                        key: "ship"
                        fieldName: "lineShip"
                    }
                    Ez.FormLabel {
                        text: qsTr("Airship")
                    }
                    Vehicle {
                        key: "airship"
                        fieldName: "lineAirship"
                    }
                }
            }
            Ez.Section {
                title: qsTr("Graphics")
                Ez.Form {
                    Ez.FormLabel {
                        text: qsTr("Title Screen")
                    }
                    Picture {
                        fieldName: "lineTitle"
                        key: "title_name"
                        folder: "Title"
                    }
                    Ez.FormLabel {
                        text: qsTr("Game Over Screen")
                    }
                    Picture {
                        fieldName: "lineGameOver"
                        key: "gameover_name"
                        folder: "GameOver"
                    }
                    Ez.FormLabel {
                        text: qsTr("System Graphic")
                    }
                    Picture {
                        fieldName: "lineSystem"
                        key: "system_name"
                        folder: "System"
                    }
                    // RPG Maker sets these two with the system graphic
                    Ez.FormLabel {
                        text: qsTr("Graphic Pattern")
                    }
                    Choice {
                        objectName: "comboStretch"
                        key: "message_stretch"
                        model: [{ text: qsTr("Stretch to Fit"), value: 0 }, { text: qsTr("Tiled Display"), value: 1 }]
                    }
                    Ez.FormLabel {
                        text: qsTr("Text Font")
                    }
                    Choice {
                        objectName: "comboFont"
                        key: "font_id"
                        model: [{ text: qsTr("MS Gothic"), value: 0 }, { text: qsTr("MS Mincho"), value: 1 }]
                    }
                }
            }
            Ez.Section {
                title: qsTr("Starting Party")
                Ez.Form {
                    Repeater {
                        model: [qsTr("First Member"), qsTr("Second Member"), qsTr("Third Member"), qsTr("Fourth Member")]
                        Ez.FormLabel {
                            required property string modelData
                            required property int index
                            text: modelData
                            Layout.row: index
                            Layout.column: 0
                        }
                    }
                    Repeater {
                        model: 4
                        Ez.ComboBox {
                            id: member
                            required property int index
                            objectName: "comboPartyMember" + (index + 1)
                            model: root.actors
                            onActivated: root.setMember(member.index, Number(currentValue))
                            Layout.row: index
                            Layout.column: 1
                            Layout.fillWidth: true

                            /** Shows the member of the party (after the combo box's own choice of a new list) */
                            function show() {
                                currentIndex = count > 0 ? Math.max(0, indexOfValue(Number(root.party[index] ?? 0))) : -1
                            }
                            onCountChanged: Qt.callLater(show)
                            Connections {
                                target: root
                                function onPartyChanged() { member.show() }
                            }
                        }
                    }
                }
            }
        }

        // the music and the screen transitions
        ColumnLayout {
            id: middle
            spacing: Ez.Metrics.sectionSpacing
            Layout.alignment: Qt.AlignTop
            Layout.fillWidth: true
            // a third of the row, wider for the combo boxes of the screen transitions
            Layout.preferredWidth: Math.max(implicitWidth, (root.column.room - 2 * columns.spacing) / 3)
            Layout.minimumWidth: implicitWidth

            Ez.Section {
                title: qsTr("System BGM")
                Ez.Form {
                    Repeater {
                        model: [{ text: qsTr("Title Screen"), key: "title_music", name: "lineTitleMusic" },
                                { text: qsTr("Battle"), key: "battle_music", name: "lineBattleMusic" },
                                { text: qsTr("Victory"), key: "battle_end_music", name: "lineVictoryMusic" },
                                { text: qsTr("Inn"), key: "inn_music", name: "lineInnMusic" },
                                { text: qsTr("Skiff"), key: "boat_music", name: "lineBoatMusic" },
                                { text: qsTr("Ship"), key: "ship_music", name: "lineShipMusic" },
                                { text: qsTr("Airship"), key: "airship_music", name: "lineAirshipMusic" },
                                { text: qsTr("Game Over Screen"), key: "gameover_music", name: "lineGameOverMusic" }]
                        Ez.FormLabel {
                            required property var modelData
                            required property int index
                            text: modelData.text
                            Layout.row: index
                            Layout.column: 0
                        }
                    }
                    Repeater {
                        model: ["title_music", "battle_music", "battle_end_music", "inn_music", "boat_music", "ship_music",
                                "airship_music", "gameover_music"]
                        Audio {
                            required property string modelData
                            required property int index
                            music: true
                            key: modelData
                            fieldName: ["lineTitleMusic", "lineBattleMusic", "lineVictoryMusic", "lineInnMusic", "lineBoatMusic",
                                        "lineShipMusic", "lineAirshipMusic", "lineGameOverMusic"][index]
                            Layout.row: index
                            Layout.column: 1
                        }
                    }
                }
            }
            Ez.Section {
                title: qsTr("Screen Transitions")
                GridLayout {
                    columns: 3
                    rowSpacing: Ez.Metrics.rowSpacing
                    columnSpacing: Ez.Metrics.columnSpacing
                    Layout.fillWidth: true

                    Item {
                        implicitWidth: 1
                    }
                    TransitionHeader {
                        text: qsTr("Erase Screen")
                    }
                    TransitionHeader {
                        text: qsTr("Show Screen")
                    }
                    TransitionLabel {
                        text: qsTr("Map Transitions")
                    }
                    Choice {
                        objectName: "comboTransitionOut"
                        key: "transition_out"
                        model: root.eraseChoices
                        Layout.preferredWidth: 1
                        Layout.minimumWidth: implicitWidth
                    }
                    Choice {
                        objectName: "comboTransitionIn"
                        key: "transition_in"
                        model: root.showChoices
                        Layout.preferredWidth: 1
                        Layout.minimumWidth: implicitWidth
                    }
                    TransitionLabel {
                        text: qsTr("Battle Entry")
                    }
                    Choice {
                        objectName: "comboBattleStartOut"
                        key: "battle_start_fadeout"
                        model: root.eraseChoices
                        Layout.preferredWidth: 1
                        Layout.minimumWidth: implicitWidth
                    }
                    Choice {
                        objectName: "comboBattleStartIn"
                        key: "battle_start_fadein"
                        model: root.showChoices
                        Layout.preferredWidth: 1
                        Layout.minimumWidth: implicitWidth
                    }
                    TransitionLabel {
                        text: qsTr("Battle Exit")
                    }
                    Choice {
                        objectName: "comboBattleEndOut"
                        key: "battle_end_fadeout"
                        model: root.eraseChoices
                        Layout.preferredWidth: 1
                        Layout.minimumWidth: implicitWidth
                    }
                    Choice {
                        objectName: "comboBattleEndIn"
                        key: "battle_end_fadein"
                        model: root.showChoices
                        Layout.preferredWidth: 1
                        Layout.minimumWidth: implicitWidth
                    }
                }
            }
        }

        // the sound effects
        ColumnLayout {
            spacing: Ez.Metrics.sectionSpacing
            Layout.alignment: Qt.AlignTop
            Layout.fillWidth: true
            // the rest of the row; the file fields narrow down to 100 pixels
            Layout.preferredWidth: root.column.room - 2 * columns.spacing - middle.Layout.preferredWidth - left.Layout.preferredWidth
            Layout.minimumWidth: implicitWidth - (Ez.Metrics.fieldWidth - 100)

            Ez.Section {
                title: qsTr("System Sound Effects")
                Ez.Form {
                    Repeater {
                        model: [qsTr("Cursor"), qsTr("Select Choice"), qsTr("Cancel"), qsTr("Buzzer"), qsTr("Battle Transition"),
                                qsTr("Flee Battle"), qsTr("Enemy Attacks"), qsTr("Enemy Damaged"), qsTr("Ally Damaged"),
                                qsTr("Evade Attack"), qsTr("Enemy Defeated"), qsTr("Item Used")]
                        Ez.FormLabel {
                            required property string modelData
                            required property int index
                            text: modelData
                            Layout.row: index
                            Layout.column: 0
                        }
                    }
                    Repeater {
                        model: [{ key: "cursor_se", name: "lineCursorSe" }, { key: "decision_se", name: "lineDecisionSe" },
                                { key: "cancel_se", name: "lineCancelSe" }, { key: "buzzer_se", name: "lineBuzzerSe" },
                                { key: "battle_se", name: "lineBattleSe" }, { key: "escape_se", name: "lineEscapeSe" },
                                { key: "enemy_attack_se", name: "lineEnemyAttackSe" },
                                { key: "enemy_damaged_se", name: "lineEnemyDamagedSe" },
                                { key: "actor_damaged_se", name: "lineActorDamagedSe" }, { key: "dodge_se", name: "lineDodgeSe" },
                                { key: "enemy_death_se", name: "lineEnemyDeathSe" }, { key: "item_se", name: "lineItemSe" }]
                        Audio {
                            required property var modelData
                            required property int index
                            key: modelData.key
                            fieldName: modelData.name
                            Layout.row: index
                            Layout.column: 1
                        }
                    }
                }
            }
        }
    }
}
