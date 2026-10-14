// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "BattleAnimation2Widget"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * A battler animation (RPG Maker 2003): how a hero looks in battle, its 32
 * poses (a row of a BattleCharSet, or a battle animation) and 32 weapons
 * (a row of a BattleWeapon), each played at the display speed, a weapon in
 * the hands of the hero. A click on the rows of a graphic picks one.
 */
RichPage {
    id: root

    readonly property alias model: model

    readonly property var poses: entry ? entry.list("poses") : null
    readonly property var weapons: entry ? entry.list("weapons") : null
    readonly property string pose: "poses/" + model.currentPose + "/"
    readonly property string weapon: "weapons/" + model.currentWeapon + "/"

    /** What the window remembers of it */
    function viewState() {
        return { tab: tabs.currentIndex }
    }
    function restoreViewState(state) {
        if (state) {
            tabs.currentIndex = state.tab ?? 0
        }
    }

    function number(row) {
        return String(row + 1).padStart(2, "0")
    }
    /** The "Graphic" column of a pose: its animation, or its graphic and row */
    function poseGraphic(row) {
        const prefix = "poses/" + row + "/"
        if (entry.num(prefix + "animation_type") !== 0) {
            const id = entry.num(prefix + "battle_animation_id")
            const animations = database ? database.list("animations") : null
            return qsTr("Animation: %1").arg(animations && id >= 1 && id <= animations.count
                                             ? animations.str((id - 1) + "/name") : String(id))
        }
        const name = entry.str(prefix + "battler_name")
        return name !== "" ? name + " (" + (entry.num(prefix + "battler_index") + 1) + ")" : ""
    }
    function weaponGraphic(row) {
        const prefix = "weapons/" + row + "/"
        const name = entry.str(prefix + "weapon_name")
        return name !== "" ? name + " (" + (entry.num(prefix + "weapon_index") + 1) + ")" : ""
    }

    // battler animations are only used by RPG Maker 2003
    enabled: !!entry && entry.valid && rpg2k3

    Ez.BattlerAnimationPreview {
        id: model
        entry: root.entry
        rpg2k3: root.rpg2k3
        weapons: tabs.currentIndex === 1
        hand: hand.currentIndex
        highlight: Kirigami.Theme.highlightColor
    }

    /** A picture of the rows of a graphic, 4 by 2: a click picks the row of the index */
    component Rows: Ez.ImageCanvas {
        id: rows
        property var index
        image: model.rows
        implicitWidth: 4 * model.frameSize
        implicitHeight: 2 * model.frameSize
        Controls.ToolTip.visible: rowsHover.hovered
        Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
        Controls.ToolTip.text: qsTr("Click a row of the graphic to use it")
        HoverHandler {
            id: rowsHover
            cursorShape: Qt.PointingHandCursor
        }
        TapHandler {
            onTapped: (point) => {
                const pixel = rows.pixelAt(point.position.x, point.position.y)
                const row = model.rowAt(pixel.x, pixel.y)
                if (row >= 0 && rows.index.enabled) {
                    rows.index.setValue(row + 1)
                }
            }
        }
    }
    /** The preview played, at twice its size in a frame */
    component Preview: Rectangle {
        property string text
        Kirigami.Theme.colorSet: Kirigami.Theme.View
        Kirigami.Theme.inherit: false
        color: "transparent"
        border.color: Ez.Metrics.sectionBorderColor
        implicitWidth: 2 * model.frameSize + 4
        implicitHeight: 2 * model.frameSize + 4
        Ez.ImageCanvas {
            anchors.fill: parent
            image: model.preview
            zoom: 2
        }
        Ez.Label {
            anchors.fill: parent
            text: parent.text
            visible: text !== ""
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.WordWrap
            font: Ez.Metrics.font
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: Ez.Metrics.sectionSpacing

        RowLayout {
            spacing: Ez.Metrics.sectionSpacing
            Layout.fillWidth: true

            Ez.FormLabel {
                text: qsTr("Name")
            }
            Ez.TextField {
                objectName: "lineName"
                jsonData: root.entry
                key: "name"
                limit: 16
                Layout.fillWidth: true
            }
            Ez.Section {
                objectName: "groupSpeed"
                title: qsTr("Animation Display Speed")
                Layout.fillWidth: false
                Ez.RadioGroup {
                    jsonData: root.entry
                    key: "speed"
                    columns: 3
                    choices: [
                        { name: "radioSpeedSlow", text: qsTr("Slow"), value: 20 },
                        { name: "radioSpeedMedium", text: qsTr("Medium"), value: 14 },
                        { name: "radioSpeedFast", text: qsTr("Fast"), value: 8 },
                    ]
                }
            }
        }

        Controls.TabBar {
            id: tabs
            objectName: "tabBattlerAnimation"
            font: Ez.Metrics.font
            Layout.fillWidth: true
            Controls.TabButton {
                text: qsTr("Poses")
                width: implicitWidth
                font: Ez.Metrics.font
            }
            Controls.TabButton {
                text: qsTr("Weapons")
                width: implicitWidth
                font: Ez.Metrics.font
            }
        }

        StackLayout {
            currentIndex: tabs.currentIndex
            Layout.fillWidth: true
            Layout.fillHeight: true

            // the poses
            RowLayout {
                spacing: Ez.Metrics.sectionSpacing

                Ez.ArrayTable {
                    objectName: "listPoses"
                    rows: root.poses ? root.poses.count : 0
                    source: root.entry
                    columns: [
                        { title: qsTr("No."), width: 34, text: row => root.number(row) },
                        { title: qsTr("Pose Label"), text: row => root.entry.str("poses/" + row + "/name") },
                        { title: qsTr("Graphic"), text: row => root.poseGraphic(row) },
                    ]
                    id: listPoses
                    onCurrentIndexChanged: model.currentPose = currentIndex
                    Binding {
                        target: listPoses
                        property: "currentIndex"
                        value: model.currentPose
                        restoreMode: Binding.RestoreNone
                    }
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }

                ColumnLayout {
                    id: poseFields
                    readonly property bool valid: model.currentPose >= 0
                    readonly property bool battle: valid && type.value === 1
                    spacing: Ez.Metrics.sectionSpacing
                    Layout.alignment: Qt.AlignTop

                    Ez.Form {
                        Ez.FormLabel {
                            text: qsTr("Pose Label")
                        }
                        Ez.TextField {
                            objectName: "editPoseName"
                            jsonData: root.entry
                            key: root.pose + "name"
                            limit: 18
                            enabled: poseFields.valid
                            // the poses the Player uses keep the names RPG Maker gives them
                            readOnly: model.isPoseNameFixed(model.currentPose)
                            Layout.fillWidth: true
                            Controls.ToolTip.visible: hovered && readOnly
                            Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                            Controls.ToolTip.text: qsTr("The first %1 poses are named by RPG Maker").arg(12)
                        }
                        Ez.FormLabel {
                            text: qsTr("Animation Type")
                            Layout.alignment: Qt.AlignTop
                        }
                        Ez.RadioGroup {
                            id: type
                            jsonData: root.entry
                            key: root.pose + "animation_type"
                            enabled: poseFields.valid
                            choices: [
                                { name: "radioPoseCharacter", text: qsTr("Character Pose") },
                                { name: "radioPoseAnimation", text: qsTr("Battle Animation") },
                            ]
                        }
                        Ez.FormLabel {
                            text: qsTr("Graphic")
                        }
                        // the graphic is used by character poses, the animation by the others
                        Ez.FileField {
                            jsonData: root.entry
                            key: root.pose + "battler_name"
                            folder: "BattleCharSet"
                            fieldName: "linePoseFile"
                            buttonName: "pushPoseFile"
                            enabled: poseFields.valid && !poseFields.battle
                        }
                        Ez.FormLabel {
                            text: qsTr("Index")
                        }
                        Ez.SpinBox {
                            id: poseIndex
                            objectName: "spinPoseIndex"
                            from: 1
                            to: 8
                            enabled: poseFields.valid && !poseFields.battle
                            Ez.JsonField {
                                id: poseIndexField
                                source: root.entry
                                path: root.pose + "battler_index"
                            }
                            Binding on value {
                                value: poseIndexField.valid ? Number(poseIndexField.value) + 1 : 1
                                restoreMode: Binding.RestoreNone
                            }
                            onValueModified: poseIndexField.value = value - 1
                            function setValue(row) {
                                poseIndexField.value = row - 1
                            }
                            Controls.ToolTip.visible: hovered
                            Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                            Controls.ToolTip.text: qsTr("The row of the graphic")
                        }
                        Ez.FormLabel {
                            text: qsTr("Battle Animation")
                        }
                        Ez.RefComboBox {
                            objectName: "comboPoseAnimation"
                            database: root.database
                            listKey: "animations"
                            jsonData: root.entry
                            key: root.pose + "battle_animation_id"
                            pickable: true
                            enabled: poseFields.battle
                            Layout.fillWidth: true
                        }
                    }
                    RowLayout {
                        spacing: Ez.Metrics.sectionSpacing
                        Preview {
                            objectName: "picturePosePreview"
                            text: poseFields.battle ? qsTr("Battle animation") : ""
                        }
                        Rows {
                            objectName: "picturePoseRows"
                            index: poseIndex
                        }
                    }
                }
            }

            // the weapons
            RowLayout {
                spacing: Ez.Metrics.sectionSpacing

                Ez.ArrayTable {
                    objectName: "listWeapons"
                    rows: root.weapons ? root.weapons.count : 0
                    source: root.entry
                    columns: [
                        { title: qsTr("No."), width: 34, text: row => root.number(row) },
                        { title: qsTr("Name"), text: row => root.entry.str("weapons/" + row + "/name") },
                        { title: qsTr("Graphic"), text: row => root.weaponGraphic(row) },
                    ]
                    id: listWeapons
                    onCurrentIndexChanged: model.currentWeapon = currentIndex
                    Binding {
                        target: listWeapons
                        property: "currentIndex"
                        value: model.currentWeapon
                        restoreMode: Binding.RestoreNone
                    }
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }

                ColumnLayout {
                    id: weaponFields
                    readonly property bool valid: model.currentWeapon >= 0
                    spacing: Ez.Metrics.sectionSpacing
                    Layout.alignment: Qt.AlignTop

                    Ez.Form {
                        Ez.FormLabel {
                            text: qsTr("Name")
                        }
                        Ez.TextField {
                            objectName: "editWeaponName"
                            jsonData: root.entry
                            key: root.weapon + "name"
                            limit: 18
                            enabled: weaponFields.valid
                            Layout.fillWidth: true
                        }
                        Ez.FormLabel {
                            text: qsTr("Graphic")
                        }
                        Ez.FileField {
                            jsonData: root.entry
                            key: root.weapon + "weapon_name"
                            folder: "BattleWeapon"
                            fieldName: "lineWeaponFile"
                            buttonName: "pushWeaponFile"
                            enabled: weaponFields.valid
                        }
                        Ez.FormLabel {
                            text: qsTr("Index")
                        }
                        Ez.SpinBox {
                            id: weaponIndex
                            objectName: "spinWeaponIndex"
                            from: 1
                            to: 8
                            enabled: weaponFields.valid
                            Ez.JsonField {
                                id: weaponIndexField
                                source: root.entry
                                path: root.weapon + "weapon_index"
                            }
                            Binding on value {
                                value: weaponIndexField.valid ? Number(weaponIndexField.value) + 1 : 1
                                restoreMode: Binding.RestoreNone
                            }
                            onValueModified: weaponIndexField.value = value - 1
                            function setValue(row) {
                                weaponIndexField.value = row - 1
                            }
                            Controls.ToolTip.visible: hovered
                            Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                            Controls.ToolTip.text: qsTr("The row of the graphic")
                        }
                        // RPG Maker shows the weapon in the hands of the hero (poses 2 and 3)
                        Ez.FormLabel {
                            text: qsTr("Preview")
                        }
                        Ez.ComboBox {
                            id: hand
                            objectName: "comboWeaponPreview"
                            textRole: ""
                            valueRole: ""
                            model: [qsTr("Right Hand"), qsTr("Left Hand"), qsTr("Both Hands"), qsTr("Weapon Only")]
                            Layout.fillWidth: true
                        }
                    }
                    RowLayout {
                        spacing: Ez.Metrics.sectionSpacing
                        Preview {
                            objectName: "pictureWeaponPreview"
                        }
                        Rows {
                            objectName: "pictureWeaponRows"
                            index: weaponIndex
                        }
                    }
                }
            }
        }
    }
}
