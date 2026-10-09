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

#include "class_editors.h"

#include <QAction>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <array>

#include "curve_dialog.h"
#include "model/project_data.h"
#include "common/signal_blocker.h"
#include "ui/common/rpg_combobox.h"

QIcon ClassEditors::rankIcon(int rank) {
	return QIcon(QString(":/ranks/rank%1").arg(rank));
}

void ClassEditors::cycleRank(QListWidget* list, std::vector<uint8_t>& ranks, int row) {
	if (row < 0) {
		return;
	}
	if (static_cast<int>(ranks.size()) <= row) {
		ranks.resize(static_cast<size_t>(row) + 1, kDefaultRank);
	}
	auto& rank = ranks[static_cast<size_t>(row)];
	rank = (rank + 1) % 5;
	list->item(row)->setIcon(rankIcon(rank));
}

void ClassEditors::fillSkills(QTableWidget* table, const std::vector<lcf::rpg::Learning>& skills, ProjectData& project) {
	const auto& database = project.database();
	table->setRowCount(0);
	auto add = [table](int row, const QString& level, const QString& name) {
		table->insertRow(row);
		for (int column = 0; column < 2; ++column) {
			auto* item = new QTableWidgetItem(column == 0 ? level : name);
			item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
			table->setItem(row, column, item);
		}
	};
	for (size_t i = 0; i < skills.size(); ++i) {
		const int id = skills[i].skill_id;
		QString name = tr("<%1?>").arg(id);
		if (id >= 1 && id <= static_cast<int>(database.skills.size())) {
			name = ToQString(database.skills[static_cast<size_t>(id) - 1].name);
		}
		add(static_cast<int>(i), QString::number(skills[i].level), name);
	}
	add(static_cast<int>(skills.size()), QString(), QString());
}

static void sortSkills(std::vector<lcf::rpg::Learning>& skills) {
	std::stable_sort(skills.begin(), skills.end(), [](const auto& a, const auto& b) {
		return a.level < b.level;
	});
	// The entries of a list are written with their IDs, which must follow each other
	for (size_t i = 0; i < skills.size(); ++i) {
		skills[i].ID = static_cast<int>(i) + 1;
	}
}

bool ClassEditors::editSkill(QWidget* parent, ProjectData& project, std::vector<lcf::rpg::Learning>& skills,
	int row, int max_level)
{
	if (row < 0) {
		return false;
	}
	const bool adding = row >= static_cast<int>(skills.size());
	lcf::rpg::Learning learning;
	if (!adding) {
		learning = skills[static_cast<size_t>(row)];
	}

	QDialog dialog(parent);
	dialog.setWindowTitle(adding ? tr("Add Skill") : tr("Edit Skill"));
	auto* layout = new QVBoxLayout(&dialog);
	auto* form = new QFormLayout();
	auto* level = new QSpinBox(&dialog);
	level->setObjectName(QStringLiteral("spinSkillLevel"));
	level->setRange(1, max_level);
	level->setValue(learning.level);
	auto* skill = new SkillRpgComboBox(&dialog);
	// the combo box itself: scripts set it by index (skill ID - 1)
	skill->comboBox()->setObjectName(QStringLiteral("comboSkill"));
	skill->makeModel(project);
	skill->setCurrentIndex(learning.skill_id - 1);
	form->addRow(tr("Level"), level);
	form->addRow(tr("Skill"), skill);
	layout->addLayout(form);
	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
	layout->addWidget(buttons);
	QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

	if (dialog.exec() != QDialog::Accepted || skill->currentIndex() < 0) {
		return false;
	}

	learning.level = level->value();
	learning.skill_id = skill->comboBox()->currentData().toInt();
	if (adding) {
		skills.push_back(learning);
	} else {
		skills[static_cast<size_t>(row)] = learning;
	}
	sortSkills(skills);
	return true;
}

void ClassEditors::removeSkill(std::vector<lcf::rpg::Learning>& skills, int row) {
	if (row < 0 || row >= static_cast<int>(skills.size())) {
		return;
	}
	skills.erase(skills.begin() + row);
	sortSkills(skills);
}

void ClassEditors::connectSkillTable(QTableWidget* table, ProjectData& project,
	std::function<std::vector<lcf::rpg::Learning>&()> skills)
{
	const int max_level = project.database().system.ldb_id == 2003 ? 99 : 50;
	// the lines are the list in its order: no sorting, no editing in place
	table->setSortingEnabled(false);
	table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	table->setSelectionBehavior(QAbstractItemView::SelectRows);
	table->setSelectionMode(QAbstractItemView::SingleSelection);
	table->setContextMenuPolicy(Qt::ActionsContextMenu);

	auto edit = [table, &project, skills, max_level](int row) {
		if (editSkill(table, project, skills(), row, max_level)) {
			fillSkills(table, skills(), project);
		}
	};
	auto remove = [table, &project, skills](int row) {
		if (row < 0 || row >= static_cast<int>(skills().size())) {
			return;
		}
		removeSkill(skills(), row);
		fillSkills(table, skills(), project);
		table->setCurrentCell(std::min(row, table->rowCount() - 1), 0);
	};

	auto* edit_action = new QAction(tr("Edit..."), table);
	edit_action->setObjectName(QStringLiteral("actionEditSkill"));
	QObject::connect(edit_action, &QAction::triggered, table, [table, edit] { edit(table->currentRow()); });
	auto* delete_action = new QAction(tr("Delete"), table);
	delete_action->setObjectName(QStringLiteral("actionDeleteSkill"));
	QObject::connect(delete_action, &QAction::triggered, table, [table, remove] { remove(table->currentRow()); });
	table->addActions({edit_action, delete_action});

	// keys read here, not as shortcuts: scripts post them to the table
	filterEvents(table, [table, edit, remove](QEvent* event) {
		if (event->type() != QEvent::KeyPress) {
			return false;
		}
		const int key = static_cast<QKeyEvent*>(event)->key();
		if (key == Qt::Key_Return || key == Qt::Key_Enter) {
			edit(table->currentRow());
			return true;
		}
		if (key == Qt::Key_Delete || key == Qt::Key_Backspace) {
			remove(table->currentRow());
			return true;
		}
		return false;
	});
	// a double-click below the lines adds a skill too
	filterEvents(table->viewport(), [table, skills, edit](QEvent* event) {
		if (event->type() != QEvent::MouseButtonDblClick) {
			return false;
		}
		const int row = table->indexAt(static_cast<QMouseEvent*>(event)->position().toPoint()).row();
		edit(row < 0 ? static_cast<int>(skills().size()) : row);
		return true;
	});
}

namespace {
class FunctionFilter : public QObject {
public:
	FunctionFilter(QObject* parent, std::function<bool(QEvent*)> filter) : QObject(parent), m_filter(std::move(filter)) {}

	bool eventFilter(QObject*, QEvent* event) override {
		return m_filter(event);
	}

private:
	std::function<bool(QEvent*)> m_filter;
};
}

void ClassEditors::filterEvents(QObject* object, std::function<bool(QEvent*)> filter) {
	object->installEventFilter(new FunctionFilter(object, std::move(filter)));
}

bool ClassEditors::editParameters(QWidget* parent, lcf::rpg::Parameters& parameters, int max_level, int max_hp, int tab) {
	CurveDialog dialog(parameters, max_level, max_hp, tab, parent);
	if (dialog.exec() != QDialog::Accepted) {
		return false;
	}
	parameters = dialog.parameters();
	return true;
}

void ClassEditors::fillBattleCommands(const CommandCombos& combos, const std::vector<int32_t>& commands,
	ProjectData& project)
{
	const auto& database_commands = project.database().battlecommands.commands;
	// the commands before the Row command (0); -1 pads the list
	std::vector<int32_t> ids;
	for (int32_t id : commands) {
		if (id > 0) {
			ids.push_back(id);
		}
	}

	for (size_t slot = 0; slot < combos.size(); ++slot) {
		auto* combo = combos[slot];
		SignalBlocker blocker(combo);
		combo->clear();
		combo->addItem(tr("<none>"), 0);
		for (const auto& command : database_commands) {
			combo->addItem(QString("%1: %2").arg(command.ID, 4, 10, QChar('0')).arg(ToQString(command.name)), command.ID);
		}
		const int id = slot < ids.size() ? ids[slot] : 0;
		if (combo->findData(id) < 0) {
			combo->addItem(tr("<%1?>").arg(id), id);
		}
		combo->setCurrentIndex(combo->findData(id));
	}
}

std::vector<int32_t> ClassEditors::battleCommands(const CommandCombos& combos) {
	std::vector<int32_t> ids;
	for (auto* combo : combos) {
		const int id = combo->currentData().toInt();
		if (id > 0) {
			ids.push_back(id);
		}
	}
	// Like RPG Maker: the Row command (0) ends the list, padded to 7 entries with -1
	ids.push_back(0);
	ids.resize(std::max<size_t>(ids.size(), kCommandSlots + 1), -1);
	return ids;
}
