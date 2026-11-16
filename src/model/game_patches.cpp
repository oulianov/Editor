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

#include "game_patches.h"
#include "common/ini_file.h"
#include "model/project.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <algorithm>

namespace {

QString iniPath(const Project& project) {
	return project.findFileOrDefault(QStringLiteral("EasyRPG.ini"));
}

/** A value of EasyRPG.ini, empty when it is not written */
QString iniValue(const Project& project, const char* section, const char* key) {
	for (const auto& [name, value] : IniFile::read(iniPath(project), section)) {
		if (name.compare(key, Qt::CaseInsensitive) == 0) {
			return QString::fromUtf8(value).trimmed();
		}
	}
	return {};
}

} // namespace

const std::vector<GamePatches::Patch>& GamePatches::list() {
	// the keys of Game_ConfigGame, with their names in the Player
	static const std::vector<Patch> patches = {
		{"Maniac", QT_TRANSLATE_NOOP("GamePatches", "Maniac Patch"),
			QT_TRANSLATE_NOOP("GamePatches", "Adds the commands of the Maniac Patch: text variables, arrays, mouse and keyboard input, global saves, picture editing and more. The Editor then offers them in the command list. Found automatically when RPG_RT.exe is the Maniac version or accord.dll is in the game folder.")},
		{"DynRPG", QT_TRANSLATE_NOOP("GamePatches", "DynRPG"),
			QT_TRANSLATE_NOOP("GamePatches", "Lets comments starting with @ call DynRPG plugins. EasyRPG Player only knows a few of these plugins and skips the others. Found automatically when dynloader.dll is in the game folder.")},
		{"KeyPatch", QT_TRANSLATE_NOOP("GamePatches", "Ineluki Key Patch"),
			QT_TRANSLATE_NOOP("GamePatches", "Lets events read any key and the mouse, or play MP3 music, by playing a sound file whose name ends in .script. Found automatically when harmony.dll is in the game folder (many games ship the original one, which does no harm).")},
		{"CommonThisEvent", QT_TRANSLATE_NOOP("GamePatches", "Common This Event"),
			QT_TRANSLATE_NOOP("GamePatches", "In RPG Maker 2000 and early versions of 2003, \"This Event\" does nothing inside a common event. With this on, it means the map event that called the common event, as in RPG Maker 2003 1.10 and later.")},
		{"PicUnlock", QT_TRANSLATE_NOOP("GamePatches", "Unlock Pictures"),
			QT_TRANSLATE_NOOP("GamePatches", "The Japanese RPG Maker makes picture commands wait while a message is open, so pictures freeze during dialogues. With this on, they run right away, as in the English versions.")},
		{"RPG2k3Commands", QT_TRANSLATE_NOOP("GamePatches", "RPG Maker 2003 Commands"),
			QT_TRANSLATE_NOOP("GamePatches", "Lets any game use the command options of RPG Maker 2003 1.12, whatever its version: the second timer, fog and sandstorm, battle conditions, Open Load Menu, Exit Game and more.")},
		{"PowerMode2003", QT_TRANSLATE_NOOP("GamePatches", "Power Mode 2003"),
			QT_TRANSLATE_NOOP("GamePatches", "Turns variables 1 to 8 into controls of the engine: load menu, mouse position, pressed key, decimal math and picture rotation. Found automatically when warp.dll is in the game folder.")},
		{"EasyRPG", QT_TRANSLATE_NOOP("GamePatches", "EasyRPG Extensions"),
			QT_TRANSLATE_NOOP("GamePatches", "Allows true-color images, large charsets and the event commands only EasyRPG Player has. A game that uses them no longer runs with RPG_RT.exe.")},
	};
	return patches;
}

QString GamePatches::explain(const Patch& patch) {
	auto tr = [](const char* text) { return QCoreApplication::translate("GamePatches", text).toHtmlEscaped(); };
	return QStringLiteral("<b>%1</b><br>%2").arg(tr(patch.name), tr(patch.text));
}

QMap<QString, int> GamePatches::detect(const Project& project) {
	return detect(project, engineSet(project) != RpgRtExe::EngineNone);
}

QMap<QString, int> GamePatches::detect(const Project& project, bool engineSet) {
	QMap<QString, int> values;
	for (const auto& patch : list()) {
		values[QString::fromLatin1(patch.key)] = 0;
	}
	auto has = [&](const char* file) { return !project.findFile(QString::fromLatin1(file)).isEmpty(); };
	// its version, from the program when the engine is not set (Player::CreateGameObjects)
	const QString exe = engineExe(project);
	if (!exe.isEmpty() && !engineSet) {
		values[QStringLiteral("Maniac")] = exeInfo(exe).maniac;
	}
	if (!values[QStringLiteral("Maniac")] && has("accord.dll")) {
		values[QStringLiteral("Maniac")] = 1;
	}
	values[QStringLiteral("DynRPG")] = has("dynloader.dll");
	values[QStringLiteral("KeyPatch")] = has("harmony.dll");
	values[QStringLiteral("PowerMode2003")] = has("warp.dll");
	return values;
}

QString GamePatches::engineSetting(const Project& project) {
	return iniValue(project, "Game", "Engine");
}

int GamePatches::engineSet(const Project& project) {
	return RpgRtExe::engineFromSetting(engineSetting(project));
}

int GamePatches::detectEngine(const Project& project, int ldbId, int databaseVersion) {
	using namespace RpgRtExe;
	// the program, when it tells
	const QString exe = engineExe(project);
	if (!exe.isEmpty()) {
		if (const int engine = exeInfo(exe).engine; engine != EngineNone) {
			return engine;
		}
	}
	// the database
	int engine = ldbId == 2003 ? EngineRpg2k3 : EngineRpg2k;
	if (ldbId == 2003 ? !project.findFile(QStringLiteral("ultimate_rt_eb.dll")).isEmpty() : databaseVersion >= 1) {
		return engine | EngineMajorUpdated | EngineEnglish;
	}
	// FileFinder::IsMajorUpdatedTree: an MP3 music file, unless harmony.dll
	// is not the official one
	const QString harmony = project.findFile(QStringLiteral("Harmony.dll"));
	if (harmony.isEmpty() || QFileInfo(harmony).size() == 473600) {
		QDir music = project.projectDir();
		for (const QString& sub : music.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
			if (sub.compare(QLatin1String("Music"), Qt::CaseInsensitive) == 0 && music.cd(sub)) {
				for (const QFileInfo& file : music.entryInfoList(QDir::Files)) {
					if (file.suffix().compare(QLatin1String("mp3"), Qt::CaseInsensitive) == 0) {
						return engine | EngineMajorUpdated;
					}
				}
				break;
			}
		}
	}
	// the size of RPG_RT.exe
	const QString rpg_rt = project.findFile(QStringLiteral("RPG_RT.exe"));
	if (!rpg_rt.isEmpty()) {
		return QFileInfo(rpg_rt).size() > (engine == EngineRpg2k ? 735000 : 927000) ? engine | EngineMajorUpdated : engine;
	}
	// the most common: Japanese and RPG Maker 2003 games the newer engine
	const QString encoding = project.encoding();
	const bool japanese = encoding == QLatin1String("932") || encoding.startsWith(QLatin1String("ibm-943"))
		|| encoding == QLatin1String("windows-932") || encoding.compare(QLatin1String("Shift_JIS"), Qt::CaseInsensitive) == 0;
	return japanese || engine == EngineRpg2k3 ? engine | EngineMajorUpdated : engine;
}

QString GamePatches::enginePathSetting(const Project& project) {
	return iniValue(project, "Game", "EnginePath");
}

QString GamePatches::engineExe(const Project& project) {
	const QString path = enginePathSetting(project);
	if (!path.isEmpty()) {
		const QString file = project.projectDir().filePath(path);
		return QFileInfo::exists(file) ? file : QString();
	}
	return project.findFile(QStringLiteral("RPG_RT.exe"));
}

RpgRtExe::Info GamePatches::exeInfo(const QString& exe) {
	static QHash<QString, RpgRtExe::Info> known;
	const QString key = exe + QFileInfo(exe).lastModified().toString(Qt::ISODateWithMs);
	auto it = known.find(key);
	if (it == known.end()) {
		it = known.insert(key, RpgRtExe::read(exe));
	}
	return *it;
}

GamePatches::State GamePatches::read(const Project& project) {
	State state;
	const auto entries = IniFile::read(iniPath(project), "Patch");
	state.chosen = !entries.isEmpty();
	if (!state.chosen) {
		state.values = detect(project);
		return state;
	}
	for (const auto& patch : list()) {
		state.values[QString::fromLatin1(patch.key)] = 0;
	}
	for (const auto& [key, value] : entries) {
		bool known = false;
		for (const auto& patch : list()) {
			if (key.compare(patch.key, Qt::CaseInsensitive) == 0) {
				// as the Player: a number, or a boolean
				const QByteArray v = value.trimmed().toLower();
				bool ok = false;
				int number = v.toInt(&ok);
				if (!ok) {
					number = v == "true" || v == "on" || v == "yes" ? 1 : 0;
				}
				state.values[QString::fromLatin1(patch.key)] = number;
				known = true;
			}
		}
		if (!known) {
			state.others << QString::fromLatin1(key);
		}
	}
	return state;
}

bool GamePatches::write(const Project& project, const std::optional<QMap<QString, int>>& values) {
	std::vector<IniFile::Change> changes;
	if (values) {
		// the patches on, and those already written; the other keys stay
		const auto entries = IniFile::read(iniPath(project), "Patch");
		auto written = [&](const char* key) {
			return std::any_of(entries.begin(), entries.end(),
				[&](const IniFile::Entry& entry) { return entry.first.compare(key, Qt::CaseInsensitive) == 0; });
		};
		for (const auto& patch : list()) {
			const int value = values->value(QString::fromLatin1(patch.key));
			if (value > 0 || written(patch.key)) {
				changes.push_back({patch.key, QByteArray::number(value)});
			}
		}
		// none on: one written off, else the section would let the Player detect them
		if (changes.empty() && entries.isEmpty()) {
			changes.push_back({list().front().key, QByteArray("0")});
		}
		const bool saved = IniFile::write(iniPath(project), "Patch", changes, false, false);
		emit GamePatchesWatch::instance().written();
		return saved;
	}
	const bool saved = IniFile::write(iniPath(project), "Patch", changes, true, false);
	emit GamePatchesWatch::instance().written();
	return saved;
}

bool GamePatches::hasManiac(const Project& project) {
	return read(project).values.value(QStringLiteral("Maniac")) > 0;
}

bool GamePatches::hasEasyRpg(const Project& project) {
	return read(project).values.value(QStringLiteral("EasyRPG")) > 0;
}

GamePatchesWatch& GamePatchesWatch::instance() {
	static GamePatchesWatch watch;
	return watch;
}
