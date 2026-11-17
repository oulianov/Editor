// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "System2Widget"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * The System 2 page of the database: the commands of the main menu and the
 * frame, battle graphics and title screen of RPG Maker 2003 (the battle
 * tests are set in their window, from Monster Groups: TestBattleDialog.qml).
 * With the Maniac Patch: Quit Game can be hidden from the menu,
 * the title and save scenes call a common event (Title Scene Control,
 * Save/Load Scene Control), and the message window has settings of its own
 * (ManiacMessageDialog.qml). Without it, they are greyed with their tag M,
 * as set (the tag warns when they are not RPG Maker's), or hidden unless set
 * when the Editor Settings hide what the game does not run.
 */
DatabaseEntryPage {
    id: root

    readonly property var system: database ? database.subtree("system") : null

    single: true
    fillWidth: true

    /** The list of the menu as the data has it */
    readonly property var menuValues: menuField.valid && menuField.value ? Array.from(menuField.value).map(Number) : []
    /** The commands of the main menu: as many as were chosen, in order (not the Maniac Patch's Quit Game, 9 or -9) */
    readonly property var menuCommands: menuValues.filter(id => Math.abs(id) !== 9)
    /** The Maniac Patch's Quit Game in the list: 9 shown, -9 hidden (none: shown) */
    readonly property var quitGame: menuValues.filter(id => Math.abs(id) === 9)
    readonly property var commandChoices: [qsTr("<none>"), qsTr("Item"), qsTr("Skill"), qsTr("Equipment"), qsTr("Save"),
                                           qsTr("Status"), qsTr("Row"), qsTr("Order"), qsTr("Wait")]
        .map((text, i) => ({ text: text, value: i }))
    function setCommand(slot, id) {
        const ids = []
        for (let i = 0; i < 8; ++i) {
            const command = i === slot ? id : Number(menuCommands[i] ?? 0)
            if (command > 0) {
                ids.push(command)
            }
        }
        // Quit Game stays last
        menuField.value = ids.concat(quitGame)
    }
    /** Shows or hides Quit Game, as the Maniac Patch's editor: -9 hidden, 9 shown (left out when it was) */
    function setQuitGame(shown) {
        menuField.value = menuCommands.concat(shown ? (quitGame.length > 0 ? [9] : []) : [-9])
    }
    /**
     * The defaults of the Maniac Patch's message window (liblcf's), by
     * field: one of them changed is a setting the game runs only with it
     */
    readonly property var messageDefaults: ({
        maniac_message_font_name: "", maniac_message_font_size: 12, maniac_message_window_width: 320,
        maniac_message_window_height: 80, maniac_message_spacing_char: 0, maniac_message_spacing_line: 4,
        maniac_message_face_x: 16, maniac_message_face_y: 16, maniac_message_face_width: 48,
        maniac_message_face_height: 48, maniac_message_text_x: 8, maniac_message_text_y: 10,
        maniac_message_text_face_x: 80, maniac_message_text_face_y: 10,
    })
    // read again when the system changes (the message window's OK)
    property int systemRevision: 0
    Connections {
        target: root.system
        ignoreUnknownSignals: true
        function onValueChanged() { root.systemRevision++ }
    }
    /** The Maniac Patch's Quit Game check box is shown: greyed without it, or hidden unless hidden from the menu */
    readonly property bool quitGameShown: Ez.Origins.shown("maniac") || quitGame.includes(-9)
    /** A scene (prefix "maniac_title_scene"...) calls a common event */
    function sceneSet(prefix) {
        return !!system && systemRevision >= 0 && system.num(prefix + "_common_event_id") > 0
    }
    /** The message window has settings of the Maniac Patch */
    readonly property bool messageWindowSet: {
        if (!system || systemRevision < 0) {
            return false
        }
        for (const key in messageDefaults) {
            const value = key === "maniac_message_font_name" ? system.str(key) : system.num(key)
            if (value !== messageDefaults[key]) {
                return true
            }
        }
        return false
    }
    Ez.JsonField {
        id: menuField
        source: root.system
        path: "menu_commands"
    }

    /**
     * A combo box of an entry of a list (a menu command): shown again once its choices are read (after the combo box's
     * own choice of a new list) or the entry changes; chosen(value) by the user
     */
    component ListBox: Ez.ComboBox {
        property int shownValue
        signal chosen(int value)
        function show() {
            currentIndex = count > 0 ? Math.max(0, indexOfValue(shownValue)) : -1
        }
        onCountChanged: Qt.callLater(show)
        onShownValueChanged: show()
        onActivated: chosen(Number(currentValue))
    }
    /**
     * The common event a scene calls (Maniac Patch), the first of the
     * variables it writes its state to before, and when it calls it
     */
    component SceneControl: Ez.Section {
        id: scene
        /** The fields: prefix + "_common_event_id"... */
        required property string prefix
        /** The objectNames: "comboTitleSceneEvent"... */
        required property string names
        /** The title scene's New Game */
        required property bool newGame
        origin: "maniac"
        enabled: root.maniac
        // a common event set in a game without the Maniac Patch: not called
        originWarning: !root.maniac && root.sceneSet(prefix)
        visible: Ez.Origins.shown("maniac") || originWarning
        Layout.alignment: Qt.AlignTop
        Layout.fillHeight: true
        Layout.preferredWidth: Math.max(implicitWidth, (root.column.room - scenes.spacing) / 2)
        Ez.Form {
            Ez.FormLabel {
                text: qsTr("Common Event")
            }
            Ez.RefComboBox {
                objectName: "combo" + scene.names + "SceneEvent"
                database: root.database
                listKey: "commonevents"
                noneText: qsTr("(None)")
                jsonData: root.system
                key: scene.prefix + "_common_event_id"
                Layout.fillWidth: true
            }
            Ez.FormLabel {
                text: qsTr("Variable")
            }
            Ez.RefComboBox {
                objectName: "combo" + scene.names + "SceneVariable"
                database: root.database
                listKey: "variables"
                noneText: qsTr("(None)")
                jsonData: root.system
                key: scene.prefix + "_variable_id"
                Layout.fillWidth: true
                Controls.ToolTip.visible: hovered
                Controls.ToolTip.delay: 700
                Controls.ToolTip.text: qsTr("The first of the variables the scene writes its state to before calling the event")
            }
            Ez.FormLabel {
                text: qsTr("Call On")
                Layout.alignment: Qt.AlignTop
            }
            GridLayout {
                columns: 3
                rowSpacing: Ez.Metrics.rowSpacing
                columnSpacing: 2 * Ez.Metrics.columnSpacing
                Repeater {
                    model: [
                        { key: "initialize", text: qsTr("Initialize") },
                        { key: "create_picture", text: qsTr("Create Picture") },
                        { key: "begin_scene", text: qsTr("Begin Scene") },
                        { key: "end_scene", text: qsTr("End Scene") },
                        { key: "every_frame", text: qsTr("Every Frame") },
                        { key: "selection_changed", text: qsTr("Selection Changed") },
                    ].concat(scene.newGame ? [{ key: "new_game", text: qsTr("New Game") }] : [])
                    Ez.CheckBox {
                        required property var modelData
                        objectName: "check" + scene.names + "Scene_" + modelData.key
                        text: modelData.text
                        jsonData: root.system
                        key: scene.prefix + "_hook_flags/" + modelData.key
                    }
                }
            }
        }
    }


    RowLayout {
        id: top
        spacing: Ez.Metrics.sectionSpacing
        Layout.fillWidth: true

        // RPG Maker 2003: the commands of the main menu, in order, then "Quit Game"
        Ez.Section {
            title: qsTr("Menu Commands")
            enabled: root.rpg2k3
            Layout.alignment: Qt.AlignTop
            Layout.fillHeight: true
            // half of the row each, wider when its fields need it (the page then scrolls)
            Layout.preferredWidth: Math.max(implicitWidth, (root.column.room - top.spacing) / 2)
            GridLayout {
                // read down the first column, then the second
                flow: GridLayout.TopToBottom
                rows: 5
                rowSpacing: Ez.Metrics.rowSpacing
                columnSpacing: Ez.Metrics.columnSpacing
                Layout.fillWidth: true

                Repeater {
                    model: 8
                    ListBox {
                        id: command
                        required property int index
                        objectName: "comboMenuCommand" + (index + 1)
                        model: root.commandChoices
                        shownValue: Number(root.menuCommands[index] ?? 0)
                        onChosen: (value) => root.setCommand(command.index, value)
                        Layout.row: index % 4
                        Layout.column: Math.floor(index / 4)
                        Layout.fillWidth: true
                    }
                }
                // without the Maniac Patch, RPG Maker's Quit Game, always there
                // (when the Editor Settings hide what the game does not run)
                Ez.Label {
                    text: qsTr("Quit Game")
                    font: Ez.Metrics.font
                    leftPadding: Ez.Metrics.columnSpacing
                    visible: !root.quitGameShown
                    Layout.row: 4
                    Layout.column: 1
                }
                // the Maniac Patch can hide it (greyed without it, hidden stays hidden)
                RowLayout {
                    spacing: Ez.Metrics.columnSpacing
                    visible: root.quitGameShown
                    Layout.row: 4
                    Layout.column: 1
                    Ez.CheckBox {
                        objectName: "checkQuitGame"
                        text: qsTr("Quit Game")
                        enabled: root.maniac
                        Binding on checked {
                            value: !root.quitGame.includes(-9)
                            restoreMode: Binding.RestoreNone
                        }
                        onToggled: root.setQuitGame(checked)
                        Ez.OriginTip {
                            origin: "maniac"
                        }
                    }
                    Ez.OriginTag {
                        origin: "maniac"
                        warning: !root.maniac && root.quitGame.includes(-9)
                    }
                }
            }
        }

        // RPG Maker 2003: frame, battle graphics and title
        Ez.Section {
            title: qsTr("Graphics")
            enabled: root.rpg2k3
            Layout.alignment: Qt.AlignTop
            Layout.fillHeight: true
            // half of the row each, wider when its fields need it (the page then scrolls)
            Layout.preferredWidth: Math.max(implicitWidth, (root.column.room - top.spacing) / 2)
            Ez.Form {
                Ez.CheckBox {
                    objectName: "checkShowFrame"
                    text: qsTr("Use Frame")
                    jsonData: root.system
                    key: "show_frame"
                    Layout.columnSpan: 2
                }
                Ez.FormLabel {
                    text: qsTr("Frame")
                }
                Ez.FileField {
                    fieldName: "lineFrame"
                    jsonData: root.system
                    key: "frame_name"
                    folder: "Frame"
                }
                Ez.FormLabel {
                    text: qsTr("Battle Graphics")
                }
                Ez.FileField {
                    fieldName: "lineSystem2"
                    jsonData: root.system
                    key: "system2_name"
                    folder: "System2"
                }
                Ez.CheckBox {
                    objectName: "checkInvertAnimations"
                    text: qsTr("Reverse Battle Animations when Attacked from Behind")
                    jsonData: root.system
                    key: "invert_animations"
                    Layout.columnSpan: 2
                }
                // RPG Maker 2003 1.11: without title screen, the game starts on the first map
                Ez.CheckBox {
                    objectName: "checkShowTitle"
                    text: qsTr("Show Title Screen")
                    jsonData: root.system
                    key: "show_title"
                    Layout.columnSpan: 2
                }
                // the Maniac Patch's Extra Settings: the message window (greyed
                // without it, or hidden unless set; its settings kept)
                RowLayout {
                    spacing: Ez.Metrics.columnSpacing
                    visible: Ez.Origins.shown("maniac") || root.messageWindowSet
                    Layout.columnSpan: 2
                    Ez.Button {
                        id: messageWindow
                        objectName: "pushMessageWindow"
                        text: qsTr("Message Window...")
                        enabled: root.maniac
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.delay: 700
                        Controls.ToolTip.text: qsTr("The font, size and layout of the message window (Maniac Patch)")
                        onClicked: Ez.Editor.exec("ManiacMessageDialog", { system: root.system }, messageWindow)
                        Ez.OriginTip {
                            origin: "maniac"
                        }
                    }
                    Ez.OriginTag {
                        origin: "maniac"
                        warning: !root.maniac && root.messageWindowSet
                    }
                }
            }
        }
    }

    // the Maniac Patch: the common event the title and save scenes call
    // (greyed without it, as set; or hidden unless set)
    RowLayout {
        id: scenes
        spacing: Ez.Metrics.sectionSpacing
        visible: Ez.Origins.shown("maniac")
                 || (!root.maniac && (root.sceneSet("maniac_title_scene") || root.sceneSet("maniac_save_scene")))
        Layout.fillWidth: true

        SceneControl {
            objectName: "groupTitleScene"
            title: qsTr("Title Scene Control")
            prefix: "maniac_title_scene"
            names: "Title"
            newGame: true
        }
        SceneControl {
            objectName: "groupSaveScene"
            title: qsTr("Save/Load Scene Control")
            prefix: "maniac_save_scene"
            names: "Save"
            newGame: false
        }
    }
}
