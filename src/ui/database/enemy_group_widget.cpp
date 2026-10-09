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

#include "enemy_group_widget.h"
#include "ui_enemy_group_widget.h"

#include <QButtonGroup>
#include <algorithm>
#include "common/dbstring.h"
#include "common/image_loader.h"
#include "common/lcf_widget_binding.h"
#include "common/signal_blocker.h"
#include "defines.h"
#include "model/project.h"
#include "model/project_data.h"
#include "ui/event/battle_page_widget.h"
#include "ui/picker/picker_backdrop_widget.h"
#include "ui/picker/picker_dialog.h"

std::optional<lcf::rpg::TroopPage> EnemyGroupWidget::s_pageClipboard;

namespace {
	// RPG Maker's limit of monsters in a group
	constexpr int kMaxMembers = 8;

	template<typename T>
	void renumber(std::vector<T>& list) {
		for (size_t i = 0; i < list.size(); ++i) {
			list[i].ID = static_cast<int>(i) + 1;
		}
	}
}

EnemyGroupWidget::EnemyGroupWidget(ProjectData& project, QWidget *parent) :
	QWidget(parent),
	ui(new Ui::EnemyGroupWidget),
	m_project(project)
{
	ui->setupUi(this);
	ui->splitter->setStretchFactor(0, 1);
	ui->splitter->setStretchFactor(1, 1);

	auto& database = project.database();
	const bool isRPG2k3 = database.system.ldb_id == 2003;

	for (const auto& enemy : database.enemies) {
		ui->listEnemies->addItem(QStringLiteral("%1: %2").arg(enemy.ID, 4, 10, QChar('0')).arg(ToQString(enemy.name)));
	}
	for (const auto& terrain : database.terrains) {
		auto* item = new QListWidgetItem(ToQString(terrain.name), ui->listTerrains);
		item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
		item->setCheckState(Qt::Checked);
	}

	LcfWidgetBinding::connect(this, ui->lineName);

	// Automatic alignment and random appearance only exist in RPG Maker 2003
	m_alignment = new QButtonGroup(this);
	m_alignment->addButton(ui->radioManual, 0);
	m_alignment->addButton(ui->radioAutomatic, 1);
	LcfWidgetBinding::connect<bool>(this, m_alignment);
	LcfWidgetBinding::connect<bool>(this, ui->checkRandom);
	ui->groupAlignment->setEnabled(isRPG2k3);
	ui->checkRandom->setEnabled(isRPG2k3);

	connect(ui->pushAutoName, &QPushButton::clicked, this, &EnemyGroupWidget::autoName);
	connect(ui->pushAdd, &QPushButton::clicked, this, &EnemyGroupWidget::addMember);
	connect(ui->listEnemies, &QListWidget::itemDoubleClicked, this, &EnemyGroupWidget::addMember);
	connect(ui->pushRemove, &QPushButton::clicked, this, &EnemyGroupWidget::removeMember);
	connect(ui->pushRemoveAll, &QPushButton::clicked, this, &EnemyGroupWidget::removeAllMembers);
	connect(ui->pushAutoAlign, &QPushButton::clicked, this, &EnemyGroupWidget::autoAlign);
	connect(ui->pushChangeBackdrop, &QPushButton::clicked, this, &EnemyGroupWidget::changeBackdrop);
	connect(ui->listTerrains, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) {
		terrainChanged(ui->listTerrains->row(item), item->checkState() == Qt::Checked);
	});

	connect(ui->pushNewPage, &QPushButton::clicked, this, &EnemyGroupWidget::newPage);
	connect(ui->pushCopyPage, &QPushButton::clicked, this, &EnemyGroupWidget::copyPage);
	connect(ui->pushPastePage, &QPushButton::clicked, this, &EnemyGroupWidget::pastePage);
	connect(ui->pushDeletePage, &QPushButton::clicked, this, &EnemyGroupWidget::deletePage);

	if (!database.enemies.empty()) {
		ui->listEnemies->setCurrentRow(0);
	}
	setData(nullptr);
}

EnemyGroupWidget::~EnemyGroupWidget() {
	delete ui;
}

void EnemyGroupWidget::setData(lcf::rpg::Troop* troop) {
	m_current = troop ? troop : &m_dummy;
	auto& t = *m_current;

	LcfWidgetBinding::bind(ui->lineName, t.name);
	LcfWidgetBinding::bind(m_alignment, t.auto_alignment);
	LcfWidgetBinding::bind(ui->checkRandom, t.appear_randomly);

	{
		// Terrains past the end of the set are allowed (RPG_RT leaves them out)
		SignalBlocker blocker(ui->listTerrains);
		for (int i = 0; i < ui->listTerrains->count(); ++i) {
			bool allowed = i >= static_cast<int>(t.terrain_set.size()) || t.terrain_set[i];
			ui->listTerrains->item(i)->setCheckState(allowed ? Qt::Checked : Qt::Unchecked);
		}
	}

	ui->graphicsTroop->setData(&m_project, m_current);
	refreshPages(0);

	setEnabled(m_current != &m_dummy);
}

void EnemyGroupWidget::autoName() {
	// "Slime*2, Bat": each monster once, in the order of the group
	const auto& enemies = m_project.database().enemies;
	std::vector<std::pair<int, int>> counts;
	for (const auto& member : m_current->members) {
		auto it = std::find_if(counts.begin(), counts.end(), [&](const auto& c) { return c.first == member.enemy_id; });
		if (it == counts.end()) {
			counts.emplace_back(member.enemy_id, 1);
		} else {
			++it->second;
		}
	}

	QStringList parts;
	for (const auto& [id, count] : counts) {
		QString name = id >= 1 && id <= static_cast<int>(enemies.size()) ? ToQString(enemies[id - 1].name) : QString();
		parts << (count > 1 ? QStringLiteral("%1*%2").arg(name).arg(count) : name);
	}
	// Through the line edit, which writes the name
	ui->lineName->setText(parts.join(QStringLiteral(", ")));
}

void EnemyGroupWidget::addMember() {
	auto& members = m_current->members;
	int row = ui->listEnemies->currentRow();
	if (m_current == &m_dummy || row < 0 || static_cast<int>(members.size()) >= kMaxMembers) {
		return;
	}

	lcf::rpg::TroopMember member;
	member.ID = static_cast<int>(members.size()) + 1;
	member.enemy_id = row + 1;
	member.x = 160;
	member.y = 120;
	members.push_back(member);

	ui->graphicsTroop->refresh();
	ui->graphicsTroop->selectMember(member.ID - 1);
	updateTriggers();
}

void EnemyGroupWidget::removeMember() {
	auto& members = m_current->members;
	if (members.empty()) {
		return;
	}
	// The selected monster, else the last one
	int index = ui->graphicsTroop->selectedMember();
	if (index < 0) {
		index = static_cast<int>(members.size()) - 1;
	}
	members.erase(members.begin() + index);
	renumber(members);
	ui->graphicsTroop->refresh();
	updateTriggers();
}

void EnemyGroupWidget::removeAllMembers() {
	m_current->members.clear();
	ui->graphicsTroop->refresh();
	updateTriggers();
}

void EnemyGroupWidget::autoAlign() {
	// One row, or two staggered rows from five monsters on
	auto& members = m_current->members;
	const int count = static_cast<int>(members.size());
	const int front = count > 4 ? (count + 1) / 2 : count;
	for (int i = 0; i < count; ++i) {
		const bool back = i >= front;
		const int in_row = back ? count - front : front;
		const int pos = back ? i - front : i;
		members[i].x = 320 * (2 * pos + 1 + (back ? 1 : 0)) / (2 * in_row + 1);
		members[i].y = count > 4 ? (back ? 150 : 100) : 120;
	}
	ui->graphicsTroop->refresh();
}

void EnemyGroupWidget::changeBackdrop() {
	auto* widget = new PickerBackdropWidget(this);
	PickerDialog dialog(m_project, FileFinder::FileType::Image, widget, this);
	connect(&dialog, &PickerDialog::fileSelected, this, [this](const QString& baseName) {
		m_backdrop = baseName;
		QString path = m_project.project().findFile(BACKDROP, baseName, FileFinder::FileType::Image);
		ui->graphicsTroop->setBackdrop(ImageLoader::Load(path));
	});
	dialog.setDirectoryAndFile(BACKDROP, m_backdrop);
	dialog.exec();
}

void EnemyGroupWidget::terrainChanged(int row, bool checked) {
	auto& set = m_current->terrain_set;
	if (row >= static_cast<int>(set.size())) {
		// Grow the set: the terrains it did not list were allowed
		lcf::DBBitArray grown(ui->listTerrains->count(), true);
		for (size_t i = 0; i < set.size(); ++i) {
			grown[i] = set[i];
		}
		set = std::move(grown);
	}
	set[row] = checked;
}

void EnemyGroupWidget::refreshPages(int current) {
	while (ui->tabPages->count() > 0) {
		QWidget* page = ui->tabPages->widget(0);
		ui->tabPages->removeTab(0);
		delete page;
	}

	auto& pages = m_current->pages;
	for (auto& page : pages) {
		auto* tab = new BattlePageWidget(ui->tabPages);
		tab->setEventPage(m_project, *m_current, &page);
		ui->tabPages->addTab(tab, QString::number(page.ID));
	}
	ui->tabPages->setCurrentIndex(std::min(current, ui->tabPages->count() - 1));
}

void EnemyGroupWidget::updateTriggers() {
	for (int i = 0; i < ui->tabPages->count(); ++i) {
		static_cast<BattlePageWidget*>(ui->tabPages->widget(i))->updateTrigger();
	}
}

void EnemyGroupWidget::newPage() {
	auto& pages = m_current->pages;
	pages.emplace_back();
	renumber(pages);
	refreshPages(static_cast<int>(pages.size()) - 1);
}

void EnemyGroupWidget::copyPage() {
	int index = ui->tabPages->currentIndex();
	if (index >= 0) {
		s_pageClipboard = m_current->pages[index];
	}
}

void EnemyGroupWidget::pastePage() {
	if (!s_pageClipboard || m_current == &m_dummy) {
		return;
	}
	// After the current page
	auto& pages = m_current->pages;
	int index = std::min(ui->tabPages->currentIndex() + 1, static_cast<int>(pages.size()));
	pages.insert(pages.begin() + index, *s_pageClipboard);
	renumber(pages);
	refreshPages(index);
}

void EnemyGroupWidget::deletePage() {
	auto& pages = m_current->pages;
	int index = ui->tabPages->currentIndex();
	if (index < 0) {
		return;
	}
	// A group keeps one page: the last one is emptied instead
	if (pages.size() > 1) {
		pages.erase(pages.begin() + index);
	} else {
		pages[0] = lcf::rpg::TroopPage();
	}
	renumber(pages);
	refreshPages(index);
}
