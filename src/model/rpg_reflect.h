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

#include "actor.h"
#include "skill.h"
#include "enemy.h"
#include "item.h"
#include "troop.h"
#include "terrain.h"
#include "attribute.h"
#include "state.h"
#include "animation.h"
#include "chipset.h"
#include "commonevent.h"
#include "class.h"
#include "battleranimation.h"
#include "switch.h"
#include "variable.h"
#include "project_data.h"
#include "rpg_metatypes.h"


template<typename T>
class RpgReflect {};

template<>
class RpgReflect<lcf::rpg::Actor> {
public:
	using model_type = ActorModel;

	static std::vector<lcf::rpg::Actor>& items(lcf::rpg::Database& database) {
		return database.actors;
	}
};

template<>
class RpgReflect<lcf::rpg::Skill> {
public:
	using model_type = SkillModel;

	static std::vector<lcf::rpg::Skill>& items(lcf::rpg::Database& database) {
		return database.skills;
	}
};

template<>
class RpgReflect<lcf::rpg::Enemy> {
public:
	using model_type = EnemyModel;

	static std::vector<lcf::rpg::Enemy>& items(lcf::rpg::Database& database) {
		return database.enemies;
	}
};

template<>
class RpgReflect<lcf::rpg::Item> {
public:
	using model_type = ItemModel;

	static std::vector<lcf::rpg::Item>& items(lcf::rpg::Database& database) {
		return database.items;
	}
};

template<>
class RpgReflect<lcf::rpg::Troop> {
public:
	using model_type = TroopModel;

	static std::vector<lcf::rpg::Troop>& items(lcf::rpg::Database& database) {
		return database.troops;
	}
};

template<>
class RpgReflect<lcf::rpg::Terrain> {
public:
	using model_type = TerrainModel;

	static std::vector<lcf::rpg::Terrain>& items(lcf::rpg::Database& database) {
		return database.terrains;
	}
};

template<>
class RpgReflect<lcf::rpg::Attribute> {
public:
	using model_type = AttributeModel;

	static std::vector<lcf::rpg::Attribute>& items(lcf::rpg::Database& database) {
		return database.attributes;
	}
};

template<>
class RpgReflect<lcf::rpg::State> {
public:
	using model_type = StateModel;

	static std::vector<lcf::rpg::State>& items(lcf::rpg::Database& database) {
		return database.states;
	}
};

template<>
class RpgReflect<lcf::rpg::Animation> {
public:
	using model_type = AnimationModel;

	static std::vector<lcf::rpg::Animation>& items(lcf::rpg::Database& database) {
		return database.animations;
	}
};

template<>
class RpgReflect<lcf::rpg::Chipset> {
public:
	using model_type = ChipsetModel;

	static std::vector<lcf::rpg::Chipset>& items(lcf::rpg::Database& database) {
		return database.chipsets;
	}
};

template<>
class RpgReflect<lcf::rpg::CommonEvent> {
public:
	using model_type = CommonEventModel;

	static std::vector<lcf::rpg::CommonEvent>& items(lcf::rpg::Database& database) {
		return database.commonevents;
	}
};

template<>
class RpgReflect<lcf::rpg::Class> {
public:
	using model_type = ClassModel;

	static std::vector<lcf::rpg::Class>& items(lcf::rpg::Database& database) {
		return database.classes;
	}
};

template<>
class RpgReflect<lcf::rpg::BattlerAnimation> {
public:
	using model_type = BattlerAnimationModel;

	static std::vector<lcf::rpg::BattlerAnimation>& items(lcf::rpg::Database& database) {
		return database.battleranimations;
	}
};

template<>
class RpgReflect<lcf::rpg::Switch> {
public:
	using model_type = SwitchModel;

	static std::vector<lcf::rpg::Switch>& items(lcf::rpg::Database& database) {
		return database.switches;
	}
};

template<>
class RpgReflect<lcf::rpg::Variable> {
public:
	using model_type = VariableModel;

	static std::vector<lcf::rpg::Variable>& items(lcf::rpg::Database& database) {
		return database.variables;
	}
};
