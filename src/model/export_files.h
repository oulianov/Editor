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
#include <QDir>
#include <QImage>
#include <QJsonObject>
#include <QString>
#include <functional>

/**
 * The files the game export writes besides the game: archives, the file
 * index of the web Player, icons, the protected map tree.
 */
namespace ExportFiles {
	/** A name for files and folders: no character that a system refuses */
	QString safeFileName(const QString& name);
	/** Lowercase ASCII letters, digits and dashes (the game name of the web Player) */
	QString slug(const QString& name);

	/**
	 * Copies a folder with its subfolders, keeping symbolic links (in macOS
	 * application bundles) and permissions (programs stay executable).
	 */
	bool copyTree(const QString& source, const QString& destination, QString* error);

	/**
	 * Writes a zip of the content of root (stored and deflated entries, UTF-8
	 * names, Unix permissions and symbolic links, read by every system).
	 * cancelled is asked between files.
	 */
	bool writeZip(const QString& zip_path, const QString& root, QString* error,
		const std::function<bool()>& cancelled = {});

	/**
	 * The index.json the web Player reads first (format 2 of EasyRPG's
	 * gencache tool): each folder ("_dirname": its real name) maps the
	 * lowercase names, with and without extension, to the real file names.
	 */
	QJsonObject webIndex(const QDir& game_dir);

	/**
	 * The map tree that only the EasyRPG Player reads: every byte plus one.
	 * The Player recognizes it by its first bytes and reads it back
	 * (FileSystem hook of "Sacred Tears"); RPG Maker and the Editor see an
	 * invalid file. Empty (and error set) if the file is not a map tree.
	 */
	QByteArray protectMapTree(const QByteArray& lmt, QString* error);

	/** An Apple icon (.icns) with the image at 16 to 1024 pixels */
	QByteArray makeIcns(const QImage& image);

	/**
	 * Replaces the icon images of a Windows program (.exe) in place: each
	 * 32-bit or PNG image of its icons is redrawn from the image when the new
	 * one fits in the room of the old (nothing in the file moves). A
	 * signature of the program is removed, since it no longer matches.
	 * Returns how many images were replaced, -1 on error.
	 */
	int replaceExeIcon(const QString& exe_path, const QImage& image, QString* message);

	/** Sets keys of the [RPG_RT] section of an RPG_RT.ini (added if missing) */
	QByteArray setIniValues(QByteArray ini, const QList<QPair<QByteArray, QByteArray>>& values);
}
