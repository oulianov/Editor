// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * The text of a message, in a monospaced font and not wrapped, with a line
 * after the 38th character (the width of a message with a face, dashed) and
 * one after the 50th (without a face). Longer lines can still be typed.
 * Ctrl+Backspace (Cmd or Control on macOS) deletes the line before the
 * cursor, as in VS Code. jsonData and key: as Ez.TextArea.
 *
 * The text is named fieldName (automation).
 */
Controls.ScrollView {
    id: root

    property alias jsonData: text.jsonData
    property alias key: text.key
    property alias fieldName: text.objectName
    readonly property alias textArea: text

    /** Where the text of so many characters ends */
    function columnX(columns) {
        return text.leftPadding + columns * sample.contentWidth / 100
    }
    /** Types text at the cursor; the number between [ and ] selected to be typed over when asked */
    function typeCode(code, selectNumber) {
        text.forceActiveFocus()
        const start = text.selectionStart
        text.remove(text.selectionStart, text.selectionEnd)
        text.insert(start, code)
        const open = code.indexOf("[")
        const close = code.indexOf("]")
        if (selectNumber && open >= 0 && close > open) {
            text.select(start + open + 1, start + close)
        } else {
            text.cursorPosition = start + code.length
        }
    }

    // a little wider than the longest message, 8 lines high
    implicitWidth: columnX(60) + text.rightPadding + effectiveScrollBarWidth
    implicitHeight: 8 * metrics.lineSpacing + text.topPadding + text.bottomPadding + effectiveScrollBarHeight

    FontMetrics {
        id: metrics
        font: Ez.Metrics.monospaceFont
    }

    Ez.TextArea {
        id: text
        wrapMode: TextEdit.NoWrap
        font: Ez.Metrics.monospaceFont

        // the width of a character as the text draws it (not the design metrics)
        Text {
            id: sample
            visible: false
            font: text.font
            renderType: text.renderType
            text: "0".repeat(100)
        }

        Keys.onPressed: (event) => {
            const modifiers = event.modifiers & ~Qt.KeypadModifier
            if (event.key !== Qt.Key_Backspace || readOnly
                    || (modifiers !== Qt.ControlModifier && modifiers !== Qt.MetaModifier)) {
                return
            }
            // the selection, else the line up to the cursor, else (at its start) the line break before it
            let start = selectionStart
            let end = selectionEnd
            if (start === end) {
                const lineStart = text.text.lastIndexOf("\n", end - 1) + 1
                start = lineStart === end ? Math.max(0, end - 1) : lineStart
            }
            remove(start, end)
            cursorPosition = start
            event.accepted = true
        }

        Repeater {
            model: [38, 50]
            // dashed for a message with a face, solid without
            Canvas {
                id: limit
                required property int modelData
                x: Math.round(root.columnX(modelData))
                y: 0
                width: 1
                height: Math.max(text.height, root.height)
                readonly property color lineColor: text.placeholderTextColor
                onLineColorChanged: requestPaint()
                onHeightChanged: requestPaint()
                onPaint: {
                    const context = getContext("2d")
                    context.reset()
                    context.fillStyle = lineColor
                    for (let y = 0; y < height; y += modelData === 38 ? 6 : height) {
                        context.fillRect(0, y, 1, modelData === 38 ? 3 : height)
                    }
                }
            }
        }
    }
}
