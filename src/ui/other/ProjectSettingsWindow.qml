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
 * EasyRPG.ini, its fonts, the game soundfont). The values are a
 * ProjectSettings'; checked on OK, then written by the window that opened
 * this one (it closes and reloads the project when the folder or the code
 * page change). The soundfont chosen or removed is done on OK first, here:
 * a large copy keeps the window open with its progress, Cancel stops it.
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

    /** The sizes of the screen offered: the player's choice (0 x 0) first, Custom (-1) last */
    readonly property var screenPresets: [
        { name: qsTr("Player's choice (320×240 or wider)"), width: 0, height: 0 },
        { name: qsTr("320×240 only"), width: 320, height: 240 },
        { name: qsTr("416×240, widescreen"), width: 416, height: 240 },
        { name: qsTr("560×240, ultrawide"), width: 560, height: 240 },
        { name: "640×480", width: 640, height: 480 },
        { name: qsTr("Custom"), width: -1, height: -1 }
    ]
    function screenPresetOf(size) {
        const found = screenPresets.findIndex(preset => preset.width === size.width && preset.height === size.height)
        return found >= 0 ? found : screenPresets.length - 1
    }
    readonly property Ez.ProjectSoundfont soundfont: settings.soundfont

    /** A soundfont file chosen: checked, confirmed when it replaces one or is large, then copied on OK */
    function chooseSoundfont(file) {
        const refused = soundfont.check(file)
        if (refused !== "") {
            invalid.text = qsTr("%1 cannot be the game soundfont: %2").arg(file.split(/[\\/]/).pop()).arg(refused)
            invalid.open()
            return
        }
        const question = soundfont.chooseConfirmation(file)
        if (question === "") {
            soundfont.choose(file)
            return
        }
        confirmSoundfont.file = file
        confirmSoundfont.remove = false
        confirmSoundfont.text = question
        confirmSoundfont.open()
    }
    function removeSoundfont() {
        const question = soundfont.removeConfirmation()
        if (question === "") {
            soundfont.remove()
            return
        }
        confirmSoundfont.remove = true
        confirmSoundfont.text = question
        confirmSoundfont.open()
    }

    objectName: "ProjectSettingsDialog"
    title: qsTr("Project Settings")
    preferredWidth: 600
    // while the soundfont is copied: Cancel stops it
    acceptable: !soundfont.busy
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
            return
        }
        // the soundfont first: the window closes once it is copied (finished)
        if (root.soundfont.busy) {
            check.ok = false
        } else if (root.soundfont.pending) {
            check.ok = false
            const refused = root.soundfont.apply()
            if (refused !== "") {
                root.settings.convertRequested = false
                invalid.text = refused
                invalid.open()
            }
        }
    }
    // Cancel while the soundfont is copied stops the copy (the old file stays), not the window
    onRejecting: check => {
        if (root.soundfont.busy) {
            check.ok = false
            root.soundfont.cancel()
        }
    }
    Component.onCompleted: {
        // opened to use a soundfont (the Jukebox): where it shows, confirmed as when chosen here
        if (root.soundfont.pending) {
            tabs.currentIndex = 2
            const question = root.soundfont.chooseConfirmation(root.soundfont.path)
            if (question !== "") {
                confirmSoundfont.file = root.soundfont.path
                confirmSoundfont.remove = false
                confirmSoundfont.dropOnNo = true
                confirmSoundfont.text = question
                Qt.callLater(confirmSoundfont.open)
            }
        }
    }
    // EasyRPG.ini edited in the script editor: what it says now
    onActiveChanged: {
        if (active) {
            root.settings.readIni()
        }
    }
    Connections {
        target: root.soundfont
        function onFinished(error, cancelled) {
            if (error !== "") {
                root.settings.convertRequested = false
                invalid.text = error
                invalid.open()
            } else if (!cancelled) {
                root.accept()
            }
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
                text: qsTr("Read by EasyRPG Player (EasyRPG.ini, Font folder; the screen size in RPG_RT.ini, "
                    + "which the Maniac Patch reads too). RPG_RT.exe is not changed.")
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
            // the size of the game's screen (WinW and WinH of RPG_RT.ini), and
            // the maps smaller than it (FakeResolution of EasyRPG.ini)
            Ez.Section {
                objectName: "groupScreen"
                title: qsTr("Screen")

                RowLayout {
                    spacing: Ez.Metrics.columnSpacing
                    Layout.fillWidth: true

                    Ez.FormLabel {
                        text: qsTr("Size")
                    }
                    Ez.ComboBox {
                        id: screenPreset
                        objectName: "comboScreenSize"
                        /** Custom chosen, the size being one of the list's */
                        property bool custom: false
                        readonly property size size: root.settings.screenSize
                        readonly property bool sized: size.width > 0
                        textRole: "name"
                        valueRole: "name"
                        model: root.screenPresets
                        Layout.fillWidth: true
                        Controls.ToolTip.visible: hovered && !popup.visible
                        Controls.ToolTip.text: qsTr("Player's choice: 320×240, or wider when the player chose Widescreen or "
                            + "Ultrawide in EasyRPG Player's settings. A size: the game always has it (WinW and WinH of "
                            + "RPG_RT.ini, EasyRPG Player 0.8 or later and the Maniac Patch); RPG Maker's menus, made "
                            + "for 320×240, are centered in it.")
                        onActivated: index => {
                            const preset = root.screenPresets[index]
                            custom = preset.width < 0
                            if (!custom) {
                                root.settings.screenSize = Qt.size(preset.width, preset.height)
                            } else if (!sized) {
                                root.settings.screenSize = Qt.size(320, 240)
                            }
                        }
                        Binding on currentIndex {
                            value: screenPreset.custom ? root.screenPresets.length - 1 : root.screenPresetOf(screenPreset.size)
                        }
                    }
                    Ez.SpinBox {
                        objectName: "spinScreenWidth"
                        from: 160
                        to: 3840
                        enabled: screenPreset.sized
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.text: qsTr("Width in pixels")
                        onValueModified: root.settings.screenSize = Qt.size(value, screenPreset.size.height)
                        Binding on value {
                            value: screenPreset.sized ? screenPreset.size.width : 320
                        }
                    }
                    Ez.Label {
                        text: "×"
                        font: Ez.Metrics.font
                        enabled: screenPreset.sized
                    }
                    Ez.SpinBox {
                        objectName: "spinScreenHeight"
                        from: 120
                        to: 2160
                        enabled: screenPreset.sized
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.text: qsTr("Height in pixels")
                        onValueModified: root.settings.screenSize = Qt.size(screenPreset.size.width, value)
                        Binding on value {
                            value: screenPreset.sized ? screenPreset.size.height : 240
                        }
                    }
                }
                Ez.CheckBox {
                    objectName: "checkCenterSmallMaps"
                    text: qsTr("Center maps smaller than the screen")
                    enabled: screenPreset.sized
                    Controls.ToolTip.visible: hovered
                    Controls.ToolTip.text: qsTr("EasyRPG Player's Fake Metrics (FakeResolution of EasyRPG.ini): a map smaller "
                        + "than the screen is drawn in its middle with black borders, with its panorama and the pictures "
                        + "that scroll with it, instead of from the top-left corner. For games made for 320×240. The Player "
                        + "does it by itself in its widescreen and ultrawide modes.")
                    onToggled: root.settings.centerSmallMaps = checked
                    Binding on checked {
                        value: root.settings.centerSmallMaps
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
            // easyrpg.soundfont: what the Player plays the MIDI music with, before the player's own
            Ez.Section {
                objectName: "groupSoundfont"
                title: qsTr("Soundfont")

                Ez.Label {
                    objectName: "labelSoundfont"
                    text: root.soundfont.summary
                    font: Ez.Metrics.font
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    HoverHandler {
                        id: soundfontHover
                    }
                    Controls.ToolTip.visible: soundfontHover.hovered && root.soundfont.path !== ""
                    Controls.ToolTip.text: root.soundfont.path
                }
                // refused (red), or what may sound wrong
                Ez.Note {
                    objectName: "labelSoundfontWarning"
                    visible: text !== ""
                    text: root.soundfont.warning
                    color: root.soundfont.invalid ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.neutralTextColor
                }
                Ez.Note {
                    objectName: "labelSoundfontPending"
                    visible: text !== ""
                    text: root.soundfont.pendingText
                    color: Ez.Metrics.mutedTextColor
                }
                RowLayout {
                    spacing: Ez.Metrics.columnSpacing
                    enabled: !root.soundfont.busy
                    Layout.fillWidth: true

                    Ez.Button {
                        objectName: "pushSoundfontChoose"
                        text: qsTr("Choose...")
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.text: qsTr("An SF2 file, copied in the game folder as easyrpg.soundfont on OK")
                        onClicked: soundfontDialog.open()
                    }
                    Ez.Button {
                        objectName: "pushSoundfontSample"
                        readonly property bool playing: samplePlayer.state !== Ez.AudioPlayer.Stopped
                        text: playing ? qsTr("Stop") : qsTr("Play a Sample")
                        icon.name: playing ? "media-playback-stop" : "media-playback-start"
                        enabled: root.soundfont.present && !root.soundfont.invalid && Ez.Soundfonts.soundfontsPlayable()
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.text: Ez.Soundfonts.soundfontsPlayable()
                            ? qsTr("A short song of the game's music (or of the Editor) through this soundfont")
                            : Ez.Soundfonts.unavailable(Ez.Soundfonts.Game)
                        onClicked: {
                            if (playing) {
                                samplePlayer.stop()
                            } else {
                                samplePlayer.play(root.soundfont.sampleMidi())
                            }
                        }
                    }
                    Ez.Button {
                        objectName: "pushSoundfontReveal"
                        text: Qt.platform.os === "osx" ? qsTr("Show in Finder")
                            : Qt.platform.os === "windows" ? qsTr("Show in Explorer") : qsTr("Show in Folder")
                        enabled: root.soundfont.path !== ""
                        onClicked: root.soundfont.reveal()
                    }
                    Ez.Button {
                        objectName: "pushSoundfontRemove"
                        text: qsTr("Remove")
                        enabled: root.soundfont.present
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.text: qsTr("No game soundfont: easyrpg.soundfont goes to the trash on OK")
                        onClicked: root.removeSoundfont()
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                    Ez.Button {
                        objectName: "pushSoundfontHelp"
                        icon.name: "help-contents"
                        display: Controls.AbstractButton.IconOnly
                        text: qsTr("Help")
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.text: qsTr("How players' MIDI music chooses its soundfont")
                        onClicked: Ez.Editor.help("soundfont", this)
                    }
                }
                // what the sample plays with, when it is not this soundfont
                Ez.Note {
                    objectName: "labelSoundfontSample"
                    visible: samplePlayer.state !== Ez.AudioPlayer.Stopped && samplePlayer.synthSource !== Ez.Soundfonts.Automatic
                        && samplePlayer.synthSource !== Ez.Soundfonts.File
                    text: Ez.Soundfonts.describe(Ez.Soundfonts.File, samplePlayer.synthSource, samplePlayer.synthSoundfont)
                    color: Kirigami.Theme.neutralTextColor
                }
                // the copy on OK: Cancel stops it
                RowLayout {
                    visible: root.soundfont.busy
                    spacing: Ez.Metrics.columnSpacing
                    Layout.fillWidth: true

                    Ez.Label {
                        text: qsTr("Copying the soundfont...")
                        font: Ez.Metrics.font
                    }
                    Ez.ProgressBar {
                        objectName: "progressSoundfont"
                        from: 0
                        to: 1
                        value: root.soundfont.progress
                        Layout.fillWidth: true
                    }
                }
            }
            Item {
                Layout.fillHeight: true
            }
        }
    }

    // the sample of the soundfont: the one chosen, before it is copied
    Ez.AudioPlayer {
        id: samplePlayer
        soundfontChoice: Ez.Soundfonts.File
        soundfontFile: root.soundfont.path
    }
    Dialogs.FileDialog {
        id: soundfontDialog
        title: qsTr("Game Soundfont")
        nameFilters: [qsTr("Soundfonts (*.sf2 *.sf3 *.soundfont)"), qsTr("All files (*)")]
        onAccepted: root.chooseSoundfont(Ez.Editor.localPath(selectedFile))
    }
    Ez.MessageDialog {
        id: confirmSoundfont
        objectName: "soundfontQuestion"
        property string file
        property bool remove: false
        /** No drops the choice made before the window opened */
        property bool dropOnNo: false
        parent: Controls.Overlay.overlay
        title: qsTr("Game Soundfont")
        standardButtons: Controls.Dialog.Yes | Controls.Dialog.No
        onAccepted: {
            dropOnNo = false
            if (remove) {
                root.soundfont.remove()
            } else {
                root.soundfont.choose(file)
            }
        }
        onRejected: {
            if (dropOnNo) {
                dropOnNo = false
                root.soundfont.revert()
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
