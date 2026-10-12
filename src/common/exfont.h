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

#include <QByteArray>
#include <QColor>
#include <QImage>
#include <QString>

class Project;

/**
 * The symbols of messages ($A to $Z, $a to $z): 12x12 glyphs of the ExFont,
 * 13 a row. As EasyRPG Player finds it: Font/ExFont or ExFont of the game,
 * else the one of its RPG_RT.exe, else EasyRPG's.
 */
namespace ExFont {
	enum { GlyphSize = 12, Columns = 13, Count = 52 };

	/** The ExFont of the game, with where it comes from */
	struct Sheet {
		QImage image;
		QString source;
	};
	Sheet load(const Project& project);

	/** The EXFONT bitmap resource of an RPG_RT.exe as a BMP file, empty if none */
	QByteArray fromExe(const QString& path);

	/** The symbol code of a glyph: 0 is "$A", 26 "$a" */
	QString code(int index);

	/** A glyph, its strokes in color (the colors of a colored ExFont stay) */
	QImage glyph(const QImage& sheet, int index, const QColor& color);
}
