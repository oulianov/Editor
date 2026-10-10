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

#include <QString>
#include <QStringList>

/**
 * A folder holding the EasyRPG Player for the web, in one of two forms:
 * - a release (the archive from easyrpg.org): index.html, a .js and a .wasm,
 *   maybe with other files the page uses;
 * - a Player build folder (Emscripten): <name>.html, <name>.js and <name>.wasm
 *   (easyrpg-player.* by default) among the build's own files.
 */
namespace WebPlayer {
	/** The page of the Player in dir (index.html, or <name>.html of a build); empty when there is none */
	QString page(const QString& dir);

	/** Whether dir is a Player build folder rather than a release */
	bool isBuild(const QString& dir);

	/**
	 * The files of the Player to publish, relative to dir: all of a release
	 * (but its games folder), only the page and its program from a build
	 * (never the build's object files). The page is published as index.html.
	 */
	QStringList files(const QString& dir);
}
