// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * A path (or any text) with a "..." button beside it to choose it
 * (browse()). The button is named after the field: objectName + "Browse".
 */
RowLayout {
    id: root

    property alias text: field.text
    property alias placeholderText: field.placeholderText
    property alias field: field
    property alias button: button
    /** An icon of the Editor (resources/icons: "folder"...) on the button instead of "..." */
    property string buttonIcon
    /** The name of the field, and of its button (by default: + "Browse") */
    property string fieldName
    property string buttonName: fieldName + "Browse"
    property string toolTip
    /** What the button does, shown when pointed at */
    property string buttonToolTip
    /** Choices the button shows instead of browsing (Ez.FieldButton.menu) */
    property alias menu: button.menu
    /** The button was clicked (with a menu: it is shown too) */
    signal browse()
    /** The text was typed (not set) */
    signal textEdited()

    spacing: Ez.Metrics.columnSpacing
    Layout.fillWidth: true

    Ez.TextField {
        id: field
        objectName: root.fieldName
        Layout.fillWidth: true
        Layout.minimumWidth: 160
        Controls.ToolTip.visible: hovered && root.toolTip !== ""
        Controls.ToolTip.text: root.toolTip
        onTextEdited: root.textEdited()
    }
    Ez.FieldButton {
        id: button
        objectName: root.buttonName
        Controls.ToolTip.visible: hovered && root.buttonToolTip !== ""
        Controls.ToolTip.text: root.buttonToolTip
        onClicked: root.browse()
        text: root.buttonIcon === "" ? "..." : ""
        // drawn for the background it is on
        icon.source: root.buttonIcon === "" ? ""
            : "qrc:/" + (Kirigami.Theme.backgroundColor.hslLightness >= 0.5 ? "bright" : "dark") + "/" + root.buttonIcon
    }
}
