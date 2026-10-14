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

#include "search_results_model.h"

#include <QCoreApplication>

namespace {

QString eventText(const SearchResult& result)
{
	return QStringLiteral("%1: %2").arg(result.eventId).arg(result.eventName);
}

QString typeIcon(SearchResult::Kind kind)
{
	switch (kind) {
	case SearchResult::Kind::Variable:
		return QStringLiteral("variable");
	case SearchResult::Kind::Switch:
		return QStringLiteral("switch");
	case SearchResult::Kind::Item:
		return QStringLiteral("resources");
	default:
		return QStringLiteral("code");
	}
}

} // namespace

SearchResultsModel::SearchResultsModel(QObject* parent) :
	QAbstractTableModel(parent)
{
}

SearchResultsModel::~SearchResultsModel() = default;

void SearchResultsModel::setResults(std::vector<SearchResult> results)
{
	const bool counted = results.size() != m_results.size();
	beginResetModel();
	m_results = std::move(results);
	endResetModel();
	if (counted) {
		emit countChanged();
	}
}

void SearchResultsModel::clear()
{
	setResults({});
}

const std::vector<SearchResult>& SearchResultsModel::results() const
{
	return m_results;
}

const SearchResult* SearchResultsModel::result(int row) const
{
	if (row < 0 || static_cast<size_t>(row) >= m_results.size()) {
		return nullptr;
	}
	return &m_results[static_cast<size_t>(row)];
}

int SearchResultsModel::count() const
{
	return static_cast<int>(m_results.size());
}

int SearchResultsModel::rowCount(const QModelIndex& parent) const
{
	return parent.isValid() ? 0 : count();
}

int SearchResultsModel::columnCount(const QModelIndex& parent) const
{
	return parent.isValid() ? 0 : ColumnCount;
}

QVariant SearchResultsModel::data(const QModelIndex& index, int role) const
{
	if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
		return {};
	}
	const SearchResult& result = m_results[static_cast<size_t>(index.row())];
	switch (role) {
	case Qt::DisplayRole:
		switch (index.column()) {
		case MapColumn:
			return result.location;
		case EventColumn:
			return eventText(result);
		case PageColumn:
			return result.pageId > 0 ? QString::number(result.pageId) : QString();
		case TypeColumn:
			return result.type;
		case MatchColumn:
			return result.match;
		default:
			return {};
		}
	case Qt::ToolTipRole:
		// the whole path, when the column cuts it
		return index.column() == MapColumn ? QVariant(result.location) : QVariant();
	case LocationRole:
		return result.location;
	case MapIdRole:
		return result.mapId;
	case EventIdRole:
		return result.eventId;
	case PageIdRole:
		return result.pageId;
	case LineRole:
		return result.line;
	case EventNameRole:
		return result.eventName;
	case EventRole:
		return eventText(result);
	case TypeRole:
		return result.type;
	case MatchRole:
		return result.match;
	case LocationIconRole:
		return result.mapId <= 0 ? QStringLiteral("database") : QStringLiteral("tree");
	case EventIconRole:
		return result.mapId <= 0 ? QStringLiteral("code") : QStringLiteral("layer-event");
	case TypeIconRole:
		return typeIcon(result.kind);
	case CharsetFileRole:
		return result.charsetFile;
	case CharsetFrameRole:
		return result.charsetFrame;
	case CharsetNameRole:
		return result.charsetFile.isEmpty() ? QString() : result.charsetName;
	default:
		return {};
	}
}

QVariant SearchResultsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
		return QAbstractTableModel::headerData(section, orientation, role);
	}
	switch (section) {
	case MapColumn:
		return QCoreApplication::translate("SearchDialog", "Map");
	case EventColumn:
		return QCoreApplication::translate("SearchDialog", "Event");
	case PageColumn:
		return QCoreApplication::translate("SearchDialog", "Page");
	case TypeColumn:
		return QCoreApplication::translate("SearchDialog", "Type");
	case MatchColumn:
		return QCoreApplication::translate("SearchDialog", "Match");
	default:
		return {};
	}
}

QHash<int, QByteArray> SearchResultsModel::roleNames() const
{
	QHash<int, QByteArray> names = QAbstractTableModel::roleNames();
	names[LocationRole] = "location";
	names[MapIdRole] = "mapId";
	names[EventIdRole] = "eventId";
	names[PageIdRole] = "pageId";
	names[LineRole] = "line";
	names[EventNameRole] = "eventName";
	names[EventRole] = "event";
	names[TypeRole] = "type";
	names[MatchRole] = "match";
	names[LocationIconRole] = "locationIcon";
	names[EventIconRole] = "eventIcon";
	names[TypeIconRole] = "typeIcon";
	names[CharsetFileRole] = "charsetFile";
	names[CharsetFrameRole] = "charsetFrame";
	names[CharsetNameRole] = "charsetName";
	return names;
}
