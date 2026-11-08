// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "AboutDialog"

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * Help > About (on macOS, the application menu): what the Editor is and
 * where it comes from, oulianov's fork of the EasyRPG Editor, over a
 * picture that moves as the about boxes of cracktros and keygens do: a
 * starfield, a sun over a scrolling grid, wireframes turning, the logo
 * glitching now and then, a sine scroller. The texts are in a box of their
 * own, legible whatever the picture does. The picture moves only while the
 * window is shown.
 */
Ez.DialogWindow {
    id: root

    /** The version of Qt the Editor runs with (qVersion()) */
    property string qtVersion
    readonly property string forkUrl: "https://github.com/oulianov/Editor"
    readonly property color textColor: "#ece9ff"
    readonly property color dimColor: "#a9a3cc"
    readonly property color linkColor: "#5ef2ff"

    objectName: "aboutWindow"
    title: qsTranslate("MainWindow", "About EasyRPG Editor")
    preferredWidth: 640
    standardButtons: Controls.DialogButtonBox.NoButton

    // the picture behind everything, the window's margins included
    background: Rectangle {
        color: "#05020d"

        Canvas {
            id: art
            objectName: "aboutArt"
            anchors.fill: parent
            /** Seconds since the window opened, the clock of the picture */
            property real time: 0
            onPaint: root.paintArt(getContext("2d"), width, height, time)
        }
        // scanlines and a vignette, as on a CRT: drawn once per size
        Canvas {
            anchors.fill: parent
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                ctx.fillStyle = Qt.rgba(0, 0, 0, 0.2)
                for (let y = 0; y < height; y += 3) {
                    ctx.fillRect(0, y, width, 1)
                }
                const vignette = ctx.createRadialGradient(width / 2, height * 0.4, height * 0.25,
                    width / 2, height * 0.4, Math.max(width, height) * 0.8)
                vignette.addColorStop(0, Qt.rgba(0, 0, 0, 0))
                vignette.addColorStop(1, Qt.rgba(0, 0, 0, 0.6))
                ctx.fillStyle = vignette
                ctx.fillRect(0, 0, width, height)
            }
        }
    }

    FrameAnimation {
        running: root.visible
        onTriggered: {
            // a frame late (the window moved, the computer slept) does not jump
            art.time += Math.min(frameTime, 0.1)
            art.requestPaint()
        }
    }

    // the picture shows through above the box
    Item {
        Layout.fillWidth: true
        Layout.preferredHeight: 232
    }

    Rectangle {
        objectName: "aboutBox"
        color: Qt.rgba(0.02, 0.01, 0.07, 0.9)
        border.color: Qt.rgba(0.37, 0.95, 1, 0.5)
        radius: 6
        Layout.fillWidth: true
        implicitHeight: info.implicitHeight + 2 * Ez.Metrics.dialogMargin

        ColumnLayout {
            id: info
            anchors.fill: parent
            anchors.margins: Ez.Metrics.dialogMargin
            spacing: Ez.Metrics.rowSpacing

            RowLayout {
                spacing: Ez.Metrics.columnSpacing
                Layout.fillWidth: true

                Ez.Label {
                    objectName: "aboutName"
                    text: "EasyRPG Editor"
                    color: root.textColor
                    font.pixelSize: Math.round(Ez.Metrics.font.pixelSize * 1.6)
                    font.bold: true
                }
                Ez.Label {
                    objectName: "aboutVersion"
                    text: qsTr("Version %1").arg(Qt.application.version)
                    color: root.dimColor
                    Layout.alignment: Qt.AlignBaseline
                }
                Item {
                    Layout.fillWidth: true
                }
            }
            Ez.Label {
                objectName: "aboutFork"
                text: qsTr("oulianov's fork of the EasyRPG Editor.")
                color: root.textColor
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            Ez.Label {
                objectName: "aboutForkLink"
                text: qsTr("Source code, releases and bug reports: %1")
                    .arg("<a href=\"" + root.forkUrl + "\">github.com/oulianov/Editor</a>")
                textFormat: Text.StyledText
                color: root.textColor
                linkColor: root.linkColor
                wrapMode: Text.Wrap
                Layout.fillWidth: true
                onLinkActivated: link => Qt.openUrlExternally(link)
                HoverHandler {
                    cursorShape: parent.hoveredLink !== "" ? Qt.PointingHandCursor : Qt.ArrowCursor
                }
            }
            Rectangle {
                color: Qt.rgba(0.37, 0.95, 1, 0.25)
                implicitHeight: 1
                Layout.fillWidth: true
                Layout.topMargin: Ez.Metrics.rowSpacing
                Layout.bottomMargin: Ez.Metrics.rowSpacing
            }
            Ez.Label {
                objectName: "aboutUpstream"
                text: qsTr("Based on the EasyRPG Editor of the EasyRPG project (%1): games for EasyRPG Player "
                    + "and RPG Maker 2000/2003.").arg("<a href=\"https://easyrpg.org/\">easyrpg.org</a>")
                textFormat: Text.StyledText
                color: root.dimColor
                linkColor: root.linkColor
                wrapMode: Text.Wrap
                Layout.fillWidth: true
                onLinkActivated: link => Qt.openUrlExternally(link)
                HoverHandler {
                    cursorShape: parent.hoveredLink !== "" ? Qt.PointingHandCursor : Qt.ArrowCursor
                }
            }
            Ez.Label {
                objectName: "aboutLicense"
                text: qsTr("Built with Qt %1. Free software, under the GNU General Public License "
                    + "(version 3 or later).").arg(root.qtVersion)
                color: root.dimColor
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            Ez.Button {
                objectName: "aboutClose"
                text: qsTr("Close")
                Layout.alignment: Qt.AlignRight
                Layout.topMargin: Ez.Metrics.rowSpacing
                onClicked: root.reject()
            }
        }
    }

    // ---- the picture ----------------------------------------------------

    /** The stars flying out: {x, y} in -1..1, z the distance (1 far, 0 here) */
    property var stars: []
    /** Widths of the characters drawn, per font */
    property var widths: ({})
    /** The time of the last picture, for the stars' step */
    property real lastTime: 0
    readonly property string scrollText: "*** EASYRPG EDITOR *** OULIANOV FORK *** GAMES FOR EASYRPG PLAYER "
        + "AND RPG MAKER 2000/2003 *** GREETINGS TO THE EASYRPG TEAM AND TO EVERY RPG MAKER DEVELOPER OUT THERE "
        + "*** NO SERIAL, NO PATCH NEEDED: FREE SOFTWARE UNDER THE GPL *** GITHUB.COM/OULIANOV/EDITOR ***     "
    readonly property string monoFamily: Kirigami.Theme.fixedWidthFont.family

    Component.onCompleted: {
        const made = []
        for (let i = 0; i < 170; ++i) {
            made.push({ x: Math.random() * 2 - 1, y: Math.random() * 2 - 1, z: Math.random() })
        }
        stars = made
    }

    /** A bright color of the hue (degrees, any value) */
    function neon(hue, lightness, alpha) {
        return Qt.hsla((((hue % 360) + 360) % 360) / 360, 1, lightness, alpha)
    }
    /** The same number for the same n, 0 to 1 */
    function noise(n) {
        const x = Math.sin(n * 12.9898) * 43758.5453
        return x - Math.floor(x)
    }
    function charWidth(ctx, ch) {
        const key = ctx.font + "|" + ch
        if (widths[key] === undefined) {
            widths[key] = ctx.measureText(ch).width
        }
        return widths[key]
    }
    /** Draws text centered on x, its letters apart by spacing */
    function spacedText(ctx, text, x, y, spacing, stroke) {
        let total = spacing * (text.length - 1)
        for (const ch of text) {
            total += charWidth(ctx, ch)
        }
        let at = x - total / 2
        for (const ch of text) {
            if (stroke) {
                ctx.strokeText(ch, at, y)
            } else {
                ctx.fillText(ch, at, y)
            }
            at += charWidth(ctx, ch) + spacing
        }
    }

    function paintArt(ctx, w, h, t) {
        ctx.reset()
        const cx = w / 2
        const horizon = 196
        const logoY = 80

        // the sun on the horizon, cut by bands drifting down
        ctx.save()
        ctx.beginPath()
        ctx.rect(0, 0, w, horizon)
        ctx.clip()
        const sunRadius = 80
        const sun = ctx.createLinearGradient(0, horizon - sunRadius, 0, horizon)
        sun.addColorStop(0, "#ffe66d")
        sun.addColorStop(0.55, "#ff7a3d")
        sun.addColorStop(1, "#ff1f9d")
        ctx.fillStyle = sun
        ctx.beginPath()
        ctx.arc(cx, horizon, sunRadius, 0, Math.PI * 2)
        ctx.fill()
        ctx.globalCompositeOperation = "destination-out"
        ctx.fillStyle = "black"
        const drift = (t * 9) % 11
        for (let i = 0; i < 7; ++i) {
            const y = horizon - 46 + i * 10 + drift
            ctx.fillRect(cx - sunRadius, y, sunRadius * 2, 1 + (y - (horizon - 46)) / 10)
        }
        ctx.restore()

        // everything else adds its light
        ctx.globalCompositeOperation = "lighter"

        // the grid flying under the horizon
        const floor = h - horizon
        ctx.lineWidth = 1
        ctx.strokeStyle = neon(300 + 20 * Math.sin(t * 0.7), 0.55, 0.45)
        ctx.beginPath()
        for (let i = -16; i <= 16; ++i) {
            ctx.moveTo(cx + i * 14, horizon)
            ctx.lineTo(cx + i * 120, h)
        }
        ctx.stroke()
        const phase = (t * 0.9) % 1
        for (let n = 0; n < 16; ++n) {
            const p = (n + phase) / 16
            const y = horizon + floor * Math.pow(p, 2.3)
            ctx.strokeStyle = neon(300 + 40 * p, 0.6, 0.1 + 0.6 * p)
            ctx.beginPath()
            ctx.moveTo(0, y)
            ctx.lineTo(w, y)
            ctx.stroke()
        }
        const glow = ctx.createLinearGradient(0, horizon - 14, 0, horizon + 14)
        glow.addColorStop(0, neon(320, 0.6, 0))
        glow.addColorStop(0.5, neon(320, 0.65, 0.55))
        glow.addColorStop(1, neon(320, 0.6, 0))
        ctx.fillStyle = glow
        ctx.fillRect(0, horizon - 14, w, 28)

        // the stars, from the logo outwards
        const step = 0.35 * Math.max(0, Math.min(0.1, t - lastTime))
        lastTime = t
        const scale = w * 0.32
        ctx.lineWidth = 1.4
        for (let band = 0; band < 3; ++band) {
            ctx.strokeStyle = neon(190 + band * 40, 0.75, 0.3 + band * 0.3)
            ctx.beginPath()
            for (let i = band; i < stars.length; i += 3) {
                const star = stars[i]
                const before = star.z
                star.z -= step
                if (star.z <= 0.04) {
                    star.x = Math.random() * 2 - 1
                    star.y = Math.random() * 2 - 1
                    star.z = 1
                    continue
                }
                const x0 = cx + star.x / before * scale * 0.2
                const y0 = logoY + star.y / before * scale * 0.2
                const x1 = cx + star.x / star.z * scale * 0.2
                const y1 = logoY + star.y / star.z * scale * 0.2
                ctx.moveTo(x0, y0)
                ctx.lineTo(x1 + (x1 - x0) * 2, y1 + (y1 - y0) * 2)
            }
            ctx.stroke()
        }

        // orbits around the logo, dots running on them
        for (let r = 0; r < 3; ++r) {
            ctx.save()
            ctx.translate(cx, logoY + 4)
            ctx.rotate(Math.sin(t * 0.3 + r) * 0.12)
            ctx.scale(1, 0.3)
            const radius = 190 + r * 26
            const turn = t * (0.5 + r * 0.25) * (r % 2 ? -1 : 1)
            ctx.strokeStyle = neon(180 + r * 60 + t * 30, 0.6, 0.55)
            ctx.lineWidth = 2
            for (let s = 0; s < 3; ++s) {
                const start = turn + s * Math.PI * 2 / 3
                ctx.beginPath()
                ctx.arc(0, 0, radius, start, start + Math.PI * 0.45)
                ctx.stroke()
            }
            ctx.fillStyle = neon(60 + r * 90, 0.8, 0.9)
            const dot = -turn * 1.7
            ctx.beginPath()
            ctx.arc(Math.cos(dot) * radius, Math.sin(dot) * radius, 6, 0, Math.PI * 2)
            ctx.fill()
            ctx.restore()
        }

        // two icosahedra turning, one on each side
        const a = 0.5257311, b = 0.8506508
        const vertices = [[-a, b, 0], [a, b, 0], [-a, -b, 0], [a, -b, 0], [0, -a, b], [0, a, b],
            [0, -a, -b], [0, a, -b], [b, 0, -a], [b, 0, a], [-b, 0, -a], [-b, 0, a]]
        const edges = []
        for (let i = 0; i < 12; ++i) {
            for (let j = i + 1; j < 12; ++j) {
                const d = (vertices[i][0] - vertices[j][0]) ** 2 + (vertices[i][1] - vertices[j][1]) ** 2
                    + (vertices[i][2] - vertices[j][2]) ** 2
                if (d < 1.2) {
                    edges.push([i, j])
                }
            }
        }
        for (let side = -1; side <= 1; side += 2) {
            const ox = cx + side * (w / 2 - 78)
            const oy = logoY + 18 + Math.sin(t * 1.3 + side) * 8
            const ry = t * 0.9 * side, rx = t * 0.6
            const size = 44 + Math.sin(t * 2 + side) * 4
            const points = vertices.map(v => {
                const x1 = v[0] * Math.cos(ry) + v[2] * Math.sin(ry)
                const z1 = -v[0] * Math.sin(ry) + v[2] * Math.cos(ry)
                const y2 = v[1] * Math.cos(rx) - z1 * Math.sin(rx)
                const z2 = v[1] * Math.sin(rx) + z1 * Math.cos(rx)
                const f = 3 / (3.4 + z2)
                return [ox + x1 * size * f, oy + y2 * size * f]
            })
            const hue = (side < 0 ? 185 : 310) + Math.sin(t * 0.8) * 25
            for (const pass of [[5, 0.16], [1.3, 0.95]]) {
                ctx.lineWidth = pass[0]
                ctx.strokeStyle = neon(hue, 0.6, pass[1])
                ctx.beginPath()
                for (const e of edges) {
                    ctx.moveTo(points[e[0]][0], points[e[0]][1])
                    ctx.lineTo(points[e[1]][0], points[e[1]][1])
                }
                ctx.stroke()
            }
        }

        // the logo: its colors apart, and now and then cut in slices
        const glitching = (t % 4.2) < 0.2
        const split = glitching ? 7 : 2 + Math.sin(t * 3) * 0.8
        const drawLogo = dx => {
            ctx.font = "bold 52px sans-serif"
            ctx.textBaseline = "middle"
            ctx.fillStyle = Qt.rgba(1, 0.15, 0.35, 0.85)
            spacedText(ctx, "EASYRPG", cx + dx - split, logoY - 12, 7, false)
            ctx.fillStyle = Qt.rgba(0.1, 0.9, 1, 0.85)
            spacedText(ctx, "EASYRPG", cx + dx + split, logoY - 12, 7, false)
            // chrome over the colors
            ctx.globalCompositeOperation = "source-over"
            const chrome = ctx.createLinearGradient(0, logoY - 34, 0, logoY + 10)
            chrome.addColorStop(0, "#ffffff")
            chrome.addColorStop(0.48, "#bfeaff")
            chrome.addColorStop(0.52, "#6a5cff")
            chrome.addColorStop(1, "#ff8ce6")
            ctx.fillStyle = chrome
            spacedText(ctx, "EASYRPG", cx + dx, logoY - 12, 7, false)
            ctx.globalCompositeOperation = "lighter"
            // EDITOR in neon: a wide faint stroke for the glow, then the letters
            ctx.font = "bold 26px sans-serif"
            const tube = neon(185 + t * 40, 0.62, 1)
            ctx.lineWidth = 6
            ctx.strokeStyle = neon(185 + t * 40, 0.6, 0.22)
            spacedText(ctx, "EDITOR", cx + dx, logoY + 26, 16, true)
            ctx.fillStyle = tube
            spacedText(ctx, "EDITOR", cx + dx, logoY + 26, 16, false)
        }
        if (glitching) {
            const frame = Math.floor(t * 24)
            for (let s = 0; s < 8; ++s) {
                ctx.save()
                ctx.beginPath()
                ctx.rect(0, logoY - 44 + s * 11, w, 11)
                ctx.clip()
                drawLogo((noise(frame * 8 + s) - 0.5) * 30)
                ctx.restore()
            }
        } else {
            drawLogo(0)
        }
        // the tag of the fork on a dark chip, its cursor blinking
        ctx.font = "bold 13px '" + monoFamily + "'"
        const tag = "OULIANOV FORK" + ((t % 1) < 0.5 ? "_" : " ")
        const tagWidth = tag.length * (charWidth(ctx, "M") + 2) + 18
        ctx.globalCompositeOperation = "source-over"
        ctx.fillStyle = Qt.rgba(0.02, 0, 0.06, 0.85)
        ctx.fillRect(cx - tagWidth / 2, logoY + 46, tagWidth, 22)
        ctx.globalCompositeOperation = "lighter"
        ctx.lineWidth = 1
        ctx.strokeStyle = neon(320, 0.65, 0.9)
        ctx.strokeRect(cx - tagWidth / 2 + 0.5, logoY + 46.5, tagWidth - 1, 21)
        ctx.fillStyle = neon(320, 0.75, 1)
        spacedText(ctx, tag, cx, logoY + 57, 2, false)

        // the sine scroller over the horizon
        ctx.font = "bold 16px '" + monoFamily + "'"
        const cw = charWidth(ctx, "M") + 1
        const length = scrollText.length * cw
        const start = w - (t * 95) % (length + w)
        for (let i = 0; i < scrollText.length; ++i) {
            const x = start + i * cw
            if (x < -cw || x > w) {
                continue
            }
            const y = horizon + 20 + Math.sin(t * 3.2 + x * 0.025) * 8
            ctx.fillStyle = neon(x * 0.6 + t * 140, 0.65, 1)
            ctx.fillText(scrollText[i], x, y)
        }

        // the sky behind it all
        ctx.globalCompositeOperation = "destination-over"
        const sky = ctx.createLinearGradient(0, 0, 0, h)
        sky.addColorStop(0, neon(255 + 15 * Math.sin(t * 0.2), 0.05, 1))
        sky.addColorStop(horizon / h, neon(285, 0.13, 1))
        sky.addColorStop(Math.min(1, horizon / h + 0.01), "#090313")
        sky.addColorStop(1, "#020106")
        ctx.fillStyle = sky
        ctx.fillRect(0, 0, w, h)
    }
}
