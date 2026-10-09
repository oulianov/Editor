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

#include "terrain_widget.h"
#include "ui_terrain_widget.h"

#include <QButtonGroup>

#include "common/dbstring.h"
#include "common/lcf_widget_binding.h"
#include "defines.h"
#include "model/project_data.h"
#include "ui/picker/picker_backdrop_widget.h"
#include "ui/picker/picker_dialog.h"

namespace {
QButtonGroup* makeGroup(QWidget* parent, std::initializer_list<QAbstractButton*> buttons) {
	auto* group = new QButtonGroup(parent);
	int id = 0;
	for (auto* button : buttons) {
		group->addButton(button, id++);
	}
	return group;
}
}

TerrainWidget::TerrainWidget(ProjectData& project, QWidget *parent) :
	QWidget(parent),
	ui(new Ui::TerrainWidget),
	m_project(project)
{
	ui->setupUi(this);

	m_bushDepth = makeGroup(this, {ui->radioBushNormal, ui->radioBushThird, ui->radioBushHalf, ui->radioBushFull});
	m_gridLocation = makeGroup(this, {ui->radioGridShallow, ui->radioGridDeep});
	m_backgroundType = makeGroup(this, {ui->radioBackground, ui->radioFrame});

	LcfWidgetBinding::connect(this, ui->lineName);
	for (auto* spin : {ui->spinDamage, ui->spinEncounterRate,
			ui->spinInitiative, ui->spinBackAttack, ui->spinSurroundAttack, ui->spinPincersAttack,
			ui->spinFrameBackScrollH, ui->spinFrameBackScrollV, ui->spinFrameForeScrollH, ui->spinFrameForeScrollV}) {
		LcfWidgetBinding::connect<int32_t>(this, spin);
	}
	for (auto* check : {ui->checkBoatPass, ui->checkShipPass, ui->checkAirshipPass, ui->checkAirshipLand,
			ui->checkOnDamageSe,
			ui->checkInitiative, ui->checkBackAttack, ui->checkSurroundAttack, ui->checkPincersAttack,
			ui->checkFrameBackScrollH, ui->checkFrameBackScrollV,
			ui->checkFrameFore, ui->checkFrameForeScrollH, ui->checkFrameForeScrollV}) {
		LcfWidgetBinding::connect<bool>(this, check);
		connect(check, &QCheckBox::toggled, this, &TerrainWidget::updateEnabled);
	}
	for (auto* group : {m_bushDepth, m_gridLocation, m_backgroundType}) {
		LcfWidgetBinding::connect<int32_t>(this, group);
	}
	connect(m_backgroundType, &QButtonGroup::idToggled, this, &TerrainWidget::updateEnabled);

	ui->lineFootstep->makeModel(project);

	connect(ui->toolBackdrop, &QToolButton::clicked, this, [this] {
		pickImage(BACKDROP, m_current->background_name, ui->lineBackdrop);
	});
	connect(ui->toolFrameBack, &QToolButton::clicked, this, [this] {
		pickImage(FRAME, m_current->background_a_name, ui->lineFrameBack);
	});
	connect(ui->toolFrameFore, &QToolButton::clicked, this, [this] {
		pickImage(FRAME, m_current->background_b_name, ui->lineFrameFore);
	});

	setData(nullptr);
}

TerrainWidget::~TerrainWidget()
{
	delete ui;
}

void TerrainWidget::setData(lcf::rpg::Terrain* terrain)
{
	if (!terrain) {
		terrain = &m_dummy;
	}
	m_current = terrain;

	LcfWidgetBinding::bind(ui->lineName, terrain->name);
	LcfWidgetBinding::bind(ui->spinDamage, terrain->damage);
	LcfWidgetBinding::bind(ui->spinEncounterRate, terrain->encounter_rate);
	LcfWidgetBinding::bind(m_bushDepth, terrain->bush_depth);
	LcfWidgetBinding::bind(m_gridLocation, terrain->grid_location);

	LcfWidgetBinding::bind(ui->checkBoatPass, terrain->boat_pass);
	LcfWidgetBinding::bind(ui->checkShipPass, terrain->ship_pass);
	LcfWidgetBinding::bind(ui->checkAirshipPass, terrain->airship_pass);
	LcfWidgetBinding::bind(ui->checkAirshipLand, terrain->airship_land);

	ui->lineFootstep->bindSound(terrain->footstep);
	LcfWidgetBinding::bind(ui->checkOnDamageSe, terrain->on_damage_se);

	LcfWidgetBinding::bind(ui->checkInitiative, terrain->special_flags.back_party);
	LcfWidgetBinding::bind(ui->spinInitiative, terrain->special_back_party);
	LcfWidgetBinding::bind(ui->checkBackAttack, terrain->special_flags.back_enemies);
	LcfWidgetBinding::bind(ui->spinBackAttack, terrain->special_back_enemies);
	LcfWidgetBinding::bind(ui->checkSurroundAttack, terrain->special_flags.lateral_party);
	LcfWidgetBinding::bind(ui->spinSurroundAttack, terrain->special_lateral_party);
	LcfWidgetBinding::bind(ui->checkPincersAttack, terrain->special_flags.lateral_enemies);
	LcfWidgetBinding::bind(ui->spinPincersAttack, terrain->special_lateral_enemies);

	LcfWidgetBinding::bind(m_backgroundType, terrain->background_type);
	ui->lineBackdrop->setText(ToQString(terrain->background_name));
	ui->lineFrameBack->setText(ToQString(terrain->background_a_name));
	LcfWidgetBinding::bind(ui->checkFrameBackScrollH, terrain->background_a_scrollh);
	LcfWidgetBinding::bind(ui->spinFrameBackScrollH, terrain->background_a_scrollh_speed);
	LcfWidgetBinding::bind(ui->checkFrameBackScrollV, terrain->background_a_scrollv);
	LcfWidgetBinding::bind(ui->spinFrameBackScrollV, terrain->background_a_scrollv_speed);
	LcfWidgetBinding::bind(ui->checkFrameFore, terrain->background_b);
	ui->lineFrameFore->setText(ToQString(terrain->background_b_name));
	LcfWidgetBinding::bind(ui->checkFrameForeScrollH, terrain->background_b_scrollh);
	LcfWidgetBinding::bind(ui->spinFrameForeScrollH, terrain->background_b_scrollh_speed);
	LcfWidgetBinding::bind(ui->checkFrameForeScrollV, terrain->background_b_scrollv);
	LcfWidgetBinding::bind(ui->spinFrameForeScrollV, terrain->background_b_scrollv_speed);

	setEnabled(terrain != &m_dummy);
	updateEnabled();
}

void TerrainWidget::pickImage(const QString& folder, lcf::DBString& name, QLineEdit* line)
{
	auto* widget = new PickerBackdropWidget(this);
	PickerDialog dialog(m_project, FileFinder::FileType::Image, widget, this);
	connect(&dialog, &PickerDialog::fileSelected, this, [&name, line](const QString& baseName) {
		name = ToDBString(baseName);
		line->setText(baseName);
	});
	dialog.setDirectoryAndFile(folder, ToQString(name));
	dialog.exec();
}

void TerrainWidget::updateEnabled()
{
	// Frames, footsteps, special battles and the grid came with RPG Maker 2003;
	// RPG Maker 2000 only has the battle background
	const bool is2k3 = m_project.database().system.ldb_id == 2003;
	const bool frame = is2k3 && ui->radioFrame->isChecked();

	for (auto* widget : std::initializer_list<QWidget*>{ui->radioBackground, ui->radioFrame,
			ui->groupSound, ui->groupSpecialBattle, ui->groupGrid}) {
		widget->setEnabled(is2k3);
	}
	ui->lineBackdrop->setEnabled(!frame);
	ui->toolBackdrop->setEnabled(!frame);
	ui->groupFrameBack->setEnabled(frame);
	ui->groupFrameFore->setEnabled(frame);

	const bool foreground = ui->checkFrameFore->isChecked();
	for (auto* widget : std::initializer_list<QWidget*>{ui->lineFrameFore, ui->toolFrameFore,
			ui->checkFrameForeScrollH, ui->checkFrameForeScrollV}) {
		widget->setEnabled(foreground);
	}
	ui->spinFrameBackScrollH->setEnabled(ui->checkFrameBackScrollH->isChecked());
	ui->spinFrameBackScrollV->setEnabled(ui->checkFrameBackScrollV->isChecked());
	ui->spinFrameForeScrollH->setEnabled(foreground && ui->checkFrameForeScrollH->isChecked());
	ui->spinFrameForeScrollV->setEnabled(foreground && ui->checkFrameForeScrollV->isChecked());

	ui->spinInitiative->setEnabled(ui->checkInitiative->isChecked());
	ui->spinBackAttack->setEnabled(ui->checkBackAttack->isChecked());
	ui->spinSurroundAttack->setEnabled(ui->checkSurroundAttack->isChecked());
	ui->spinPincersAttack->setEnabled(ui->checkPincersAttack->isChecked());
}
