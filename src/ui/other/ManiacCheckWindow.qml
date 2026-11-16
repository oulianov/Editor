// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "ManiacCheckDialog"

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * The Maniac Patch check of a game (ManiacCompat): what it uses that the
 * Maniac Patch's RPG_RT does not run, by group (RPG Maker 2000, EasyRPG
 * Player, Power Mode 2003, the Ineluki Key Patch), how many times, what the
 * conversion makes of it, and where. Yes converts what has an equivalent,
 * No leaves the game as it is; OK alone when nothing converts. The
 * conversion is the caller's (ManiacCheckWindow::ask).
 *
 * turnedOff: what the game uses from the add-ons Project Settings turns off
 * instead (ManiacCompat::uses), by add-on, and where. Keep It Off (OK)
 * writes the settings, what is listed staying in the game as it is, not
 * run; Go Back (Cancel) returns to Project Settings.
 */
Ez.DialogWindow {
    id: root

    /** {section, what, count, becomes, places} by group; turned off: {origin, what, count, places} */
    property var findings: []
    /** The add-ons turned off whose uses are listed */
    property bool turnedOff: false
    /** Their names, in the order of the findings */
    readonly property var addOnNames: {
        const names = []
        for (const finding of findings) {
            const name = Ez.Origins.name(finding.origin ?? "")
            if (turnedOff && name !== "" && !names.includes(name)) {
                names.push(name)
            }
        }
        return names
    }
    /** The places converted, and those that stay */
    property int convertible: 0
    property int remaining: 0
    /** An RPG Maker 2000 game, converted to RPG Maker 2003 first */
    property bool rpg2000: false
    /** The RPG_RT.exe of RPG Maker 2000 is in the game folder */
    property bool exe2000: false
    /** The folder of the files of RPG Maker 2000 */
    property string backup2000Path
    /** Power Mode 2003 found: turned off */
    property bool powerMode: false
    /** The folder the files go to before they change */
    property string backupPath
    /** The finding whose places are listed */
    property int current: findings.length > 0 ? 0 : -1

    objectName: turnedOff ? "TurnedOffDialog" : "ManiacCheckDialog"
    title: turnedOff ? qsTr("Add-ons Turned Off") : qsTr("Maniac Patch Compatibility")
    preferredWidth: 760
    preferredHeight: 560
    stateKey: "maniac_check"
    standardButtons: turnedOff ? Controls.DialogButtonBox.Ok | Controls.DialogButtonBox.Cancel
                     : convertible > 0 ? Controls.DialogButtonBox.Yes | Controls.DialogButtonBox.No
                     : Controls.DialogButtonBox.Ok

    Component.onCompleted: {
        currentChanged()
        if (turnedOff) {
            buttonBox.standardButton(Controls.DialogButtonBox.Ok).text = addOnNames.length > 1 ? qsTr("Keep Them Off")
                                                                                                : qsTr("Keep It Off")
            buttonBox.standardButton(Controls.DialogButtonBox.Cancel).text = qsTr("Go Back")
        }
        // turned off: the uses of each add-on under its name
        const totals = {}
        for (const finding of findings) {
            totals[finding.origin] = (totals[finding.origin] ?? 0) + finding.count
        }
        for (let i = 0; i < findings.length; ++i) {
            const finding = findings[i]
            lines.append({
                section: turnedOff ? "%1 (%2)".arg(Ez.Origins.name(finding.origin)).arg(totals[finding.origin]) : finding.section,
                what: finding.what,
                count: finding.count,
                becomes: finding.becomes !== "" ? finding.becomes : qsTr("Stays (no equivalent)"),
                stays: finding.becomes === ""
            })
        }
    }

    ListModel {
        id: lines
    }

    Ez.Note {
        objectName: "labelQuestion"
        font.bold: true
        text: root.turnedOff
              ? (root.remaining === 1
                 ? qsTr("This game uses %1 in one place. Turned off, it stays in the game as it is but is not "
                        + "run.").arg(root.addOnNames.join(", "))
                 : qsTr("This game uses %1 in %2 places. Turned off, they stay in the game as they are but are not "
                        + "run.").arg(root.addOnNames.join(", ")).arg(root.remaining))
              : root.remaining + root.convertible === 1
              ? qsTr("This game uses one thing the Maniac Patch does not run.") + " "
                + (root.convertible === 1 ? qsTr("It can be converted. Convert it?") : qsTr("It has no equivalent in the Maniac Patch."))
              : qsTr("This game uses %1 things the Maniac Patch does not run.").arg(root.remaining + root.convertible) + " "
                + (root.convertible === 0 ? qsTr("None of them has an equivalent in the Maniac Patch.")
                   : root.remaining === 0 ? qsTr("All of them can be converted. Convert them?")
                   : qsTr("%1 of them can be converted, %2 stay as they are. Convert them?")
                       .arg(root.convertible).arg(root.remaining))
    }

    Ez.ColumnList {
        listName: "listFindings"
        model: lines
        sectionRole: "section"
        currentRow: root.current
        visibleLines: 10
        columns: [
            { title: qsTr("Found"), role: "what" },
            { title: qsTr("Times"), role: "count", sample: "00000", alignment: Text.AlignRight },
            { title: qsTr("Converted to"), role: "becomes", dimmed: line => line.stays }
        ].slice(0, root.turnedOff ? 2 : 3)
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 160
        onPicked: row => root.current = row
    }

    // the line chosen in full, and its places
    Ez.Note {
        objectName: "labelFinding"
        readonly property var finding: root.current >= 0 ? root.findings[root.current] : null
        text: !finding ? "" : root.turnedOff ? finding.what
              : finding.becomes !== "" ? qsTr("%1\nConverted to: %2").arg(finding.what).arg(finding.becomes)
                                                     : qsTr("%1\nStays as it is: the Maniac Patch has no equivalent.").arg(finding.what)
    }
    Ez.ColumnList {
        id: places
        listName: "listPlaces"
        model: ListModel {
            id: placeLines
        }
        columns: [{ title: qsTr("Where"), role: "place" }]
        visibleLines: 5
        Layout.fillWidth: true
        onPicked: row => places.currentRow = row
    }
    onCurrentChanged: {
        placeLines.clear()
        places.currentRow = -1
        for (const place of current >= 0 ? findings[current].places : []) {
            placeLines.append({ place: place })
        }
    }

    Ez.Section {
        title: qsTr("Conversion")
        visible: !root.turnedOff && root.convertible > 0
        Ez.Note {
            objectName: "labelConversion"
            text: root.convertible > (root.rpg2000 ? 1 : 0)
                  ? qsTr("Yes closes the game, copies the database, the maps that change and EasyRPG.ini to a new "
                         + "folder of the game, converts them, turns the Maniac Patch on in EasyRPG.ini and opens the "
                         + "game again:\n%1").arg(root.backupPath)
                  : qsTr("Yes closes the game, converts it, turns the Maniac Patch on in EasyRPG.ini and opens it again.")
        }
        Ez.Note {
            objectName: "labelConversion2000"
            visible: root.rpg2000
            text: qsTr("Before, the game becomes an RPG Maker 2003 game as Convert to RPG Maker 2003 does (its battles "
                       + "become those of RPG Maker 2003); its database, map tree and maps of RPG Maker 2000 go to:\n%1")
                    .arg(root.backup2000Path)
        }
        Ez.Note {
            objectName: "labelExe2000"
            visible: root.rpg2000 && root.exe2000
            text: qsTr("RPG_RT.exe, the runtime of RPG Maker 2000, cannot run the game anymore (the EasyRPG Player "
                       + "would play it as an RPG Maker 2000 game): it is moved to that folder too. Copy the RPG_RT.exe "
                       + "of the Maniac Patch into the game folder to play the game with it; the EasyRPG Player plays "
                       + "it without.")
        }
        Ez.Note {
            objectName: "labelPowerMode"
            visible: root.powerMode
            text: qsTr("Power Mode 2003 is turned off in EasyRPG.ini: its registers V[0001] to V[0008] become plain "
                       + "variables.")
        }
        Ez.Note {
            text: qsTr("No leaves the game as it is. Game Project > Check Maniac Compatibility shows this list again.")
            color: Ez.Metrics.mutedTextColor
        }
    }
}
