// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/** The picker of a monster's battler: the image with its color alteration (hue), chosen below it */
PickerBase {
    id: root

    previewContent: Ez.MonsterPaintedItem {
        pickerMode: true
        path: root.picker.currentPath
        hue: root.pickerData.hue
        width: implicitWidth * root.zoom
        height: implicitHeight * root.zoom
    }

    Ez.Section {
        title: qsTranslate("EnemyPickerWidget", "Color Alteration")
        RowLayout {
            spacing: Ez.Metrics.columnSpacing
            Ez.Slider {
                objectName: "sliderHue"
                from: 0
                to: 359
                Layout.fillWidth: true
                onMoved: root.pickerData.hue = value
                Binding on value {
                    value: root.pickerData.hue
                }
            }
            Ez.SpinBox {
                objectName: "spinHue"
                from: 0
                to: 359
                onValueModified: root.pickerData.hue = value
                Binding on value {
                    value: root.pickerData.hue
                }
            }
        }
    }
}
