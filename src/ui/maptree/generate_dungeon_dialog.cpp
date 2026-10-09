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

#include "generate_dungeon_dialog.h"
#include "common/dbstring.h"
#include "model/project_data.h"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QRandomGenerator>
#include <QSpinBox>
#include <QVBoxLayout>
#include <algorithm>

GenerateDungeonDialog::GenerateDungeonDialog(ProjectData& project, const lcf::rpg::Map& source,
		const QString& name, QWidget* parent) :
	QDialog(parent), m_source(source), m_seed(QRandomGenerator::global()->generate())
{
	setWindowTitle(tr("Generate Dungeon"));
	auto* layout = new QVBoxLayout(this);
	auto addGroup = [&](const QString& title) {
		auto* group = new QGroupBox(title, this);
		layout->addWidget(group);
		auto* row = new QHBoxLayout(group);
		return row;
	};
	auto spin = [&](const QString& name, int min, int max, int value) {
		auto* box = new QSpinBox(this);
		box->setObjectName(name);
		box->setRange(min, max);
		box->setValue(value);
		return box;
	};

	m_name = new QLineEdit(name, this);
	m_name->setObjectName(QStringLiteral("editName"));
	m_name->setMaxLength(24);
	addGroup(tr("Name"))->addWidget(m_name);

	m_tileset = new QComboBox(this);
	m_tileset->setObjectName(QStringLiteral("comboTileset"));
	const auto& chipsets = project.database().chipsets;
	for (const auto& chipset : chipsets)
		m_tileset->addItem(ToQString(chipset.name), chipset.ID);
	m_tileset->setCurrentIndex(std::max(0, m_tileset->findData(source.chipset_id)));
	addGroup(tr("Tileset"))->addWidget(m_tileset);

	m_width = spin(QStringLiteral("spinWidth"), 20, 500, std::clamp(source.width, 20, 500));
	m_height = spin(QStringLiteral("spinHeight"), 15, 500, std::clamp(source.height, 15, 500));
	QHBoxLayout* size = addGroup(tr("Dimensions"));
	size->addWidget(m_width, 1);
	size->addWidget(new QLabel(QStringLiteral("×"), this));
	size->addWidget(m_height, 1);

	// Cells the dungeon keeps on the floor and joined to the rest, to put
	// the teleports in and out there afterwards. 0, 0 is in the border: none
	auto addPoint = [&](const QString& title, const QString& name, QSpinBox*& x, QSpinBox*& y) {
		x = spin(QStringLiteral("spin%1X").arg(name), 0, 100, 0);
		y = spin(QStringLiteral("spin%1Y").arg(name), 0, 100, 0);
		QHBoxLayout* row = addGroup(title);
		row->parentWidget()->setToolTip(tr("A cell kept on the floor and reachable from the whole dungeon. 0, 0: none."));
		row->addWidget(new QLabel(tr("X"), this));
		row->addWidget(x, 1);
		row->addWidget(new QLabel(tr("Y"), this));
		row->addWidget(y, 1);
	};
	addPoint(tr("Entrance"), QStringLiteral("Entrance"), m_entranceX, m_entranceY);
	addPoint(tr("Exit"), QStringLiteral("Exit"), m_exitX, m_exitY);
	connect(m_width, &QSpinBox::valueChanged, this, &GenerateDungeonDialog::updateRanges);
	connect(m_height, &QSpinBox::valueChanged, this, &GenerateDungeonDialog::updateRanges);
	updateRanges();

	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	layout->addWidget(buttons);
}

void GenerateDungeonDialog::updateRanges()
{
	// RPG Maker stops at 100; a cell of a larger map is allowed too
	for (QSpinBox* x : {m_entranceX, m_exitX})
		x->setMaximum(m_width->value() - 1);
	for (QSpinBox* y : {m_entranceY, m_exitY})
		y->setMaximum(m_height->value() - 1);
}

QString GenerateDungeonDialog::name() const
{
	return m_name->text();
}

std::vector<DungeonGenerator::Point> GenerateDungeonDialog::keepPoints() const
{
	std::vector<DungeonGenerator::Point> points;
	if (m_entranceX->value() != 0 || m_entranceY->value() != 0)
		points.push_back({m_entranceX->value(), m_entranceY->value()});
	if (m_exitX->value() != 0 || m_exitY->value() != 0)
		points.push_back({m_exitX->value(), m_exitY->value()});
	return points;
}

void GenerateDungeonDialog::setSeed(uint32_t seed)
{
	m_seed = seed;
}

lcf::rpg::Map GenerateDungeonDialog::map() const
{
	// help: "a new map, retaining the same settings"
	lcf::rpg::Map map = m_source;
	map.events.clear();
	map.save_count = 0;
	map.save_count_2k3e = 0;
	map.chipset_id = m_tileset->currentData().toInt();
	map.width = m_width->value();
	map.height = m_height->value();
	DungeonGenerator::generate(map, m_seed, keepPoints());
	return map;
}
