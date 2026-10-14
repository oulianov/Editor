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

#include "troop_layout.h"
#include "common/dbstring.h"
#include <QStringList>
#include <algorithm>
#include <utility>

namespace {
bool validMember(const lcf::rpg::Troop& troop, int index) {
	return index >= 0 && index < static_cast<int>(troop.members.size());
}
}

QPoint TroopLayout::clamp(const QPointF& pos) {
	return {std::clamp(qRound(pos.x()), 0, ScreenWidth), std::clamp(qRound(pos.y()), 0, ScreenHeight)};
}

void TroopLayout::moveMember(lcf::rpg::Troop& troop, int index, const QPointF& pos) {
	if (!validMember(troop, index)) {
		return;
	}
	const QPoint at = clamp(pos);
	auto& member = troop.members[static_cast<size_t>(index)];
	member.x = at.x();
	member.y = at.y();
}

int TroopLayout::addMember(lcf::rpg::Troop& troop, int enemy_id) {
	auto& members = troop.members;
	if (static_cast<int>(members.size()) >= MaxMembers) {
		return -1;
	}
	lcf::rpg::TroopMember member;
	member.ID = static_cast<int>(members.size()) + 1;
	member.enemy_id = enemy_id;
	member.x = ScreenWidth / 2;
	member.y = ScreenHeight / 2;
	members.push_back(member);
	return member.ID - 1;
}

bool TroopLayout::removeMember(lcf::rpg::Troop& troop, int index) {
	auto& members = troop.members;
	if (members.empty()) {
		return false;
	}
	if (!validMember(troop, index)) {
		index = static_cast<int>(members.size()) - 1;
	}
	members.erase(members.begin() + index);
	for (size_t i = 0; i < members.size(); ++i) {
		members[i].ID = static_cast<int>(i) + 1;
	}
	return true;
}

void TroopLayout::removeAllMembers(lcf::rpg::Troop& troop) {
	troop.members.clear();
}

void TroopLayout::autoAlign(lcf::rpg::Troop& troop) {
	auto& members = troop.members;
	const int count = static_cast<int>(members.size());
	const int front = count > 4 ? (count + 1) / 2 : count;
	for (int i = 0; i < count; ++i) {
		const bool back = i >= front;
		const int in_row = back ? count - front : front;
		const int pos = back ? i - front : i;
		auto& member = members[static_cast<size_t>(i)];
		member.x = ScreenWidth * (2 * pos + 1 + (back ? 1 : 0)) / (2 * in_row + 1);
		member.y = count > 4 ? (back ? 150 : 100) : 120;
	}
}

void TroopLayout::setInvisible(lcf::rpg::Troop& troop, int index, bool invisible) {
	if (validMember(troop, index)) {
		troop.members[static_cast<size_t>(index)].invisible = invisible;
	}
}

qreal TroopLayout::opacity(const lcf::rpg::TroopMember& member, bool transparent) {
	return member.invisible ? 0.35 : (transparent ? 0.6 : 1.0);
}

QString TroopLayout::autoName(const lcf::rpg::Troop& troop, const std::vector<lcf::rpg::Enemy>& enemies) {
	std::vector<std::pair<int, int>> counts;
	for (const auto& member : troop.members) {
		auto it = std::find_if(counts.begin(), counts.end(), [&](const auto& c) { return c.first == member.enemy_id; });
		if (it == counts.end()) {
			counts.emplace_back(member.enemy_id, 1);
		} else {
			++it->second;
		}
	}

	QStringList parts;
	for (const auto& [id, count] : counts) {
		QString name = id >= 1 && id <= static_cast<int>(enemies.size()) ? ToQString(enemies[static_cast<size_t>(id) - 1].name) : QString();
		parts << (count > 1 ? QStringLiteral("%1*%2").arg(name).arg(count) : name);
	}
	return parts.join(QStringLiteral(", "));
}

bool TroopLayout::terrainAllowed(const lcf::rpg::Troop& troop, int index) {
	return index >= static_cast<int>(troop.terrain_set.size()) || troop.terrain_set[static_cast<size_t>(index)];
}

void TroopLayout::setTerrainAllowed(lcf::rpg::Troop& troop, int index, bool allowed, int terrain_count) {
	if (index < 0) {
		return;
	}
	auto& set = troop.terrain_set;
	if (index >= static_cast<int>(set.size())) {
		// the terrains the set did not list were allowed
		lcf::DBBitArray grown(static_cast<size_t>(std::max(terrain_count, index + 1)), true);
		for (size_t i = 0; i < set.size(); ++i) {
			// the bit, not the proxy: assigning a proxy only rebinds it
			grown[i] = static_cast<bool>(set[i]);
		}
		set = std::move(grown);
	}
	set[static_cast<size_t>(index)] = allowed;
}
