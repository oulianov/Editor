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

#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <array>

#include "model/project_data.h"
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
	skill->setObjectName(QStringLiteral("comboSkill"));
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

bool ClassEditors::editParameters(QWidget* parent, lcf::rpg::Parameters& parameters, int max_level, int max_hp) {
	lcf::rpg::Parameters edited = parameters;
	// a level without a value would read as 0 in the game
	edited.Setup(max_level);

	struct Column {
		QString title;
		std::vector<int16_t>& values;
		int minimum;
		int maximum;
	};
	const std::array<Column, 6> columns = {{
		{tr("Max HP"), edited.maxhp, 1, max_hp},
		{tr("Max MP"), edited.maxsp, 0, 999},
		{tr("Attack"), edited.attack, 1, 999},
		{tr("Defense"), edited.defense, 1, 999},
		{tr("Intelligence"), edited.spirit, 1, 999},
		{tr("Agility"), edited.agility, 1, 999},
	}};

	QDialog dialog(parent);
	dialog.setWindowTitle(tr("Parameter Progression"));
	auto* layout = new QVBoxLayout(&dialog);
	auto* table = new QTableWidget(max_level, static_cast<int>(columns.size()), &dialog);
	table->setObjectName(QStringLiteral("tableParameters"));
	QStringList titles;
	for (int column = 0; column < static_cast<int>(columns.size()); ++column) {
		const auto& values = columns[static_cast<size_t>(column)].values;
		titles.append(columns[static_cast<size_t>(column)].title);
		for (int row = 0; row < max_level; ++row) {
			auto* item = new QTableWidgetItem();
			item->setData(Qt::EditRole, static_cast<int>(values[static_cast<size_t>(row)]));
			table->setItem(row, column, item);
		}
	}
	// the row headers are the levels, 1 to the maximum
	table->setHorizontalHeaderLabels(titles);
	table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
	layout->addWidget(table);
	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
	layout->addWidget(buttons);
	QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

	if (dialog.exec() != QDialog::Accepted) {
		return false;
	}

	for (int column = 0; column < static_cast<int>(columns.size()); ++column) {
		const auto& c = columns[static_cast<size_t>(column)];
		for (int row = 0; row < max_level; ++row) {
			const int value = table->item(row, column)->data(Qt::EditRole).toInt();
			c.values[static_cast<size_t>(row)] = static_cast<int16_t>(std::clamp(value, c.minimum, c.maximum));
		}
	}
	parameters = edited;
	return true;
}
