// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "SettingsDialog"

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import QtQuick.Dialogs as Dialogs
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * The options of the Editor (Ez.Editor), written on OK. The controls are
 * given the options once, not bound to them: writing one option tells all
 * the bindings to Ez.Editor to read them again, which would undo the
 * choices not written yet.
 */
Ez.DialogWindow {
    id: root

    objectName: "SettingsDialog"
    title: qsTr("Settings")
    // the table of the RTP folders: a path beside its marks
    preferredWidth: 470
    // not closed while the Player or the RTP is downloaded for it
    acceptable: !download.busy && !rtpDownload.busy && soundfontRefusal === ""

    /** Why the MIDI soundfont typed or chosen is refused, empty when it is none or an SF2 */
    readonly property string soundfontRefusal: Ez.Soundfonts.refusal(soundfontPath.text.trim())

    /**
     * The RTP folders as edited, written on OK: {path, exists, engine,
     * translation, easyrpg} in their order, the folder marked for each engine, and the
     * game open ({folder, name, engine}, empty without one) with the folder
     * it chose (empty: its engine's default)
     */
    property var rtpRows: []
    property string rtpDefault2000: ""
    property string rtpDefault2003: ""
    property var rtpProject: ({})
    property string rtpActive: ""
    readonly property bool rtpHasProject: (rtpProject.folder ?? "") !== ""
    readonly property var rtpCurrent: rtpTable.currentIndex >= 0 && rtpTable.currentIndex < rtpRows.length
        ? rtpRows[rtpTable.currentIndex] : null

    /** A line of the table (an empty one while the table catches up with a line taken out) */
    function rtpAt(index) {
        return rtpRows[index] ?? { path: "", exists: true, engine: 0, translation: "", easyrpg: false }
    }
    function rtpRow(path) {
        const info = Ez.Editor.rtpFolderInfo(path)
        return { path: path, exists: info.exists, engine: info.engine, translation: info.translation, easyrpg: info.easyrpg }
    }
    /** A folder for the games of an engine: its RTP, or the EasyRPG RTP (both) */
    function rtpServes(row, engine) {
        return row.easyrpg || row.engine === engine
    }
    /** The folder the game open uses: its own, else its engine's default,
     *  else the first one there is (EditorSettings::rtpSearchPath) */
    function rtpUsed() {
        if (!rtpHasProject) {
            return ""
        }
        const has = path => path !== "" && rtpRows.some(row => row.path === path)
        if (has(rtpActive)) {
            return rtpActive
        }
        const fallback = rtpProject.engine === 2003 ? rtpDefault2003 : rtpDefault2000
        return has(fallback) ? fallback : (rtpRows.find(row => row.exists) ?? { path: "" }).path
    }
    /** Adds a folder at the end (what the folder dialog gives, and the
     *  scripts, the EasyRPG RTP downloaded); the first one of an engine is its default */
    function addRtpFolder(path) {
        let folder = String(path).trim()
        while (folder.length > 1 && (folder.endsWith("/") || folder.endsWith("\\"))) {
            folder = folder.slice(0, -1)
        }
        if (folder === "") {
            return
        }
        const known = rtpRows.findIndex(row => row.path === folder)
        if (known < 0) {
            const row = rtpRow(folder)
            if (rtpDefault2000 === "" && (row.engine !== 2003 || row.easyrpg)) {
                rtpDefault2000 = folder
            }
            if (rtpDefault2003 === "" && (row.engine !== 2000 || row.easyrpg)) {
                rtpDefault2003 = folder
            }
            rtpRows = rtpRows.concat([row])
        }
        rtpTable.currentIndex = known >= 0 ? known : rtpRows.length - 1
        rtpTable.revision++
    }
    /** The mark of a folder taken out goes to the next folder of the same engine */
    function removeRtpFolder(index) {
        if (index < 0 || index >= rtpRows.length) {
            return
        }
        const path = rtpRows[index].path
        rtpRows = rtpRows.filter((row, i) => i !== index)
        const next = engine => (rtpRows.find(row => rtpServes(row, engine)) ?? { path: "" }).path
        if (rtpDefault2000 === path) {
            rtpDefault2000 = next(2000)
        }
        if (rtpDefault2003 === path) {
            rtpDefault2003 = next(2003)
        }
        if (rtpActive === path) {
            rtpActive = ""
        }
        rtpTable.currentIndex = Math.min(index, rtpRows.length - 1)
        rtpTable.revision++
    }
    function moveRtpFolder(index, delta) {
        const to = index + delta
        if (index < 0 || to < 0 || to >= rtpRows.length) {
            return
        }
        const rows = rtpRows.slice()
        rows.splice(to, 0, rows.splice(index, 1)[0])
        rtpRows = rows
        rtpTable.currentIndex = to
        rtpTable.revision++
    }
    function rtpTranslationText(code) {
        switch (code) {
        case "ja": return qsTr("official Japanese")
        case "en": return qsTr("official English")
        case "dm": return qsTr("translated by %1").arg("Don Miguel")
        case "rpga": return qsTr("translated by %1").arg("RPG Advocate")
        case "ru": return qsTr("Russian, translated by %1").arg("Vlad")
        case "rpgu": return qsTr("translated by %1").arg("RPG Universe")
        case "ko": return qsTr("Korean")
        case "zh": return qsTr("official Traditional Chinese")
        }
        return ""
    }
    /** What a folder holds: the tool tip of its line */
    function rtpDescription(row) {
        if (!row || row.path === "") {
            return ""
        }
        if (!row.exists) {
            return qsTr("%1 is missing: it is skipped.").arg(row.path)
        }
        if (row.engine === 0) {
            return qsTr("%1 is not a whole RTP: its files fill in for those the other folders lack.").arg(row.path)
        }
        if (row.easyrpg) {
            return qsTr("%1: EasyRPG RTP, for the games of RPG Maker 2000 and 2003.").arg(row.path)
        }
        const translation = rtpTranslationText(row.translation)
        return translation === "" ? qsTr("%1: RTP of RPG Maker %2.").arg(row.path).arg(row.engine)
                                  : qsTr("%1: RTP of RPG Maker %2, %3.").arg(row.path).arg(row.engine).arg(translation)
    }

    Component.onCompleted: {
        language.currentIndex = Math.max(0, language.indexOfValue(Ez.Editor.language))
        colorScheme.currentIndex = Math.max(0, colorScheme.indexOfValue(Ez.Editor.colorScheme))
        reloadExternal.checked = Ez.Editor.reloadExternalChanges
        maniacMode.checked = Ez.Editor.maniacMode
        searchCache.checked = Ez.Editor.searchCache
        rtpDefault2000 = Ez.Editor.rtpDefault(2000)
        rtpDefault2003 = Ez.Editor.rtpDefault(2003)
        rtpRows = Ez.Editor.rtpFolders.map(path => rtpRow(path))
        rtpProject = Ez.Editor.rtpProject()
        rtpActive = rtpProject.active ?? ""
        rtpTable.currentIndex = rtpRows.length > 0 ? 0 : -1
        playerPath.text = Ez.Editor.playerPath
        scriptEditor.text = Ez.Editor.scriptEditor
        soundfontPath.text = Ez.Soundfonts.editorDefaultSetting()
        animateTiles.checked = Ez.Editor.animateTiles
        commandColors.checked = Ez.Editor.commandColors
        commandStripes.checked = Ez.Editor.commandStripes
        checkUpdates.checked = Ez.Editor.checkForUpdates
    }

    onAccepted: {
        // all read before the first is written; the language last, the
        // windows show it at once
        const rtpFolders = rtpRows.map(row => row.path)
        const rtp2000 = rtpDefault2000
        const rtp2003 = rtpDefault2003
        const rtpGame = rtpProject.folder ?? ""
        const rtpOwn = rtpActive
        const reload = reloadExternal.checked
        const maniac = maniacMode.checked
        const cache = searchCache.checked
        const player = playerPath.text.trim()
        const editor = scriptEditor.text.trim()
        const soundfont = soundfontPath.text.trim()
        const animate = animateTiles.checked
        const colors = commandColors.checked
        const stripes = commandStripes.checked
        const updates = checkUpdates.checked
        const scheme = colorScheme.currentValue
        const code = language.currentValue
        Ez.Editor.reloadExternalChanges = reload
        Ez.Editor.maniacMode = maniac
        Ez.Editor.searchCache = cache
        // the folders first: the games' choices of folders taken out go with them
        Ez.Editor.rtpFolders = rtpFolders
        Ez.Editor.setRtpDefault(2000, rtp2000)
        Ez.Editor.setRtpDefault(2003, rtp2003)
        if (rtpGame !== "") {
            Ez.Editor.setRtpActive(rtpGame, rtpOwn)
        }
        Ez.Editor.playerPath = player
        Ez.Editor.scriptEditor = editor
        if (soundfont !== Ez.Soundfonts.editorDefaultSetting()) {
            Ez.Soundfonts.setEditorDefault(soundfont)
        }
        Ez.Editor.animateTiles = animate
        Ez.Editor.commandColors = colors
        Ez.Editor.commandStripes = stripes
        Ez.Editor.checkForUpdates = updates
        Ez.Editor.colorScheme = scheme
        Ez.Editor.language = code
    }

    Ez.Section {
        title: qsTr("General")
        Ez.Form {
            Ez.FormLabel {
                text: qsTr("Language")
            }
            Ez.ComboBox {
                id: language
                objectName: "comboLanguage"
                textRole: "name"
                valueRole: "code"
                model: [{ code: "", name: qsTr("System Language") }].concat(Ez.Editor.languages)
            }
            // every window light or dark at once on OK, title bars and
            // menus too where the system can
            Ez.FormLabel {
                text: qsTr("Color scheme")
            }
            Ez.ComboBox {
                id: colorScheme
                objectName: "comboColorScheme"
                model: [
                    { value: "system", text: qsTr("System") },
                    { value: "light", text: qsTr("Light") },
                    { value: "dark", text: qsTr("Dark") }
                ]
            }
        }
        // none without a place to look for them (Ez.Updater)
        Ez.CheckBox {
            id: checkUpdates
            objectName: "checkCheckUpdates"
            visible: Ez.Updater.enabled
            text: qsTr("Check for updates automatically")
            Controls.ToolTip.visible: hovered
            Controls.ToolTip.text: qsTr("A few seconds after start-up, then every hour while the Editor runs.")
        }
    }

    Ez.Section {
        title: qsTr("Project")
        Ez.CheckBox {
            id: reloadExternal
            objectName: "checkReloadExternal"
            text: qsTr("rpgsync mode: reload the files other programs change")
            Controls.ToolTip.visible: hovered
            Controls.ToolTip.text: qsTr("For rpgsync and other programs that edit the game while the Editor is open: "
                + "what they change is shown, and merged with your unsaved changes when you save.")
        }
        // switched on with a game open: its compatibility is checked once the window closes
        Ez.CheckBox {
            id: maniacMode
            objectName: "checkManiacMode"
            text: qsTr("Maniac mode: edit every game as a Maniac Patch game")
            Controls.ToolTip.visible: hovered
            Controls.ToolTip.text: qsTr("The event commands are edited as Maniac Patch commands: its commands, options "
                + "and value ranges are offered in every game. Switched on, and when a game opens, the game is checked "
                + "for what the Maniac Patch does not run (EasyRPG Player commands, RPG Maker 2000, Power Mode 2003, "
                + "the Ineluki Key Patch), which can be converted.")
        }
        Ez.CheckBox {
            id: searchCache
            objectName: "checkSearchCache"
            text: qsTr("Keep search results between searches")
        }
    }

    // the RTP folders of every game, in the Editor's settings: none in the
    // game's folder, no EasyRPG.ini key
    Ez.Section {
        title: qsTr("RTP folders")
        Ez.ArrayTable {
            id: rtpTable
            objectName: "tableRtp"
            rows: root.rtpRows.length
            visibleRows: 4
            Layout.fillWidth: true
            toolTip: row => root.rtpDescription(root.rtpAt(row))
            columns: [
                { title: qsTr("Folder"), elide: Text.ElideMiddle,
                  toolTip: qsTr("A game looks for the files it lacks in its RTP folder, then in the other folders from top to bottom."),
                  text: row => root.rtpAt(row).path, dimmed: row => !root.rtpAt(row).exists },
                { title: qsTr("RTP"), width: rtpTable.kindWidth,
                  text: row => !root.rtpAt(row).exists ? qsTr("Missing") : root.rtpAt(row).easyrpg ? "EasyRPG"
                               : root.rtpAt(row).engine > 0 ? String(root.rtpAt(row).engine) : qsTr("Other"),
                  dimmed: row => !root.rtpAt(row).exists },
                // radio-like: the folder of each engine's games
                { title: "2000", width: rtpTable.markWidth, name: "radioRtp2000_", exclusive: true,
                  alignment: Text.AlignHCenter, toolTip: qsTr("The folder of the games of RPG Maker 2000"),
                  checked: row => root.rtpAt(row).path === root.rtpDefault2000,
                  toggle: (row, on) => {
                      if (on) {
                          root.rtpDefault2000 = root.rtpAt(row).path
                      }
                      rtpTable.revision++
                  } },
                { title: "2003", width: rtpTable.markWidth, name: "radioRtp2003_", exclusive: true,
                  alignment: Text.AlignHCenter, toolTip: qsTr("The folder of the games of RPG Maker 2003"),
                  checked: row => root.rtpAt(row).path === root.rtpDefault2003,
                  toggle: (row, on) => {
                      if (on) {
                          root.rtpDefault2003 = root.rtpAt(row).path
                      }
                      rtpTable.revision++
                  } }
            ].concat(!root.rtpHasProject ? [] : [
                // the folder of the game open: its engine's default, or one of
                // its own (the default chosen again: none of its own)
                { title: qsTr("This Game"), width: rtpTable.gameWidth, name: "radioRtpGame_", exclusive: true,
                  alignment: Text.AlignHCenter,
                  toolTip: qsTr("The folder %1 uses: the one marked for its engine, or one of its own.").arg(root.rtpProject.name),
                  checked: row => root.rtpAt(row).path !== "" && root.rtpAt(row).path === root.rtpUsed(),
                  toggle: (row, on) => {
                      if (on) {
                          const path = root.rtpAt(row).path
                          root.rtpActive = path === (root.rtpProject.engine === 2003 ? root.rtpDefault2003
                                                                                    : root.rtpDefault2000) ? "" : path
                      }
                      rtpTable.revision++
                  } }
            ])
            onRemoveRequested: row => root.removeRtpFolder(row)

            // the columns as wide as their titles and texts, the folder the rest
            readonly property int kindWidth: Math.ceil(Math.max(...[qsTr("RTP"), qsTr("Missing"), qsTr("Other"), "EasyRPG"]
                .map(text => rtpMetrics.advanceWidth(text)))) + 2 * Ez.Metrics.listTextPadding + 6
            readonly property int markWidth: Math.max(Math.ceil(rtpMetrics.advanceWidth("2000")) + 12, 34)
            readonly property int gameWidth: Math.ceil(rtpMetrics.advanceWidth(qsTr("This Game"))) + 12
            FontMetrics {
                id: rtpMetrics
                font: Ez.Metrics.font
            }
        }
        RowLayout {
            spacing: Ez.Metrics.columnSpacing
            Layout.fillWidth: true
            // a folder of this computer, or EasyRPG's free RTP (for both
            // engines) downloaded into the Editor's data folder
            Ez.FieldButton {
                objectName: "buttonRtpAdd"
                text: qsTr("Add")
                menu: Controls.Menu {
                    Controls.MenuItem {
                        objectName: "addRtpFolder"
                        text: qsTr("From Folder...")
                        onTriggered: rtpDialog.open()
                    }
                    Controls.MenuItem {
                        objectName: "downloadRtp"
                        text: qsTr("Download EasyRPG RTP...")
                        enabled: !rtpDownload.busy
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.text: qsTr("EasyRPG's free RTP, for the games of RPG Maker 2000 and 2003: downloaded into %1 and added to the list.")
                            .arg(rtpDownload.folder)
                        onTriggered: {
                            if (rtpDownload.downloaded() !== "") {
                                rtpThere.open()
                            } else {
                                rtpDownload.start()
                            }
                        }
                    }
                }
            }
            Ez.Button {
                objectName: "buttonRtpRemove"
                text: qsTr("Remove")
                enabled: root.rtpCurrent !== null
                onClicked: root.removeRtpFolder(rtpTable.currentIndex)
            }
            // arrows: as narrow as they are high
            Ez.Button {
                objectName: "buttonRtpUp"
                text: "▲"
                implicitWidth: implicitHeight + 6
                enabled: rtpTable.currentIndex > 0 && root.rtpCurrent !== null
                Controls.ToolTip.visible: hovered
                Controls.ToolTip.text: qsTr("Move Up")
                onClicked: root.moveRtpFolder(rtpTable.currentIndex, -1)
            }
            Ez.Button {
                objectName: "buttonRtpDown"
                text: "▼"
                implicitWidth: implicitHeight + 6
                enabled: root.rtpCurrent !== null && rtpTable.currentIndex < root.rtpRows.length - 1
                Controls.ToolTip.visible: hovered
                Controls.ToolTip.text: qsTr("Move Down")
                onClicked: root.moveRtpFolder(rtpTable.currentIndex, 1)
            }
            Item {
                Layout.fillWidth: true
            }
            // back to the engine's default, for the game open
            Ez.Button {
                objectName: "buttonRtpEngineDefault"
                visible: root.rtpHasProject
                text: qsTr("Engine's Default")
                enabled: root.rtpActive !== ""
                Controls.ToolTip.visible: hovered
                Controls.ToolTip.text: qsTr("%1 uses the folder marked for RPG Maker %2 again.")
                    .arg(root.rtpProject.name ?? "").arg(root.rtpProject.engine ?? 2000)
                onClicked: {
                    root.rtpActive = ""
                    rtpTable.revision++
                }
            }
        }
        Ez.DownloadBar {
            download: rtpDownload
        }
    }

    Ez.Section {
        title: qsTr("Paths")
        Ez.Form {
            Ez.FormLabel {
                text: qsTr("EasyRPG Player")
            }
            // a Player of this computer, or the one of easyrpg.org
            Ez.PathField {
                id: playerPath
                fieldName: "linePlayerPath"
                placeholderText: qsTr("Searched next to the Editor and in the PATH")
                menu: Controls.Menu {
                    Controls.MenuItem {
                        objectName: "choosePlayer"
                        text: qsTr("Choose...")
                        onTriggered: playerDialog.open()
                    }
                    Controls.MenuItem {
                        objectName: "downloadPlayer"
                        text: qsTranslate("ExportDialog", "Download EasyRPG Player %1").arg(download.version)
                        enabled: !download.busy
                        onTriggered: download.start()
                    }
                }
            }
            // where rpgsync scripts open: a URL or a command, with {file} and {line}
            Ez.FormLabel {
                text: qsTr("Script editor")
            }
            Ez.PathField {
                id: scriptEditor
                fieldName: "lineScriptEditor"
                placeholderText: qsTr("The program of the system, at the start of the file")
                toolTip: qsTr("For games edited with rpgsync: Open in Script Editor opens the script at the line "
                    + "of the command. A URL or a command, with {file} and {line}.")
                buttonName: "scriptEditorPresets"
                menu: Controls.Menu {
                    Controls.MenuItem {
                        text: "Visual Studio Code"
                        onTriggered: scriptEditor.text = Ez.Editor.defaultScriptEditor
                    }
                    Controls.MenuItem {
                        text: "Cursor"
                        onTriggered: scriptEditor.text = "cursor://file/{file}:{line}"
                    }
                    Controls.MenuItem {
                        text: qsTr("The program of the system (no line)")
                        onTriggered: scriptEditor.text = ""
                    }
                }
            }
            // the soundfont of MIDI music when the game has none, as a
            // player's own; test play gives it to the Player
            Ez.FormLabel {
                text: qsTr("MIDI soundfont")
            }
            Ez.PathField {
                id: soundfontPath
                fieldName: "lineSoundfont"
                placeholderText: qsTr("None")
                toolTip: qsTr("An SF2 file: the Editor plays MIDI music with it when the game has no soundfont, "
                    + "and test play gives it to the Player. None: a soundfont of the system, else its synthesizer.")
                menu: Controls.Menu {
                    Controls.MenuItem {
                        objectName: "chooseSoundfont"
                        text: qsTr("Choose...")
                        onTriggered: soundfontDialog.open()
                    }
                    Controls.MenuItem {
                        objectName: "noSoundfont"
                        text: qsTr("None")
                        enabled: soundfontPath.text !== ""
                        onTriggered: soundfontPath.text = ""
                    }
                }
            }
        }
        // refused: OK waits for another file, or none
        Ez.Note {
            objectName: "labelSoundfontRefused"
            visible: root.soundfontRefusal !== ""
            text: root.soundfontRefusal
            color: Kirigami.Theme.negativeTextColor
        }
        Ez.DownloadBar {
            download: download
        }
    }

    Ez.Section {
        title: qsTr("Appearance")
        Ez.CheckBox {
            id: animateTiles
            objectName: "checkAnimateTiles"
            text: qsTr("Animate water and animated tiles in the map")
        }
        Ez.CheckBox {
            id: commandColors
            objectName: "checkCommandColors"
            text: qsTr("Color the event commands by kind, as RPG Maker 2003")
        }
        Ez.CheckBox {
            id: commandStripes
            objectName: "checkCommandStripes"
            text: qsTr("Stripe the lines of the event commands")
        }
    }

    ColumnLayout {
        spacing: Ez.Metrics.rowSpacing
        Layout.fillWidth: true

        Ez.Button {
            objectName: "buttonResetLocalState"
            text: qsTr("Reset local state")
            onClicked: {
                Ez.Editor.resetLocalState()
                resetNote.text = qsTr("Local state reset to defaults.")
            }
        }
        Ez.Note {
            id: resetNote
            text: qsTr("Reset window sizes, tabs, selections and scroll positions to their defaults. Game data and preferences are kept.")
        }
    }

    Item {
        Layout.fillHeight: true
    }

    Ez.RtpDownload {
        id: rtpDownload
        window: root
        // written on OK, as the other options
        onFinished: folder => {
            if (folder !== "") {
                root.addRtpFolder(folder)
            }
        }
    }
    // downloaded before: again, or the folder as it is
    Ez.MessageDialog {
        id: rtpThere
        objectName: "rtpThereDialog"
        title: qsTr("Download EasyRPG RTP")
        text: qsTr("The EasyRPG RTP is already in %1. Download it again, or add this folder as it is?").arg(rtpDownload.folder)
        standardButtons: Controls.Dialog.Yes | Controls.Dialog.No | Controls.Dialog.Cancel
        Component.onCompleted: {
            standardButton(Controls.Dialog.Yes).text = qsTr("Download Again")
            standardButton(Controls.Dialog.No).text = qsTr("Add Folder")
        }
        onAccepted: rtpDownload.start()
        // No (Add Folder) and Cancel both reject: which button was it
        Connections {
            target: rtpThere.footer
            function onClicked(button) {
                if (button === rtpThere.standardButton(Controls.Dialog.No)) {
                    root.addRtpFolder(rtpDownload.folder)
                }
            }
        }
    }

    Ez.PlayerDownload {
        id: download
        window: root
        // written on OK, as the other options
        onFinished: program => {
            if (program !== "") {
                playerPath.text = program
            }
        }
    }

    Dialogs.FolderDialog {
        id: rtpDialog
        title: qsTr("Select the RTP folder")
        currentFolder: Ez.Editor.url(root.rtpCurrent ? root.rtpCurrent.path : "")
        onAccepted: root.addRtpFolder(Ez.Editor.localPath(selectedFolder))
    }
    Dialogs.FileDialog {
        id: playerDialog
        title: qsTranslate("RunGameDialog", "Locate the EasyRPG Player")
        currentFolder: Ez.Editor.playerFolder()
        onAccepted: {
            const program = Ez.Editor.executableOf(selectedFile)
            if (program === "") {
                notProgram.text = qsTranslate("RunGameDialog", "%1 is not a program.").arg(Ez.Editor.localPath(selectedFile))
                notProgram.open()
                return
            }
            playerPath.text = program
        }
    }
    Ez.MessageDialog {
        id: notProgram
        title: qsTranslate("RunGameDialog", "Test Play")
    }
    Dialogs.FileDialog {
        id: soundfontDialog
        title: qsTr("MIDI Soundfont")
        nameFilters: [qsTranslate("ProjectSettingsDialog", "Soundfonts (*.sf2 *.sf3 *.soundfont)"),
            qsTranslate("ProjectSettingsDialog", "All files (*)")]
        currentFolder: Ez.Editor.url(soundfontPath.text.trim() !== ""
            ? soundfontPath.text.trim().substring(0, soundfontPath.text.trim().lastIndexOf("/")) : "")
        onAccepted: soundfontPath.text = Ez.Editor.localPath(selectedFile)
    }
}
