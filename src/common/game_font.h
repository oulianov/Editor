/*
 * This file is part of EasyRPG Editor.
 *
 * EasyRPG Editor is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * EasyRPG Editor is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with EasyRPG Editor. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <QColor>
#include <QImage>
#include <QString>

/**
 * Text drawn the way EasyRPG Player draws it: with a font file of the game
 * (Font/Font, Font/Font2), else with its pixel fonts (Font::DefaultBitmapFont):
 * ttyp0 and RMG2000 for western code pages, Shinonome for Asian ones.
 */
namespace GameFont {
	/** font: 0 for font 1 (gothic), 1 for font 2 (mincho); file: a font file, else the pixel font;
	 *  cjk: the game is in an Asian code page. In pixels of the game (12 high). */
	QImage render(const QString& text, int font, const QString& file, bool cjk, const QColor& color);
}
