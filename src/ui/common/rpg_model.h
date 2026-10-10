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

#pragma once

#include <QAbstractListModel>
#include <QPixmap>
#include <QMessageBox>

#include <lcf/rpg/database.h>
#include "core.h"
#include "common/dbstring.h"
#include "common/filefinder.h"
#include "common/image_loader.h"
#include "model/rpg_reflect.h"
#include "ui/database/database_list_data.h"

template<typename LCF>
class RpgModel : public QAbstractListModel {
public:
	/** Model fetches automatically the data from the Project based on LCF */
	RpgModel(ProjectData& project, QObject *parent = nullptr) :
			RpgModel(project, RpgReflect<LCF>::items(project.database()), parent) {}
	/** Model takes the data from the passed vector */
	RpgModel(ProjectData& project, std::vector<LCF>& data, QObject *parent = nullptr) :
			QAbstractListModel(parent), m_project(project), m_data(data), m_rows(static_cast<int>(data.size())) {
		// another tab or a picker resized or renamed the list
		QObject::connect(&DatabaseListNotifier::instance(), &DatabaseListNotifier::listChanged, this, [this](const void* list) {
			if (list == &m_data) {
				sync();
			}
		});
	}
	int rowCount(const QModelIndex& = QModelIndex()) const override { return m_rows; }
	QVariant data(const QModelIndex &index, int role) const override;
	std::vector<LCF>& lcfData() const {
		return m_data;
	}
	void resize(int count) {
		if (count < 1 || count == static_cast<int>(m_data.size())) return;
		const int previous = static_cast<int>(m_data.size());
		m_data.resize(count);
		for (int i = previous; i < count; ++i) {
			m_data[i] = DatabaseList::defaultEntry<LCF>(m_project, i + 1);
		}
		sync();
	}
	/** Tells the views about rows added or removed and texts changed in the data */
	void sync() {
		// rows inserted or removed instead of a reset: combo boxes keep their entry
		const int rows = static_cast<int>(m_data.size());
		if (rows > m_rows) {
			beginInsertRows(QModelIndex(), m_rows, rows - 1);
			m_rows = rows;
			endInsertRows();
		} else if (rows < m_rows) {
			beginRemoveRows(QModelIndex(), rows, m_rows - 1);
			m_rows = rows;
			endRemoveRows();
		}
		if (rows > 0) {
			emit dataChanged(index(0), index(rows - 1));
		}
	}

private:
	ProjectData& m_project;
	std::vector<LCF>& m_data;
	// the rows the views know (the vector can change before they are told)
	int m_rows;
};

enum ModelData {
	ModelDataId = Qt::UserRole,
	ModelDataObject
};

template<class LCF>
QVariant RpgModel<LCF>::data(const QModelIndex &index, int role) const {
	if (!index.isValid() || index.row() < 0 || index.row() >= rowCount() || index.row() >= static_cast<int>(m_data.size())) {
		return QVariant();
	} else if (role == Qt::DisplayRole || role == Qt::EditRole) {
		const auto& data = m_data[index.row()];
		return QString("%1: %2").arg(data.ID, 4, 10, QChar('0')).arg(ToQString(data.name));
	} else if (role == Qt::DecorationRole) {
		return typename RpgReflect<LCF>::model_type(m_project, m_data[index.row()]).preview();
	} else if (role == Qt::UserRole) {
		return m_data[index.row()].ID;
	} else if (role == ModelDataObject) {
		return QVariant::fromValue(&m_data[index.row()]);
	}

	return QVariant();
}
