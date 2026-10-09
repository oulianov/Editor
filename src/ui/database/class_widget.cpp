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

#include "class_widget.h"
#include "ui_class_widget.h"

#include <QAction>
#include <QGraphicsScene>
#include <QTimer>

#include "class_editors.h"
#include "common/dbstring.h"
#include "common/lcf_widget_binding.h"
#include "model/project_data.h"

namespace {
// RPG Maker 2003 shows six commands, then the Row command
constexpr int kCommandSlots = 6;
}

ClassWidget::ClassWidget(ProjectData& project, QWidget *parent) :
	QWidget(parent),
	ui(new Ui::ClassWidget),
	m_project(project)
{
	ui->setupUi(this);

	const bool is2k3 = project.database().system.ldb_id == 2003;
	const int max_level = is2k3 ? 99 : 50;
	const int max_hp = is2k3 ? 9999 : 999;

	m_battlerItem = new BattleAnimationItem();
	ui->graphicsBattler->setScene(new QGraphicsScene(this));
	ui->graphicsBattler->scene()->addItem(m_battlerItem);
	ui->graphicsBattler->scene()->setSceneRect(0, 0, 48, 48);
	auto* timer = new QTimer(this);
	connect(timer, &QTimer::timeout, ui->graphicsBattler->scene(), &QGraphicsScene::advance);
	timer->start(200);

	const std::array<QColor, 6> colors = {Qt::red, Qt::magenta, Qt::yellow, Qt::green, Qt::darkBlue, Qt::blue};
	std::vector<int16_t> no_values;
	const auto views = curveViews();
	for (size_t i = 0; i < views.size(); ++i) {
		// small graphs that follow the size of the page
		views[i]->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
		views[i]->setMinimumSize(60, 36);
		views[i]->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		views[i]->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		views[i]->setScene(new QGraphicsScene(this));
		// CurveItem fills the scene rect: it must be set (resizeEvent updates it)
		views[i]->scene()->setSceneRect(0, 0, 60, 36);
		m_curves[i] = new CurveItem(colors[i], no_values);
		m_curves[i]->setMaxLevel(max_level);
		m_curves[i]->setMaxValue(i == 0 ? max_hp : 999);
		views[i]->scene()->addItem(m_curves[i]);
	}
	connect(ui->pushEditCurves, &QPushButton::clicked, this, [this, max_level, max_hp] {
		if (ClassEditors::editParameters(this, m_current->parameters, max_level, max_hp)) {
			refreshCurves();
		}
	});

	LcfWidgetBinding::connect(this, ui->lineName);
	for (auto* check : {ui->checkAutoBattle, ui->checkTwoWeapons, ui->checkLockEquipment, ui->checkMightyGuard}) {
		LcfWidgetBinding::connect<bool>(this, check);
	}
	for (auto* spin : {ui->spinExpBase, ui->spinExpInflation, ui->spinExpCorrection}) {
		LcfWidgetBinding::connect<int32_t>(this, spin);
	}
	LcfWidgetBinding::connect<int32_t>(this, ui->comboBattler);
	connect(ui->comboBattler, &QComboBox::currentIndexChanged, this, &ClassWidget::refreshBattler);

	for (auto* combo : {ui->comboBattleCommand1, ui->comboBattleCommand2, ui->comboBattleCommand3,
			ui->comboBattleCommand4, ui->comboBattleCommand5, ui->comboBattleCommand6}) {
		connect(combo, &QComboBox::currentIndexChanged, this, &ClassWidget::writeBattleCommands);
	}

	ui->tableSkills->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	connect(ui->tableSkills, &QTableWidget::cellDoubleClicked, this, [this](int row) { editSkill(row); });
	auto* edit_skill = new QAction(tr("Edit..."), ui->tableSkills);
	edit_skill->setShortcut(Qt::Key_Return);
	edit_skill->setShortcutContext(Qt::WidgetShortcut);
	connect(edit_skill, &QAction::triggered, this, [this] { editSkill(ui->tableSkills->currentRow()); });
	auto* delete_skill = new QAction(tr("Delete"), ui->tableSkills);
	delete_skill->setShortcut(QKeySequence::Delete);
	delete_skill->setShortcutContext(Qt::WidgetShortcut);
	connect(delete_skill, &QAction::triggered, this, [this] {
		ClassEditors::removeSkill(m_current->skills, ui->tableSkills->currentRow());
		refreshSkills();
	});
	ui->tableSkills->addActions({edit_skill, delete_skill});

	connect(ui->listStates, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
		ClassEditors::cycleRank(ui->listStates, m_current->state_ranks, ui->listStates->row(item));
	});
	connect(ui->listAttributes, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
		ClassEditors::cycleRank(ui->listAttributes, m_current->attribute_ranks, ui->listAttributes->row(item));
	});

	setData(nullptr);
}

ClassWidget::~ClassWidget()
{
	delete ui;
}

void ClassWidget::setData(lcf::rpg::Class* cls)
{
	if (!cls) {
		cls = &m_dummy;
	}
	m_current = cls;

	// The other pages of the dialog can rename or add entries: list them again
	const auto& database = m_project.database();
	{
		SignalBlocker blocker(ui->comboBattler);
		ui->comboBattler->clear();
		ui->comboBattler->addItem(tr("<none>"), 0);
		for (const auto& animation : database.battleranimations) {
			ui->comboBattler->addItem(QString("%1: %2").arg(animation.ID, 4, 10, QChar('0')).arg(ToQString(animation.name)), animation.ID);
		}
	}

	LcfWidgetBinding::bind(ui->lineName, cls->name);
	LcfWidgetBinding::bind(ui->checkAutoBattle, cls->auto_battle);
	LcfWidgetBinding::bind(ui->checkTwoWeapons, cls->two_weapon);
	LcfWidgetBinding::bind(ui->checkLockEquipment, cls->lock_equipment);
	LcfWidgetBinding::bind(ui->checkMightyGuard, cls->super_guard);
	LcfWidgetBinding::bind(ui->spinExpBase, cls->exp_base);
	LcfWidgetBinding::bind(ui->spinExpInflation, cls->exp_inflation);
	LcfWidgetBinding::bind(ui->spinExpCorrection, cls->exp_correction);
	LcfWidgetBinding::bind(ui->comboBattler, cls->battler_animation);

	fillBattleCommands();
	ClassEditors::fillRanks(ui->listStates, database.states, cls->state_ranks);
	ClassEditors::fillRanks(ui->listAttributes, database.attributes, cls->attribute_ranks);
	refreshSkills();
	refreshCurves();
	refreshBattler();

	setEnabled(cls != &m_dummy);
}

std::array<QGraphicsView*, 6> ClassWidget::curveViews() const {
	return {ui->graphicsHp, ui->graphicsMp, ui->graphicsAttack, ui->graphicsDefense,
		ui->graphicsIntelligence, ui->graphicsAgility};
}

void ClassWidget::resizeEvent(QResizeEvent* event)
{
	QWidget::resizeEvent(event);
	for (auto* view : curveViews()) {
		view->scene()->setSceneRect(QRectF(QPointF(0, 0), view->viewport()->size()));
	}
}

void ClassWidget::refreshBattler()
{
	const auto& animations = m_project.database().battleranimations;
	const int id = m_current->battler_animation;
	if (id >= 1 && id <= static_cast<int>(animations.size())) {
		m_battlerItem->setDemoAnimation(animations[static_cast<size_t>(id) - 1]);
	} else {
		m_battlerItem->setBasePix(BattleAnimationItem::Battler, "");
	}
}

void ClassWidget::refreshCurves()
{
	auto& parameters = m_current->parameters;
	m_curves[0]->setData(parameters.maxhp);
	m_curves[1]->setData(parameters.maxsp);
	m_curves[2]->setData(parameters.attack);
	m_curves[3]->setData(parameters.defense);
	m_curves[4]->setData(parameters.spirit);
	m_curves[5]->setData(parameters.agility);
}

void ClassWidget::refreshSkills()
{
	ClassEditors::fillSkills(ui->tableSkills, m_current->skills, m_project);
}

void ClassWidget::editSkill(int row)
{
	const int max_level = m_project.database().system.ldb_id == 2003 ? 99 : 50;
	if (ClassEditors::editSkill(this, m_project, m_current->skills, row, max_level)) {
		refreshSkills();
	}
}

void ClassWidget::fillBattleCommands()
{
	const auto& commands = m_project.database().battlecommands.commands;
	// The commands before the Row command (0); -1 pads the list
	std::vector<int32_t> ids;
	for (int32_t id : m_current->battle_commands) {
		if (id > 0) {
			ids.push_back(id);
		}
	}

	int slot = 0;
	for (auto* combo : {ui->comboBattleCommand1, ui->comboBattleCommand2, ui->comboBattleCommand3,
			ui->comboBattleCommand4, ui->comboBattleCommand5, ui->comboBattleCommand6}) {
		SignalBlocker blocker(combo);
		combo->clear();
		combo->addItem(tr("<none>"), 0);
		for (const auto& command : commands) {
			combo->addItem(QString("%1: %2").arg(command.ID, 4, 10, QChar('0')).arg(ToQString(command.name)), command.ID);
		}
		const int id = slot < static_cast<int>(ids.size()) ? ids[static_cast<size_t>(slot)] : 0;
		if (combo->findData(id) < 0) {
			combo->addItem(tr("<%1?>").arg(id), id);
		}
		combo->setCurrentIndex(combo->findData(id));
		++slot;
	}
	Q_ASSERT(slot == kCommandSlots);
}

void ClassWidget::writeBattleCommands()
{
	std::vector<int32_t> ids;
	for (auto* combo : {ui->comboBattleCommand1, ui->comboBattleCommand2, ui->comboBattleCommand3,
			ui->comboBattleCommand4, ui->comboBattleCommand5, ui->comboBattleCommand6}) {
		const int id = combo->currentData().toInt();
		if (id > 0) {
			ids.push_back(id);
		}
	}
	// Like RPG Maker: the Row command (0) ends the list, padded to 7 entries with -1
	ids.push_back(0);
	ids.resize(std::max<size_t>(ids.size(), kCommandSlots + 1), -1);
	m_current->battle_commands = ids;
}
