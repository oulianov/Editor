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

#include "web_player.h"
#include <QDir>
#include <QFileInfo>

namespace {
const QString kIndex = QStringLiteral("index.html");

bool hasFiles(const QDir& dir, const QString& pattern)
{
	return !dir.entryList({ pattern }, QDir::Files).isEmpty();
}

/** <name>.html with <name>.js and <name>.wasm next to it (what Emscripten writes) */
QString buildPage(const QDir& dir)
{
	QStringList pages = dir.entryList({ QStringLiteral("*.html") }, QDir::Files, QDir::Name);
	// the Player's default output name first
	pages.removeOne(QStringLiteral("easyrpg-player.html"));
	if (dir.exists(QStringLiteral("easyrpg-player.html"))) {
		pages.prepend(QStringLiteral("easyrpg-player.html"));
	}
	for (const QString& page : pages) {
		const QString name = QFileInfo(page).completeBaseName();
		if (dir.exists(name + QStringLiteral(".js")) && dir.exists(name + QStringLiteral(".wasm"))) {
			return page;
		}
	}
	return QString();
}
}

QString WebPlayer::page(const QString& dir)
{
	const QDir d(dir);
	if (!QFileInfo(dir).isDir()) {
		return QString();
	}
	if (d.exists(kIndex) && hasFiles(d, QStringLiteral("*.wasm")) && hasFiles(d, QStringLiteral("*.js"))) {
		return kIndex;
	}
	return buildPage(d);
}

bool WebPlayer::isBuild(const QString& dir)
{
	const QString p = page(dir);
	return !p.isEmpty() && p != kIndex;
}

QStringList WebPlayer::files(const QString& dir)
{
	const QString p = page(dir);
	if (p.isEmpty()) {
		return {};
	}
	const QDir d(dir);
	if (p == kIndex) {
		QStringList out;
		for (const QString& entry : d.entryList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::Name)) {
			if (entry.compare(QStringLiteral("games"), Qt::CaseInsensitive) != 0) {
				out << entry;
			}
		}
		return out;
	}
	// a build: the page, the program and its WebAssembly (and the data file of builds that preload some)
	const QString name = QFileInfo(p).completeBaseName();
	QStringList out = { p, name + QStringLiteral(".js"), name + QStringLiteral(".wasm") };
	if (d.exists(name + QStringLiteral(".data"))) {
		out << name + QStringLiteral(".data");
	}
	return out;
}
