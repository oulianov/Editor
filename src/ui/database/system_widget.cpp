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

#include "system_widget.h"
#include <array>
#include <initializer_list>
#include <tuple>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>
#include <lcf/rpg/database.h>
#include "defines.h"
#include "common/lcf_widget_binding.h"
#include "model/project_data.h"
#include "system_fields.h"

SystemWidget::SystemWidget(ProjectData& project, QWidget *parent) :
	QWidget(parent)
{
	auto& database = project.database();
	auto& sys = database.system;

	auto* page = new QWidget();
	auto* columns = new QHBoxLayout(page);
	std::array<QVBoxLayout*, 3> col;
	for (auto*& column : col) {
		column = new QVBoxLayout();
		columns->addLayout(column, 1);
	}
	auto addGroup = [page](QVBoxLayout* column, const QString& title) {
		auto* group = new QGroupBox(title, page);
		auto* form = new QFormLayout(group);
		form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
		column->addWidget(group);
		return form;
	};

	// Graphics and starting party
	auto* vehicles = addGroup(col[0], tr("Vehicles"));
	vehicles->addRow(tr("Skiff"), SystemFields::imageField(project, CHARSET, sys.boat_name, page, &sys.boat_index));
	vehicles->addRow(tr("Ship"), SystemFields::imageField(project, CHARSET, sys.ship_name, page, &sys.ship_index));
	vehicles->addRow(tr("Airship"), SystemFields::imageField(project, CHARSET, sys.airship_name, page, &sys.airship_index));

	auto* graphics = addGroup(col[0], tr("Graphics"));
	graphics->addRow(tr("Title Screen"), SystemFields::imageField(project, TITLE, sys.title_name, page));
	graphics->addRow(tr("Game Over Screen"), SystemFields::imageField(project, GAMEOVER, sys.gameover_name, page));
	graphics->addRow(tr("System Graphic"), SystemFields::imageField(project, SYSTEM, sys.system_name, page));
	// RPG Maker sets these two with the system graphic
	auto* stretch = new QComboBox(page);
	stretch->addItem(tr("Stretch to Fit"), lcf::rpg::System::Stretch_stretch);
	stretch->addItem(tr("Tiled Display"), lcf::rpg::System::Stretch_tiled);
	graphics->addRow(tr("Graphic Pattern"), stretch);
	auto* font = new QComboBox(page);
	font->addItem(tr("MS Gothic"), lcf::rpg::System::Font_gothic);
	font->addItem(tr("MS Mincho"), lcf::rpg::System::Font_mincho);
	graphics->addRow(tr("Text Font"), font);
	for (auto [combo, value] : std::initializer_list<std::pair<QComboBox*, int32_t*>>{
			{stretch, &sys.message_stretch}, {font, &sys.font_id}}) {
		LcfWidgetBinding::connect<int32_t>(this, combo);
		LcfWidgetBinding::bind(combo, *value);
	}

	auto* party = addGroup(col[0], tr("Starting Party"));
	std::vector<QComboBox*> members;
	for (const QString& name : {tr("First Member"), tr("Second Member"), tr("Third Member"), tr("Fourth Member")}) {
		auto* combo = new QComboBox(page);
		SystemFields::fillWithNone(combo, database.actors);
		party->addRow(name, combo);
		members.push_back(combo);
	}
	SystemFields::bindIdList(this, members, sys.party);

	// Music and screen transitions
	auto* music = addGroup(col[1], tr("System BGM"));
	for (auto [label, value] : std::initializer_list<std::pair<QString, lcf::rpg::Music*>>{
			{tr("Title Screen"), &sys.title_music},
			{tr("Battle"), &sys.battle_music},
			{tr("Victory"), &sys.battle_end_music},
			{tr("Inn"), &sys.inn_music},
			{tr("Skiff"), &sys.boat_music},
			{tr("Ship"), &sys.ship_music},
			{tr("Airship"), &sys.airship_music},
			{tr("Game Over Screen"), &sys.gameover_music}}) {
		music->addRow(label, SystemFields::audioField(project, *value, page));
	}

	// The value is the index in the list; showing has no "No Transition", like in RPG Maker
	const QStringList erase = {
		tr("Fade Out"), tr("Remove Blocks"), tr("Wipe Downward"), tr("Wipe Upward"),
		tr("Venetian Blinds"), tr("Vertical Blinds"), tr("Horizontal Blinds"),
		tr("Receding Square"), tr("Expanding Square"), tr("Screen Moves Up"),
		tr("Screen Moves Down"), tr("Screen Moves Left"), tr("Screen Moves Right"),
		tr("Vertical Division"), tr("Horizontal Division"), tr("Quadrasection"),
		tr("Zoom In"), tr("Mosaic"), tr("Waver Screen"), tr("Instantaneous"), tr("No Transition")
	};
	const QStringList show = {
		tr("Fade In"), tr("Reconstitute Blocks"), tr("Unwipe Downward"), tr("Unwipe Upward"),
		tr("Venetian Blinds"), tr("Vertical Blinds"), tr("Horizontal Blinds"),
		tr("Receding Square"), tr("Expanding Square"), tr("Screen Moves Down"),
		tr("Screen Moves Up"), tr("Screen Moves Right"), tr("Screen Moves Left"),
		tr("Vertical Unify"), tr("Horizontal Unify"), tr("Unify Quadrants"),
		tr("Zoom Out"), tr("Mosaic"), tr("Waver Screen"), tr("Instantaneous")
	};
	auto* transitions = new QGroupBox(tr("Screen Transitions"), page);
	auto* grid = new QGridLayout(transitions);
	grid->addWidget(new QLabel(tr("Erase Screen"), transitions), 0, 1);
	grid->addWidget(new QLabel(tr("Show Screen"), transitions), 0, 2);
	int row = 1;
	for (auto [label, out, in] : std::initializer_list<std::tuple<QString, int32_t*, int32_t*>>{
			{tr("Map Transitions"), &sys.transition_out, &sys.transition_in},
			{tr("Battle Entry"), &sys.battle_start_fadeout, &sys.battle_start_fadein},
			{tr("Battle Exit"), &sys.battle_end_fadeout, &sys.battle_end_fadein}}) {
		grid->addWidget(new QLabel(label, transitions), row, 0);
		int column = 1;
		for (auto [names, value] : {std::pair{&erase, out}, std::pair{&show, in}}) {
			auto* combo = new QComboBox(transitions);
			combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
			combo->setMinimumContentsLength(8);
			for (int i = 0; i < names->size(); ++i) {
				combo->addItem(names->at(i), i);
			}
			LcfWidgetBinding::connect<int32_t>(this, combo);
			LcfWidgetBinding::bind(combo, *value);
			grid->addWidget(combo, row, column++);
		}
		++row;
	}
	grid->setColumnStretch(1, 1);
	grid->setColumnStretch(2, 1);
	col[1]->addWidget(transitions);

	// Sound effects
	auto* sounds = addGroup(col[2], tr("System Sound Effects"));
	for (auto [label, value] : std::initializer_list<std::pair<QString, lcf::rpg::Sound*>>{
			{tr("Cursor"), &sys.cursor_se},
			{tr("Select Choice"), &sys.decision_se},
			{tr("Cancel"), &sys.cancel_se},
			{tr("Buzzer"), &sys.buzzer_se},
			{tr("Battle Transition"), &sys.battle_se},
			{tr("Flee Battle"), &sys.escape_se},
			{tr("Enemy Attacks"), &sys.enemy_attack_se},
			{tr("Enemy Damaged"), &sys.enemy_damaged_se},
			{tr("Ally Damaged"), &sys.actor_damaged_se},
			{tr("Evade Attack"), &sys.dodge_se},
			{tr("Enemy Defeated"), &sys.enemy_death_se},
			{tr("Item Used"), &sys.item_se}}) {
		sounds->addRow(label, SystemFields::audioField(project, *value, page));
	}

	for (auto* column : col) {
		column->addStretch(1);
	}

	auto* layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(SystemFields::scrollable(page));
}
