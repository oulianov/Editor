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

#include "area_properties_dialog.h"
#include "encounter_list.h"
#include "common/dbstring.h"
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

AreaPropertiesDialog::AreaPropertiesDialog(ProjectData& project, lcf::rpg::MapInfo& info, int map_width, int map_height, QWidget* parent) :
	QDialog(parent), m_info(info), m_mapWidth(std::max(map_width, 1)), m_mapHeight(std::max(map_height, 1))
{
	setWindowTitle(tr("Area Properties"));

	auto* groupName = new QGroupBox(tr("Name"), this);
	m_name = new QLineEdit(ToQString(info.name), groupName);
	m_name->setObjectName(QStringLiteral("lineName"));
	// the length RPG Maker allows
	m_name->setMaxLength(24);
	auto* nameLayout = new QHBoxLayout(groupName);
	nameLayout->addWidget(m_name);

	// The rectangle is stored with exclusive right and bottom edges
	auto* groupBounds = new QGroupBox(tr("Boundaries"), this);
	auto makeSpin = [groupBounds](const QString& name) {
		auto* spin = new QSpinBox(groupBounds);
		spin->setObjectName(name);
		return spin;
	};
	m_left = makeSpin(QStringLiteral("spinLeft"));
	m_top = makeSpin(QStringLiteral("spinTop"));
	m_width = makeSpin(QStringLiteral("spinWidth"));
	m_height = makeSpin(QStringLiteral("spinHeight"));
	m_left->setRange(0, m_mapWidth - 1);
	m_top->setRange(0, m_mapHeight - 1);
	m_left->setValue(static_cast<int>(info.area_rect.l));
	m_top->setValue(static_cast<int>(info.area_rect.t));
	updateLimits();
	m_width->setValue(static_cast<int>(info.area_rect.r) - m_left->value());
	m_height->setValue(static_cast<int>(info.area_rect.b) - m_top->value());
	connect(m_left, &QSpinBox::valueChanged, this, &AreaPropertiesDialog::updateLimits);
	connect(m_top, &QSpinBox::valueChanged, this, &AreaPropertiesDialog::updateLimits);

	auto* boundsLayout = new QGridLayout(groupBounds);
	boundsLayout->addWidget(new QLabel(tr("X"), groupBounds), 0, 0);
	boundsLayout->addWidget(m_left, 0, 1);
	boundsLayout->addWidget(new QLabel(tr("Y"), groupBounds), 0, 2);
	boundsLayout->addWidget(m_top, 0, 3);
	boundsLayout->addWidget(new QLabel(tr("Width"), groupBounds), 1, 0);
	boundsLayout->addWidget(m_width, 1, 1);
	boundsLayout->addWidget(new QLabel(tr("Height"), groupBounds), 1, 2);
	boundsLayout->addWidget(m_height, 1, 3);
	boundsLayout->addWidget(new QLabel(tr("Map size: %1 x %2").arg(m_mapWidth).arg(m_mapHeight), groupBounds), 2, 0, 1, 4);
	boundsLayout->setColumnStretch(1, 1);
	boundsLayout->setColumnStretch(3, 1);

	auto* groupEncounters = new QGroupBox(tr("Enemy Encounters"), this);
	auto* table = new QTableWidget(groupEncounters);
	table->setObjectName(QStringLiteral("tableEncounters"));
	table->verticalHeader()->setVisible(false);
	m_encounters = new EncounterList(project, table, this);
	m_encounters->setEncounters(info.encounters);
	auto* add = new QPushButton(tr("Add"), groupEncounters);
	add->setObjectName(QStringLiteral("pushAddEncounter"));
	auto* remove = new QPushButton(tr("Delete"), groupEncounters);
	remove->setObjectName(QStringLiteral("pushRemoveEncounter"));
	connect(add, &QPushButton::clicked, m_encounters, &EncounterList::add);
	connect(remove, &QPushButton::clicked, m_encounters, &EncounterList::remove);
	auto* buttonsLayout = new QHBoxLayout();
	buttonsLayout->addWidget(add);
	buttonsLayout->addWidget(remove);
	auto* encountersLayout = new QVBoxLayout(groupEncounters);
	encountersLayout->addWidget(table);
	encountersLayout->addLayout(buttonsLayout);

	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(buttons, &QDialogButtonBox::accepted, this, &AreaPropertiesDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &AreaPropertiesDialog::reject);

	auto* layout = new QVBoxLayout(this);
	layout->addWidget(groupName);
	layout->addWidget(groupBounds);
	layout->addWidget(groupEncounters, 1);
	layout->addWidget(buttons);
}

void AreaPropertiesDialog::updateLimits() {
	// the area stays inside the map
	m_width->setRange(1, m_mapWidth - m_left->value());
	m_height->setRange(1, m_mapHeight - m_top->value());
}

void AreaPropertiesDialog::accept() {
	m_info.name = ToDBString(m_name->text());
	m_info.area_rect.l = static_cast<uint32_t>(m_left->value());
	m_info.area_rect.t = static_cast<uint32_t>(m_top->value());
	m_info.area_rect.r = static_cast<uint32_t>(m_left->value() + m_width->value());
	m_info.area_rect.b = static_cast<uint32_t>(m_top->value() + m_height->value());
	m_info.encounters = m_encounters->encounters();
	QDialog::accept();
}
