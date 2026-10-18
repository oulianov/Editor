#!/usr/bin/env python3
# /// script
# requires-python = ">=3.10"
# dependencies = ["pillow>=10"]
# ///
"""Generate the EasyRPG Editor application icons.

The Editor uses the EasyRPG artwork (builds/icons/easyrpg-icon-1024.png, the
1024 px image of the original Editor.icns, unchanged) with an "EDIT" badge in
the bottom-right corner: white bold letters on a black rectangle. The letters
are drawn as shapes (no font file needed), pixel-snapped for the small sizes
and smooth for the large ones, so the output is the same on every system.

Outputs (paths relative to the repository root):
  resources/app/Editor.icns                    macOS bundle (16 to 1024 px)
  resources/app/editor.ico                     Windows (16, 24, 32, 48, 64, 128, 256)
  resources/app/easyrpg.png                    48 px (Linux pixmaps / hicolor 48x48)
  resources/app/icons/easyrpg-editor-<N>.png   Linux hicolor icons, other sizes

Run from anywhere:  uv run builds/icons/make-editor-icon.py [--preview DIR]
Editor.icns is written with macOS iconutil when available, else with Pillow.
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
SOURCE = Path(__file__).resolve().parent / "easyrpg-icon-1024.png"
APP = ROOT / "resources" / "app"

ICNS_SET = {  # iconset file name -> pixel size
	"icon_16x16.png": 16, "icon_16x16@2x.png": 32,
	"icon_32x32.png": 32, "icon_32x32@2x.png": 64,
	"icon_128x128.png": 128, "icon_128x128@2x.png": 256,
	"icon_256x256.png": 256, "icon_256x256@2x.png": 512,
	"icon_512x512.png": 512, "icon_512x512@2x.png": 1024,
}
ICO_SIZES = [16, 24, 32, 48, 64, 128, 256]
HICOLOR_SIZES = [16, 24, 32, 64, 128, 256, 512]  # 48 is resources/app/easyrpg.png

BLACK = (0, 0, 0, 255)
WHITE = (255, 255, 255, 255)

# "EDIT" on a 5-unit-high grid, one unit = one stroke. Pixel bitmaps for the
# small sizes; the large sizes use the same shapes with a rounded D.
GLYPHS = {
	"E": ["###", "#..", "##.", "#..", "###"],
	"D": ["##.", "#.#", "#.#", "#.#", "##."],
	"I": ["#", "#", "#", "#", "#"],
	"T": ["###", ".#.", ".#.", ".#.", ".#."],
}
TEXT = "EDIT"
TEXT_W = sum(len(GLYPHS[c][0]) for c in TEXT) + len(TEXT) - 1  # 13 units
TEXT_H = 5

# Pixel badges: size -> (unit px, horizontal padding px, vertical padding px).
PIXEL_BADGES = {16: (1, 1, 1), 24: (1, 2, 1), 32: (1, 2, 2), 48: (2, 2, 2)}
# Smooth badges: padding in units, width as a fraction of the icon.
PAD_X, PAD_Y = 1.6, 1.3
BADGE_FRACTION = 0.56


def artwork(size: int) -> Image.Image:
	src = Image.open(SOURCE).convert("RGBA")
	return src if size == src.width else src.resize((size, size), Image.LANCZOS)


def pixel_badge(size: int) -> Image.Image:
	unit, px, py = PIXEL_BADGES[size]
	w, h = TEXT_W * unit + 2 * px, TEXT_H * unit + 2 * py
	badge = Image.new("RGBA", (w, h), BLACK)
	draw = ImageDraw.Draw(badge)
	x = px
	for ch in TEXT:
		rows = GLYPHS[ch]
		for gy, row in enumerate(rows):
			for gx, cell in enumerate(row):
				if cell == "#":
					x0, y0 = x + gx * unit, py + gy * unit
					draw.rectangle([x0, y0, x0 + unit - 1, y0 + unit - 1], fill=WHITE)
		x += (len(rows[0]) + 1) * unit
	if size >= 24:  # soften the corners
		for cx, cy in ((0, 0), (w - 1, 0), (0, h - 1), (w - 1, h - 1)):
			badge.putpixel((cx, cy), (0, 0, 0, 0))
	return badge


def smooth_badge(size: int) -> Image.Image:
	# Badge size in whole pixels, then drawn 8x larger and reduced.
	unit = size * BADGE_FRACTION / (TEXT_W + 2 * PAD_X)
	w = round((TEXT_W + 2 * PAD_X) * unit)
	h = round((TEXT_H + 2 * PAD_Y) * unit)
	ss = 8
	big = Image.new("RGBA", (w * ss, h * ss), (0, 0, 0, 0))
	draw = ImageDraw.Draw(big)
	u = unit * ss
	border = max(ss, round(0.18 * u))  # thin light rim against the dark green
	draw.rounded_rectangle([0, 0, w * ss - 1, h * ss - 1], radius=0.9 * u, fill=(255, 255, 255, 230))
	draw.rounded_rectangle([border, border, w * ss - 1 - border, h * ss - 1 - border],
		radius=0.9 * u - border, fill=BLACK)
	ox = (w * ss - TEXT_W * u) / 2
	oy = (h * ss - TEXT_H * u) / 2

	def rect(x: float, y: float, rw: float, rh: float) -> None:
		draw.rectangle([ox + x * u, oy + y * u, ox + (x + rw) * u - 1, oy + (y + rh) * u - 1], fill=WHITE)

	# E
	rect(0, 0, 1, 5); rect(0, 0, 3, 1); rect(0, 2, 2.5, 1); rect(0, 4, 3, 1)
	# D: outer shape rounded on the right, counter cut out
	draw.rounded_rectangle([ox + 4 * u, oy, ox + 7 * u - 1, oy + 5 * u - 1], radius=1.6 * u,
		fill=WHITE, corners=(False, True, True, False))
	draw.rounded_rectangle([ox + 5 * u, oy + 1 * u, ox + 6 * u - 1, oy + 4 * u - 1], radius=0.5 * u,
		fill=BLACK, corners=(False, True, True, False))
	# I
	rect(8, 0, 1, 5)
	# T
	rect(10, 0, 3, 1); rect(11, 0, 1, 5)
	return big.resize((w, h), Image.LANCZOS)


def editor_icon(size: int) -> Image.Image:
	img = artwork(size)
	badge = pixel_badge(size) if size in PIXEL_BADGES else smooth_badge(size)
	margin = 0 if size in PIXEL_BADGES else round(size * 0.012)
	img.alpha_composite(badge, (size - badge.width - margin, size - badge.height - margin))
	return img


def write_icns(images: dict[int, Image.Image], out: Path) -> None:
	if shutil.which("iconutil"):
		with tempfile.TemporaryDirectory() as tmp:
			iconset = Path(tmp) / "Editor.iconset"
			iconset.mkdir()
			for name, size in ICNS_SET.items():
				images[size].save(iconset / name)
			subprocess.run(["iconutil", "-c", "icns", "-o", str(out), str(iconset)], check=True)
	else:
		sizes = sorted(set(ICNS_SET.values()))
		images[1024].save(out, append_images=[images[s] for s in sizes if s != 1024])


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
	parser.add_argument("--preview", type=Path, help="also save every size as PNG into this folder")
	args = parser.parse_args()

	sizes = sorted(set(ICNS_SET.values()) | set(ICO_SIZES) | set(HICOLOR_SIZES) | {48})
	images = {s: editor_icon(s) for s in sizes}

	write_icns(images, APP / "Editor.icns")
	images[256].save(APP / "editor.ico", sizes=[(s, s) for s in ICO_SIZES],
		append_images=[images[s] for s in ICO_SIZES if s != 256])
	images[48].save(APP / "easyrpg.png", optimize=True)
	icons = APP / "icons"
	icons.mkdir(exist_ok=True)
	for s in HICOLOR_SIZES:
		images[s].save(icons / f"easyrpg-editor-{s}.png", optimize=True)

	if args.preview:
		args.preview.mkdir(parents=True, exist_ok=True)
		for s, img in images.items():
			img.save(args.preview / f"editor-{s}.png")
	return 0


if __name__ == "__main__":
	sys.exit(main())
