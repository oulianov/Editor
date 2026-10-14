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

#include "encounter_list_model.h"
#include "common/dbstring.h"
#include "model/project_data.h"
#include <QCoreApplication>
#include <QPointer>

namespace {
// Shared by the map and area windows, like RPG Maker's clipboard
std::vector<int> encounter_clipboard;
// the lists shown, told when the clipboard fills
QList<QPointer<EncounterListModel>> listening;
}

EncounterListModel::EncounterListModel(const ProjectData& project, QObject* parent) :
	QAbstractListModel(parent), m_project(project)
{
	listening.removeAll(nullptr);
	listening.append(this);
}

void EncounterListModel::setEncounters(const std::vector<lcf::rpg::Encounter>& encounters)
{
	beginResetModel();
	m_troops.clear();
	for (const auto& encounter : encounters)
		m_troops.push_back(encounter.troop_id);
	endResetModel();
	emit countChanged();
}

std::vector<lcf::rpg::Encounter> EncounterListModel::encounters() const
{
	std::vector<lcf::rpg::Encounter> result;
	for (size_t i = 0; i < m_troops.size(); ++i) {
		lcf::rpg::Encounter encounter;
		encounter.ID = static_cast<int>(i) + 1;
		encounter.troop_id = m_troops[i];
		result.push_back(encounter);
	}
	return result;
}

int EncounterListModel::count() const
{
	return static_cast<int>(m_troops.size());
}

int EncounterListModel::troopId(int row) const
{
	return row >= 0 && row < count() ? m_troops[static_cast<size_t>(row)] : 0;
}

QString EncounterListModel::text(int row) const
{
	if (row < 0 || row >= count())
		return QString();
	const int troop_id = m_troops[static_cast<size_t>(row)];
	const auto& troops = m_project.database().troops;
	// ids of deleted monster groups are kept, not dropped
	const QString name = troop_id >= 1 && troop_id <= static_cast<int>(troops.size())
		? ToQString(troops[static_cast<size_t>(troop_id - 1)].name)
		: QCoreApplication::translate("QObject", "<missing monster group>");
	return QStringLiteral("%1: %2").arg(troop_id, 4, 10, QChar('0')).arg(name);
}

int EncounterListModel::troopCount() const
{
	return static_cast<int>(m_project.database().troops.size());
}

int EncounterListModel::insert(int row, int troop_id)
{
	row = row < 0 || row > count() ? count() : row;
	beginInsertRows(QModelIndex(), row, row);
	m_troops.insert(m_troops.begin() + row, troop_id);
	endInsertRows();
	emit countChanged();
	return row;
}

void EncounterListModel::setTroop(int row, int troop_id)
{
	if (row < 0 || row >= count())
		return;
	m_troops[static_cast<size_t>(row)] = troop_id;
	emit dataChanged(index(row), index(row));
}

void EncounterListModel::remove(int row)
{
	if (row < 0 || row >= count())
		return;
	beginRemoveRows(QModelIndex(), row, row);
	m_troops.erase(m_troops.begin() + row);
	endRemoveRows();
	emit countChanged();
}

void EncounterListModel::copy(int row)
{
	if (row < 0 || row >= count())
		return;
	encounter_clipboard = {m_troops[static_cast<size_t>(row)]};
	for (const auto& list : listening)
		if (list)
			emit list->clipboardChanged();
}

void EncounterListModel::cut(int row)
{
	copy(row);
	remove(row);
}

int EncounterListModel::paste(int row)
{
	if (encounter_clipboard.empty())
		return -1;
	int at = row < 0 || row >= count() ? count() : row + 1;
	for (int troop_id : encounter_clipboard)
		insert(at++, troop_id);
	return at - 1;
}

bool EncounterListModel::canPaste() const
{
	return !encounter_clipboard.empty();
}

int EncounterListModel::rowCount(const QModelIndex& parent) const
{
	return parent.isValid() ? 0 : count();
}

QVariant EncounterListModel::data(const QModelIndex& index, int role) const
{
	if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
		return QVariant();
	if (role == Qt::DisplayRole)
		return text(index.row());
	if (role == TroopRole)
		return troopId(index.row());
	return QVariant();
}

QHash<int, QByteArray> EncounterListModel::roleNames() const
{
	return {{Qt::DisplayRole, "text"}, {TroopRole, "troop"}};
}
