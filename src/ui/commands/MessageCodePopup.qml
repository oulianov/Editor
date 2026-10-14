// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "MessageCodePopup"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * The codes of a message (\N[n], \C[n], \. ...) as buttons that type them
 * into its text (a MessageTextArea), each explained by its tooltip. A hero
 * name or a variable is chosen in the searchable list of them (heroList,
 * variableList), a symbol in the game's ExFont ($A...).
 *
 *   MessageCodePopup { id: codes; session: root.session; message: text }
 *   Ez.FieldButton { onClicked: codes.showCodes(this) }
 */
Item {
    id: root

    required property Ez.CommandEditSession session
    /** The MessageTextArea the codes are typed into */
    required property var message

    // the codes of RPG Maker 2000 and 2003 messages: text, look, timing
    readonly property var sections: [
        [
            { code: "\\N[1]", name: QT_TR_NOOP("Hero name"), tip: QT_TR_NOOP("The name of hero n (0: the party leader)"), pick: "actors" },
            { code: "\\V[1]", name: QT_TR_NOOP("Variable"), tip: QT_TR_NOOP("The value of variable n"), pick: "variables" },
            { code: "\\$", name: QT_TR_NOOP("Money"), tip: QT_TR_NOOP("Shows the money window") },
            { code: "\\\\", name: QT_TR_NOOP("Backslash"), tip: QT_TR_NOOP("Shows a backslash") },
            { code: "\\_", name: QT_TR_NOOP("Half space"), tip: QT_TR_NOOP("A space half as wide") },
        ], [
            { code: "\\C[0]", name: QT_TR_NOOP("Color"), tip: QT_TR_NOOP("The text after it in color n of the System graphic (0: normal)") },
            { code: "\\S[1]", name: QT_TR_NOOP("Speed"), tip: QT_TR_NOOP("Text speed: 1 fast to 20 slow") },
            { code: "\\>", name: QT_TR_NOOP("Instant on"), tip: QT_TR_NOOP("Shows the text after it at once, until Instant off or the end of the line") },
            { code: "\\<", name: QT_TR_NOOP("Instant off"), tip: QT_TR_NOOP("Back to letter by letter") },
        ], [
            { code: "\\.", name: QT_TR_NOOP("Short pause"), tip: QT_TR_NOOP("Small pause (a quarter of a second)") },
            { code: "\\|", name: QT_TR_NOOP("Long pause"), tip: QT_TR_NOOP("Pause of one second") },
            { code: "\\!", name: QT_TR_NOOP("Wait for key"), tip: QT_TR_NOOP("Waits for a key before the rest") },
            { code: "\\^", name: QT_TR_NOOP("Close by itself"), tip: QT_TR_NOOP("Closes the message without waiting for a key") },
        ]
    ]
    /** Where the pop-ups open: under this item (the Add Code button, under the text) */
    property Item anchor: null
    // counts the openings: the place of a pop-up is measured again at each one
    property int openings: 0

    /** A text shown as rich text */
    function escaped(text) {
        return text.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
    }

    /** Shows the codes under anchor, or closes them (or the symbols) when shown: a second click on the button */
    function showCodes(anchor) {
        if (d.shownAtPress && root.anchor === anchor) {
            d.shownAtPress = false
            codes.close()
            symbols.close()
            return
        }
        root.anchor = anchor
        symbols.close()
        openings++
        codes.open()
    }

    /** The list of the heroes or variables, under the button: the one chosen is typed as "\N[id]" */
    function pick(list) {
        const box = list === "actors" ? heroes : variables
        box.currentIndex = -1
        box.openSearch("")
    }

    // The pop-ups open under the anchor, in a window of their own past the
    // edges of the dialog: the text of the message stays in sight. Above it
    // only when the screen has no room under it. (In the anchor's coordinates.)
    function popupX(popup) {
        if (!anchor || openings < 0) {
            return 0
        }
        const area = Ez.Editor.screenArea(anchor)
        const at = anchor.mapToGlobal(0, 0)
        return Math.max(area.x - at.x, Math.min(0, area.x + area.width - at.x - popup.width))
    }
    function popupY(popup) {
        if (!anchor || openings < 0) {
            return 0
        }
        const area = Ez.Editor.screenArea(anchor)
        const below = anchor.mapToGlobal(0, anchor.height).y
        return below + popup.height <= area.y + area.height || below - anchor.height - popup.height < area.y
            ? anchor.height : -popup.height
    }

    QtObject {
        id: d
        /** A pop-up was shown when the anchor was pressed: its click closes it */
        property bool shownAtPress: false
        /** A pop-up is being closed, by the event being handled */
        property bool closing: false

        function closingNow() {
            closing = true
            Qt.callLater(() => { closing = false })
        }
    }
    // A pop-up in a window of its own is closed by any press outside it: the
    // press on the button comes as it closes, and is remembered as made on
    // the pop-up shown (the click closes it, not shows it again)
    Connections {
        target: root.anchor
        ignoreUnknownSignals: true
        function onPressed() {
            d.shownAtPress = codes.visible || symbols.visible || d.closing
        }
    }

    component CodeButton: Controls.ToolButton {
        id: codeButton
        property string tip
        flat: true
        font: Ez.Metrics.font
        hoverEnabled: true
        Layout.fillWidth: true
        Controls.ToolTip.visible: hovered && tip !== ""
        Controls.ToolTip.text: tip
        Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
    }

    // the lists the hero or variable is searched in, as RPG Maker's: only their search shows
    component HiddenList: Ez.RefComboBox {
        id: list
        /** N or V */
        property string code
        database: root.session.database
        // drawn nowhere, its search opens under the button
        opacity: 0
        x: root.anchor ? root.mapFromItem(root.anchor, 0, 0).x : 0
        // no room taken: the clicks go to what is under it
        y: root.anchor ? root.mapFromItem(root.anchor, 0, root.anchor.height).y : 0
        width: root.anchor ? Math.max(root.anchor.width, 240) : 240
        height: 0
        focusPolicy: Qt.NoFocus
        onActivated: (index) => {
            if (index >= 0) {
                root.message.typeCode("\\%1[%2]".arg(code).arg(currentValue), false)
            }
        }
        // closed: the text again
        onActiveFocusChanged: {
            if (activeFocus && !popup.visible) {
                root.message.textArea.forceActiveFocus()
            }
        }
    }
    HiddenList {
        id: heroes
        objectName: "heroList"
        listKey: "actors"
        code: "N"
    }
    HiddenList {
        id: variables
        objectName: "variableList"
        listKey: "variables"
        code: "V"
    }

    Controls.Popup {
        id: codes
        objectName: "messageCodes"
        property bool shown: false
        onAboutToShow: shown = true
        onAboutToHide: d.closingNow()
        popupType: Controls.Popup.Window
        parent: root.anchor ?? root
        x: root.popupX(this)
        y: root.popupY(this)
        // a press on the button leaves the pop-up to its click (showCodes closes it)
        closePolicy: Controls.Popup.CloseOnEscape | Controls.Popup.CloseOnPressOutsideParent
        padding: 8
        focus: true
        font: Ez.Metrics.font

        contentItem: ColumnLayout {
            spacing: 2
            Repeater {
                model: root.sections
                GridLayout {
                    id: block
                    required property var modelData
                    required property int index
                    // one block of buttons per kind of code, without a title
                    columns: 4
                    rowSpacing: 2
                    columnSpacing: 2
                    Layout.fillWidth: true
                    Repeater {
                        model: block.modelData
                        CodeButton {
                            required property var modelData
                            objectName: "code " + modelData.code
                            text: modelData.code + "\n" + qsTr(modelData.name)
                            tip: "<b>%1</b> %2".arg(root.escaped(modelData.code)).arg(root.escaped(qsTr(modelData.tip)))
                            onClicked: {
                                codes.close()
                                if (modelData.pick) {
                                    root.pick(modelData.pick)
                                } else {
                                    root.message.typeCode(modelData.code, true)
                                }
                            }
                        }
                    }
                    // the symbols of the game, chosen in a second pop-up
                    CodeButton {
                        objectName: "codeSymbol"
                        visible: block.index === 0
                        text: qsTr("Symbol…")
                        tip: qsTr("A symbol of the game's font, $A to $z (the list of them)")
                        display: Controls.AbstractButton.TextUnderIcon
                        // the glyph as large as in the game: the button no taller than the others
                        contentItem: Column {
                            spacing: 0
                            Loader {
                                anchors.horizontalCenter: parent.horizontalCenter
                                // the font of the game is read once shown
                                active: codes.shown
                                sourceComponent: Ez.PreviewImage {
                                    ratio: 1
                                    image: root.session.symbolImage(0, codes.palette.buttonText, 1)
                                }
                            }
                            Ez.Label {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: qsTr("Symbol…")
                                font: Ez.Metrics.font
                            }
                        }
                        onClicked: {
                            codes.close()
                            root.openings++
                            symbols.open()
                        }
                    }
                }
            }
        }
    }

    Controls.Popup {
        id: symbols
        objectName: "messageSymbols"
        property bool shown: false
        onAboutToShow: shown = true
        onAboutToHide: d.closingNow()
        popupType: Controls.Popup.Window
        parent: root.anchor ?? root
        x: root.popupX(this)
        y: root.popupY(this)
        // a press on the button leaves the pop-up to its click (showCodes closes it)
        closePolicy: Controls.Popup.CloseOnEscape | Controls.Popup.CloseOnPressOutsideParent
        padding: 8
        focus: true
        font: Ez.Metrics.font

        contentItem: ColumnLayout {
            spacing: 4
            Ez.Label {
                text: qsTr("Symbols (%1)").arg(symbols.shown ? root.session.symbolsSource() : "")
                font.bold: true
            }
            GridLayout {
                columns: 13
                rowSpacing: 1
                columnSpacing: 1
                Repeater {
                    // made once shown: the font of the game is read then
                    model: symbols.shown ? 52 : 0
                    CodeButton {
                        id: symbolButton
                        required property int index
                        readonly property string code: root.session.symbolCode(index)
                        objectName: "symbol " + code
                        tip: code
                        Accessible.name: code
                        Layout.fillWidth: false
                        padding: 2
                        contentItem: Ez.PreviewImage {
                            ratio: 1
                            image: root.session.symbolImage(symbolButton.index, symbols.palette.buttonText, 2)
                        }
                        onClicked: {
                            symbols.close()
                            root.message.typeCode(symbolButton.code, true)
                        }
                    }
                }
            }
        }
    }
}
