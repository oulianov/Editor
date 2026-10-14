// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

PickerBase {
    id: root

    directory: "Sound"

    Ez.SpinBox {
        id: volumeInput
        from: 0
        to: 100
        value: root.pickerData.volume
        Kirigami.FormData.label: "Volume:"
    }

    Ez.SpinBox {
        id: tempoInput
        from: 50
        to: 150
        value: root.pickerData.tempo
        Kirigami.FormData.label: "Pitch:"
    }

    Ez.SpinBox {
        id: balanceInput
        from: 0
        to: 100
        value: root.pickerData.balance
        Kirigami.FormData.label: "Balance:"
    }

    onAccepted: {
        root.pickerData.volume = volumeInput.value
        root.pickerData.tempo = tempoInput.value
        root.pickerData.balance = balanceInput.value
    }
}
