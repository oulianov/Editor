// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "ProjectSettingsDialog"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import QtQuick.Dialogs as Dialogs
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * Game Project > Project Settings: the folder and title of the game (RPG
 * Maker's Rename Project), the code page of its texts, the engine and the
 * fonts it runs with, and what only EasyRPG Player reads (the patches of
 * EasyRPG.ini, its fonts). The values are a ProjectSettings'; checked on
 * OK, then written by the window that opened this one (it closes and
 * reloads the project when the folder or the code page change).
 */
Ez.DialogWindow {
    id: root

    required property Ez.ProjectSettings settings
    /** Read again when EasyRPG.ini changes, the patches, the fonts */
    property int iniRevision: 0
    property int patchRevision: 0
    property int fontRevision: 0
    /** The patch pointed at or focused, explained under them ("": none) */
    property string explained: ""

    objectName: "ProjectSettingsDialog"
    title: qsTr("Project Settings")
    preferredWidth: 600
    buttons: [
        // what the Editor does not show: edited by hand, read again on coming back
        Ez.Button {
            objectName: "pushOpenIni"
            text: qsTr("Open EasyRPG.ini")
            Controls.DialogButtonBox.buttonRole: Controls.DialogButtonBox.ActionRole
            Controls.ToolTip.visible: hovered
            Controls.ToolTip.text: qsTr("For the settings not shown here: opens it in the script editor (created if missing)")
            onClicked: dialogs.run(() => root.settings.openIni())
        }
    ]

    onAccepting: check => {
        const error = root.settings.validate()
        if (error !== "") {
            check.ok = false
            root.settings.convertRequested = false
            invalid.text = error
            invalid.open()
        }
    }
    // EasyRPG.ini edited in the script editor: what it says now
    onActiveChanged: {
        if (active) {
            root.settings.readIni()
        }
    }
    Connections {
        target: root.settings
        function onIniChanged() { root.iniRevision++ }
        function onPatchesChanged() { root.patchRevision++ }
        function onPlayerFontsChanged() { root.fontRevision++ }
    }

    Ez.DialogScope {
        id: dialogs
        window: root
    }

    Controls.TabBar {
        id: tabs
        objectName: "tabs"
        font: Ez.Metrics.font
        Layout.fillWidth: true
        Controls.TabButton {
            text: qsTr("General")
            width: implicitWidth
        }
        Controls.TabButton {
            text: qsTr("Engine")
            width: implicitWidth
        }
        Controls.TabButton {
            text: qsTr("EasyRPG Player")
            width: implicitWidth
        }
    }

    StackLayout {
        currentIndex: tabs.currentIndex
        Layout.fillWidth: true
        Layout.fillHeight: true

        // General: RPG Maker's Rename Project, the code page of the texts
        ColumnLayout {
            spacing: Ez.Metrics.sectionSpacing

            Ez.Section {
                title: qsTr("Project")
                Ez.Form {
                    Ez.FormLabel {
                        text: qsTr("Directory Name")
                    }
                    Ez.TextField {
                        objectName: "lineFolderName"
                        text: root.settings.folderName
                        onTextEdited: root.settings.folderName = text
                        Layout.fillWidth: true
                    }
                    Ez.FormLabel {
                        text: qsTr("Game Name")
                    }
                    Ez.TextField {
                        objectName: "lineGameTitle"
                        text: root.settings.gameTitle
                        onTextEdited: root.settings.gameTitle = text
                        Layout.fillWidth: true
                    }
                }
            }
            // [EasyRPG] Encoding of RPG_RT.ini
            Ez.Section {
                title: qsTr("Text")
                Ez.Form {
                    Ez.FormLabel {
                        text: qsTr("Encoding")
                    }
                    Ez.Label {
                        visible: !root.settings.encodingEditable
                        text: "UTF-8"
                        font: Ez.Metrics.font
                    }
                    Ez.ComboBox {
                        id: encoding
                        objectName: "comboEncoding"
                        visible: root.settings.encodingEditable
                        textRole: "name"
                        valueRole: "value"
                        model: root.settings.encodingChoices()
                        Layout.fillWidth: true
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.text: qsTr("The code page the texts of the game are written in. "
                            + "A wrong one shows them garbled: the game is read again with the one chosen.")
                        onActivated: root.settings.encoding = currentValue
                        Binding on currentIndex {
                            // again once the entries are there
                            value: { encoding.count; return Math.max(0, encoding.indexOfValue(root.settings.encoding)) }
                        }
                    }
                }
                Ez.Note {
                    visible: root.settings.encodingEditable && root.settings.open
                    text: qsTr("Changing it closes the project (asking to save it) and opens it again.")
                    color: Ez.Metrics.mutedTextColor
                }
            }
            Item {
                Layout.fillHeight: true
            }
        }

        // Engine: what runs the game, and the fonts it draws with
        ColumnLayout {
            spacing: Ez.Metrics.sectionSpacing

            Ez.Section {
                title: qsTr("Engine")
                Ez.Form {
                    Ez.FormLabel {
                        visible: root.settings.open
                        text: qsTr("Project")
                    }
                    RowLayout {
                        visible: root.settings.open
                        spacing: Ez.Metrics.columnSpacing
                        Layout.fillWidth: true
                        Ez.Label {
                            objectName: "labelEngine"
                            text: root.settings.engineName
                            font: Ez.Metrics.font
                            Layout.fillWidth: true
                        }
                        Ez.Button {
                            objectName: "pushConvert"
                            visible: root.settings.canConvert
                            text: qsTr("Convert to RPG Maker 2003...")
                            Controls.ToolTip.visible: hovered
                            Controls.ToolTip.text: qsTr("Writes the settings of this window, then converts the game (a backup is made first)")
                            onClicked: {
                                root.settings.convertRequested = true
                                root.accept()
                            }
                        }
                    }
                    Ez.FormLabel {
                        text: qsTr("Player")
                        Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                    }
                    Ez.Label {
                        objectName: "labelRuntime"
                        text: { root.iniRevision; return root.settings.runtime }
                        font: Ez.Metrics.font
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        HoverHandler {
                            id: runtimeHover
                        }
                        Controls.ToolTip.visible: runtimeHover.hovered && text !== "" && root.settings.runtimeExe !== ""
                        Controls.ToolTip.text: root.settings.runtimeExe
                    }
                }
            }
            // the font of the database, and those RPG_RT.exe draws them with
            Ez.Section {
                title: qsTr("Fonts")
                Ez.Form {
                    Ez.FormLabel {
                        visible: root.settings.open
                        text: qsTr("Text font")
                    }
                    Ez.ComboBox {
                        id: textFont
                        objectName: "comboTextFont"
                        visible: root.settings.open
                        textRole: "name"
                        valueRole: "value"
                        model: root.settings.textFontChoices()
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.text: qsTr("The font of the game (Database > System), saved in the database")
                        onActivated: root.settings.textFont = currentValue
                        Binding on currentIndex {
                            value: { textFont.count; return textFont.indexOfValue(root.settings.textFont) }
                        }
                    }
                    Item {
                        visible: root.settings.open
                        implicitWidth: 1
                    }
                    Ez.PreviewImage {
                        objectName: "labelFontPreview"
                        visible: root.settings.open
                        ratio: 2
                        image: { root.fontRevision; return root.settings.fontPreview(Kirigami.Theme.textColor) }
                        Layout.fillWidth: true
                        HoverHandler {
                            id: previewHover
                        }
                        Controls.ToolTip.visible: previewHover.hovered
                        Controls.ToolTip.text: qsTr("As EasyRPG Player draws it")
                    }
                    Ez.FormLabel {
                        text: qsTr("RPG_RT.exe fonts")
                    }
                    Ez.Label {
                        objectName: "labelExeFonts"
                        text: { root.iniRevision; return root.settings.exeFonts }
                        font: Ez.Metrics.font
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        HoverHandler {
                            id: exeHover
                        }
                        Controls.ToolTip.visible: exeHover.hovered
                        Controls.ToolTip.text: qsTr("Fonts of Windows: the ones of font 1 and font 2")
                    }
                    // the symbols of messages, crisp at twice the pixels
                    Ez.FormLabel {
                        text: qsTr("Symbols")
                    }
                    RowLayout {
                        spacing: Ez.Metrics.columnSpacing
                        Layout.fillWidth: true
                        Ez.PreviewImage {
                            objectName: "labelExFont"
                            ratio: 2
                            image: root.settings.symbolsImage(Kirigami.Theme.textColor)
                            HoverHandler {
                                id: symbolsHover
                            }
                            Controls.ToolTip.visible: symbolsHover.hovered
                            Controls.ToolTip.text: qsTr("$A to $Z, then $a to $z")
                        }
                        Ez.Note {
                            objectName: "labelExFontSource"
                            text: root.settings.symbolsSource
                            color: Ez.Metrics.mutedTextColor
                        }
                    }
                }
            }
            Item {
                Layout.fillHeight: true
            }
        }

        // EasyRPG Player: what it alone reads, the patches and the fonts
        ColumnLayout {
            spacing: Ez.Metrics.sectionSpacing

            // settings, not patches applied
            Ez.Note {
                objectName: "labelPlayerAbout"
                text: qsTr("Read by EasyRPG Player only (EasyRPG.ini, Font folder). RPG_RT.exe is not changed.")
                color: Ez.Metrics.mutedTextColor
            }
            Ez.Section {
                title: qsTr("Patches")
                Ez.Form {
                    Ez.FormLabel {
                        text: qsTr("Patches")
                    }
                    Ez.ComboBox {
                        id: patchMode
                        objectName: "comboPatchMode"
                        textRole: "name"
                        valueRole: "value"
                        enabledRole: "enabled"
                        // any key of [Patch] stops the detection of the Player
                        model: { root.iniRevision; return root.settings.patchModeChoices() }
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.text: qsTr("Detect: as EasyRPG Player does when EasyRPG.ini sets none. "
                            + "Choose: the patches checked are written in EasyRPG.ini. "
                            + "As written: the Editor leaves EasyRPG.ini as it is.")
                        onActivated: root.settings.patchMode = currentValue
                        Binding on currentIndex {
                            value: { patchMode.count; return patchMode.indexOfValue(root.settings.patchMode) }
                        }
                    }
                }
                // detected and as written are greyed: only chosen patches are checked by hand
                GridLayout {
                    id: patchGrid
                    columns: 2
                    rowSpacing: Ez.Metrics.rowSpacing
                    columnSpacing: Ez.Metrics.columnSpacing * 2
                    Layout.fillWidth: true

                    Repeater {
                        model: root.settings.patches()
                        delegate: Ez.CheckBox {
                            id: patch
                            required property var modelData
                            readonly property string patchKey: modelData.key
                            objectName: "patch" + patchKey
                            text: modelData.name
                            enabled: root.settings.patchMode === Ez.ProjectSettings.Choose
                            Layout.fillWidth: true
                            onToggled: {
                                root.settings.setPatchOn(patchKey, checked)
                                root.explained = patchKey
                            }
                            onActiveFocusChanged: {
                                if (activeFocus) {
                                    root.explained = patchKey
                                }
                            }
                            Binding on checked {
                                value: { root.patchRevision; return root.settings.patchOn(patch.patchKey) }
                            }
                        }
                    }
                    // greyed boxes get no pointer: the grid tells which one is pointed at
                    HoverHandler {
                        onPointChanged: {
                            // "patchManiac": the key after "patch"
                            const child = patchGrid.childAt(point.position.x, point.position.y)
                            if (child && child.objectName.startsWith("patch")) {
                                root.explained = child.objectName.substring(5)
                            }
                        }
                    }
                }
                Ez.Note {
                    objectName: "labelOtherPatches"
                    readonly property list<string> others: { root.iniRevision; return root.settings.otherPatches }
                    visible: others.length > 0
                    text: qsTr("Also in EasyRPG.ini: %1").arg(others.join(", "))
                    color: Ez.Metrics.mutedTextColor
                }
                // what the patch pointed at changes, as high as the longest explanation
                Rectangle {
                    color: "transparent"
                    border.color: Ez.Metrics.sectionBorderColor
                    radius: Ez.Metrics.sectionRadius / 2
                    Layout.fillWidth: true
                    implicitHeight: explanations.implicitHeight + 2 * Ez.Metrics.sectionPadding

                    StackLayout {
                        id: explanations
                        objectName: "labelPatchHelp"
                        anchors.fill: parent
                        anchors.margins: Ez.Metrics.sectionPadding
                        currentIndex: Math.max(0, root.settings.patches().findIndex(patch => patch.key === root.explained) + 1)

                        Ez.Label {
                            text: qsTr("Point at a patch to see what it changes.")
                            font: Ez.Metrics.font
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                        }
                        Repeater {
                            model: root.settings.patches()
                            delegate: Ez.Label {
                                required property var modelData
                                text: modelData.explanation
                                textFormat: Text.RichText
                                font: Ez.Metrics.font
                                wrapMode: Text.Wrap
                                Layout.fillWidth: true
                            }
                        }
                    }
                }
            }
            // the fonts the Player draws with, in the Font folder of the game
            Ez.Section {
                title: qsTr("Fonts")
                Ez.Form {
                    Repeater {
                        model: 2
                        delegate: RowLayout {
                            id: fontRow
                            required property int index
                            spacing: Ez.Metrics.columnSpacing
                            Layout.columnSpan: 2
                            Layout.fillWidth: true

                            Ez.FormLabel {
                                text: qsTr("Font %1").arg(fontRow.index + 1)
                            }
                            Ez.Label {
                                objectName: "labelPlayerFont" + (fontRow.index + 1)
                                text: { root.fontRevision; return root.settings.playerFontText(fontRow.index) }
                                font: Ez.Metrics.font
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                                HoverHandler {
                                    id: fontHover
                                }
                                Controls.ToolTip.visible: fontHover.hovered
                                Controls.ToolTip.text: qsTr("Font/%1 of the game (.ttf, .otf, .fon...), else a font of the Player")
                                    .arg(fontRow.index === 0 ? "Font" : "Font2")
                            }
                            Ez.Button {
                                objectName: "pushPlayerFont" + (fontRow.index + 1)
                                text: qsTr("Choose...")
                                Controls.ToolTip.visible: hovered
                                Controls.ToolTip.text: qsTr("Copied in the Font folder of the game on OK")
                                onClicked: {
                                    fontDialog.font = fontRow.index
                                    fontDialog.open()
                                }
                            }
                            Ez.Button {
                                objectName: "pushPlayerFontBuiltin" + (fontRow.index + 1)
                                text: qsTr("Built-in")
                                Controls.ToolTip.visible: hovered
                                Controls.ToolTip.text: qsTr("The Player's own font; the game's font file goes to the trash on OK")
                                onClicked: root.settings.useBuiltinPlayerFont(fontRow.index)
                            }
                        }
                    }
                }
            }
            Item {
                Layout.fillHeight: true
            }
        }
    }

    Dialogs.FileDialog {
        id: fontDialog
        property int font: 0
        title: qsTr("Font %1 for EasyRPG Player").arg(font + 1)
        nameFilters: [qsTr("Fonts (%1)").arg(root.settings.fontPatterns().join(" "))]
        onAccepted: root.settings.choosePlayerFont(font, Ez.Editor.localPath(selectedFile))
    }
    Ez.MessageDialog {
        id: invalid
        title: qsTr("Project Settings")
    }
}
