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

#include "test_battle.h"
#include <algorithm>

namespace {
const lcf::rpg::Item* findItem(const lcf::rpg::Database& database, int id) {
	return id >= 1 && id <= static_cast<int>(database.items.size()) ? &database.items[static_cast<size_t>(id) - 1] : nullptr;
}

/** A parameter of the hero's curve at a level */
int curveValue(const std::vector<int16_t>& curve, int level) {
	return level >= 1 && level <= static_cast<int>(curve.size()) ? curve[static_cast<size_t>(level) - 1] : 0;
}
}

TestBattleParty::TestBattleParty(lcf::rpg::Database& database) :
	m_database(database)
{
	load();
}

void TestBattleParty::load() {
	const auto& data = m_database.system.battletest_data;
	for (int i = 0; i < MaxMembers; ++i) {
		auto& battler = m_battlers[static_cast<size_t>(i)];
		if (i < static_cast<int>(data.size())) {
			battler = data[static_cast<size_t>(i)];
			// RPG Maker leaves garbage in the upper bits (see Game_Party::SetupBattleTest)
			for (int slot = 0; slot < SlotCount; ++slot) {
				slotData(battler, slot) &= 0xFFFF;
			}
			setLevel(i, battler.level);
		} else {
			battler = lcf::rpg::TestBattler();
			battler.actor_id = std::min(i + 1, static_cast<int>(m_database.actors.size()));
			initialStats(i);
		}
	}
	m_size = std::min(static_cast<int>(data.size()), MaxMembers);
}

void TestBattleParty::save() {
	auto& data = m_database.system.battletest_data;
	data.clear();
	for (int i = 0; i < m_size; ++i) {
		if (actor(i)) {
			data.push_back(m_battlers[static_cast<size_t>(i)]);
			data.back().ID = static_cast<int>(data.size());
		}
	}
}

int TestBattleParty::size() const {
	return m_size;
}

void TestBattleParty::setSize(int size) {
	m_size = std::clamp(size, 0, MaxMembers);
}

const lcf::rpg::TestBattler& TestBattleParty::member(int index) const {
	return m_battlers[static_cast<size_t>(index)];
}

void TestBattleParty::setHero(int index, int actor_id) {
	m_battlers[static_cast<size_t>(index)].actor_id = actor_id;
	initialStats(index);
}

void TestBattleParty::setLevel(int index, int level) {
	m_battlers[static_cast<size_t>(index)].level = std::clamp(level, 1, maxLevel(index));
}

void TestBattleParty::setEquipment(int index, int slot, int item_id) {
	slotData(m_battlers[static_cast<size_t>(index)], slot) = item_id;
}

void TestBattleParty::initialStats(int index) {
	auto& battler = m_battlers[static_cast<size_t>(index)];
	if (const auto* hero = actor(index)) {
		battler.level = hero->initial_level;
		battler.weapon_id = hero->initial_equipment.weapon_id;
		battler.shield_id = hero->initial_equipment.shield_id;
		battler.armor_id = hero->initial_equipment.armor_id;
		battler.helmet_id = hero->initial_equipment.helmet_id;
		battler.accessory_id = hero->initial_equipment.accessory_id;
	}
	setLevel(index, battler.level);
}

const lcf::rpg::Actor* TestBattleParty::actor(int index) const {
	const int id = m_battlers[static_cast<size_t>(index)].actor_id;
	return id >= 1 && id <= static_cast<int>(m_database.actors.size()) ? &m_database.actors[static_cast<size_t>(id) - 1] : nullptr;
}

int TestBattleParty::maxLevel(int index) const {
	const auto* hero = actor(index);
	return hero && hero->final_level > 0 ? hero->final_level : (m_database.system.ldb_id == 2003 ? 99 : 50);
}

bool TestBattleParty::twoWeapons(int index) const {
	const auto* hero = actor(index);
	return hero && hero->two_weapon;
}

int TestBattleParty::slotType(int index, int slot) const {
	switch (slot) {
		case Weapon: return lcf::rpg::Item::Type_weapon;
		case Shield: return twoWeapons(index) ? lcf::rpg::Item::Type_weapon : lcf::rpg::Item::Type_shield;
		case Armor: return lcf::rpg::Item::Type_armor;
		case Helmet: return lcf::rpg::Item::Type_helmet;
		default: return lcf::rpg::Item::Type_accessory;
	}
}

std::vector<int> TestBattleParty::equipment(int index, int slot) const {
	const auto* hero = actor(index);
	const int type = slotType(index, slot);
	std::vector<int> ids;
	for (const auto& item : m_database.items) {
		if (item.type == type && (!hero || canEquip(*hero, item))) {
			ids.push_back(item.ID);
		}
	}
	return ids;
}

std::array<int, TestBattleParty::ParameterCount> TestBattleParty::parameters(int index) const {
	const auto& battler = m_battlers[static_cast<size_t>(index)];
	std::array<int, ParameterCount> values{};
	if (const auto* hero = actor(index)) {
		const auto& curves = hero->parameters;
		values = {curveValue(curves.maxhp, battler.level), curveValue(curves.maxsp, battler.level),
			curveValue(curves.attack, battler.level), curveValue(curves.defense, battler.level),
			curveValue(curves.spirit, battler.level), curveValue(curves.agility, battler.level)};
	}
	for (int id : {battler.weapon_id, battler.shield_id, battler.armor_id, battler.helmet_id, battler.accessory_id}) {
		if (const auto* item = findItem(m_database, id)) {
			values[Attack] += item->atk_points1;
			values[Defense] += item->def_points1;
			values[Intellect] += item->spi_points1;
			values[Agility] += item->agi_points1;
		}
	}
	return values;
}

int32_t& TestBattleParty::slotData(lcf::rpg::TestBattler& battler, int slot) {
	switch (slot) {
		case Weapon: return battler.weapon_id;
		case Shield: return battler.shield_id;
		case Armor: return battler.armor_id;
		case Helmet: return battler.helmet_id;
		default: return battler.accessory_id;
	}
}

int32_t TestBattleParty::slotData(const lcf::rpg::TestBattler& battler, int slot) {
	return slotData(const_cast<lcf::rpg::TestBattler&>(battler), slot);
}

bool TestBattleParty::canEquip(const lcf::rpg::Actor& actor, const lcf::rpg::Item& item) {
	// a set too short for the hero is an optimization of the file: all the
	// heroes missing can equip the item
	const int index = actor.ID - 1;
	return index < 0 || index >= static_cast<int>(item.actor_set.size()) || item.actor_set[static_cast<size_t>(index)];
}

TestBattle::TestBattle(lcf::rpg::Database& database, int troop_id) :
	m_database(database),
	m_troopId(troop_id)
{
	load();
}

void TestBattle::load() {
	const auto& system = m_database.system;
	const int formation = system.battletest_formation;
	m_formation = formation == lcf::rpg::System::BattleFormation_terrain || formation == lcf::rpg::System::BattleFormation_loose
		|| formation == lcf::rpg::System::BattleFormation_tight ? formation : lcf::rpg::System::BattleFormation_loose;
	const int condition = system.battletest_condition;
	m_condition = condition >= lcf::rpg::System::BattleCondition_none && condition <= lcf::rpg::System::BattleCondition_pincers
		? condition : lcf::rpg::System::BattleCondition_none;
	// the terrain of the last test, else the first one
	const auto& terrains = m_database.terrains;
	const bool found = std::any_of(terrains.begin(), terrains.end(), [&](const auto& terrain) {
		return terrain.ID == system.battletest_terrain;
	});
	m_terrainId = found ? system.battletest_terrain : (terrains.empty() ? 0 : terrains.front().ID);
	if (rpg2k3() && !conditionAllowed(m_condition)) {
		m_condition = lcf::rpg::System::BattleCondition_none;
	}
}

void TestBattle::save() {
	auto& system = m_database.system;
	system.battletest_formation = m_formation;
	system.battletest_condition = m_condition;
	system.battletest_terrain = m_terrainId;
}

int TestBattle::troopId() const {
	return m_troopId;
}

bool TestBattle::rpg2k3() const {
	return m_database.system.ldb_id == 2003;
}

int TestBattle::formation() const {
	return m_formation;
}

void TestBattle::setFormation(int formation) {
	m_formation = formation;
}

int TestBattle::condition() const {
	return m_condition;
}

void TestBattle::setCondition(int condition) {
	m_condition = condition;
}

int TestBattle::terrainId() const {
	return m_terrainId;
}

void TestBattle::setTerrainId(int terrain_id) {
	m_terrainId = terrain_id;
}

bool TestBattle::manualHeroes() const {
	return m_database.battlecommands.placement == lcf::rpg::BattleCommands::Placement_manual;
}

bool TestBattle::manualMonsters() const {
	const auto& troops = m_database.troops;
	const auto* troop = m_troopId >= 1 && m_troopId <= static_cast<int>(troops.size())
		? &troops[static_cast<size_t>(m_troopId) - 1] : nullptr;
	return troop && !troop->auto_alignment;
}

bool TestBattle::formationEnabled() const {
	return rpg2k3() && !(manualHeroes() && manualMonsters());
}

bool TestBattle::terrainEnabled() const {
	return formationEnabled() && m_formation == lcf::rpg::System::BattleFormation_terrain;
}

bool TestBattle::conditionAllowed(int condition) const {
	return !(manualHeroes() && (condition == lcf::rpg::System::BattleCondition_surround
		|| condition == lcf::rpg::System::BattleCondition_pincers));
}

TestBattle::PlayerBattle TestBattle::playerBattle() const {
	if (!rpg2k3()) {
		// the Player only takes the group
		return {};
	}
	return {m_formation, m_condition, std::max(1, m_terrainId)};
}
