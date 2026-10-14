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

#include "entry_filter_model.h"
#include "common/search_match.h"
#include "json_list_view.h"

#include <QJSValue>

EntryFilterModel::EntryFilterModel(QObject* parent) :
	QSortFilterProxyModel(parent), m_idRole(JsonListView::IdRole)
{
	// the order of the list, the rows of the source re-checked when they change
	setDynamicSortFilter(true);
}

void EntryFilterModel::setFilterText(const QString& text) {
	if (m_filterText == text) {
		return;
	}
	beginFilterChange();
	m_filterText = text;
	endFilterChange(Direction::Rows);
	emit filterTextChanged();
}

void EntryFilterModel::setIdRole(int role) {
	if (m_idRole == role) {
		return;
	}
	beginFilterChange();
	m_idRole = role;
	endFilterChange(Direction::Rows);
	emit idRoleChanged();
}

void EntryFilterModel::setIds(const QVariant& ids) {
	if (m_ids == ids) {
		return;
	}
	beginFilterChange();
	m_ids = ids;
	m_idSet.reset();
	// a JavaScript array comes as a QJSValue
	const QVariant list = ids.metaType() == QMetaType::fromType<QJSValue>() ? ids.value<QJSValue>().toVariant() : ids;
	if (list.isValid() && !list.isNull()) {
		QSet<int> set;
		for (const QVariant& id : list.toList()) {
			set.insert(id.toInt());
		}
		m_idSet = std::move(set);
	}
	endFilterChange(Direction::Rows);
	emit idsChanged();
}

int EntryFilterModel::entryId(const QModelIndex& index) const {
	if (m_idRole >= 0) {
		bool ok = false;
		const int id = index.data(m_idRole).toInt(&ok);
		if (ok) {
			return id;
		}
	}
	return SearchMatch::entryId(index.data().toString());
}

bool EntryFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const {
	const QModelIndex index = sourceModel()->index(sourceRow, filterKeyColumn(), sourceParent);
	if (m_idSet && !m_idSet->contains(entryId(index))) {
		return false;
	}
	if (m_filterText.trimmed().isEmpty()) {
		return true;
	}
	return SearchMatch::matches(index.data().toString(), entryId(index), m_filterText);
}

int EntryFilterModel::sourceRow(int row) const {
	const QModelIndex source = mapToSource(index(row, 0));
	return source.isValid() ? source.row() : -1;
}

int EntryFilterModel::rowOf(int sourceRow) const {
	if (!sourceModel()) {
		return -1;
	}
	const QModelIndex shown = mapFromSource(sourceModel()->index(sourceRow, 0));
	return shown.isValid() ? shown.row() : -1;
}

int EntryFilterModel::bestRow() const {
	const QString number = SearchMatch::number(m_filterText.trimmed());
	const int id = number.isEmpty() ? -1 : number.toInt();
	for (int row = 0; id >= 0 && row < rowCount(); ++row) {
		if (entryId(index(row, 0)) == id) {
			return row;
		}
	}
	return rowCount() > 0 ? 0 : -1;
}

ChoiceListModel::ChoiceListModel(QObject* parent) :
	QAbstractListModel(parent)
{}

void ChoiceListModel::setEntries(const QVariantList& entries) {
	if (m_entries == entries) {
		return;
	}
	beginResetModel();
	m_entries = entries;
	endResetModel();
	emit entriesChanged();
}

int ChoiceListModel::rowCount(const QModelIndex& parent) const {
	return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant ChoiceListModel::data(const QModelIndex& index, int role) const {
	if (!index.isValid() || index.row() >= m_entries.size()) {
		return {};
	}
	const QVariantMap entry = m_entries[index.row()].toMap();
	switch (role) {
		case Qt::DisplayRole:
			return entry.value(QStringLiteral("text"));
		case JsonListView::IdRole:
			return entry.value(QStringLiteral("value"));
		default:
			return {};
	}
}

QHash<int, QByteArray> ChoiceListModel::roleNames() const {
	return {{Qt::DisplayRole, "text"}, {JsonListView::IdRole, "value"}};
}
