// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "BattleAnimationWidget"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * A battle animation, as RPG Maker's page: the graphic, the scope and the
 * Y axis line; the frames with their cels (a pattern of the graphic placed,
 * zoomed, faded and tinted around the target), shown over a monster of the
 * database; the effect timings (a sound effect, a flash and a shake on a
 * frame). The cels, frames and timings have Cut/Copy/Paste menus; the
 * frame tools are Mass Copy/Clear, Autocomplete and Mass Modify. What is
 * done to the animation is the canvas's (Ez.AnimationCanvas).
 */
RichPage {
    id: root

    readonly property alias canvas: canvas

    /** What the window remembers of it: the monster shown */
    function viewState() {
        return { target: targetCombo.currentIndex }
    }
    function restoreViewState(state) {
        if (state && state.target !== undefined) {
            targetCombo.currentIndex = state.target
        }
    }

    onEntryChanged: canvas.stop()

    function editTiming(index) {
        Ez.Editor.exec("AnimationTimingDialog", { canvas: canvas, index: index, rpg2k3: root.rpg2k3 }, timings)
    }
    /** RPG Maker opens the cel properties: here the fields beside */
    function editCel() {
        cellPattern.forceActiveFocus()
        const field = cellPattern.contentItem as TextInput
        if (field) {
            field.selectAll()
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: Ez.Metrics.sectionSpacing

        // the general settings
        GridLayout {
            columns: 6
            rowSpacing: Ez.Metrics.rowSpacing
            columnSpacing: Ez.Metrics.columnSpacing
            Layout.fillWidth: true

            Ez.FormLabel {
                text: qsTr("Name")
            }
            Ez.TextField {
                objectName: "lineName"
                jsonData: root.entry
                key: "name"
                limit: 20
                Layout.preferredWidth: 100
                Layout.fillWidth: true
            }
            Ez.FormLabel {
                text: qsTr("Animation File")
            }
            RowLayout {
                spacing: Ez.Metrics.columnSpacing
                Layout.columnSpan: 3
                Layout.fillWidth: true
                Ez.JsonField {
                    id: large
                    source: root.entry
                    path: "large"
                }
                // large animations (128 pixel cels) are in Battle2
                Ez.FileField {
                    jsonData: root.entry
                    key: "animation_name"
                    folder: large.value ? "Battle2" : "Battle"
                    fieldName: "lineAnimationFile"
                    buttonName: "pushAnimationFile"
                    Layout.preferredWidth: 120
                }
                // RPG Maker 2003 picks it in the Large Animations tab of its file chooser
                Ez.CheckBox {
                    objectName: "checkLarge"
                    text: qsTr("Large")
                    jsonData: root.entry
                    key: "large"
                    enabled: root.rpg2k3
                    Controls.ToolTip.visible: hovered
                    Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                    Controls.ToolTip.text: qsTr("A large animation: cels of 128x128 pixels, from the Battle2 folder (RPG Maker 2003)")
                }
            }
            Ez.FormLabel {
                text: qsTr("Scope")
            }
            Ez.ComboBox {
                objectName: "comboScope"
                jsonData: root.entry
                key: "scope"
                model: [{ text: qsTr("Local"), value: 0 }, { text: qsTr("Global"), value: 1 }]
                implicitWidth: 100
                Layout.fillWidth: true
                Controls.ToolTip.visible: hovered
                Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                Controls.ToolTip.text: qsTr("Local: on each target. Global: once, on the whole screen.")
            }
            Ez.FormLabel {
                text: qsTr("Y Axis Line")
            }
            Ez.ComboBox {
                objectName: "comboPosition"
                jsonData: root.entry
                key: "position"
                model: [{ text: qsTr("High"), value: 0 }, { text: qsTr("Center"), value: 1 }, { text: qsTr("Low"), value: 2 }]
                implicitWidth: 100
                Layout.fillWidth: true
            }
            Ez.FormLabel {
                text: qsTr("Target")
            }
            Ez.ComboBox {
                id: targetCombo
                objectName: "comboTarget"
                textRole: "name"
                valueRole: "index"
                model: root.database ? root.database.list("enemies") : null
                implicitWidth: 120
                Layout.fillWidth: true
                Component.onCompleted: currentIndex = canvas.firstTarget()
                Controls.ToolTip.visible: hovered
                Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                Controls.ToolTip.text: qsTr("The monster the preview shows the animation on")
            }
        }

        RowLayout {
            spacing: Ez.Metrics.sectionSpacing
            Layout.fillWidth: true
            Layout.fillHeight: true

            // the frames
            Ez.Section {
                title: qsTr("Frames")
                Layout.fillHeight: true
                Layout.fillWidth: false

                Ez.ArrayTable {
                    id: frames
                    objectName: "listFrames"
                    rows: canvas.frameCount
                    showHeader: false
                    visibleRows: 6
                    columns: [{ text: row => String(row + 1).padStart(3, "0") }]
                    implicitWidth: 60
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    onCurrentIndexChanged: canvas.currentFrame = currentIndex
                    Binding {
                        target: frames
                        property: "currentIndex"
                        value: canvas.currentFrame
                        restoreMode: Binding.RestoreNone
                    }
                    // RPG Maker's order: Clear empties the frame, Delete removes it
                    Keys.onPressed: (event) => {
                        if (event.matches(StandardKey.Copy)) {
                            canvas.copyFrame()
                        } else if (event.matches(StandardKey.Paste)) {
                            canvas.pasteFrame()
                        } else if (event.matches(StandardKey.Cut)) {
                            canvas.cutFrame()
                        } else if (event.key === Qt.Key_Insert && canvas.frameCount < 999) {
                            canvas.insertFrame()
                        } else if (event.key === Qt.Key_Delete && (event.modifiers & Qt.ShiftModifier)) {
                            canvas.deleteFrame()
                        } else if (event.key === Qt.Key_Delete) {
                            canvas.clearFrame()
                        } else {
                            return
                        }
                        event.accepted = true
                    }
                    TapHandler {
                        acceptedButtons: Qt.RightButton
                        onTapped: (eventPoint) => frameMenu.popupAt(parent, eventPoint.position.x, eventPoint.position.y)
                    }
                    Ez.ContextMenu {
                        id: frameMenu
                        font: Ez.Metrics.font
                        Controls.MenuItem {
                            objectName: "actionFrameCopy"
                            text: qsTranslate("EditActions", "Copy")
                            onTriggered: canvas.copyFrame()
                        }
                        Controls.MenuItem {
                            objectName: "actionFramePaste"
                            text: qsTranslate("EditActions", "Paste")
                            enabled: frameMenu.visible && canvas.canPasteFrame()
                            onTriggered: canvas.pasteFrame()
                        }
                        Controls.MenuItem {
                            objectName: "actionFrameClear"
                            text: qsTranslate("EditActions", "Clear")
                            onTriggered: canvas.clearFrame()
                        }
                        Controls.MenuItem {
                            objectName: "actionFrameCut"
                            text: qsTranslate("EditActions", "Cut")
                            onTriggered: canvas.cutFrame()
                        }
                        Controls.MenuSeparator {}
                        // a blank frame, no window: no "..."
                        Controls.MenuItem {
                            objectName: "actionFrameInsert"
                            text: qsTr("Insert")
                            enabled: canvas.frameCount < 999
                            onTriggered: canvas.insertFrame()
                        }
                        Controls.MenuItem {
                            objectName: "actionFrameDelete"
                            text: qsTranslate("EditActions", "Delete")
                            enabled: canvas.frameCount > 1
                            onTriggered: canvas.deleteFrame()
                        }
                    }
                }
                Ez.Button {
                    objectName: "pushFrameCount"
                    text: qsTr("Number of Frames...")
                    Layout.fillWidth: true
                    onClicked: {
                        frameCount.value = canvas.frameCount
                        frameCount.open()
                    }
                }
                Ez.Button {
                    objectName: "pushClonePrevious"
                    text: qsTr("Clone Previous")
                    enabled: canvas.currentFrame > 0
                    Layout.fillWidth: true
                    Controls.ToolTip.visible: hovered
                    Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                    Controls.ToolTip.text: qsTr("Replaces the cels of this frame by the ones of the previous frame")
                    onClicked: canvas.clonePrevious()
                }
                Ez.Button {
                    objectName: "pushMassCopyClear"
                    text: qsTr("Mass Copy/Clear...")
                    Layout.fillWidth: true
                    Controls.ToolTip.visible: hovered
                    Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                    Controls.ToolTip.text: qsTr("Copies the cels of a range of frames to other frames, or removes them")
                    onClicked: Ez.Editor.exec("MassCopyClearDialog", { canvas: canvas }, this)
                }
                Ez.Button {
                    objectName: "pushAutocomplete"
                    text: qsTr("Autocomplete...")
                    Layout.fillWidth: true
                    Controls.ToolTip.visible: hovered
                    Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                    Controls.ToolTip.text: qsTr("Places the cels between two frames, moving from the first frame to the last")
                    onClicked: Ez.Editor.exec("AutocompleteDialog", { canvas: canvas }, this)
                }
                Ez.Button {
                    objectName: "pushMassModify"
                    text: qsTr("Mass Modify...")
                    Layout.fillWidth: true
                    Controls.ToolTip.visible: hovered
                    Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                    Controls.ToolTip.text: qsTr("Gives a pattern, zoom, transparency or color to cels of a range of frames")
                    onClicked: Ez.Editor.exec("MassModifyDialog", { canvas: canvas }, this)
                }
            }

            // the preview, the frame shown, the patterns of the graphic
            ColumnLayout {
                spacing: Ez.Metrics.rowSpacing
                Layout.fillWidth: true
                Layout.fillHeight: true

                Item {
                    id: screenFrame
                    implicitWidth: 240
                    implicitHeight: 180
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    // the whole screen stays visible: shrunk when the page is
                    // small, enlarged by whole steps (pixels stay square)
                    readonly property real fit: Math.min(width / 320, height / 240)
                    Ez.AnimationCanvas {
                        id: canvas
                        objectName: "viewAnimation"
                        entry: root.entry
                        target: targetCombo.currentIndex
                        highlight: Kirigami.Theme.highlightColor
                        patternBackground: Kirigami.Theme.backgroundColor
                        transformOrigin: Item.TopLeft
                        scale: screenFrame.fit >= 1 ? Math.floor(screenFrame.fit) : Math.max(screenFrame.fit, 0.1)
                        x: (screenFrame.width - 320 * scale) / 2
                        y: (screenFrame.height - 240 * scale) / 2
                        onContextMenuRequested: (x, y) => celMenu.popupAt(canvas, x, y)
                        onCellDoubleClicked: root.editCel()
                    }
                    Ez.ContextMenu {
                        id: celMenu
                        font: Ez.Metrics.font
                        Controls.MenuItem {
                            objectName: "actionCelProperties"
                            text: qsTr("Cel Properties")
                            enabled: canvas.cellEditable
                            onTriggered: root.editCel()
                        }
                        Controls.MenuSeparator {}
                        Controls.MenuItem {
                            objectName: "actionCelCut"
                            text: qsTranslate("EditActions", "Cut")
                            enabled: canvas.cellEditable
                            onTriggered: canvas.cutCell()
                        }
                        Controls.MenuItem {
                            objectName: "actionCelCopy"
                            text: qsTranslate("EditActions", "Copy")
                            enabled: canvas.cellEditable
                            onTriggered: canvas.copyCell()
                        }
                        Controls.MenuItem {
                            objectName: "actionCelPaste"
                            text: qsTranslate("EditActions", "Paste")
                            enabled: celMenu.visible && canvas.canPasteCell()
                            onTriggered: canvas.pasteCell()
                        }
                        Controls.MenuItem {
                            objectName: "actionCelDelete"
                            text: qsTranslate("EditActions", "Delete")
                            enabled: canvas.cellEditable
                            onTriggered: canvas.deleteCell()
                        }
                        Controls.MenuSeparator {}
                        Controls.MenuItem {
                            objectName: "actionCelUp"
                            text: qsTr("Increment Image ID")
                            enabled: canvas.cellEditable
                            onTriggered: canvas.changePattern(1)
                        }
                        Controls.MenuItem {
                            objectName: "actionCelDown"
                            text: qsTr("Decrement Image ID")
                            enabled: canvas.cellEditable
                            onTriggered: canvas.changePattern(-1)
                        }
                    }
                }

                RowLayout {
                    spacing: Ez.Metrics.columnSpacing
                    Layout.fillWidth: true
                    Ez.Button {
                        objectName: "pushBack"
                        text: qsTr("&Back")
                        enabled: canvas.currentFrame > 0
                        onClicked: canvas.previousFrame()
                    }
                    Ez.Label {
                        objectName: "labelFrame"
                        text: canvas.frameCount > 0 ? qsTr("Frame %1 / %2").arg(canvas.currentFrame + 1).arg(canvas.frameCount) : ""
                        horizontalAlignment: Text.AlignHCenter
                        font: Ez.Metrics.font
                        Layout.fillWidth: true
                    }
                    Ez.Button {
                        objectName: "pushNext"
                        text: qsTr("&Next")
                        enabled: canvas.currentFrame + 1 < canvas.frameCount
                        onClicked: canvas.nextFrame()
                    }
                    Ez.Button {
                        objectName: "pushPlay"
                        text: canvas.playing ? qsTr("Stop") : qsTr("Play")
                        onClicked: canvas.togglePlay()
                    }
                }

                // the patterns of the graphic: a click selects one, given to the selected cel
                Rectangle {
                    Kirigami.Theme.colorSet: Kirigami.Theme.View
                    Kirigami.Theme.inherit: false
                    color: Kirigami.Theme.backgroundColor
                    border.color: Ez.Metrics.sectionBorderColor
                    implicitHeight: canvas.thumbSize + patternBar.implicitHeight + 2
                    Layout.fillWidth: true

                    Flickable {
                        id: patternFlick
                        anchors.fill: parent
                        anchors.margins: 1
                        clip: true
                        contentWidth: patterns.width
                        contentHeight: canvas.thumbSize
                        boundsBehavior: Flickable.StopAtBounds
                        Controls.ScrollBar.horizontal: Controls.ScrollBar {
                            id: patternBar
                            policy: Controls.ScrollBar.AlwaysOn
                        }
                        Ez.ImageCanvas {
                            id: patterns
                            objectName: "listPatterns"
                            image: canvas.patterns
                            width: implicitWidth
                            height: canvas.thumbSize
                            TapHandler {
                                onTapped: (point) => canvas.pattern = Math.floor(point.position.x / canvas.thumbSize)
                            }
                        }
                    }
                    // the pattern selected stays in view
                    Connections {
                        target: canvas
                        function onPatternChanged() {
                            const left = canvas.pattern * canvas.thumbSize
                            if (left < patternFlick.contentX) {
                                patternFlick.contentX = left
                            } else if (left + canvas.thumbSize > patternFlick.contentX + patternFlick.width) {
                                patternFlick.contentX = Math.max(0, left + canvas.thumbSize - patternFlick.width)
                            }
                        }
                    }
                }
                Ez.Note {
                    color: Ez.Metrics.mutedTextColor
                    text: qsTr("Click the preview to place a cel of the selected pattern, drag a cel to move it (with Shift: all the cels), right-click it for its menu. The arrow keys move it.")
                    Layout.fillWidth: true
                }
            }

            ColumnLayout {
                spacing: Ez.Metrics.sectionSpacing
                Layout.fillHeight: true
                Layout.fillWidth: false

                // the cels of the frame shown
                Ez.Section {
                    title: qsTr("Cels")

                    /** A field of the current cel (AnimationEditor::CellField) */
                    component CelField: Ez.SpinBox {
                        id: celField
                        property int field
                        /** What is shown more than the data (patterns are numbered from 1) */
                        property int offset: 0
                        enabled: canvas.cellEditable
                        Binding on value {
                            value: canvas.revision >= 0 ? canvas.cellValue(celField.field) + celField.offset : 0
                            restoreMode: Binding.RestoreNone
                        }
                        onValueModified: canvas.setCellValue(field, value - offset)
                    }

                    GridLayout {
                        columns: 4
                        rowSpacing: Ez.Metrics.rowSpacing
                        columnSpacing: Ez.Metrics.columnSpacing

                        Ez.ComboBox {
                            id: cel
                            objectName: "comboCel"
                            textRole: ""
                            valueRole: ""
                            model: canvas.cellTexts
                            implicitWidth: 120
                            Layout.columnSpan: 3
                            Layout.fillWidth: true
                            onActivated: canvas.currentCell = currentIndex
                            Binding {
                                target: cel
                                property: "currentIndex"
                                value: canvas.revision >= 0 ? canvas.currentCell : -1
                                restoreMode: Binding.RestoreNone
                            }
                        }
                        Ez.Button {
                            objectName: "pushDeleteCel"
                            text: qsTr("Delete")
                            enabled: canvas.cellEditable
                            onClicked: canvas.deleteCell()
                        }
                        Ez.FormLabel { text: qsTr("Pattern") }
                        CelField {
                            id: cellPattern
                            objectName: "spinCelPattern"
                            field: 0
                            offset: 1
                            from: 1
                            to: 999
                        }
                        Ez.FormLabel { text: qsTr("Zoom") }
                        CelField {
                            objectName: "spinCelZoom"
                            field: 3
                            from: 0
                            to: 999
                            suffix: "%"
                        }
                        Ez.FormLabel { text: qsTr("X") }
                        CelField {
                            objectName: "spinCelX"
                            field: 1
                            from: -999
                            to: 999
                            stepSize: canvas.moveStep
                        }
                        Ez.FormLabel { text: qsTr("Y") }
                        CelField {
                            objectName: "spinCelY"
                            field: 2
                            from: -999
                            to: 999
                            stepSize: canvas.moveStep
                        }
                        Ez.FormLabel { text: qsTr("Red") }
                        CelField {
                            objectName: "spinCelRed"
                            field: 5
                            from: 0
                            to: 200
                            suffix: "%"
                        }
                        Ez.FormLabel { text: qsTr("Green") }
                        CelField {
                            objectName: "spinCelGreen"
                            field: 6
                            from: 0
                            to: 200
                            suffix: "%"
                        }
                        Ez.FormLabel { text: qsTr("Blue") }
                        CelField {
                            objectName: "spinCelBlue"
                            field: 7
                            from: 0
                            to: 200
                            suffix: "%"
                        }
                        Ez.FormLabel { text: qsTr("Saturation") }
                        CelField {
                            objectName: "spinCelSaturation"
                            field: 8
                            from: 0
                            to: 200
                            suffix: "%"
                        }
                        Ez.FormLabel {
                            text: qsTr("Transparency")
                            Layout.columnSpan: 3
                        }
                        CelField {
                            objectName: "spinCelTransparency"
                            field: 4
                            from: 0
                            to: 100
                            suffix: "%"
                        }
                        // where the cels go (below the preview it made the window wider)
                        Ez.CheckBox {
                            objectName: "checkGrid"
                            text: qsTr("Use Grid")
                            checked: canvas.useGrid
                            onToggled: canvas.useGrid = checked
                            Layout.columnSpan: 4
                            Controls.ToolTip.visible: hovered
                            Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                            Controls.ToolTip.text: qsTr("Cels are placed and moved by steps of %1 pixels").arg(8)
                        }
                    }
                }

                // sound effects, flashes and shakes
                Ez.Section {
                    title: qsTr("Effect Timing")
                    Layout.fillHeight: true

                    Ez.ArrayTable {
                        id: timings
                        objectName: "listTimings"
                        rows: canvas.timingRows.length
                        source: canvas
                        visibleRows: 5
                        columns: [
                            { title: qsTr("Frame"), width: 40, text: row => canvas.timingRows[row] ? canvas.timingRows[row][0] : "" },
                            { title: qsTr("Sound Effect"), text: row => canvas.timingRows[row] ? canvas.timingRows[row][1] : "" },
                            { title: qsTr("Flash"), text: row => canvas.timingRows[row] ? canvas.timingRows[row][2] : "" },
                            { title: qsTr("Shake"), text: row => canvas.timingRows[row] ? canvas.timingRows[row][3] : "" },
                        ]
                        implicitWidth: 250
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        onCurrentIndexChanged: canvas.currentTiming = currentIndex
                        onActivated: (row) => root.editTiming(row < canvas.timingRows.length ? row : -1)
                        onRemoveRequested: canvas.deleteTiming()
                        Binding {
                            target: timings
                            property: "currentIndex"
                            value: canvas.currentTiming
                            restoreMode: Binding.RestoreNone
                        }
                        Connections {
                            target: canvas
                            function onTimingsChanged() { timings.revision++ }
                        }
                        Keys.onPressed: (event) => {
                            if (event.matches(StandardKey.Copy)) {
                                canvas.copyTiming()
                            } else if (event.matches(StandardKey.Cut)) {
                                canvas.cutTiming()
                            } else if (event.matches(StandardKey.Paste)) {
                                canvas.pasteTiming()
                            } else {
                                return
                            }
                            event.accepted = true
                        }
                        TapHandler {
                            acceptedButtons: Qt.RightButton
                            onTapped: (eventPoint) => timingMenu.popupAt(parent, eventPoint.position.x, eventPoint.position.y)
                        }
                        Ez.ContextMenu {
                            id: timingMenu
                            readonly property bool onTiming: canvas.currentTiming >= 0
                            font: Ez.Metrics.font
                            Controls.MenuItem {
                                objectName: "actionTimingEdit"
                                text: qsTranslate("EditActions", "Edit...")
                                onTriggered: root.editTiming(canvas.currentTiming)
                            }
                            Controls.MenuSeparator {}
                            Controls.MenuItem {
                                objectName: "actionTimingCut"
                                text: qsTranslate("EditActions", "Cut")
                                enabled: timingMenu.onTiming
                                onTriggered: canvas.cutTiming()
                            }
                            Controls.MenuItem {
                                objectName: "actionTimingCopy"
                                text: qsTranslate("EditActions", "Copy")
                                enabled: timingMenu.onTiming
                                onTriggered: canvas.copyTiming()
                            }
                            Controls.MenuItem {
                                objectName: "actionTimingPaste"
                                text: qsTranslate("EditActions", "Paste")
                                enabled: timingMenu.visible && canvas.canPasteTiming()
                                onTriggered: canvas.pasteTiming()
                            }
                            Controls.MenuItem {
                                objectName: "actionTimingDelete"
                                text: qsTranslate("EditActions", "Delete")
                                enabled: timingMenu.onTiming
                                onTriggered: canvas.deleteTiming()
                            }
                        }
                    }
                    RowLayout {
                        spacing: Ez.Metrics.columnSpacing
                        Layout.fillWidth: true
                        Ez.Button {
                            objectName: "pushAddTiming"
                            text: qsTr("Add")
                            Layout.fillWidth: true
                            onClicked: root.editTiming(-1)
                        }
                        Ez.Button {
                            objectName: "pushEditTiming"
                            text: qsTr("Edit")
                            enabled: canvas.currentTiming >= 0
                            Layout.fillWidth: true
                            onClicked: root.editTiming(canvas.currentTiming)
                        }
                        Ez.Button {
                            objectName: "pushDeleteTiming"
                            text: qsTr("Delete")
                            enabled: canvas.currentTiming >= 0
                            Layout.fillWidth: true
                            onClicked: canvas.deleteTiming()
                        }
                    }
                }
            }
        }
    }

    Ez.NumberDialog {
        id: frameCount
        title: qsTr("Number of Frames")
        label: qsTr("Frames")
        spinName: "spinFrameCount"
        from: 1
        to: 999
        onChosen: (value) => canvas.resizeFrames(value)
    }
}
