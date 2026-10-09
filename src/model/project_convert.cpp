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

#include "project_convert.h"
#include "project.h"
#include "defines.h"
#include "common/dbstring.h"
#include "common/filefinder.h"
#include <lcf/ldb/reader.h>
#include <lcf/lmt/reader.h>
#include <lcf/lmu/reader.h>
#include <lcf/reader_lcf.h>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <lcf/saveopt.h>
#include <filesystem>
#include <iterator>
#include <sstream>
#include <system_error>

namespace {

/** A file of the project written again by the conversion */
struct Rewrite {
	QString target;
	QByteArray data;
	QString temp;
	QString backup;
	bool replaced = false;
};

/** Replaces `to` by `from` in one step (on Windows too, unlike QFile::rename) */
bool replaceFile(const QString& from, const QString& to) {
	std::error_code ec;
	std::filesystem::rename(std::filesystem::path(from.toStdU16String()),
		std::filesystem::path(to.toStdU16String()), ec);
	return !ec;
}

bool writeFile(const QString& path, const QByteArray& data) {
	QFile file(path);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		return false;
	}
	const bool written = file.write(data) == data.size() && file.flush();
	file.close();
	return written && file.error() == QFileDevice::NoError;
}

QByteArray toBytes(const std::ostringstream& out) {
	return QByteArray::fromStdString(out.str());
}

QString lcfError() {
	const QString error = QString::fromStdString(lcf::LcfReader::GetError());
	return error.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(error);
}

}

QString ProjectConvert::backupFolderName() {
	return QStringLiteral("Backup (RPG Maker 2000)");
}

bool ProjectConvert::canConvert(const Project& project) {
	// EasyRPG projects have no engine of their own: only RPG_RT.ldb is converted
	return project.projectType() == FileFinder::ProjectType::Legacy
		&& project.database().system.ldb_id != 2003;
}

std::vector<lcf::rpg::BattlerAnimation> ProjectConvert::defaultBattlerAnimations() {
	// The battlers of RPG Maker 2003's database: their name and the graphics
	// of the RTP (BattleCharSet/<file> A and B). The English RTP names: the
	// EasyRPG Player finds the files of the other RTPs from them.
	static const struct {
		const char* name;
		const char* file;
	} battlers[] = {
		{QT_TR_NOOP("Male Hero"), "Male Hero"},
		{QT_TR_NOOP("Female Hero"), "Female Hero"},
		{QT_TR_NOOP("Male Soldier"), "Male Warrior"},
		{QT_TR_NOOP("Female Soldier"), "Female Warrior"},
		{QT_TR_NOOP("Male Mage"), "Male Mage"},
		{QT_TR_NOOP("Female Mage"), "Female Mage"},
		{QT_TR_NOOP("Priest"), "Male Monk"},
		{QT_TR_NOOP("Priestess"), "Female Monk"},
		{QT_TR_NOOP("Male Monk"), "Male Fighter"},
		{QT_TR_NOOP("Female Monk"), "Female Fighter"},
		{QT_TR_NOOP("Male Thief"), "Male Bandit"},
		{QT_TR_NOOP("Female Thief"), "Female Bandit"},
		{QT_TR_NOOP("Male Pirate"), "Male Pirate"},
		{QT_TR_NOOP("Female Pirate"), "Female Pirate"},
		{QT_TR_NOOP("Male Elf"), "Male Elf"},
		{QT_TR_NOOP("Female Elf"), "Female Elf"},
		// the file name of the RTP has this typo
		{QT_TR_NOOP("Heavy Armor"), "Armored Warror"},
		{QT_TR_NOOP("Samurai"), "Samurai"},
		{QT_TR_NOOP("Male Ninja"), "Male Ninja"},
		{QT_TR_NOOP("Female Ninja"), "Female Ninja"},
		{QT_TR_NOOP("Normal Man"), "Man1"},
		{QT_TR_NOOP("Normal Woman"), "Woman1"},
		{QT_TR_NOOP("Chinese Man"), "Chinese Man"},
		{QT_TR_NOOP("Chinese Woman"), "Chinese Woman"},
		{QT_TR_NOOP("Normal Man 2"), "Man2"},
		{QT_TR_NOOP("Normal Man 3"), "Man3"},
		{QT_TR_NOOP("Normal Man 4"), "Man4"},
		{QT_TR_NOOP("Normal Man 5"), "Man5"},
		{QT_TR_NOOP("Normal Woman 2"), "Woman2"},
		{QT_TR_NOOP("Normal Woman 3"), "Woman3"},
		{QT_TR_NOOP("Normal Woman 4"), "Woman4"},
		{QT_TR_NOOP("Normal Woman 5"), "Woman5"},
	};
	// The 12 poses of RPG Maker 2003 (its names) and their cell: sheet A or B, row
	static const struct {
		const char* name;
		char sheet;
		int index;
	} poses[] = {
		{QT_TR_NOOP("Idle Stance"), 'B', 2},
		{QT_TR_NOOP("Right Hand"), 'B', 1},
		{QT_TR_NOOP("Left Hand"), 'B', 0},
		{QT_TR_NOOP("Skill Use"), 'A', 0},
		{QT_TR_NOOP("Dead"), 'A', 3},
		{QT_TR_NOOP("Damage"), 'A', 2},
		{QT_TR_NOOP("Bad Status"), 'A', 4},
		{QT_TR_NOOP("Defending"), 'A', 1},
		{QT_TR_NOOP("Walking Left"), 'A', 6},
		{QT_TR_NOOP("Walking Right"), 'A', 7},
		{QT_TR_NOOP("Victory"), 'A', 5},
		{QT_TR_NOOP("Item"), 'A', 0},
	};
	// The weapons of BattleWeapon/Weapon, one per row
	static const char* const weapons[] = {
		QT_TR_NOOP("Sword"), QT_TR_NOOP("Spear"), QT_TR_NOOP("Bow"), QT_TR_NOOP("Whip"),
		QT_TR_NOOP("Staff"), QT_TR_NOOP("Axe"), QT_TR_NOOP("Boomerang"), QT_TR_NOOP("Throwing Knife"),
	};
	// RPG Maker 2003 writes 32 poses and 32 weapons, the unnamed ones too
	constexpr int kSlots = 32;

	std::vector<lcf::rpg::BattlerAnimation> list;
	int id = 1;
	for (const auto& battler : battlers) {
		lcf::rpg::BattlerAnimation animation;
		animation.ID = id++;
		animation.name = ToDBString(tr(battler.name));
		animation.speed = lcf::rpg::BattlerAnimation::Speed_slow;
		for (int i = 0; i < kSlots; ++i) {
			lcf::rpg::BattlerAnimationPose pose;
			pose.ID = i + 1;
			if (i < static_cast<int>(std::size(poses))) {
				pose.name = ToDBString(tr(poses[i].name));
				pose.battler_name = ToDBString(QStringLiteral("%1 %2").arg(QLatin1String(battler.file)).arg(QLatin1Char(poses[i].sheet)));
				pose.battler_index = poses[i].index;
			}
			animation.poses.push_back(std::move(pose));

			lcf::rpg::BattlerAnimationWeapon weapon;
			weapon.ID = i + 1;
			weapon.weapon_name = lcf::DBString("Weapon");
			if (i < static_cast<int>(std::size(weapons))) {
				weapon.name = ToDBString(tr(weapons[i]));
				weapon.weapon_index = i;
			}
			animation.weapons.push_back(std::move(weapon));
		}
		list.push_back(std::move(animation));
	}
	return list;
}

void ProjectConvert::convertDatabase(lcf::rpg::Database& db, const Options& options, const QString& system2_name) {
	auto& system = db.system;
	// What the engines (RPG_RT, liblcf, the EasyRPG Player) read as RPG Maker 2003
	system.ldb_id = 2003;

	// Battle commands: those of RPG Maker 2000's battles, named with its terms,
	// given to every hero (the commands, the Row command 0, padded with -1)
	if (db.battlecommands.commands.empty()) {
		const auto& terms = db.terms;
		const struct {
			const lcf::DBString& term;
			const char* fallback;
			int type;
		} commands[] = {
			{terms.command_attack, QT_TR_NOOP("Fight"), lcf::rpg::BattleCommand::Type_attack},
			{terms.command_skill, QT_TR_NOOP("Skill"), lcf::rpg::BattleCommand::Type_skill},
			{terms.command_defend, QT_TR_NOOP("Defend"), lcf::rpg::BattleCommand::Type_defense},
			{terms.command_item, QT_TR_NOOP("Item"), lcf::rpg::BattleCommand::Type_item},
			{terms.battle_escape, QT_TR_NOOP("Escape"), lcf::rpg::BattleCommand::Type_escape},
		};
		std::vector<int32_t> hero_commands;
		for (const auto& entry : commands) {
			lcf::rpg::BattleCommand command;
			command.ID = static_cast<int>(db.battlecommands.commands.size()) + 1;
			command.name = entry.term.empty() ? ToDBString(tr(entry.fallback)) : entry.term;
			command.type = entry.type;
			db.battlecommands.commands.push_back(std::move(command));
			hero_commands.push_back(db.battlecommands.commands.back().ID);
		}
		hero_commands.push_back(0);
		hero_commands.resize(7, -1);
		for (auto& actor : db.actors) {
			if (actor.battle_commands.empty()) {
				actor.battle_commands = hero_commands;
			}
		}
	}

	// Battler animations: the side view battle shows the heroes with them
	if (db.battleranimations.empty()) {
		db.battleranimations = defaultBattlerAnimations();
	}
	for (auto& actor : db.actors) {
		if (actor.battler_animation < 1 || actor.battler_animation > static_cast<int>(db.battleranimations.size())) {
			actor.battler_animation = 1;
		}
	}

	// A dead hero lies down (the Dead pose), the other conditions keep the
	// default (100: the Bad Status pose), like RPG Maker 2003's database
	if (!db.states.empty() && db.states[0].battler_animation_id == 100) {
		db.states[0].battler_animation_id = lcf::rpg::BattlerAnimation::Pose_Dead;
	}

	// The menu of RPG Maker 2000: Item, Skill, Equipment, Save (Quit is always there)
	if (system.menu_commands.empty() || system.menu_commands == std::vector<int16_t>{1}) {
		system.menu_commands = {1, 2, 3, 4};
	}

	// The cursors and gauges of the battle
	if (system.system2_name.empty()) {
		system.system2_name = ToDBString(system2_name);
	}

	// The terms that RPG Maker 2000 does not have
	auto fill = [](lcf::DBString& term, const char* text) {
		if (term.empty()) {
			term = ToDBString(tr(text));
		}
	};
	fill(db.terms.miss, QT_TR_NOOP("Miss!"));
	fill(db.terms.status, QT_TR_NOOP("Status"));
	fill(db.terms.row, QT_TR_NOOP("Row"));
	fill(db.terms.order, QT_TR_NOOP("Order"));
	fill(db.terms.wait_on, QT_TR_NOOP("Wait"));
	fill(db.terms.wait_off, QT_TR_NOOP("Active"));

	// EasyRPG Player extensions, ignored by RPG_RT
	if (options.keep2kBattleSystem) {
		system.easyrpg_use_rpg2k_battle_system = true;
		system.easyrpg_use_rpg2k_battle_commands = true;
		// the battle messages of RPG Maker 2000 v1.50+ (English) have placeholders (%S...)
		system.easyrpg_battle_use_rpg2ke_strings = db.version >= 1;
	}
	if (options.keep2kExperience) {
		// 1: the curve of RPG Maker 2000
		system.easyrpg_alternative_exp = 1;
	}
}

QString ProjectConvert::to2003(Project& project, const Options& options, QString* backup_path) {
	if (project.projectType() != FileFinder::ProjectType::Legacy) {
		return tr("Only the projects of RPG Maker (RPG_RT.ldb) can be converted.");
	}
	if (project.database().system.ldb_id == 2003) {
		return tr("The project is already an RPG Maker 2003 project.");
	}

	const QDir dir = project.projectDir();
	const std::string encoding = project.encoding().toStdString();
	// the files of the project folder, by lower case name (names are case-insensitive)
	QHash<QString, QString> files;
	for (const QString& name : dir.entryList(QDir::Files)) {
		files.insert(name.toLower(), dir.filePath(name));
	}
	auto find = [&](const QString& name) {
		return files.value(name.toLower());
	};

	const QString db_file = find(QStringLiteral(RM_DB));
	const QString mt_file = find(QStringLiteral(RM_MT));
	if (db_file.isEmpty() || mt_file.isEmpty()) {
		return tr("RPG_RT.ldb or RPG_RT.lmt is missing in %1.").arg(QDir::toNativeSeparators(dir.absolutePath()));
	}

	// 1. Everything converted in memory: nothing is written when a file cannot be read
	lcf::rpg::Database db = project.database();
	QString system2 = QStringLiteral("System2A");
	{
		const QString system2_dir = FileFinder::Find(dir, QStringLiteral(SYSTEM2), FileFinder::FileType::Default, QDir::Dirs);
		if (!system2_dir.isEmpty()) {
			const QStringList images = QDir(system2_dir).entryList(
				{QStringLiteral("*.png"), QStringLiteral("*.bmp"), QStringLiteral("*.xyz")}, QDir::Files, QDir::Name | QDir::IgnoreCase);
			if (!images.isEmpty()) {
				system2 = QFileInfo(images.first()).completeBaseName();
			}
		}
	}
	convertDatabase(db, options, system2);
	const auto engine = lcf::GetEngineVersion(db);

	std::vector<Rewrite> rewrites;
	for (const auto& info : project.treeMap().maps) {
		if (info.ID <= 0 || info.type != lcf::rpg::TreeMap::MapType_map) {
			continue;
		}
		const QString name = QStringLiteral("Map%1.lmu").arg(info.ID, 4, 10, QLatin1Char('0'));
		const QString path = find(name);
		if (path.isEmpty()) {
			// a map of the tree without a file: nothing to convert
			continue;
		}
		QFile file(path);
		if (!file.open(QIODevice::ReadOnly)) {
			return tr("%1 cannot be read.").arg(name);
		}
		std::istringstream in(file.readAll().toStdString());
		auto map = lcf::LMU_Reader::Load(in, encoding);
		if (!map) {
			return tr("%1 cannot be read%2.").arg(name, lcfError());
		}
		std::ostringstream out;
		if (!lcf::LMU_Reader::Save(out, *map, engine, encoding)) {
			return tr("%1 cannot be converted%2.").arg(name, lcfError());
		}
		rewrites.push_back({path, toBytes(out)});
	}
	{
		std::ostringstream out;
		if (!lcf::LMT_Reader::Save(out, project.treeMap(), engine, encoding)) {
			return tr("The map tree cannot be converted%1.").arg(lcfError());
		}
		rewrites.push_back({mt_file, toBytes(out)});
	}
	{
		// the database last: until it is replaced, the project is the one of RPG Maker 2000
		std::ostringstream out;
		if (!lcf::LDB_Reader::Save(out, db, encoding)) {
			return tr("The database cannot be converted%1.").arg(lcfError());
		}
		rewrites.push_back({db_file, toBytes(out)});
	}

	// 2. The backup: a new folder with a copy of each file rewritten
	QString backup_name = backupFolderName();
	for (int n = 2; dir.exists(backup_name); ++n) {
		backup_name = QStringLiteral("%1 %2").arg(backupFolderName()).arg(n);
	}
	const QString backup_dir = dir.filePath(backup_name);
	if (!dir.mkdir(backup_name)) {
		return tr("The backup folder %1 cannot be created.").arg(QDir::toNativeSeparators(backup_dir));
	}
	auto remove_backup = [&]() {
		QDir(backup_dir).removeRecursively();
	};
	for (auto& rewrite : rewrites) {
		const QString name = QFileInfo(rewrite.target).fileName();
		rewrite.backup = QDir(backup_dir).filePath(name);
		if (!QFile::copy(rewrite.target, rewrite.backup)) {
			remove_backup();
			return tr("%1 cannot be copied to the backup folder.").arg(name);
		}
	}

	// 3. The new files, next to the old ones
	auto remove_temps = [&]() {
		for (const auto& rewrite : rewrites) {
			if (!rewrite.temp.isEmpty() && !rewrite.replaced) {
				QFile::remove(rewrite.temp);
			}
		}
	};
	for (auto& rewrite : rewrites) {
		rewrite.temp = rewrite.target + QStringLiteral(".convert2003");
		if (!writeFile(rewrite.temp, rewrite.data)) {
			const QString name = QFileInfo(rewrite.target).fileName();
			remove_temps();
			remove_backup();
			return tr("The converted %1 cannot be written.").arg(name);
		}
	}

	// 4. The new files replace the old ones. RPG_RT.exe, the runtime of
	// RPG Maker 2000, cannot run the game anymore: it goes to the backup
	// (the EasyRPG Player would read the engine from it and run the game as
	// RPG Maker 2000).
	const QString exe_file = find(QStringLiteral("RPG_RT.exe"));
	const QString exe_backup = exe_file.isEmpty() ? QString() : QDir(backup_dir).filePath(QFileInfo(exe_file).fileName());
	bool exe_moved = false;
	auto rollback = [&]() {
		bool restored = true;
		for (auto& rewrite : rewrites) {
			if (!rewrite.replaced) {
				continue;
			}
			const QString temp = rewrite.target + QStringLiteral(".restore2000");
			QFile::remove(temp);
			restored = QFile::copy(rewrite.backup, temp) && replaceFile(temp, rewrite.target) && restored;
		}
		if (exe_moved) {
			restored = QFile::rename(exe_backup, exe_file) && restored;
		}
		remove_temps();
		return restored;
	};
	for (size_t i = 0; i < rewrites.size(); ++i) {
		auto& rewrite = rewrites[i];
		if (i + 1 == rewrites.size() && !exe_file.isEmpty()) {
			exe_moved = QFile::rename(exe_file, exe_backup);
			if (!exe_moved) {
				const bool restored = rollback();
				if (restored) {
					remove_backup();
				}
				return tr("RPG_RT.exe cannot be moved to the backup folder.")
					+ (restored ? QString() : QStringLiteral("\n") + tr("Some files could not be restored: the files of RPG Maker 2000 are in %1.").arg(QDir::toNativeSeparators(backup_dir)));
			}
		}
		if (!replaceFile(rewrite.temp, rewrite.target)) {
			const QString name = QFileInfo(rewrite.target).fileName();
			const bool restored = rollback();
			if (restored) {
				remove_backup();
			}
			return tr("%1 cannot be replaced.").arg(name)
				+ (restored ? QString() : QStringLiteral("\n") + tr("Some files could not be restored: the files of RPG Maker 2000 are in %1.").arg(QDir::toNativeSeparators(backup_dir)));
		}
		rewrite.replaced = true;
	}

	project.database() = std::move(db);
	if (backup_path) {
		*backup_path = backup_dir;
	}
	return {};
}
