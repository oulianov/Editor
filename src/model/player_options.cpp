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

#include "player_options.h"
#include "common/ini_file.h"
#include "model/project.h"

#include <QCoreApplication>
#include <algorithm>
#include <lcf/rpg/database.h>
#include <optional>
#include <vector>

namespace {

/** The section of EasyRPG.ini the Player does not read, and its keys */
constexpr const char* section = "EasyRPG Editor";
constexpr const char* battle_key = "UseRpg2kBattleSystem";
constexpr const char* exp_key = "AlternativeExp";

QString iniPath(const Project& project) {
	return project.findFileOrDefault(QStringLiteral("EasyRPG.ini"));
}

} // namespace

PlayerOptions::Values PlayerOptions::of(const lcf::rpg::Database& database) {
	return {database.system.easyrpg_use_rpg2k_battle_system, database.system.easyrpg_alternative_exp};
}

void PlayerOptions::setBattle2k(lcf::rpg::Database& database, bool on) {
	// as ProjectConvert::convertDatabase keeps them
	auto& system = database.system;
	system.easyrpg_use_rpg2k_battle_system = on;
	system.easyrpg_use_rpg2k_battle_commands = on;
	// the battle messages of RPG Maker 2000 v1.50+ (English) have placeholders (%S...)
	system.easyrpg_battle_use_rpg2ke_strings = on && database.version >= 1;
}

void PlayerOptions::set(lcf::rpg::Database& database, const Values& values, int options) {
	if (options & Battle2k) {
		setBattle2k(database, values.battle2k);
	}
	if (options & AlternativeExp) {
		database.system.easyrpg_alternative_exp = values.alternativeExp;
	}
}

PlayerOptions::Kept PlayerOptions::kept(const Project& project) {
	Kept kept;
	for (const auto& [key, value] : IniFile::read(iniPath(project), section)) {
		bool ok = false;
		const int number = value.trimmed().toInt(&ok);
		if (!ok) {
			continue;
		}
		if (key.trimmed().compare(battle_key, Qt::CaseInsensitive) == 0) {
			kept.values.battle2k = number != 0;
			kept.options |= Battle2k;
		} else if (key.trimmed().compare(exp_key, Qt::CaseInsensitive) == 0) {
			kept.values.alternativeExp = number;
			kept.options |= AlternativeExp;
		}
	}
	return kept;
}

bool PlayerOptions::keep(const Project& project, const Values& values, int options) {
	std::vector<IniFile::Change> changes;
	if (options & Battle2k) {
		changes.push_back({battle_key, values.battle2k ? std::optional(QByteArray("1")) : std::nullopt});
	}
	if (options & AlternativeExp) {
		changes.push_back({exp_key, values.alternativeExp != 0 ? std::optional(QByteArray::number(values.alternativeExp)) : std::nullopt});
	}
	if (changes.empty()) {
		return true;
	}
	// nothing to remove from a file that has none: no file made for it
	const bool any = std::any_of(changes.begin(), changes.end(), [](const IniFile::Change& change) { return change.second.has_value(); });
	if (!any && kept(project).options == 0) {
		return true;
	}
	return IniFile::write(iniPath(project), section, changes, false, false);
}

int PlayerOptions::dropped(const Project& project) {
	const Kept copy = kept(project);
	const auto& database = project.database();
	const Values now = of(database);
	int lost = 0;
	// the battles of RPG Maker 2000 mean something in an RPG Maker 2003 game only
	if ((copy.options & Battle2k) && copy.values.battle2k && database.system.ldb_id == 2003 && !now.battle2k) {
		lost |= Battle2k;
	}
	if ((copy.options & AlternativeExp) && copy.values.alternativeExp != 0 && now.alternativeExp == 0) {
		lost |= AlternativeExp;
	}
	return lost;
}

QStringList PlayerOptions::describe(const Values& values, int options) {
	QStringList names;
	if (options & Battle2k) {
		names << QCoreApplication::translate("MainWindow", "the battle system of RPG Maker 2000");
	}
	if (options & AlternativeExp) {
		names << (values.alternativeExp == 1 ? QCoreApplication::translate("MainWindow", "the experience curve of RPG Maker 2000")
			: QCoreApplication::translate("MainWindow", "the experience curve of RPG Maker 2003"));
	}
	return names;
}
