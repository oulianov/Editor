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

#include <array>
#include <cstdint>
#include <vector>
#include <lcf/rpg/database.h>

/**
 * The party of the battle tests (lcf::rpg::System::battletest_data), as in
 * RPG Maker's Test Battle window: up to 4 heroes, each with a level and
 * equipment, and the parameters they give. A copy is edited; save()
 * writes it to the database, from which the EasyRPG Player reads it
 * (Game_Party::SetupBattleTest).
 */
class TestBattleParty
{
public:
	static constexpr int MaxMembers = 4;
	enum Slot { Weapon, Shield, Armor, Helmet, Accessory, SlotCount };
	enum Parameter { MaxHp, MaxMp, Attack, Defense, Intellect, Agility, ParameterCount };

	/** The party of the database (load()) */
	explicit TestBattleParty(lcf::rpg::Database& database);

	/** Reads the party of the database. The members it lacks are the
	 *  next heroes, with the level and equipment they start with */
	void load();
	/** Writes the party to the database, without the members past the size
	 *  nor those with an unknown hero: the Player adds every hero listed */
	void save();

	/** The members in the party (0 to 4) */
	int size() const;
	void setSize(int size);

	const lcf::rpg::TestBattler& member(int index) const;
	/** Another hero, who starts from their own stats, like in RPG Maker */
	void setHero(int index, int actor_id);
	/** The level, kept from 1 to maxLevel() */
	void setLevel(int index, int level);
	/** An item in a slot (0: none) */
	void setEquipment(int index, int slot, int item_id);
	/** The level and equipment the hero starts the game with */
	void initialStats(int index);

	/** The member's hero, nullptr when unknown */
	const lcf::rpg::Actor* actor(int index) const;
	/** The highest level of the member: the final level of the hero, else the engine's */
	int maxLevel(int index) const;
	/** Whether the hero holds a weapon in the shield slot */
	bool twoWeapons(int index) const;
	/** The type of the items of a slot (lcf::rpg::Item::Type) */
	int slotType(int index, int slot) const;
	/** The items the member can equip in a slot, in the order of the database */
	std::vector<int> equipment(int index, int slot) const;
	/** The parameters at the member's level, with what the equipment adds,
	 *  as the Player counts them */
	std::array<int, ParameterCount> parameters(int index) const;

	/** The item of a slot */
	static int32_t& slotData(lcf::rpg::TestBattler& battler, int slot);
	static int32_t slotData(const lcf::rpg::TestBattler& battler, int slot);
	/** Whether a hero can equip an item (its actor_set): the rule of ActorModel::IsItemUsable */
	static bool canEquip(const lcf::rpg::Actor& actor, const lcf::rpg::Item& item);

private:
	lcf::rpg::Database& m_database;
	std::array<lcf::rpg::TestBattler, MaxMembers> m_battlers;
	int m_size = 0;
};

/**
 * The battle of RPG Maker's Test Battle window against a monster group:
 * the formation and the special condition (RPG Maker 2003), kept in the
 * database (System) for the next test, and what the Player is given.
 */
class TestBattle
{
public:
	/** What the Player is given besides the group (GameRunner::battleOptions) */
	struct PlayerBattle {
		/** lcf::rpg::System::BattleFormation, -1: none (RPG Maker 2000) */
		int formation = -1;
		int condition = 0;
		int terrain_id = 1;
	};

	/** The settings of the last test (load()) */
	TestBattle(lcf::rpg::Database& database, int troop_id);

	/** Reads the settings of the last test */
	void load();
	/** Writes them to the database */
	void save();

	int troopId() const;
	/** RPG Maker 2000 battles have neither formation nor condition */
	bool rpg2k3() const;

	/** lcf::rpg::System::BattleFormation */
	int formation() const;
	void setFormation(int formation);
	/** lcf::rpg::System::BattleCondition */
	int condition() const;
	void setCondition(int condition);
	/** The terrain of the Terrain formation (its ID, 0: none) */
	int terrainId() const;
	void setTerrainId(int terrain_id);

	/** The heroes are placed by hand (Battle Layout) */
	bool manualHeroes() const;
	/** The monsters are placed by hand (the group's alignment) */
	bool manualMonsters() const;
	/** The formation places whoever is placed automatically: when both are
	 *  placed by hand, it does nothing */
	bool formationEnabled() const;
	bool terrainEnabled() const;
	/** Like RPG Maker: no surround or pincers attack when the heroes are placed by hand */
	bool conditionAllowed(int condition) const;

	PlayerBattle playerBattle() const;

private:
	lcf::rpg::Database& m_database;
	int m_troopId;
	int m_formation = 0;
	int m_condition = 0;
	int m_terrainId = 0;
};
