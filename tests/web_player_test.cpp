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

// The web Player folders the export accepts: a release (index.html) and a
// Player build folder (easyrpg-player.html/.js/.wasm among the build's files)

#include "model/web_player.h"
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <cstdio>

namespace {
int failures = 0;

void check(bool condition, const char* what) {
	if (!condition) {
		std::printf("FAILED: %s\n", what);
		++failures;
	}
}

void touch(const QDir& dir, const QString& name) {
	QDir().mkpath(QFileInfo(dir.filePath(name)).absolutePath());
	QFile f(dir.filePath(name));
	f.open(QIODevice::WriteOnly);
}
}

int main()
{
	QTemporaryDir tmp;
	const QDir root(tmp.path());

	// a release: everything is published but its games
	QDir release(root.filePath("release"));
	for (const char* name : { "index.html", "easyrpg-player.js", "easyrpg-player.wasm", "favicon.png", "games/default/index.json" }) {
		touch(release, name);
	}
	check(WebPlayer::page(release.path()) == "index.html", "release: index.html is the page");
	check(!WebPlayer::isBuild(release.path()), "release: not a build");
	check(WebPlayer::files(release.path()) == QStringList({ "easyrpg-player.js", "easyrpg-player.wasm", "favicon.png", "index.html" }),
		"release: every file but games");

	// a Player build folder: only the page and its program, never the build's files
	QDir build(root.filePath("build"));
	for (const char* name : { "easyrpg-player.html", "easyrpg-player.js", "easyrpg-player.wasm", "build.ninja", "CMakeCache.txt",
			"CMakeFiles/x.o", "compile_commands.json", "lib/liblcf/liblcf.a" }) {
		touch(build, name);
	}
	check(WebPlayer::page(build.path()) == "easyrpg-player.html", "build: easyrpg-player.html is the page");
	check(WebPlayer::isBuild(build.path()), "build: a build");
	check(WebPlayer::files(build.path()) == QStringList({ "easyrpg-player.html", "easyrpg-player.js", "easyrpg-player.wasm" }),
		"build: only the page, the program and its WebAssembly");

	// another output name (PLAYER_JS_OUTPUT_NAME), with a preload data file
	QDir named(root.filePath("named"));
	for (const char* name : { "other.html", "player.html", "player.js", "player.wasm", "player.data" }) {
		touch(named, name);
	}
	check(WebPlayer::page(named.path()) == "player.html", "named build: the page with its .js and .wasm");
	check(WebPlayer::files(named.path()) == QStringList({ "player.html", "player.js", "player.wasm", "player.data" }),
		"named build: its data file too");

	// not a web Player: a page without its program, a missing folder
	QDir half(root.filePath("half"));
	touch(half, "easyrpg-player.html");
	touch(half, "easyrpg-player.js");
	check(WebPlayer::page(half.path()).isEmpty(), "no .wasm: not a Player");
	check(WebPlayer::page(root.filePath("missing")).isEmpty(), "missing folder: not a Player");
	check(WebPlayer::files(half.path()).isEmpty(), "no Player: nothing to publish");

	std::printf("%s\n", failures ? "web_player: FAILED" : "web_player: ok");
	return failures ? 1 : 0;
}
