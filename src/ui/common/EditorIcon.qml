// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import org.kde.kirigami as Kirigami

/**
 * An icon of the Editor (resources/icons: tree, variable, layer-event...)
 * in the variant drawn for the background it is on
 */
Image {
    /** Its name, empty for none */
    property string name

    readonly property bool lightBackground: Kirigami.Theme.backgroundColor.hslLightness >= 0.5

    source: name === "" ? "" : "qrc:/" + (lightBackground ? "bright" : "dark") + "/" + name
    sourceSize: Qt.size(width, height)
    fillMode: Image.PreserveAspectFit
    width: 16
    height: 16
}
