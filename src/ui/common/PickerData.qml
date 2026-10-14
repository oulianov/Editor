// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick

/**
 * What a resource picker picks: a file (its name, without its extension) and
 * what is chosen with it: the character of a CharSet or the face of a
 * FaceSet (index), a pose (direction, pattern), the hue of a monster, the
 * settings of a music or a sound. A picker (Ez.PickerBase) edits its own;
 * a viewer (Ez.CharSetViewer...) gives one to Ez.Pickers.pickFile and gets
 * the choice back into it.
 *
 * fromMusic, fromSound, toMusic and toSound read and write a Music or a
 * Sound of the project's data (a JSON object: name, fadein, volume...).
 */
QtObject {
    id: root

    property string filename
    /** The character of a CharSet (0 to 7), the face of a FaceSet (0 to 15) */
    property int index: 0
    /** Drawn half transparent (a hero's option) */
    property bool transparent: false
    /** Of a pose: 0 up, 1 right, 2 down, 3 left */
    property int direction: 2
    /** Of a pose: 0 left, 1 middle, 2 right */
    property int pattern: 1
    /** The color alteration of a monster, 0 to 359 degrees */
    property int hue: 0

    /** Of a music, in milliseconds */
    property int fadein: 0
    property int volume: 100
    property int tempo: 100
    property int balance: 50

    function fromMusic(jsonData) {
        fromSound(jsonData)
        fadein = jsonData.num("fadein")
        return root
    }

    function fromSound(jsonData) {
        filename = jsonData.str("name")
        volume = jsonData.num("volume")
        tempo = jsonData.num("tempo")
        balance = jsonData.num("balance")
        return root
    }

    function toMusic(jsonData) {
        toSound(jsonData)
        jsonData.set("fadein", fadein)
        return root
    }

    function toSound(jsonData) {
        jsonData.set("name", filename)
        jsonData.set("volume", volume)
        jsonData.set("tempo", tempo)
        jsonData.set("balance", balance)
        return root
    }

    /** The file and its choice as Ez.Pickers.pickFile asks them (with folder and preview) */
    function toRequest(folder, preview) {
        return {
            folder: folder, preview: preview, name: filename, index: index, direction: direction,
            pattern: pattern, hue: hue, fadeIn: fadein, volume: volume, tempo: tempo, balance: balance
        }
    }

    /** The choice of Ez.Pickers.pickFile (null: cancelled, nothing changes); true when one was given */
    function fromChoice(choice) {
        if (!choice) {
            return false
        }
        filename = choice.name
        index = choice.index
        direction = choice.direction
        pattern = choice.pattern
        hue = choice.hue
        fadein = choice.fadeIn
        volume = choice.volume
        tempo = choice.tempo
        balance = choice.balance
        return true
    }

    /** The choice a picker starts on (its Ez.FilePickerModel: the data's) */
    function fromModel(model) {
        filename = model.currentName
        index = model.spriteIndex
        direction = model.direction
        pattern = model.pattern
        hue = model.hue
        fadein = model.fadeIn
        volume = model.volume
        tempo = model.tempo
        balance = model.balance
        return root
    }
}
