// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * A text of a window in the font of the forms, greyed when disabled
 * (Ez.Metrics.disabledOpacity: Breeze's label keeps the color of an enabled
 * one). The windows use it for every label (tests/check_labels.cmake);
 * the text of an Ez control is the control's (faded with it).
 */
Controls.Label {
    font: Ez.Metrics.font
    opacity: enabled ? 1 : Ez.Metrics.disabledOpacity
}
