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

#include "export_references.h"
#include "defines.h"
#include "move_route_codec.h"
#include <QRegularExpression>
#include <lcf/rpg/eventcommand.h>
#include <lcf/rpg/movecommand.h>

using Cmd = lcf::rpg::EventCommand::Code;

QString GameReferences::kindName(Kind kind)
{
	switch (kind) {
	case Actor: return tr("Heroes");
	case Class: return tr("Classes");
	case Skill: return tr("Skills");
	case Item: return tr("Items");
	case Enemy: return tr("Monsters");
	case Troop: return tr("Monster Groups");
	case Terrain: return tr("Terrain");
	case Attribute: return tr("Attributes");
	case State: return tr("Conditions");
	case Animation: return tr("Battle Animations");
	case Chipset: return tr("Chipsets");
	case CommonEvent: return tr("Common Events");
	case BattlerAnimation: return tr("Battler Animations");
	case KindCount: break;
	}
	return QString();
}

void GameReferences::addFile(const char* folder, std::string_view name)
{
	// "(OFF)" is the music and sound of nothing
	if (name.empty() || name == "(OFF)") {
		return;
	}
	files[QString::fromLatin1(folder)].insert(QString::fromUtf8(name.data(), name.size()));
}

void GameReferences::addId(Kind kind, int id)
{
	if (id > 0) {
		ids[kind].insert(id);
	}
}

template <typename Bits>
void GameReferences::addBits(Kind kind, const Bits& bits)
{
	// a set of entries (states inflicted, attributes...): bit i is id i + 1
	for (size_t i = 0; i < bits.size(); ++i) {
		if (bits[i]) {
			addId(kind, static_cast<int>(i) + 1);
		}
	}
}

void GameReferences::collect(const lcf::rpg::Database& db, const lcf::rpg::TreeMap& tree, const Maps& maps, const std::string& encoding)
{
	m_encoding = encoding;
	collectDatabase(db);

	for (const auto& info : tree.maps) {
		// the music and the battle background of a map can be given by the tree
		if (info.music_type == 2) {
			addFile(MUSIC, info.music.name);
		}
		if (info.background_type == 2) {
			addFile(BACKDROP, info.background_name);
		}
		for (const auto& encounter : info.encounters) {
			addId(Troop, encounter.troop_id);
		}
	}

	for (const auto& [id, map] : maps) {
		if (!map) {
			continue;
		}
		addId(Chipset, map->chipset_id);
		if (map->parallax_flag) {
			addFile(PANORAMA, map->parallax_name);
		}
		for (const auto& event : map->events) {
			const QString where = tr("Map %1, event %2").arg(id, 4, 10, QLatin1Char('0')).arg(event.ID);
			for (const auto& page : event.pages) {
				addFile(CHARSET, page.character_name);
				if (page.condition.flags.item) {
					addId(Item, page.condition.item_id);
				}
				if (page.condition.flags.actor) {
					addId(Actor, page.condition.actor_id);
				}
				for (const auto& move : page.move_route.move_commands) {
					collectMove(move);
				}
				collectCommands(page.event_commands, where);
			}
		}
	}
}

void GameReferences::collectDatabase(const lcf::rpg::Database& db)
{
	const auto& sys = db.system;
	addFile(CHARSET, sys.boat_name);
	addFile(CHARSET, sys.ship_name);
	addFile(CHARSET, sys.airship_name);
	addFile(TITLE, sys.title_name);
	addFile(GAMEOVER, sys.gameover_name);
	addFile(SYSTEM, sys.system_name);
	addFile(SYSTEM2, sys.system2_name);
	addFile(BACKDROP, sys.battletest_background);
	if (sys.show_frame) {
		addFile(FRAME, sys.frame_name);
	}
	for (const auto* music : { &sys.title_music, &sys.battle_music, &sys.battle_end_music, &sys.inn_music,
			&sys.boat_music, &sys.ship_music, &sys.airship_music, &sys.gameover_music }) {
		addFile(MUSIC, music->name);
	}
	for (const auto* sound : { &sys.cursor_se, &sys.decision_se, &sys.cancel_se, &sys.buzzer_se, &sys.battle_se,
			&sys.escape_se, &sys.enemy_attack_se, &sys.enemy_damaged_se, &sys.actor_damaged_se, &sys.dodge_se,
			&sys.enemy_death_se, &sys.item_se }) {
		addFile(SOUND, sound->name);
	}
	for (int16_t actor : sys.party) {
		addId(Actor, actor);
	}
	for (const auto& battler : sys.battletest_data) {
		addId(Actor, battler.actor_id);
		for (int item : { battler.weapon_id, battler.shield_id, battler.armor_id, battler.helmet_id, battler.accessory_id }) {
			addId(Item, item);
		}
	}
	addId(Terrain, sys.battletest_terrain);

	for (const auto& actor : db.actors) {
		addFile(CHARSET, actor.character_name);
		addFile(FACESET, actor.face_name);
		const auto& eq = actor.initial_equipment;
		for (int item : { eq.weapon_id, eq.shield_id, eq.armor_id, eq.helmet_id, eq.accessory_id }) {
			addId(Item, item);
		}
		addId(Animation, actor.unarmed_animation);
		addId(Class, actor.class_id);
		addId(BattlerAnimation, actor.battler_animation);
		for (const auto& learning : actor.skills) {
			addId(Skill, learning.skill_id);
		}
		addBits(State, actor.easyrpg_unarmed_state_set);
		addBits(Attribute, actor.easyrpg_unarmed_attribute_set);
	}
	for (const auto& cls : db.classes) {
		addId(BattlerAnimation, cls.battler_animation);
		for (const auto& learning : cls.skills) {
			addId(Skill, learning.skill_id);
		}
	}
	for (const auto& skill : db.skills) {
		addId(Animation, skill.animation_id);
		addFile(SOUND, skill.sound_effect.name);
		addBits(State, skill.state_effects);
		addBits(Attribute, skill.attribute_effects);
		for (const auto& data : skill.battler_animation_data) {
			addId(Animation, data.ranged_animation_id);
			addId(Animation, data.battle_animation_id);
		}
	}
	for (const auto& item : db.items) {
		addId(Animation, item.animation_id);
		addId(Skill, item.skill_id);
		addBits(State, item.state_set);
		addBits(Attribute, item.attribute_set);
		for (const auto& data : item.animation_data) {
			addId(Animation, data.ranged_animation_id);
			addId(Animation, data.battle_animation_id);
		}
	}
	for (const auto& enemy : db.enemies) {
		addFile(MONSTER, enemy.battler_name);
		addId(Item, enemy.drop_id);
		addId(Animation, enemy.maniac_unarmed_animation);
		addBits(State, enemy.easyrpg_state_set);
		addBits(Attribute, enemy.easyrpg_attribute_set);
		for (const auto& action : enemy.actions) {
			if (action.kind == lcf::rpg::EnemyAction::Kind_skill) {
				addId(Skill, action.skill_id);
			} else if (action.kind == lcf::rpg::EnemyAction::Kind_transformation) {
				addId(Enemy, action.enemy_id);
			}
		}
	}
	for (const auto& troop : db.troops) {
		for (const auto& member : troop.members) {
			addId(Enemy, member.enemy_id);
		}
		for (const auto& page : troop.pages) {
			const auto& c = page.condition;
			if (c.flags.actor_hp) {
				addId(Actor, c.actor_id);
			}
			if (c.flags.turn_actor) {
				addId(Actor, c.turn_actor_id);
			}
			if (c.flags.command_actor) {
				addId(Actor, c.command_actor_id);
			}
			collectCommands(page.event_commands, tr("Monster Group %1").arg(troop.ID));
		}
	}
	for (const auto& terrain : db.terrains) {
		addFile(BACKDROP, terrain.background_name);
		addFile(FRAME, terrain.background_a_name);
		if (terrain.background_b) {
			addFile(FRAME, terrain.background_b_name);
		}
		addFile(SOUND, terrain.footstep.name);
	}
	for (const auto& state : db.states) {
		addBits(State, state.easyrpg_immune_states);
	}
	for (const auto& animation : db.animations) {
		// big animations of RPG Maker 2003 are in their own folder
		addFile(animation.large ? BATTLE2 : BATTLE, animation.animation_name);
		for (const auto& timing : animation.timings) {
			addFile(SOUND, timing.se.name);
		}
	}
	for (const auto& chipset : db.chipsets) {
		addFile(CHIPSET, chipset.chipset_name);
		// the terrain of each tile: damage, encounters, footsteps, battle backgrounds
		for (int16_t terrain : chipset.terrain_data) {
			addId(Terrain, terrain);
		}
	}
	for (const auto& event : db.commonevents) {
		// the ones that start by themselves are used even if nothing calls them
		if (event.trigger != lcf::rpg::CommonEvent::Trigger_call) {
			addId(CommonEvent, event.ID);
		}
		collectCommands(event.event_commands, tr("Common Event %1").arg(event.ID));
	}
	for (const auto& battler : db.battleranimations) {
		for (const auto& pose : battler.poses) {
			addFile(BATTLECHARSET, pose.battler_name);
			addId(Animation, pose.battle_animation_id);
		}
		for (const auto& weapon : battler.weapons) {
			addFile(BATTLEWEAPON, weapon.weapon_name);
		}
	}
}

void GameReferences::collectCommands(const std::vector<lcf::rpg::EventCommand>& commands, const QString& where)
{
	for (const auto& com : commands) {
		collectCommand(com, where);
	}
}

void GameReferences::collectMove(const lcf::rpg::MoveCommand& move)
{
	if (move.command_id == static_cast<int>(lcf::rpg::MoveCommand::Code::change_graphic)) {
		addFile(CHARSET, move.parameter_string);
	} else if (move.command_id == static_cast<int>(lcf::rpg::MoveCommand::Code::play_sound_effect)) {
		addFile(SOUND, move.parameter_string);
	}
}

void GameReferences::collectCommand(const lcf::rpg::EventCommand& com, const QString& where)
{
	const auto& p = com.parameters;
	auto at = [&](size_t i) { return i < p.size() ? p[i] : 0; };
	// Maniac Patch: 4 bits per value of a "mode" parameter, 0 for a constant
	auto viaVariable = [&](size_t mode_index, int shift) { return ((at(mode_index) >> (shift * 4)) & 0xF) != 0; };
	auto file = [&](const char* folder, bool variable) {
		if (variable) {
			dynamicFiles << tr("%1: a file of %2 named by a variable").arg(where, QString::fromLatin1(folder));
		} else {
			addFile(folder, com.string);
		}
	};
	auto id = [&](Kind kind, int value, bool variable) {
		if (variable) {
			dynamic[kind] = true;
		} else {
			addId(kind, value);
		}
	};
	// the many commands on "the party, a hero, the hero of a variable"
	auto actors = [&]() {
		if (at(0) == 1) {
			addId(Actor, at(1));
		} else if (at(0) == 2) {
			dynamic[Actor] = true;
		}
	};

	if (com.code >= 2000 && com.code < 4000) {
		// EasyRPG and Maniac Patch commands read ids and names from variables and strings
		extended = true;
	}

	switch (static_cast<Cmd>(com.code)) {
	case Cmd::ShowMessage:
	case Cmd::ShowMessage_2:
	case Cmd::ShowChoice:
	case Cmd::ShowChoiceOption: {
		// \N[n] shows the name of hero n
		static const QRegularExpression name(QStringLiteral("[\\\\¥][nN]\\[(\\d+)\\]"));
		static const QRegularExpression name_var(QStringLiteral("[\\\\¥][nN]\\[[\\\\¥]"));
		const QString text = QString::fromUtf8(com.string.c_str(), com.string.size());
		auto it = name.globalMatch(text);
		while (it.hasNext()) {
			addId(Actor, it.next().captured(1).toInt());
		}
		if (text.contains(name_var)) {
			dynamic[Actor] = true;
		}
		break;
	}
	case Cmd::Comment:
	case Cmd::Comment_2:
		// DynRPG plugins and EasyRPG read commands from comments starting with @
		if (!com.string.empty() && com.string.c_str()[0] == '@') {
			extended = true;
		}
		break;
	case Cmd::ChangeFaceGraphic:
		file(FACESET, viaVariable(3, 0));
		break;
	case Cmd::PlayBGM:
		file(MUSIC, viaVariable(4, 0));
		break;
	case Cmd::PlaySound:
		file(SOUND, viaVariable(3, 0));
		break;
	case Cmd::ShowPicture:
		if (p.size() > 17 && viaVariable(17, 2)) {
			file(PICTURE, true);
		} else if (p.size() > 19 && p[19] != 0) {
			// RPG Maker 2003 1.12: a number from a variable ends the name
			filePrefixes[QStringLiteral(PICTURE)].insert(QString::fromUtf8(com.string.c_str(), com.string.size()));
			addFile(PICTURE, com.string);
		} else {
			addFile(PICTURE, com.string);
		}
		break;
	case Cmd::ChangeSpriteAssociation:
		id(Actor, at(0), viaVariable(3, 0));
		file(CHARSET, viaVariable(3, 1));
		break;
	case Cmd::ChangeActorFace:
		id(Actor, at(0), viaVariable(2, 0));
		file(FACESET, viaVariable(2, 1));
		break;
	case Cmd::ChangeHeroName:
	case Cmd::ChangeHeroTitle:
		id(Actor, at(0), viaVariable(1, 0));
		break;
	case Cmd::ChangeVehicleGraphic:
		addFile(CHARSET, com.string);
		break;
	case Cmd::ChangeSystemBGM:
		file(MUSIC, viaVariable(5, 0));
		break;
	case Cmd::ChangeSystemSFX:
		file(SOUND, viaVariable(4, 0));
		break;
	case Cmd::ChangeSystemGraphics:
		file(SYSTEM, viaVariable(2, 0));
		break;
	case Cmd::PlayMovie:
		addFile(MOVIE, com.string);
		break;
	case Cmd::ChangePBG:
		file(PANORAMA, viaVariable(6, 0));
		break;
	case Cmd::ChangeBattleBG:
		addFile(BACKDROP, com.string);
		break;
	case Cmd::EnemyEncounter:
		id(Troop, at(1), at(0) != 0);
		if (at(2) == 1) {
			addFile(BACKDROP, com.string);
		} else if (at(2) == 2) {
			addId(Terrain, at(8));
		}
		break;
	case Cmd::OpenShop:
		for (size_t i = 4; i < p.size(); ++i) {
			addId(Item, p[i]);
		}
		break;
	case Cmd::ShowBattleAnimation:
	case Cmd::ShowBattleAnimation_B:
		addId(Animation, at(0));
		break;
	case Cmd::ChangeMapTileset:
		id(Chipset, at(0), viaVariable(1, 0));
		break;
	case Cmd::CallEvent:
		if (at(0) == 0) {
			addId(CommonEvent, at(1));
		} else if (at(0) == 3 || at(0) == 4) {
			dynamic[CommonEvent] = true;
		}
		break;
	case Cmd::CallCommonEvent:
		id(CommonEvent, at(2), at(1) != 0);
		break;
	case Cmd::ControlVars:
		if (at(4) == 4) {
			addId(Item, at(5));
		} else if (at(4) == 5) {
			addId(Actor, at(5));
		}
		break;
	case Cmd::ChangeItems:
		id(Item, at(2), at(1) != 0);
		break;
	case Cmd::ChangePartyMembers:
		id(Actor, at(2), at(1) != 0);
		break;
	case Cmd::ChangeSkills:
		actors();
		id(Skill, at(4), at(3) != 0);
		break;
	case Cmd::ChangeEquipment:
		actors();
		if (at(2) == 0) {
			id(Item, at(4), at(3) != 0);
		}
		break;
	case Cmd::ChangeCondition:
		actors();
		addId(State, at(3));
		break;
	case Cmd::ChangeExp:
	case Cmd::ChangeLevel:
	case Cmd::ChangeParameters:
	case Cmd::ChangeHP:
	case Cmd::ChangeSP:
	case Cmd::FullHeal:
	case Cmd::SimulatedAttack:
	case Cmd::ChangeBattleCommands:
		actors();
		break;
	case Cmd::ChangeClass:
		actors();
		addId(Class, at(2));
		break;
	case Cmd::ConditionalBranch:
		if (at(0) == 4) {
			addId(Item, at(1));
		} else if (at(0) == 5) {
			addId(Actor, at(1));
			if (at(2) == 4) {
				addId(Skill, at(3));
			} else if (at(2) == 5) {
				addId(Item, at(3));
			} else if (at(2) == 6) {
				addId(State, at(3));
			}
		}
		break;
	case Cmd::ConditionalBranch_B:
		if (at(0) == 2 || at(0) == 5) {
			addId(Actor, at(1));
		}
		break;
	case Cmd::MoveEvent: {
		const auto move = MoveRouteCodec::decode(com, m_encoding);
		for (const auto& command : move.route.move_commands) {
			collectMove(command);
		}
		break;
	}
	default:
		break;
	}
}

namespace {
using Kind = GameReferences::Kind;

/** An entry with no name and no data, at its place */
template <typename T>
T blank(int id, bool is2k3)
{
	T entry;
	entry.ID = id;
	if constexpr (std::is_same_v<T, lcf::rpg::Actor>) {
		entry.Setup(is2k3);
	}
	return entry;
}

template <typename T>
bool isBlank(const T& entry, bool is2k3)
{
	return entry == blank<T>(entry.ID, is2k3);
}

/** Clears the unused entries of a list; returns how many */
template <typename T>
int clearUnused(std::vector<T>& list, const QSet<int>& used, const QSet<int>& keep, bool is2k3)
{
	int cleared = 0;
	for (auto& entry : list) {
		if (!used.contains(entry.ID) && !keep.contains(entry.ID) && !isBlank(entry, is2k3)) {
			entry = blank<T>(entry.ID, is2k3);
			++cleared;
		}
	}
	return cleared;
}

/** Removes the blank entries at the end (one entry is always kept); returns how many */
template <typename T>
int trimEnd(std::vector<T>& list, const QSet<int>& used, bool is2k3)
{
	int removed = 0;
	while (list.size() > 1 && !used.contains(list.back().ID) && isBlank(list.back(), is2k3)) {
		list.pop_back();
		++removed;
	}
	return removed;
}

/** Runs f on the list of a kind */
template <typename F>
void withList(lcf::rpg::Database& db, Kind kind, F&& f)
{
	switch (kind) {
	case GameReferences::Actor: f(db.actors); break;
	case GameReferences::Class: f(db.classes); break;
	case GameReferences::Skill: f(db.skills); break;
	case GameReferences::Item: f(db.items); break;
	case GameReferences::Enemy: f(db.enemies); break;
	case GameReferences::Troop: f(db.troops); break;
	case GameReferences::Terrain: f(db.terrains); break;
	case GameReferences::Attribute: f(db.attributes); break;
	case GameReferences::State: f(db.states); break;
	case GameReferences::Animation: f(db.animations); break;
	case GameReferences::Chipset: f(db.chipsets); break;
	case GameReferences::CommonEvent: f(db.commonevents); break;
	case GameReferences::BattlerAnimation: f(db.battleranimations); break;
	case GameReferences::KindCount: break;
	}
}

/**
 * Lists of other entries have a value per state, attribute and terrain
 * (resistances, where a group appears): the Player reads them by index,
 * so these lists keep their length.
 */
bool canTrim(Kind kind)
{
	return kind != GameReferences::State && kind != GameReferences::Attribute && kind != GameReferences::Terrain;
}
}

QStringList DatabaseCleanup::clean(lcf::rpg::Database& db, const lcf::rpg::TreeMap& tree, const GameReferences::Maps& maps, const std::string& encoding)
{
	QStringList log;
	const bool is2k3 = db.system.ldb_id == 2003;
	// always used: state 1 is death, terrain 1 the default of new chipsets
	QSet<int> keep[GameReferences::KindCount];
	keep[GameReferences::State] = { 1 };
	keep[GameReferences::Terrain] = { 1 };

	int cleared[GameReferences::KindCount] = {};
	GameReferences refs;
	// clearing an entry can leave others unused (the monsters of a cleared group): again until nothing changes
	for (int pass = 0; pass < 10; ++pass) {
		refs = GameReferences();
		refs.collect(db, tree, maps, encoding);
		if (refs.extended) {
			log << QCoreApplication::translate("GameReferences",
				"The database is kept as it is: the game uses Maniac Patch or EasyRPG commands, or DynRPG comments, which can use any entry.");
			return log;
		}
		int changed = 0;
		for (int k = 0; k < GameReferences::KindCount; ++k) {
			const Kind kind = static_cast<Kind>(k);
			if (refs.dynamic[kind]) {
				continue;
			}
			withList(db, kind, [&](auto& list) {
				const int n = clearUnused(list, refs.ids[kind], keep[kind], is2k3);
				cleared[kind] += n;
				changed += n;
			});
		}
		if (changed == 0) {
			break;
		}
	}

	for (int k = 0; k < GameReferences::KindCount; ++k) {
		const Kind kind = static_cast<Kind>(k);
		if (refs.dynamic[kind]) {
			log << QCoreApplication::translate("GameReferences", "%1: kept, a command reads their number from a variable.")
				.arg(GameReferences::kindName(kind));
			continue;
		}
		int removed = 0;
		if (canTrim(kind)) {
			withList(db, kind, [&](auto& list) { removed = trimEnd(list, refs.ids[kind], is2k3); });
		}
		if (cleared[kind] > 0 || removed > 0) {
			log << QCoreApplication::translate("GameReferences", "%1: %2 unused entries cleared, %3 removed at the end of the list.")
				.arg(GameReferences::kindName(kind)).arg(cleared[kind]).arg(removed);
		}
	}
	if (log.isEmpty()) {
		log << QCoreApplication::translate("GameReferences", "The database has no unused entries.");
	}
	return log;
}
