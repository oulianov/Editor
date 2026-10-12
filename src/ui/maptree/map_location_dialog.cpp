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

#include "map_location_dialog.h"
#include "core.h"
#include "common/dbstring.h"
#include "model/project_data.h"
#include "ui/common/map_preview.h"
#include "ui/common/search_combobox.h"
#include "ui/common/theme_icon.h"
#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSplitter>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>
#include <algorithm>
#include <functional>

MapLocationDialog::MapLocationDialog(ProjectData& project, int map_id, int x, int y, int direction,
	bool fixed_map, QWidget* parent) :
	QDialog(parent), m_project(project)
{
	setWindowTitle(tr("Select Location"));

	m_preview = new MapPreview(this);
	m_preview->setObjectName(QStringLiteral("mapPreview"));
	m_preview->setMinimumSize(240, 180);
	m_status = new QLabel(this);
	m_status->setObjectName(QStringLiteral("labelLocation"));
	auto* preview_panel = new QWidget(this);
	auto* preview_layout = new QVBoxLayout(preview_panel);
	preview_layout->setContentsMargins(0, 0, 0, 0);
	preview_layout->addWidget(m_preview, 1);
	auto* status_layout = new QHBoxLayout();
	status_layout->addWidget(m_preview->createZoomButtons(preview_panel));
	status_layout->addWidget(m_status, 1);
	preview_layout->addLayout(status_layout);

	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	m_ok = buttons->button(QDialogButtonBox::Ok);
	connect(buttons, &QDialogButtonBox::accepted, this, &MapLocationDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &MapLocationDialog::reject);
	connect(m_preview, &MapPreview::selectionChanged, this, &MapLocationDialog::updateStatus);
	connect(m_preview, &MapPreview::cellActivated, this, [this] {
		if (m_ok->isEnabled()) {
			accept();
		}
	});

	auto* layout = new QVBoxLayout(this);
	if (fixed_map) {
		layout->addWidget(preview_panel, 1);
	} else {
		// the maps of the project in the order of the map tree; areas are
		// shown but are no location
		m_tree = new QTreeWidget(this);
		m_tree->setObjectName(QStringLiteral("treeMaps"));
		m_tree->setHeaderHidden(true);
		m_tree->setMinimumWidth(160);
		auto* root = new QTreeWidgetItem(m_tree);
		root->setText(0, core().project() ? core().project()->gameTitle() : tr("Project"));
		root->setIcon(0, themeIcon(this, QStringLiteral("folder")));
		root->setFlags(Qt::ItemIsEnabled);
		QHash<int, QTreeWidgetItem*> items;
		const auto& tree_map = project.treeMap();
		for (const auto& info : tree_map.maps) {
			if (info.ID <= 0) {
				continue;
			}
			auto* item = new QTreeWidgetItem();
			item->setText(0, ToQString(info.name));
			item->setData(0, Qt::UserRole, info.ID);
			if (info.type == lcf::rpg::TreeMap::MapType_area) {
				item->setIcon(0, themeIcon(this, QStringLiteral("select")));
				item->setFlags(Qt::ItemIsEnabled);
			}
			items[info.ID] = item;
		}
		auto attach = [&](int id) {
			QTreeWidgetItem* item = items.value(id);
			if (!item || item->parent()) {
				return;
			}
			const auto it = std::find_if(tree_map.maps.begin(), tree_map.maps.end(),
				[id](const lcf::rpg::MapInfo& info) { return info.ID == id; });
			QTreeWidgetItem* parent = it != tree_map.maps.end() ? items.value(it->parent_map) : nullptr;
			(parent && parent != item ? parent : root)->addChild(item);
		};
		for (int id : tree_map.tree_order) {
			attach(id);
		}
		// maps missing from the tree order
		for (auto it = items.cbegin(); it != items.cend(); ++it) {
			attach(it.key());
		}
		m_tree->expandAll();
		connect(m_tree, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* item) {
			if (item && (item->flags() & Qt::ItemIsSelectable)) {
				showMap(item->data(0, Qt::UserRole).toInt());
			}
		});

		// the maps searched by number or name, as the lists of maps
		auto* search = new QLineEdit(this);
		search->setObjectName(QStringLiteral("searchMaps"));
		search->setPlaceholderText(tr("Search by number or name"));
		search->setClearButtonEnabled(true);
		connect(search, &QLineEdit::textChanged, this, [this, root](const QString& text) {
			// an area stays while one of its maps matches
			std::function<bool(QTreeWidgetItem*)> filter = [&](QTreeWidgetItem* item) {
				bool shown = false;
				for (int i = 0; i < item->childCount(); ++i) {
					shown = filter(item->child(i)) || shown;
				}
				const int id = item->data(0, Qt::UserRole).toInt();
				if (id > 0 && SearchComboBox::matches(QStringLiteral("%1: %2").arg(id, 4, 10, QChar('0')).arg(item->text(0)), id, text)) {
					shown = true;
				}
				item->setHidden(!shown && item != root);
				return shown;
			};
			filter(root);
			m_tree->expandAll();
		});
		connect(search, &QLineEdit::returnPressed, this, [this] {
			// the first map shown
			for (QTreeWidgetItemIterator it(m_tree, QTreeWidgetItemIterator::NotHidden | QTreeWidgetItemIterator::Selectable); *it; ++it) {
				m_tree->setCurrentItem(*it);
				m_tree->setFocus();
				break;
			}
		});
		auto* maps = new QWidget(this);
		auto* maps_layout = new QVBoxLayout(maps);
		maps_layout->setContentsMargins(0, 0, 0, 0);
		maps_layout->addWidget(search);
		maps_layout->addWidget(m_tree);

		auto* splitter = new QSplitter(this);
		splitter->addWidget(maps);
		splitter->addWidget(preview_panel);
		splitter->setStretchFactor(1, 1);
		splitter->setSizes({200, 600});
		layout->addWidget(splitter, 1);

		// a location without a map (a new command) starts on the open map
		QTreeWidgetItem* item = items.value(map_id);
		if (!item || !(item->flags() & Qt::ItemIsSelectable)) {
			map_id = MapPreview::openMapId();
			item = items.value(map_id);
		}
		if (item && (item->flags() & Qt::ItemIsSelectable)) {
			m_tree->setCurrentItem(item);
			m_tree->scrollToItem(item);
		}
	}

	if (direction >= 0) {
		auto* group = new QGroupBox(tr("Character Facing"), this);
		auto* facing_layout = new QHBoxLayout(group);
		m_facing = new QButtonGroup(this);
		m_facing->setObjectName(QStringLiteral("groupFacing"));
		const QStringList labels = {tr("Retain Current Facing"), tr("Up"), tr("Right"), tr("Down"), tr("Left")};
		for (int i = 0; i < labels.size(); i++) {
			auto* radio = new QRadioButton(labels[i], group);
			radio->setObjectName(QStringLiteral("radioFacing%1").arg(i));
			m_facing->addButton(radio, i);
			facing_layout->addWidget(radio);
		}
		facing_layout->addStretch();
		if (QAbstractButton* button = m_facing->button(direction)) {
			button->setChecked(true);
		} else {
			m_facing->button(0)->setChecked(true);
		}
		layout->addWidget(group);
	}
	layout->addWidget(buttons);

	if (m_preview->mapId() != map_id) {
		showMap(map_id);
	}
	if (m_preview->columns() > 0) {
		m_preview->setCell(std::clamp(x, 0, m_preview->columns() - 1), std::clamp(y, 0, m_preview->rows() - 1));
		m_preview->centerOn(x, y);
	}
	updateStatus();
	resize(860, 560);
}

void MapLocationDialog::showMap(int map_id) {
	const QRect previous = m_preview->selection();
	if (map_id > 0 && m_preview->showMap(map_id)) {
		// the cell stays where it was, inside the new map
		const QPoint cell = previous.isEmpty() ? QPoint(0, 0) : previous.topLeft();
		m_preview->setCell(std::min(cell.x(), m_preview->columns() - 1), std::min(cell.y(), m_preview->rows() - 1));
	} else {
		m_preview->clear();
	}
	updateStatus();
}

void MapLocationDialog::updateStatus() {
	const int id = m_preview->mapId();
	const QRect cell = m_preview->selection();
	m_ok->setEnabled(id > 0 && !cell.isEmpty());
	if (id <= 0) {
		m_status->setText(tr("Choose a map"));
		return;
	}
	QString name;
	for (const auto& info : m_project.treeMap().maps) {
		if (info.ID == id) {
			name = ToQString(info.name);
			break;
		}
	}
	m_status->setText(cell.isEmpty() ? name : tr("%1 (%2, %3)").arg(name).arg(cell.x()).arg(cell.y()));
}

int MapLocationDialog::mapId() const {
	return m_preview->mapId();
}

QPoint MapLocationDialog::cell() const {
	return m_preview->selection().topLeft();
}

int MapLocationDialog::direction() const {
	return m_facing ? m_facing->checkedId() : -1;
}

bool MapLocationDialog::pick(ProjectData& project, int& map_id, int& x, int& y, QWidget* parent) {
	MapLocationDialog dialog(project, map_id, x, y, -1, false, parent);
	if (dialog.exec() != QDialog::Accepted) {
		return false;
	}
	map_id = dialog.mapId();
	x = dialog.cell().x();
	y = dialog.cell().y();
	return true;
}

bool MapLocationDialog::pick(ProjectData& project, int& map_id, int& x, int& y, int& direction, QWidget* parent) {
	MapLocationDialog dialog(project, map_id, x, y, std::max(direction, 0), false, parent);
	if (dialog.exec() != QDialog::Accepted) {
		return false;
	}
	map_id = dialog.mapId();
	x = dialog.cell().x();
	y = dialog.cell().y();
	direction = dialog.direction();
	return true;
}

bool MapLocationDialog::pickOnMap(ProjectData& project, int map_id, int& x, int& y, QWidget* parent) {
	MapLocationDialog dialog(project, map_id, x, y, -1, true, parent);
	if (dialog.mapId() != map_id || dialog.exec() != QDialog::Accepted) {
		return false;
	}
	x = dialog.cell().x();
	y = dialog.cell().y();
	return true;
}
