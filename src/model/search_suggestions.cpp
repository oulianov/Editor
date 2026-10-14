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

#include "search_suggestions.h"
#include <QRegularExpression>
#include <algorithm>
#include <tuple>

SearchSuggestions::SearchSuggestions(QObject* parent) : QAbstractListModel(parent)
{
}

void SearchSuggestions::setEntries(std::vector<Entry> entries)
{
	m_entries = std::move(entries);
	update();
}

void SearchSuggestions::setQuery(const QString& text)
{
	QString query = text.trimmed();
	// a number, or a suggestion chosen ("12: Variable — Name"): that entry
	static const QRegularExpression numbered(QStringLiteral("^(\\d+)(?::.*)?$"));
	const auto number = numbered.match(query);
	bool valid = false;
	int id = number.hasMatch() ? number.captured(1).toInt(&valid) : -1;
	if (valid) {
		query = QString::number(id);
	} else {
		id = -1;
	}
	if (m_query == query && m_id == id) {
		return;
	}
	m_query = query;
	m_id = id;
	update();
}

int SearchSuggestions::rank(const Entry& entry) const
{
	if (m_query.isEmpty()) {
		return 6;
	}
	const QString digits = QString::number(entry.id);
	if (m_id >= 0 && entry.id == m_id) {
		return 0;
	}
	if (entry.name.compare(m_query, Qt::CaseInsensitive) == 0) {
		return 1;
	}
	if (digits.startsWith(m_query)) {
		return 2;
	}
	if (entry.name.startsWith(m_query, Qt::CaseInsensitive)) {
		return 3;
	}
	if (digits.contains(m_query)) {
		return 4;
	}
	if (entry.name.contains(m_query, Qt::CaseInsensitive)) {
		return 5;
	}
	return 6;
}

void SearchSuggestions::update()
{
	const int before = rowCount();
	beginResetModel();
	m_shown.clear();
	std::vector<int> ranks(m_entries.size());
	for (size_t i = 0; i < m_entries.size(); ++i) {
		ranks[i] = rank(m_entries[i]);
		if (ranks[i] < 6) {
			m_shown.push_back(static_cast<int>(i));
		}
	}
	std::stable_sort(m_shown.begin(), m_shown.end(), [&](int a, int b) {
		return std::tuple{ranks[a], m_entries[a].id, m_entries[a].type} < std::tuple{ranks[b], m_entries[b].id, m_entries[b].type};
	});
	endResetModel();
	if (rowCount() != before) {
		emit countChanged();
	}
}

const SearchSuggestions::Entry* SearchSuggestions::entry(int row) const
{
	return row >= 0 && row < rowCount() ? &m_entries[static_cast<size_t>(m_shown[static_cast<size_t>(row)])] : nullptr;
}

QString SearchSuggestions::labelAt(int row) const
{
	const Entry* shown = entry(row);
	return shown ? shown->label : QString();
}

int SearchSuggestions::typeAt(int row) const
{
	const Entry* shown = entry(row);
	return shown ? shown->type : -1;
}

int SearchSuggestions::rowCount(const QModelIndex& parent) const
{
	return parent.isValid() ? 0 : static_cast<int>(m_shown.size());
}

QVariant SearchSuggestions::data(const QModelIndex& index, int role) const
{
	const Entry* shown = index.isValid() ? entry(index.row()) : nullptr;
	if (!shown) {
		return {};
	}
	switch (role) {
	case Qt::DisplayRole:
	case LabelRole:
		return shown->label;
	case IdRole:
		return shown->id;
	case TypeRole:
		return shown->type;
	case NameRole:
		return shown->name;
	case IconRole:
		return shown->icon;
	default:
		return {};
	}
}

QHash<int, QByteArray> SearchSuggestions::roleNames() const
{
	QHash<int, QByteArray> names = QAbstractListModel::roleNames();
	names[IdRole] = "entryId";
	names[TypeRole] = "type";
	names[NameRole] = "name";
	names[LabelRole] = "label";
	names[IconRole] = "iconName";
	return names;
}
