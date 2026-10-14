// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Singleton

import QtQuick
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * The design system of the Editor's windows: the density of RPG Maker 2003's
 * forms (a small font, compact rows) on top of the Breeze style. The Ez
 * controls and windows take their sizes from here: a layout problem is
 * fixed here, not in each window.
 */
QtObject {
    /** The font of the forms: the small size of the platform */
    readonly property font font: Ez.Editor.font
    /** Fixed-width font of the texts the game shows in windows of its own size */
    readonly property font monospaceFont: Ez.Editor.monospaceFont

    /**
     * Above and below the text of a field, a combo box or a button: they are
     * as high as the spin boxes (the frame Breeze draws)
     */
    readonly property int fieldPadding: Kirigami.Units.mediumSpacing

    /** The width a text field asks for, whatever its text (a window is not widened by a long one) */
    readonly property int fieldWidth: 140
    /**
     * The narrowest a combo box asks to be; wider, as its text needs
     * (Breeze asks for 200 whatever its text: every form was wider)
     */
    readonly property int comboWidth: 60

    /**
     * The buttons at the bottom of a window (OK, Cancel, Help...: Ez.ButtonBox,
     * Ez.DialogButton): half as large again as the other buttons (23 high,
     * 70 wide at least), the same text
     */
    readonly property int dialogButtonHeight: 28
    readonly property int dialogButtonMinimumWidth: 86
    /** Beside the text of a longer one */
    readonly property int dialogButtonPadding: 18
    readonly property int dialogButtonSpacing: 8
    /** At least between the buttons at the left of a window and those at the right */
    readonly property int dialogButtonGroupSpacing: 24

    /** Around the content of a dialog window */
    readonly property int dialogMargin: 10
    /** Between the sections of a window, and between a section and the buttons */
    readonly property int sectionSpacing: 6
    /** Between the border of a section and its controls */
    readonly property int sectionPadding: 6
    /** Between the title of a section and its border */
    readonly property int sectionTitleGap: 2
    readonly property real sectionRadius: 6
    /** Between the rows of a form */
    readonly property int rowSpacing: 4
    /** Between the controls of a row, and between a label and its field */
    readonly property int columnSpacing: 6
    /** Above and below a check box or radio button: options stacked at rowSpacing have 8 px between their texts */
    readonly property int optionPadding: 2

    /** The titles of sections: a bit smaller than the controls */
    readonly property font sectionTitleFont: Qt.font({
        family: font.family,
        pointSize: font.pointSize * 0.9
    })
    /** The muted color of the titles of sections: the text's, partly transparent (light and dark) */
    readonly property color mutedTextColor: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g,
                                                     Kirigami.Theme.textColor.b, 0.62)
    /** The light border of sections */
    readonly property color sectionBorderColor: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g,
                                                         Kirigami.Theme.textColor.b, 0.14)

    /** Above and below the text of a line of a list (the database entries...): as dense as RPG Maker's */
    readonly property int listRowPadding: 1
    /** The line a list of database entries is as wide as (its number and a name of 12 letters) */
    readonly property string entryListText: "0002: High Potion…"
    /** Before the text of a line of a list or table */
    readonly property int listTextPadding: 3
    /** Around the page of a database entry */
    readonly property int pagePadding: 6
    /**
     * The icon of a toolbar button showing only its icon (the main window's,
     * the palette's): a quarter larger than RPG Maker's 16 pixels
     */
    readonly property int toolIconSize: 20
    /** Around the icon of a toolbar button: 28 pixels square (22 at 16 pixels) */
    readonly property int toolButtonPadding: 4
    /** The widest a tool tip gets: a longer text wraps */
    readonly property int toolTipWidth: 320

    /** The width of the column of a database page (wider windows leave the rest empty, as RPG Maker) */
    readonly property int pageColumnWidth: 545

    /**
     * A disabled control is drawn whole at this opacity (its text, frame, box
     * and arrows), in light and dark. Breeze takes its colors from Kirigami's
     * basic theme (no KDE color schemes here), which only darkens the colors
     * of a disabled item by a fifth: black text stayed black, white text
     * nearly white, and the face of a button darker, as if pressed. The Ez
     * controls fade instead, on the colors of an enabled control.
     */
    readonly property real disabledOpacity: 0.4
    /** The titles of sections, muted already, fade less: 0.35 of the text's color */
    readonly property real disabledTitleOpacity: 0.35 / 0.62
    /** The colors of the platform (or of `color-scheme dark`): the face of an enabled button, field... */
    readonly property SystemPalette palette: SystemPalette {
        colorGroup: SystemPalette.Active
    }
}
