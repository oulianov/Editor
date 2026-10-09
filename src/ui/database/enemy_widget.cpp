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

#include "enemy_widget.h"
#include "database_list_data.h"
#include "common/signal_blocker.h"
#include "ui_enemy_widget.h"

#include <QAction>
#include <algorithm>
#include <QGraphicsPixmapItem>
#include <QHeaderView>
#include <QMenu>
#include "common/dbstring.h"
#include "common/lcf_widget_binding.h"
#include "defines.h"
#include "model/project_data.h"
#include "ui/picker/picker_dialog.h"
#include "enemy_action_dialog.h"
#include "enemy_graphics.h"
#include "enemy_picker_widget.h"

std::optional<lcf::rpg::EnemyAction> EnemyWidget::s_clipboard;

EnemyWidget::EnemyWidget(ProjectData& project, QWidget *parent) :
	QWidget(parent),
	ui(new Ui::EnemyWidget),
	m_project(project)
{
	ui->setupUi(this);

	auto& database = project.database();
	const bool isRPG2k3 = database.system.ldb_id == 2003;

	// Limits of RPG Maker's monster editor
	ui->spinMaxHp->setRange(1, isRPG2k3 ? 99999 : 9999);
	ui->spinMaxSp->setRange(0, isRPG2k3 ? 9999 : 999);
	for (auto* spin : {ui->spinAttack, ui->spinDefense, ui->spinSpirit, ui->spinAgility}) {
		spin->setRange(1, 999);
	}
	ui->spinExp->setRange(0, 999999);
	ui->spinGold->setRange(0, 999999);

	fillLists();
	// Array Size, a paste or a rename in another tab: list them again
	connect(&DatabaseListNotifier::instance(), &DatabaseListNotifier::listChanged, this, [this](const void* list) {
		const auto& database = m_project.database();
		if (list == &database.items || list == &database.attributes || list == &database.states) {
			fillLists();
			setData(m_current == &m_dummy ? nullptr : m_current);
		}
	});

	LcfWidgetBinding::connect(this, ui->lineName);
	for (auto* spin : {
			ui->spinMaxHp, ui->spinMaxSp, ui->spinAttack, ui->spinDefense, ui->spinSpirit, ui->spinAgility,
			ui->spinExp, ui->spinGold, ui->spinDropProb, ui->spinCritChance }) {
		LcfWidgetBinding::connect<int32_t>(this, spin);
	}
	LcfWidgetBinding::connect<int32_t>(this, ui->comboDropItem);
	for (auto* check : {ui->checkTransparent, ui->checkFlying, ui->checkCritical, ui->checkMiss}) {
		LcfWidgetBinding::connect<bool>(this, check);
	}

	// Graphic
	m_graphicItem = new QGraphicsPixmapItem();
	ui->graphicsMonster->setItem(m_graphicItem);
	// the viewport: the view gets its resize before its viewport is resized
	ui->graphicsMonster->viewport()->installEventFilter(this);
	connect(ui->graphicsMonster, &RpgGraphicsViewBase::clicked, this, &EnemyWidget::chooseGraphic);
	connect(ui->pushSetGraphic, &QPushButton::clicked, this, &EnemyWidget::chooseGraphic);
	connect(ui->checkTransparent, &QCheckBox::toggled, this, [this](bool checked) {
		m_graphicItem->setOpacity(checked ? 0.6 : 1.0);
	});
	connect(ui->checkCritical, &QCheckBox::toggled, ui->spinCritChance, &QWidget::setEnabled);

	// Behavior list: double-click or Return edits, like RPG Maker's list
	ui->treeActions->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	ui->treeActions->header()->setSectionResizeMode(1, QHeaderView::Stretch);
	ui->treeActions->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
	connect(ui->treeActions, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item) {
		editAction(ui->treeActions->indexOfTopLevelItem(item));
	});

	auto addAction = [this](const QString& text, const QKeySequence& key, auto slot) {
		auto* action = new QAction(text, ui->treeActions);
		action->setShortcut(key);
		action->setShortcutContext(Qt::WidgetShortcut);
		connect(action, &QAction::triggered, this, slot);
		ui->treeActions->addAction(action);
		return action;
	};
	auto* actEdit = addAction(tr("Edit..."), QKeySequence(Qt::Key_Return), [this] { editAction(currentActionIndex()); });
	auto* actCut = addAction(tr("Cut"), QKeySequence::Cut, [this] { copyAction(true); });
	auto* actCopy = addAction(tr("Copy"), QKeySequence::Copy, [this] { copyAction(false); });
	auto* actPaste = addAction(tr("Paste"), QKeySequence::Paste, [this] { pasteAction(); });
	auto* actDelete = addAction(tr("Delete"), QKeySequence::Delete, [this] { deleteAction(); });
	connect(ui->treeActions, &QWidget::customContextMenuRequested, this, [this, actEdit, actCut, actCopy, actPaste, actDelete](const QPoint& pos) {
		const bool onAction = currentActionIndex() < static_cast<int>(m_current->actions.size());
		actCut->setEnabled(onAction);
		actCopy->setEnabled(onAction);
		actDelete->setEnabled(onAction);
		actPaste->setEnabled(s_clipboard.has_value());
		QMenu menu(this);
		menu.addActions({actEdit, actCut, actCopy, actPaste, actDelete});
		menu.exec(ui->treeActions->viewport()->mapToGlobal(pos));
	});

	// Resistances: a click gives the next rank
	connect(ui->listAttributeRanks, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
		cycleRank(ui->listAttributeRanks, m_current->attribute_ranks, ui->listAttributeRanks->row(item));
	});
	connect(ui->listStateRanks, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
		cycleRank(ui->listStateRanks, m_current->state_ranks, ui->listStateRanks->row(item));
	});

	setData(nullptr);
}

EnemyWidget::~EnemyWidget()
{
	delete ui;
}

void EnemyWidget::fillLists() {
	const auto& database = m_project.database();
	SignalBlocker blocker(ui->comboDropItem);
	ui->comboDropItem->clear();
	ui->listAttributeRanks->clear();
	ui->listStateRanks->clear();
	ui->comboDropItem->addItem(tr("(None)"), 0);
	for (const auto& item : database.items) {
		ui->comboDropItem->addItem(QStringLiteral("%1: %2").arg(item.ID, 4, 10, QChar('0')).arg(ToQString(item.name)), item.ID);
	}
	for (const auto& attribute : database.attributes) {
		ui->listAttributeRanks->addItem(ToQString(attribute.name));
	}
	for (const auto& state : database.states) {
		ui->listStateRanks->addItem(ToQString(state.name));
	}
}

void EnemyWidget::setData(lcf::rpg::Enemy* enemy)
{
	m_current = enemy ? enemy : &m_dummy;
	auto& e = *m_current;

	LcfWidgetBinding::bind(ui->lineName, e.name);
	LcfWidgetBinding::bind(ui->spinMaxHp, e.max_hp);
	LcfWidgetBinding::bind(ui->spinMaxSp, e.max_sp);
	LcfWidgetBinding::bind(ui->spinAttack, e.attack);
	LcfWidgetBinding::bind(ui->spinDefense, e.defense);
	LcfWidgetBinding::bind(ui->spinSpirit, e.spirit);
	LcfWidgetBinding::bind(ui->spinAgility, e.agility);
	LcfWidgetBinding::bind(ui->spinExp, e.exp);
	LcfWidgetBinding::bind(ui->spinGold, e.gold);
	LcfWidgetBinding::bind(ui->comboDropItem, e.drop_id);
	LcfWidgetBinding::bind(ui->spinDropProb, e.drop_prob);
	LcfWidgetBinding::bind(ui->checkTransparent, e.transparent);
	LcfWidgetBinding::bind(ui->checkFlying, e.levitate);
	LcfWidgetBinding::bind(ui->checkCritical, e.critical_hit);
	LcfWidgetBinding::bind(ui->spinCritChance, e.critical_hit_chance);
	LcfWidgetBinding::bind(ui->checkMiss, e.miss);
	ui->spinCritChance->setEnabled(e.critical_hit);

	refreshGraphic();
	refreshActions();
	refreshRanks(ui->listAttributeRanks, e.attribute_ranks);
	refreshRanks(ui->listStateRanks, e.state_ranks);

	setEnabled(m_current != &m_dummy);
}

bool EnemyWidget::eventFilter(QObject* watched, QEvent* event) {
	if (watched == ui->graphicsMonster->viewport() && event->type() == QEvent::Resize) {
		fitGraphic();
	}
	return QWidget::eventFilter(watched, event);
}

void EnemyWidget::chooseGraphic() {
	if (m_current == &m_dummy) {
		return;
	}

	auto* widget = new EnemyPickerWidget(m_current->battler_hue, this);
	PickerDialog dialog(m_project, FileFinder::FileType::Image, widget, this);
	dialog.setWindowTitle(tr("Select Monster Graphic"));
	connect(&dialog, &PickerDialog::fileSelected, this, [&](const QString& baseName) {
		m_current->battler_name = ToDBString(baseName);
		m_current->battler_hue = widget->hue();
		refreshGraphic();
	});
	dialog.setDirectoryAndFile(MONSTER, ToQString(m_current->battler_name));
	dialog.exec();
}

void EnemyWidget::refreshGraphic() {
	m_graphicItem->setPixmap(EnemyGraphics::load(m_project, *m_current));
	m_graphicItem->setOpacity(m_current->transparent ? 0.6 : 1.0);
	ui->graphicsMonster->setItem(m_graphicItem);
	fitGraphic();
}

void EnemyWidget::fitGraphic() {
	// Large monsters are shown smaller, small ones at their size
	auto* view = ui->graphicsMonster;
	view->resetTransform();
	QRectF rect = m_graphicItem->boundingRect();
	// centered on this monster, not on the largest one shown before
	view->setSceneRect(rect);
	QSize area = view->viewport()->size();
	if (rect.width() > area.width() || rect.height() > area.height()) {
		view->fitInView(m_graphicItem, Qt::KeepAspectRatio);
	}
}

void EnemyWidget::refreshActions(int select) {
	auto* tree = ui->treeActions;
	tree->clear();
	for (const auto& action : m_current->actions) {
		auto* item = new QTreeWidgetItem(tree);
		item->setText(0, EnemyActionDialog::actionText(m_project, action));
		item->setText(1, EnemyActionDialog::conditionText(m_project, action));
		item->setText(2, QString::number(action.rating));
	}
	// The empty line where new actions are added
	new QTreeWidgetItem(tree);
	if (select >= 0) {
		tree->setCurrentItem(tree->topLevelItem(std::min(select, tree->topLevelItemCount() - 1)));
	}
}

int EnemyWidget::currentActionIndex() const {
	auto* item = ui->treeActions->currentItem();
	return item ? ui->treeActions->indexOfTopLevelItem(item) : static_cast<int>(m_current->actions.size());
}

void EnemyWidget::editAction(int index) {
	auto& actions = m_current->actions;
	if (m_current == &m_dummy || index < 0) {
		return;
	}

	const bool isNew = index >= static_cast<int>(actions.size());
	lcf::rpg::EnemyAction action;
	if (isNew) {
		action.basic = lcf::rpg::EnemyAction::Basic_attack;
	} else {
		action = actions[index];
	}

	EnemyActionDialog dialog(m_project, action, this);
	if (dialog.exec() != QDialog::Accepted) {
		return;
	}

	if (isNew) {
		index = static_cast<int>(actions.size());
		action.ID = index + 1;
		actions.push_back(action);
	} else {
		actions[index] = action;
	}
	refreshActions(index);
}

void EnemyWidget::copyAction(bool cut) {
	int index = currentActionIndex();
	if (index >= static_cast<int>(m_current->actions.size())) {
		return;
	}
	s_clipboard = m_current->actions[index];
	if (cut) {
		deleteAction();
	}
}

void EnemyWidget::pasteAction() {
	if (!s_clipboard || m_current == &m_dummy) {
		return;
	}
	auto& actions = m_current->actions;
	// Before the current line, like RPG Maker
	int index = std::min(currentActionIndex(), static_cast<int>(actions.size()));
	actions.insert(actions.begin() + index, *s_clipboard);
	for (size_t i = 0; i < actions.size(); ++i) {
		actions[i].ID = static_cast<int>(i) + 1;
	}
	refreshActions(index);
}

void EnemyWidget::deleteAction() {
	auto& actions = m_current->actions;
	int index = currentActionIndex();
	if (index >= static_cast<int>(actions.size())) {
		return;
	}
	actions.erase(actions.begin() + index);
	for (size_t i = 0; i < actions.size(); ++i) {
		actions[i].ID = static_cast<int>(i) + 1;
	}
	refreshActions(index);
}

void EnemyWidget::refreshRanks(QListWidget* list, const std::vector<uint8_t>& ranks) {
	for (int i = 0; i < list->count(); ++i) {
		// Entries past the end of the list have the default rank, C
		int rank = i < static_cast<int>(ranks.size()) ? ranks[i] : 2;
		list->item(i)->setIcon(QIcon(QStringLiteral(":/ranks/rank%1").arg(rank)));
	}
}

void EnemyWidget::cycleRank(QListWidget* list, std::vector<uint8_t>& ranks, int row) {
	if (m_current == &m_dummy || row < 0) {
		return;
	}
	if (static_cast<int>(ranks.size()) <= row) {
		ranks.resize(row + 1, 2);
	}
	ranks[row] = (ranks[row] + 1) % 5;
	refreshRanks(list, ranks);
}
