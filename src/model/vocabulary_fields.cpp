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

#include "vocabulary_fields.h"
#include <QCoreApplication>
#include <array>
#include <map>

// The texts are in the context VocabularyWidget, as VocabularyPage.qml's
// (written out for lupdate): their translations stay
namespace VocabularyFields {

namespace {

std::vector<Row> shop(const std::array<const char*, 11>& t) {
	constexpr auto Both = Engine::Both;
	return {
		{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Greeting"), t[0]},
		{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "After One Transaction"), t[1]},
		{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Buy"), t[2]},
		{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Sell"), t[3]},
		{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Finished"), t[4]},
		{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Choosing an Item to Buy"), t[5]},
		{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Choosing How Many to Buy"), t[6]},
		{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Buy Transaction Completed"), t[7]},
		{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Choosing an Item to Sell"), t[8]},
		{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Choosing How Many to Sell"), t[9]},
		{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Sell Transaction Completed"), t[10]},
	};
}

std::vector<Row> inn(const char* greeting_1, const char* greeting_2, const char* greeting_3,
		const char* accept, const char* cancel) {
	constexpr auto Both = Engine::Both;
	return {
		{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Innkeeper Message"), greeting_1, QT_TRANSLATE_NOOP("VocabularyWidget", "(X Currency)"), greeting_2},
		{Both, nullptr, greeting_3},
		{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Accept"), accept},
		{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Decline"), cancel},
	};
}

std::vector<Page> makePages() {
	constexpr auto Both = Engine::Both;
	constexpr auto Rpg2k = Engine::Rpg2k;
	constexpr auto Rpg2k3 = Engine::Rpg2k3;
	return {
		{QT_TRANSLATE_NOOP("VocabularyWidget", "Battle"), {
			{QT_TRANSLATE_NOOP("VocabularyWidget", "Battle Messages"), 0, 1, {
				{Rpg2k3, QT_TRANSLATE_NOOP("VocabularyWidget", "Message at the Beginning of Each Battle"), "battle_start"},
				{Rpg2k, QT_TRANSLATE_NOOP("VocabularyWidget", "Monster Appears"), "encounter", QT_TRANSLATE_NOOP("VocabularyWidget", "(Monster)")},
				{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Initiative Message"), "special_combat"},
				{Rpg2k3, QT_TRANSLATE_NOOP("VocabularyWidget", "\"Miss\" Message"), "miss"},
				{Rpg2k, QT_TRANSLATE_NOOP("VocabularyWidget", "Escape Success Message"), "escape_success"},
				{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Escape Failure Message"), "escape_failure"},
				{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Victory Message"), "victory"},
				{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Defeat Message"), "defeat"},
				{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Experience Gain Message"), "exp_received", QT_TRANSLATE_NOOP("VocabularyWidget", "( X )")},
				{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Money Acquisition Message"), "gold_recieved_a", QT_TRANSLATE_NOOP("VocabularyWidget", "( X ) (Currency Unit)"), "gold_recieved_b"},
				{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Item Discovery Message"), "item_recieved", QT_TRANSLATE_NOOP("VocabularyWidget", "( Item Name )")},
			}},
			{QT_TRANSLATE_NOOP("VocabularyWidget", "Level-up Messages"), 1, 1, {
				{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Level Up"), "level_up", QT_TRANSLATE_NOOP("VocabularyWidget", "(Character) (Level) (X)")},
				{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Skill Learned"), "skill_learned", QT_TRANSLATE_NOOP("VocabularyWidget", "(Skill Name)")},
			}},
		}},
		// RPG Maker 2000 battles only
		{QT_TRANSLATE_NOOP("VocabularyWidget", "Battle Actions"), {
			{QT_TRANSLATE_NOOP("VocabularyWidget", "Actions"), 0, 1, {
				{Rpg2k, QT_TRANSLATE_NOOP("VocabularyWidget", "Attacks"), "attacking"},
				{Rpg2k, QT_TRANSLATE_NOOP("VocabularyWidget", "Critical Hit by a Hero"), "actor_critical"},
				{Rpg2k, QT_TRANSLATE_NOOP("VocabularyWidget", "Critical Hit by a Monster"), "enemy_critical"},
				{Rpg2k, QT_TRANSLATE_NOOP("VocabularyWidget", "Defends"), "defending"},
				{Rpg2k, QT_TRANSLATE_NOOP("VocabularyWidget", "Observes"), "observing"},
				{Rpg2k, QT_TRANSLATE_NOOP("VocabularyWidget", "Charges Up"), "focus"},
				{Rpg2k, QT_TRANSLATE_NOOP("VocabularyWidget", "Self-Destructs"), "autodestruction"},
				{Rpg2k, QT_TRANSLATE_NOOP("VocabularyWidget", "Monster Escapes"), "enemy_escape"},
				{Rpg2k, QT_TRANSLATE_NOOP("VocabularyWidget", "Monster Transforms"), "enemy_transform"},
				{Rpg2k, QT_TRANSLATE_NOOP("VocabularyWidget", "Skill Failure A"), "skill_failure_a"},
				{Rpg2k, QT_TRANSLATE_NOOP("VocabularyWidget", "Skill Failure B"), "skill_failure_b"},
				{Rpg2k, QT_TRANSLATE_NOOP("VocabularyWidget", "Skill Failure C"), "skill_failure_c"},
				{Rpg2k, QT_TRANSLATE_NOOP("VocabularyWidget", "Uses an Item"), "use_item"},
			}},
			{QT_TRANSLATE_NOOP("VocabularyWidget", "Results"), 1, 1, {
				{Rpg2k, QT_TRANSLATE_NOOP("VocabularyWidget", "Monster Damaged"), "enemy_damaged"},
				{Rpg2k, QT_TRANSLATE_NOOP("VocabularyWidget", "Monster Undamaged"), "enemy_undamaged"},
				{Rpg2k, QT_TRANSLATE_NOOP("VocabularyWidget", "Hero Damaged"), "actor_damaged"},
				{Rpg2k, QT_TRANSLATE_NOOP("VocabularyWidget", "Hero Undamaged"), "actor_undamaged"},
				{Rpg2k, QT_TRANSLATE_NOOP("VocabularyWidget", "Evades the Attack"), "dodge"},
				{Rpg2k, QT_TRANSLATE_NOOP("VocabularyWidget", "HP Recovered"), "hp_recovery"},
				{Rpg2k, QT_TRANSLATE_NOOP("VocabularyWidget", "Stat Increased"), "parameter_increase"},
				{Rpg2k, QT_TRANSLATE_NOOP("VocabularyWidget", "Stat Decreased"), "parameter_decrease"},
				{Rpg2k, QT_TRANSLATE_NOOP("VocabularyWidget", "HP Absorbed from a Monster"), "enemy_hp_absorbed"},
				{Rpg2k, QT_TRANSLATE_NOOP("VocabularyWidget", "HP Absorbed from a Hero"), "actor_hp_absorbed"},
				{Rpg2k, QT_TRANSLATE_NOOP("VocabularyWidget", "Resistance Increased"), "resistance_increase"},
				{Rpg2k, QT_TRANSLATE_NOOP("VocabularyWidget", "Resistance Decreased"), "resistance_decrease"},
			}},
		}},
		{QT_TRANSLATE_NOOP("VocabularyWidget", "Shops"), {
			{QT_TRANSLATE_NOOP("VocabularyWidget", "Messages for Shop Type A"), 0, 1, shop({"shop_greeting1", "shop_regreeting1",
				"shop_buy1", "shop_sell1", "shop_leave1", "shop_buy_select1", "shop_buy_number1",
				"shop_purchased1", "shop_sell_select1", "shop_sell_number1", "shop_sold1"})},
			{QT_TRANSLATE_NOOP("VocabularyWidget", "Messages for Shop Type B"), 1, 1, shop({"shop_greeting2", "shop_regreeting2",
				"shop_buy2", "shop_sell2", "shop_leave2", "shop_buy_select2", "shop_buy_number2",
				"shop_purchased2", "shop_sell_select2", "shop_sell_number2", "shop_sold2"})},
		}},
		{QT_TRANSLATE_NOOP("VocabularyWidget", "Shops and Inns"), {
			{QT_TRANSLATE_NOOP("VocabularyWidget", "Messages for Shop Type C"), 0, 1, shop({"shop_greeting3", "shop_regreeting3",
				"shop_buy3", "shop_sell3", "shop_leave3", "shop_buy_select3", "shop_buy_number3",
				"shop_purchased3", "shop_sell_select3", "shop_sell_number3", "shop_sold3"})},
			{QT_TRANSLATE_NOOP("VocabularyWidget", "Messages for Inn Type A"), 1, 1, inn("inn_a_greeting_1", "inn_a_greeting_2",
				"inn_a_greeting_3", "inn_a_accept", "inn_a_cancel")},
			{QT_TRANSLATE_NOOP("VocabularyWidget", "Messages for Inn Type B"), 1, 1, inn("inn_b_greeting_1", "inn_b_greeting_2",
				"inn_b_greeting_3", "inn_b_accept", "inn_b_cancel")},
			{QT_TRANSLATE_NOOP("VocabularyWidget", "Other Strings"), 1, 1, {
				{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Items Possessed"), "possessed_items"},
				{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Item Equipped"), "equipped_items"},
				{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Currency"), "gold"},
			}},
		}},
		{QT_TRANSLATE_NOOP("VocabularyWidget", "Commands"), {
			{QT_TRANSLATE_NOOP("VocabularyWidget", "Commands"), 0, 2, {
				{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Fight"), "battle_fight"},
				{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Auto-Fight"), "battle_auto"},
				{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Escape"), "battle_escape"},
				{Rpg2k, QT_TRANSLATE_NOOP("VocabularyWidget", "Attack"), "command_attack"},
				{Rpg2k, QT_TRANSLATE_NOOP("VocabularyWidget", "Defend"), "command_defend"},
				{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Item"), "command_item"},
				{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Skill"), "command_skill"},
				{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Equip"), "menu_equipment"},
				{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Save"), "menu_save"},
				{Rpg2k3, QT_TRANSLATE_NOOP("VocabularyWidget", "Status"), "status"},
				{Rpg2k3, QT_TRANSLATE_NOOP("VocabularyWidget", "Row"), "row"},
				{Rpg2k3, QT_TRANSLATE_NOOP("VocabularyWidget", "Order"), "order"},
				{Rpg2k3, QT_TRANSLATE_NOOP("VocabularyWidget", "Wait ON"), "wait_on"},
				{Rpg2k3, QT_TRANSLATE_NOOP("VocabularyWidget", "Wait OFF"), "wait_off"},
				{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Quit"), "menu_quit"},
				{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "New Game"), "new_game"},
				{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Load Game"), "load_game"},
				{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "End Game"), "exit_game"},
			}},
			{QT_TRANSLATE_NOOP("VocabularyWidget", "Save and Quit Screen Messages"), 1, 1, {
				{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Choosing a Save Slot"), "save_game_message"},
				{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Choosing Which Slot to Load"), "load_game_message"},
				{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Confirm Quit"), "exit_game_message"},
				{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "File"), "file"},
				{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Yes"), "yes"},
				{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "No"), "no"},
			}},
		}},
		{QT_TRANSLATE_NOOP("VocabularyWidget", "Statistics"), {
			{QT_TRANSLATE_NOOP("VocabularyWidget", "Statistics and Equipment"), 0, 2, {
				{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Level"), "level"},
				{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "HP"), "health_points"},
				{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "MP"), "spirit_points"},
				{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Normal Status"), "normal_status"},
				{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Level (Short Form)"), "lvl_short"},
				{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "HP (Short Form)"), "hp_short"},
				{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "MP (Short Form)"), "sp_short"},
				{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Experience"), "exp_short"},
				{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "MP Consumed"), "sp_cost"},
				{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Attack"), "attack"},
				{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Defense"), "defense"},
				{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Intelligence"), "spirit"},
				{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Agility"), "agility"},
				{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Weapon"), "weapon"},
				{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Shield"), "shield"},
				{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Armor"), "armor"},
				{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Helmet"), "helmet"},
				{Both, QT_TRANSLATE_NOOP("VocabularyWidget", "Accessory"), "accessory"},
			}},
		}},
	};
}

}

const std::vector<Page>& pages() {
	static const std::vector<Page> table = makePages();
	return table;
}

bool shown(const Row& row, bool rpg2k3) {
	return row.engine == Engine::Both || (row.engine == Engine::Rpg2k3) == rpg2k3;
}

int limit(std::string_view name) {
	static const std::map<std::string_view, int> limits = {
		{"battle_start", 30}, {"special_combat", 50}, {"miss", 8}, {"escape_failure", 50},
		{"victory", 50}, {"defeat", 50}, {"exp_received", 38}, {"gold_recieved_a", 10},
		{"gold_recieved_b", 24}, {"item_recieved", 30}, {"level_up", 16}, {"skill_learned", 26},
		{"possessed_items", 16}, {"equipped_items", 16}, {"gold", 6},
		{"battle_fight", 10}, {"battle_auto", 10}, {"battle_escape", 10}, {"command_item", 10},
		{"command_skill", 10}, {"menu_equipment", 12}, {"menu_save", 12}, {"menu_quit", 12},
		{"status", 12}, {"row", 10}, {"order", 12}, {"wait_on", 12}, {"wait_off", 12},
		{"new_game", 16}, {"load_game", 16}, {"exit_game", 16},
		{"level", 10}, {"health_points", 10}, {"spirit_points", 10}, {"normal_status", 8},
		{"lvl_short", 2}, {"hp_short", 2}, {"sp_short", 2}, {"exp_short", 2}, {"sp_cost", 10},
		{"attack", 10}, {"defense", 10}, {"spirit", 10}, {"agility", 10},
		{"weapon", 10}, {"shield", 10}, {"armor", 10}, {"helmet", 10}, {"accessory", 10},
		{"save_game_message", 50}, {"load_game_message", 50}, {"exit_game_message", 50},
		{"file", 10}, {"yes", 10}, {"no", 10},
	};
	if (auto it = limits.find(name); it != limits.end()) {
		return it->second;
	}
	// the three shops and the two inns
	auto starts = [&](std::string_view prefix) { return name.substr(0, prefix.size()) == prefix; };
	if (starts("shop_buy_") || starts("shop_sell_") || starts("shop_greeting") || starts("shop_regreeting")
			|| starts("shop_purchased") || starts("shop_sold")) {
		return 50;
	}
	if (starts("shop_buy") || starts("shop_sell") || starts("shop_leave")) {
		return 12;
	}
	if (starts("inn_")) {
		if (name.size() >= 2 && name.substr(name.size() - 2) == "_1") return 16;
		if (name.size() >= 2 && name.substr(name.size() - 2) == "_2") return 20;
		if (name.size() >= 2 && name.substr(name.size() - 2) == "_3") return 50;
		return 12;
	}
	return 0;
}

QString tr(const char* text) {
	return text ? QCoreApplication::translate("VocabularyWidget", text) : QString();
}

}
