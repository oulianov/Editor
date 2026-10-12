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
#include <QList>
#include <QString>
#include <optional>
#include <utility>
#include <vector>

/**
 * Reads and changes a section of an ini file of a game (RPG_RT.ini,
 * EasyRPG.ini) line by line, as bytes: the other lines, the comments, the
 * code page and the line ends stay as they are. Sections and keys are found
 * whatever their case, as the Player does.
 */
namespace IniFile {
	using Entry = std::pair<QByteArray, QByteArray>;

	/** The keys of a section with their values, as written */
	QList<Entry> read(const QString& path, const QByteArray& section);

	/** A key to set, or to remove (no value) */
	using Change = std::pair<QByteArray, std::optional<QByteArray>>;

	/**
	 * Sets or removes keys of a section, adding the section (at the end) or
	 * the file when needed; with replace, the other keys of the section are
	 * removed. A section left without keys is removed. New lines end with
	 * CR LF when the file does (crlf: for a new file).
	 */
	bool write(const QString& path, const QByteArray& section, const std::vector<Change>& changes,
		bool replace = false, bool crlf = true);
}
