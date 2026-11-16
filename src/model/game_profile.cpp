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

#include "game_profile.h"
#include "common/filefinder.h"
#include "common/ini_file.h"
#include "common/rpg_rt_exe.h"
#include "model/game_patches.h"
#include "model/game_setup.h"
#include "model/project.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QFile>
#include <QRegularExpression>
#include <lcf/ldb/chunks.h>
#include <optional>

namespace {

/** The values of the System of a database the profile tells */
struct System {
	int ldbId = 0;
	bool battle2k = false;
	int alternativeExp = 0;
};

/** A number of RPG Maker's files: 7 bits a byte, the last one without its high bit */
std::optional<qint32> readNumber(QIODevice& device) {
	quint32 value = 0;
	for (int i = 0; i < 5; ++i) {
		char byte = 0;
		if (!device.getChar(&byte)) {
			return std::nullopt;
		}
		value = (value << 7) | (static_cast<quint8>(byte) & 0x7F);
		if ((static_cast<quint8>(byte) & 0x80) == 0) {
			return static_cast<qint32>(value);
		}
	}
	return std::nullopt;
}

/**
 * The System of a database of RPG Maker (RPG_RT.ldb): the chunks before it
 * are skipped, not read, and its own chunks other than those of System
 */
std::optional<System> readLdbSystem(const QString& path) {
	using lcf::LDB_Reader::ChunkDatabase;
	using lcf::LDB_Reader::ChunkSystem;
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly)) {
		return std::nullopt;
	}
	// its header, after its length
	const auto length = readNumber(file);
	if (!length || *length != 11 || file.read(11) != "LcfDataBase") {
		return std::nullopt;
	}
	// the chunks of the database: their number, their size, their data
	while (true) {
		const auto id = readNumber(file);
		const auto size = id && *id != 0 ? readNumber(file) : std::nullopt;
		if (!size || *size < 0) {
			return std::nullopt;
		}
		if (*id != ChunkDatabase::system) {
			if (!file.seek(file.pos() + *size)) {
				return std::nullopt;
			}
			continue;
		}
		QByteArray data = file.read(*size);
		QBuffer chunks(&data);
		chunks.open(QIODevice::ReadOnly);
		System system;
		while (true) {
			const auto field = readNumber(chunks);
			const auto field_size = field && *field != 0 ? readNumber(chunks) : std::nullopt;
			if (!field_size || *field_size < 0) {
				break;
			}
			const qint64 next = chunks.pos() + *field_size;
			const auto value = *field_size > 0 ? readNumber(chunks) : std::nullopt;
			if (value) {
				switch (*field) {
				case ChunkSystem::ldb_id:
					system.ldbId = *value;
					break;
				case ChunkSystem::easyrpg_use_rpg2k_battle_system:
					system.battle2k = *value != 0;
					break;
				case ChunkSystem::easyrpg_alternative_exp:
					system.alternativeExp = *value;
					break;
				default:
					break;
				}
			}
			if (!chunks.seek(next)) {
				break;
			}
		}
		return system;
	}
}

/** The System of a database of EasyRPG (EASY_RT.edb, XML) */
std::optional<System> readXmlSystem(const QString& path) {
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly)) {
		return std::nullopt;
	}
	const QString text = QString::fromUtf8(file.readAll());
	auto value = [&text](const char* name) {
		const QRegularExpression element(QStringLiteral("<%1>([^<]*)</%1>").arg(QLatin1String(name)));
		return element.match(text).captured(1).trimmed();
	};
	System system;
	system.ldbId = value("ldb_id").toInt();
	system.battle2k = value("easyrpg_use_rpg2k_battle_system") == QLatin1String("T");
	system.alternativeExp = value("easyrpg_alternative_exp").toInt();
	return system;
}

} // namespace

GameProfile& GameProfile::instance()
{
	static GameProfile profile;
	return profile;
}

GameProfile::GameProfile()
{
	connect(&GameSetup::instance(), &GameSetup::changed, this, &GameProfile::chipsChanged);
}

QVariantList GameProfile::chips() const
{
	return chipsOf(GameSetup::instance().values());
}

QString GameProfile::text() const
{
	return textOf(chips());
}

QVariantMap GameProfile::read(const Project& project)
{
	const std::optional<System> system = project.projectType() == FileFinder::ProjectType::Legacy
		? readLdbSystem(project.databaseFile()) : readXmlSystem(project.databaseFile());
	if (!system) {
		return {};
	}
	// the patches as GamePatches::read tells them; the program, read here
	// when they are detected (GamePatches keeps what it reads for the
	// Editor's own thread)
	const int engine_set = GamePatches::engineSet(project);
	QMap<QString, int> patches;
	if (!IniFile::read(project.findFileOrDefault(QStringLiteral("EasyRPG.ini")), "Patch").isEmpty()) {
		patches = GamePatches::read(project).values;
	} else {
		patches = GamePatches::detect(project, true);
		const QString exe = GamePatches::engineExe(project);
		if (engine_set == RpgRtExe::EngineNone && !exe.isEmpty()) {
			if (const int version = RpgRtExe::read(exe).maniac) {
				patches[QStringLiteral("Maniac")] = version;
			}
		}
	}
	// as GameSetup reads the game open
	const bool rpg2k3 = system->ldbId == 2003;
	const bool commands2k3 = rpg2k3 || patches.value(QStringLiteral("RPG2k3Commands")) > 0;
	const bool maniac = patches.value(QStringLiteral("Maniac")) > 0;
	const bool easyrpg = patches.value(QStringLiteral("EasyRPG")) > 0;
	const bool battle2k = !rpg2k3 || system->battle2k;
	const QString engine = RpgRtExe::settingOf(engine_set);
	const QStringList only = GameSetup::playerOnlyOf(rpg2k3, commands2k3, battle2k, system->alternativeExp, easyrpg,
		!engine.isEmpty());
	QVariantMap values;
	for (auto it = patches.cbegin(); it != patches.cend(); ++it) {
		values.insert(it.key(), it.value());
	}
	return {
		{QStringLiteral("rpg2k3"), rpg2k3},
		{QStringLiteral("engine"), rpg2k3 ? 2003 : 2000},
		{QStringLiteral("commands2k3"), commands2k3},
		{QStringLiteral("maniac"), maniac},
		{QStringLiteral("easyrpg"), easyrpg},
		{QStringLiteral("battle2k"), battle2k},
		{QStringLiteral("alternativeExp"), system->alternativeExp},
		{QStringLiteral("playerEngine"), engine.isEmpty() ? QStringLiteral("detect") : engine},
		{QStringLiteral("playerOnly"), only},
		{QStringLiteral("patches"), values},
		{QStringLiteral("runsWith"), GameSetup::runsWithOf(rpg2k3, maniac, only)},
	};
}

QVariantList GameProfile::chipsOf(const QVariantMap& setup)
{
	if (setup.isEmpty() || !setup.value(QStringLiteral("open"), true).toBool()) {
		return {};
	}
	const bool rpg2k3 = setup.value(QStringLiteral("rpg2k3")).toBool();
	const QStringList only = setup.value(QStringLiteral("playerOnly")).toStringList();
	const QVariantMap patches = setup.value(QStringLiteral("patches")).toMap();
	// what runs the game, under the line of each chip
	QStringList programs;
	for (const QString& key : setup.value(QStringLiteral("runsWith")).toStringList()) {
		programs << (key == QLatin1String("rpgrt") ? tr("RPG_RT.exe")
			: key == QLatin1String("maniac") ? tr("the RPG_RT.exe of the Maniac Patch") : tr("EasyRPG Player"));
	}
	const QString runs = tr("Runs with: %1").arg(programs.join(QStringLiteral(", ")));
	QVariantList chips;
	auto chip = [&](const QString& key, const QString& text, const char* tag, const QString& tip, const char* page) {
		chips << QVariantMap{
			{QStringLiteral("key"), key},
			{QStringLiteral("text"), text},
			{QStringLiteral("tag"), QString::fromLatin1(tag)},
			{QStringLiteral("tip"), tip + u'\n' + runs},
			{QStringLiteral("page"), QString::fromLatin1(page)},
		};
	};
	const QString format = rpg2k3 ? tr("RPG Maker 2003") : tr("RPG Maker 2000");
	chip(QStringLiteral("engine"), format, "", tr("The format of the game: %1").arg(format), "engine");
	if (setup.value(QStringLiteral("maniac")).toBool()) {
		chip(QStringLiteral("maniac"), tr("Maniac Patch"), "M", tr("Maniac Patch: runs with its RPG_RT.exe and with EasyRPG Player"),
			"engine");
	}
	if (setup.value(QStringLiteral("easyrpg")).toBool()) {
		chip(QStringLiteral("easyrpg"), tr("EasyRPG Extensions"), "E",
			tr("EasyRPG Extensions: the commands of EasyRPG Player, true color images...; EasyRPG Player only"), "player");
	}
	if (!rpg2k3 && setup.value(QStringLiteral("commands2k3")).toBool()) {
		chip(QStringLiteral("commands2k3"), tr("RPG Maker 2003 commands"), "E",
			tr("RPG Maker 2003's commands and options in this RPG Maker 2000 game: EasyRPG Player only"), "player");
	}
	if (only.contains(QStringLiteral("battle2k"))) {
		chip(QStringLiteral("battle2k"), tr("RPG Maker 2000 battles"), "E",
			tr("The battle system of RPG Maker 2000 in this RPG Maker 2003 game: EasyRPG Player only"), "player");
	}
	if (only.contains(QStringLiteral("alternativeExp"))) {
		// the Player takes any other value for RPG Maker 2003's
		const bool exp2000 = setup.value(QStringLiteral("alternativeExp")).toInt() == 1;
		chip(QStringLiteral("alternativeExp"), exp2000 ? tr("RPG Maker 2000 experience") : tr("RPG Maker 2003 experience"), "E",
			exp2000 ? tr("The experience curve of RPG Maker 2000: EasyRPG Player only")
				: tr("The experience curve of RPG Maker 2003: EasyRPG Player only"), "player");
	}
	if (only.contains(QStringLiteral("engine"))) {
		const QString name = RpgRtExe::engineName(RpgRtExe::engineFromSetting(setup.value(QStringLiteral("playerEngine")).toString()));
		chip(QStringLiteral("playerEngine"), tr("Played as %1").arg(name), "E",
			tr("EasyRPG Player runs the game as %1 ([Game] Engine of EasyRPG.ini): EasyRPG Player only").arg(name), "player");
	}
	// the patches of RPG_RT.exe that EasyRPG Player runs too
	for (const auto& patch : GamePatches::list()) {
		const QString key = QString::fromLatin1(patch.key);
		if (key == QLatin1String("Maniac") || key == QLatin1String("EasyRPG") || key == QLatin1String("RPG2k3Commands")
				|| patches.value(key).toInt() <= 0) {
			continue;
		}
		const QString name = QCoreApplication::translate("GamePatches", patch.name);
		chip(key, name, "P", key == QLatin1String("DynRPG") ? tr("DynRPG: EasyRPG Player runs only some of its plugins")
			: tr("%1: a patch of RPG_RT.exe, which EasyRPG Player runs too").arg(name), "player");
	}
	return chips;
}

QString GameProfile::textOf(const QVariantList& chips)
{
	QStringList texts;
	for (const QVariant& chip : chips) {
		texts << chip.toMap().value(QStringLiteral("text")).toString();
	}
	return texts.join(QStringLiteral(", "));
}
