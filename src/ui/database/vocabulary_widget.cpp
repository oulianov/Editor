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

#include "vocabulary_widget.h"
#include <array>
#include <map>
#include <string_view>
#include <vector>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QTabWidget>
#include <QVBoxLayout>
#include <lcf/rpg/database.h>
#include "common/dbstring.h"
#include "common/lcf_widget_binding.h"
#include "model/project_data.h"
#include "system_fields.h"

namespace {
using Terms = lcf::rpg::Terms;
using Term = lcf::DBString Terms::*;

enum class Engine { Both, Rpg2k, Rpg2k3 };

/** A term; hint shows what the game puts before it, or between it and second */
struct Row {
	Engine engine;
	QString label;
	Term term;
	QString hint = {};
	Term second = nullptr;
};

struct Group {
	QString title;
	int column;
	int pairs; // label and field pairs on each line
	std::vector<Row> rows;
};

struct Page {
	QString title;
	std::vector<Group> groups;
};
}

VocabularyWidget::VocabularyWidget(ProjectData& project, QWidget *parent) :
	QWidget(parent)
{
	auto& terms = project.database().terms;
	const bool rpg2k3 = project.database().system.ldb_id == 2003;
	constexpr auto Both = Engine::Both;
	constexpr auto Rpg2k = Engine::Rpg2k;
	constexpr auto Rpg2k3 = Engine::Rpg2k3;

	auto shop = [this](std::array<Term, 11> t) {
		return std::vector<Row>{
			{Both, tr("Greeting"), t[0]},
			{Both, tr("After One Transaction"), t[1]},
			{Both, tr("Buy"), t[2]},
			{Both, tr("Sell"), t[3]},
			{Both, tr("Finished"), t[4]},
			{Both, tr("Choosing an Item to Buy"), t[5]},
			{Both, tr("Choosing How Many to Buy"), t[6]},
			{Both, tr("Buy Transaction Completed"), t[7]},
			{Both, tr("Choosing an Item to Sell"), t[8]},
			{Both, tr("Choosing How Many to Sell"), t[9]},
			{Both, tr("Sell Transaction Completed"), t[10]},
		};
	};
	auto inn = [this](Term greeting_1, Term greeting_2, Term greeting_3, Term accept, Term cancel) {
		return std::vector<Row>{
			{Both, tr("Innkeeper Message"), greeting_1, tr("(X Currency)"), greeting_2},
			{Both, QString(), greeting_3},
			{Both, tr("Accept"), accept},
			{Both, tr("Decline"), cancel},
		};
	};

	// The pages of RPG Maker, the last one split in two to fit small screens
	const std::vector<Page> pages = {
		{tr("Battle"), {
			{tr("Battle Messages"), 0, 1, {
				{Rpg2k3, tr("Message at the Beginning of Each Battle"), &Terms::battle_start},
				{Rpg2k, tr("Monster Appears"), &Terms::encounter, tr("(Monster)")},
				{Both, tr("Initiative Message"), &Terms::special_combat},
				{Rpg2k3, tr("\"Miss\" Message"), &Terms::miss},
				{Rpg2k, tr("Escape Success Message"), &Terms::escape_success},
				{Both, tr("Escape Failure Message"), &Terms::escape_failure},
				{Both, tr("Victory Message"), &Terms::victory},
				{Both, tr("Defeat Message"), &Terms::defeat},
				{Both, tr("Experience Gain Message"), &Terms::exp_received, tr("( X )")},
				{Both, tr("Money Acquisition Message"), &Terms::gold_recieved_a, tr("( X ) (Currency Unit)"), &Terms::gold_recieved_b},
				{Both, tr("Item Discovery Message"), &Terms::item_recieved, tr("( Item Name )")},
			}},
			{tr("Level-up Messages"), 1, 1, {
				{Both, tr("Level Up"), &Terms::level_up, tr("(Character) (Level) (X)")},
				{Both, tr("Skill Learned"), &Terms::skill_learned, tr("(Skill Name)")},
			}},
		}},
		// RPG Maker 2000 battles only
		{tr("Battle Actions"), {
			{tr("Actions"), 0, 1, {
				{Rpg2k, tr("Attacks"), &Terms::attacking},
				{Rpg2k, tr("Critical Hit by a Hero"), &Terms::actor_critical},
				{Rpg2k, tr("Critical Hit by a Monster"), &Terms::enemy_critical},
				{Rpg2k, tr("Defends"), &Terms::defending},
				{Rpg2k, tr("Observes"), &Terms::observing},
				{Rpg2k, tr("Charges Up"), &Terms::focus},
				{Rpg2k, tr("Self-Destructs"), &Terms::autodestruction},
				{Rpg2k, tr("Monster Escapes"), &Terms::enemy_escape},
				{Rpg2k, tr("Monster Transforms"), &Terms::enemy_transform},
				{Rpg2k, tr("Skill Failure A"), &Terms::skill_failure_a},
				{Rpg2k, tr("Skill Failure B"), &Terms::skill_failure_b},
				{Rpg2k, tr("Skill Failure C"), &Terms::skill_failure_c},
				{Rpg2k, tr("Uses an Item"), &Terms::use_item},
			}},
			{tr("Results"), 1, 1, {
				{Rpg2k, tr("Monster Damaged"), &Terms::enemy_damaged},
				{Rpg2k, tr("Monster Undamaged"), &Terms::enemy_undamaged},
				{Rpg2k, tr("Hero Damaged"), &Terms::actor_damaged},
				{Rpg2k, tr("Hero Undamaged"), &Terms::actor_undamaged},
				{Rpg2k, tr("Evades the Attack"), &Terms::dodge},
				{Rpg2k, tr("HP Recovered"), &Terms::hp_recovery},
				{Rpg2k, tr("Stat Increased"), &Terms::parameter_increase},
				{Rpg2k, tr("Stat Decreased"), &Terms::parameter_decrease},
				{Rpg2k, tr("HP Absorbed from a Monster"), &Terms::enemy_hp_absorbed},
				{Rpg2k, tr("HP Absorbed from a Hero"), &Terms::actor_hp_absorbed},
				{Rpg2k, tr("Resistance Increased"), &Terms::resistance_increase},
				{Rpg2k, tr("Resistance Decreased"), &Terms::resistance_decrease},
			}},
		}},
		{tr("Shops"), {
			{tr("Messages for Shop Type A"), 0, 1, shop({&Terms::shop_greeting1, &Terms::shop_regreeting1,
				&Terms::shop_buy1, &Terms::shop_sell1, &Terms::shop_leave1, &Terms::shop_buy_select1,
				&Terms::shop_buy_number1, &Terms::shop_purchased1, &Terms::shop_sell_select1,
				&Terms::shop_sell_number1, &Terms::shop_sold1})},
			{tr("Messages for Shop Type B"), 1, 1, shop({&Terms::shop_greeting2, &Terms::shop_regreeting2,
				&Terms::shop_buy2, &Terms::shop_sell2, &Terms::shop_leave2, &Terms::shop_buy_select2,
				&Terms::shop_buy_number2, &Terms::shop_purchased2, &Terms::shop_sell_select2,
				&Terms::shop_sell_number2, &Terms::shop_sold2})},
		}},
		{tr("Shops and Inns"), {
			{tr("Messages for Shop Type C"), 0, 1, shop({&Terms::shop_greeting3, &Terms::shop_regreeting3,
				&Terms::shop_buy3, &Terms::shop_sell3, &Terms::shop_leave3, &Terms::shop_buy_select3,
				&Terms::shop_buy_number3, &Terms::shop_purchased3, &Terms::shop_sell_select3,
				&Terms::shop_sell_number3, &Terms::shop_sold3})},
			{tr("Messages for Inn Type A"), 1, 1, inn(&Terms::inn_a_greeting_1, &Terms::inn_a_greeting_2,
				&Terms::inn_a_greeting_3, &Terms::inn_a_accept, &Terms::inn_a_cancel)},
			{tr("Messages for Inn Type B"), 1, 1, inn(&Terms::inn_b_greeting_1, &Terms::inn_b_greeting_2,
				&Terms::inn_b_greeting_3, &Terms::inn_b_accept, &Terms::inn_b_cancel)},
			{tr("Other Strings"), 1, 1, {
				{Both, tr("Items Possessed"), &Terms::possessed_items},
				{Both, tr("Item Equipped"), &Terms::equipped_items},
				{Both, tr("Currency"), &Terms::gold},
			}},
		}},
		{tr("Commands"), {
			{tr("Commands"), 0, 2, {
				{Both, tr("Fight"), &Terms::battle_fight},
				{Both, tr("Auto-Fight"), &Terms::battle_auto},
				{Both, tr("Escape"), &Terms::battle_escape},
				{Rpg2k, tr("Attack"), &Terms::command_attack},
				{Rpg2k, tr("Defend"), &Terms::command_defend},
				{Both, tr("Item"), &Terms::command_item},
				{Both, tr("Skill"), &Terms::command_skill},
				{Both, tr("Equip"), &Terms::menu_equipment},
				{Both, tr("Save"), &Terms::menu_save},
				{Rpg2k3, tr("Status"), &Terms::status},
				{Rpg2k3, tr("Row"), &Terms::row},
				{Rpg2k3, tr("Order"), &Terms::order},
				{Rpg2k3, tr("Wait ON"), &Terms::wait_on},
				{Rpg2k3, tr("Wait OFF"), &Terms::wait_off},
				{Both, tr("Quit"), &Terms::menu_quit},
				{Both, tr("New Game"), &Terms::new_game},
				{Both, tr("Load Game"), &Terms::load_game},
				{Both, tr("End Game"), &Terms::exit_game},
			}},
			{tr("Save and Quit Screen Messages"), 1, 1, {
				{Both, tr("Choosing a Save Slot"), &Terms::save_game_message},
				{Both, tr("Choosing Which Slot to Load"), &Terms::load_game_message},
				{Both, tr("Confirm Quit"), &Terms::exit_game_message},
				{Both, tr("File"), &Terms::file},
				{Both, tr("Yes"), &Terms::yes},
				{Both, tr("No"), &Terms::no},
			}},
		}},
		{tr("Statistics"), {
			{tr("Statistics and Equipment"), 0, 2, {
				{Both, tr("Level"), &Terms::level},
				{Both, tr("HP"), &Terms::health_points},
				{Both, tr("MP"), &Terms::spirit_points},
				{Both, tr("Normal Status"), &Terms::normal_status},
				{Both, tr("Level (Short Form)"), &Terms::lvl_short},
				{Both, tr("HP (Short Form)"), &Terms::hp_short},
				{Both, tr("MP (Short Form)"), &Terms::sp_short},
				{Both, tr("Experience"), &Terms::exp_short},
				{Both, tr("MP Consumed"), &Terms::sp_cost},
				{Both, tr("Attack"), &Terms::attack},
				{Both, tr("Defense"), &Terms::defense},
				{Both, tr("Intelligence"), &Terms::spirit},
				{Both, tr("Agility"), &Terms::agility},
				{Both, tr("Weapon"), &Terms::weapon},
				{Both, tr("Shield"), &Terms::shield},
				{Both, tr("Armor"), &Terms::armor},
				{Both, tr("Helmet"), &Terms::helmet},
				{Both, tr("Accessory"), &Terms::accessory},
			}},
		}},
	};

	// The fields are named after their terms (for scripts)
	std::map<const lcf::DBString*, std::string_view> names;
	lcf::rpg::ForEachString(terms, [&](auto& value, const auto& ctx) {
		names[&value] = ctx.name;
	});
	auto field = [&](QWidget* parent, Term term) {
		auto& value = terms.*term;
		auto* line = new QLineEdit(parent);
		line->setObjectName(ToQString(names[&value]));
		LcfWidgetBinding::connect(this, line);
		LcfWidgetBinding::bind(line, value);
		return line;
	};
	auto shown = [rpg2k3](const Row& row) {
		return row.engine == Engine::Both || (row.engine == Engine::Rpg2k3) == rpg2k3;
	};

	auto* tabs = new QTabWidget(this);
	tabs->setObjectName(QStringLiteral("tabVocabulary"));
	for (const Page& page_data : pages) {
		auto* page = new QWidget();
		auto* columns = new QHBoxLayout(page);
		std::array<QVBoxLayout*, 2> col;
		for (auto*& column : col) {
			column = new QVBoxLayout();
			columns->addLayout(column, 1);
		}
		bool empty = true;
		for (const Group& group_data : page_data.groups) {
			std::vector<const Row*> rows;
			for (const Row& row : group_data.rows) {
				if (shown(row)) {
					rows.push_back(&row);
				}
			}
			if (rows.empty()) {
				continue;
			}
			empty = false;
			auto* group = new QGroupBox(group_data.title, page);
			auto* grid = new QGridLayout(group);
			for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
				const Row& row = *rows[i];
				const int line = i / group_data.pairs;
				const int column = (i % group_data.pairs) * 2;
				grid->addWidget(new QLabel(row.label, group), line, column);
				grid->setColumnStretch(column + 1, 1);
				auto* box = new QHBoxLayout();
				if (!row.hint.isEmpty() && !row.second) {
					box->addWidget(new QLabel(row.hint, group));
				}
				box->addWidget(field(group, row.term), 1);
				if (row.second) {
					box->addWidget(new QLabel(row.hint, group));
					box->addWidget(field(group, row.second), 1);
				}
				grid->addLayout(box, line, column + 1);
			}
			col[group_data.column]->addWidget(group);
		}
		if (empty) {
			delete page;
			continue;
		}
		for (auto* column : col) {
			column->addStretch(1);
		}
		tabs->addTab(SystemFields::scrollable(page), page_data.title);
	}

	auto* layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(tabs);
}
