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

#include "encounter_list.h"
#include "common/dbstring.h"
#include "model/project_data.h"
#include "ui/common/rpg_combobox.h"
#include <QAction>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHeaderView>
#include <QMenu>
#include "ui/common/edit_actions.h"
#include <QMessageBox>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
// Shared by the map and area dialogs, like RPG Maker's clipboard
std::vector<int> encounter_clipboard;

QString troopName(ProjectData& project, int troop_id) {
	const auto& troops = project.database().troops;
	// ids of deleted monster groups are kept, not dropped
	const QString name = troop_id >= 1 && troop_id <= static_cast<int>(troops.size())
		? ToQString(troops[static_cast<size_t>(troop_id - 1)].name)
		: QObject::tr("<missing monster group>");
	return QString("%1: %2").arg(troop_id, 4, 10, QChar('0')).arg(name);
}
}

EncounterList::EncounterList(ProjectData& project, QTableWidget* table, QObject* parent) :
	QObject(parent), m_project(project), m_table(table)
{
	m_table->setColumnCount(1);
	m_table->setHorizontalHeaderLabels({tr("Monster Groups")});
	m_table->horizontalHeader()->setStretchLastSection(true);
	m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_table->setContextMenuPolicy(Qt::CustomContextMenu);

	// only while the list has the focus: the dialog has other fields
	auto makeAction = [this](EditActions::Kind kind, void (EncounterList::*slot)()) {
		auto* action = EditActions::create(kind, m_table);
		connect(action, &QAction::triggered, this, slot);
		return action;
	};
	using EditActions::Kind;
	QAction* edit = makeAction(Kind::Edit, &EncounterList::edit);
	QAction* cut = makeAction(Kind::Cut, &EncounterList::cut);
	QAction* copy = makeAction(Kind::Copy, &EncounterList::copy);
	m_paste = makeAction(Kind::Paste, &EncounterList::paste);
	QAction* remove = makeAction(Kind::Delete, &EncounterList::remove);

	connect(m_table, &QTableWidget::cellDoubleClicked, this, &EncounterList::edit);
	connect(m_table, &QWidget::customContextMenuRequested, this, [=, this](const QPoint& pos) {
		const bool selected = !selectedRows().isEmpty();
		cut->setEnabled(selected);
		copy->setEnabled(selected);
		remove->setEnabled(selected);
		m_paste->setEnabled(!encounter_clipboard.empty());
		QMenu menu(m_table);
		menu.addAction(edit);
		menu.addSeparator();
		menu.addActions({cut, copy, m_paste, remove});
		EditActions::exec(menu, m_table, pos);
	});
}

void EncounterList::setEncounters(const std::vector<lcf::rpg::Encounter>& encounters) {
	m_table->setRowCount(0);
	for (const auto& encounter : encounters) {
		insertRow(m_table->rowCount(), encounter.troop_id);
	}
}

std::vector<lcf::rpg::Encounter> EncounterList::encounters() const {
	std::vector<lcf::rpg::Encounter> result;
	for (int row = 0; row < m_table->rowCount(); ++row) {
		lcf::rpg::Encounter encounter;
		encounter.ID = row + 1;
		encounter.troop_id = m_table->item(row, 0)->data(Qt::UserRole).toInt();
		result.push_back(encounter);
	}
	return result;
}

int EncounterList::defineEncounter(ProjectData& project, int troop_id, QWidget* parent) {
	if (project.database().troops.empty()) {
		QMessageBox::information(parent, tr("Define Encounter"), tr("The database has no monster group."));
		return 0;
	}

	QDialog dialog(parent);
	dialog.setWindowTitle(tr("Define Encounter"));
	auto* group = new QGroupBox(tr("Monster Group"), &dialog);
	auto* combo = new TroopRpgComboBox(group);
	combo->setObjectName(QStringLiteral("comboTroop"));
	combo->makeModel(project);
	const int count = static_cast<int>(project.database().troops.size());
	combo->setCurrentIndex(troop_id >= 1 && troop_id <= count ? troop_id - 1 : 0);
	auto* groupLayout = new QVBoxLayout(group);
	groupLayout->addWidget(combo);

	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
	connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	auto* layout = new QVBoxLayout(&dialog);
	layout->addWidget(group);
	layout->addWidget(buttons);

	if (dialog.exec() != QDialog::Accepted || combo->currentIndex() < 0) {
		return 0;
	}
	return combo->currentIndex() + 1;
}

void EncounterList::add() {
	const int troop_id = defineEncounter(m_project, 1, m_table->window());
	if (troop_id > 0) {
		insertRow(m_table->rowCount(), troop_id);
		m_table->selectRow(m_table->rowCount() - 1);
	}
}

void EncounterList::edit() {
	const int row = m_table->currentRow();
	if (row < 0 || !m_table->item(row, 0) || !m_table->item(row, 0)->isSelected()) {
		// on the empty part of the list: a new encounter
		add();
		return;
	}
	const int troop_id = defineEncounter(m_project, m_table->item(row, 0)->data(Qt::UserRole).toInt(), m_table->window());
	if (troop_id > 0) {
		m_table->item(row, 0)->setText(troopName(m_project, troop_id));
		m_table->item(row, 0)->setData(Qt::UserRole, troop_id);
	}
}

void EncounterList::cut() {
	copy();
	remove();
}

void EncounterList::copy() {
	const QList<int> rows = selectedRows();
	if (rows.isEmpty()) {
		return;
	}
	encounter_clipboard.clear();
	for (int row : rows) {
		encounter_clipboard.push_back(m_table->item(row, 0)->data(Qt::UserRole).toInt());
	}
}

void EncounterList::paste() {
	// after the current line, like a paste in a text
	int row = m_table->currentRow() < 0 ? m_table->rowCount() : m_table->currentRow() + 1;
	for (int troop_id : encounter_clipboard) {
		insertRow(row++, troop_id);
	}
}

void EncounterList::remove() {
	const QList<int> rows = selectedRows();
	for (auto it = rows.crbegin(); it != rows.crend(); ++it) {
		m_table->removeRow(*it);
	}
}

void EncounterList::insertRow(int row, int troop_id) {
	auto* item = new QTableWidgetItem(troopName(m_project, troop_id));
	item->setData(Qt::UserRole, troop_id);
	m_table->insertRow(row);
	m_table->setItem(row, 0, item);
}

QList<int> EncounterList::selectedRows() const {
	QList<int> rows;
	for (const auto& index : m_table->selectionModel()->selectedRows()) {
		rows << index.row();
	}
	std::sort(rows.begin(), rows.end());
	return rows;
}
