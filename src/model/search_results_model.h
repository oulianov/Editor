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

#include <QAbstractTableModel>
#include <QRect>
#include <QString>
#include <vector>

/** A place where the search of the project (ProjectSearch) found something */
struct SearchResult
{
	/** What was found, for the icon of its type */
	enum class Kind { Variable, Switch, Item, Text, Name };

	/** A map, else ProjectSearch::CommonEventResults (0) or TroopResults (-1) */
	int mapId = 0;
	/** The event of the map, the common event or the monster group */
	int eventId = 0;
	/** Its page, 0 for none (a common event, the event itself) */
	int pageId = 0;
	/** The line of the command, from 1; 0 for none (a name, a condition) */
	int line = 0;
	/** The maps from the top of the tree ("World > Town"), else "Common
	 *  Events" or "Monster Groups" */
	QString location;
	/** The name of the event, the common event or the monster group */
	QString eventName;
	/** What refers to the query: "Variable, Switch", "Event text", "Event"... */
	QString type;
	/** The first type */
	Kind kind = Kind::Text;
	/** What was found: the command as the event list shows it, "Event page condition"... */
	QString match;
	/** The CharSet file of the event's graphic on the page (empty for
	 *  none) and the frame shown, as the map shows the event */
	QString charsetFile;
	/** Its name in the CharSet folder, as the event names it (image://project/CharSet/...) */
	QString charsetName;
	QRect charsetFrame;
};

/**
 * The results of a search of the project as a table: the columns of the
 * Search window (map, event, page, type, match), and roles with what each
 * column shows and what opens the result (map, event, page and line). A
 * QML list reads the roles of the first column.
 *
 * The icons are named (resources/icons), the interface picks their variant.
 */
class SearchResultsModel : public QAbstractTableModel
{
	Q_OBJECT
	Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
	enum Column { MapColumn, EventColumn, PageColumn, TypeColumn, MatchColumn, ColumnCount };
	Q_ENUM(Column)

	enum Role {
		LocationRole = Qt::UserRole + 1,
		MapIdRole,
		EventIdRole,
		PageIdRole,
		LineRole,
		EventNameRole,
		/** "12: Name", as the event column shows it */
		EventRole,
		TypeRole,
		MatchRole,
		/** The icons of the columns: tree or database (map), layer-event or
		 *  code (event), variable, switch, resources or code (type) */
		LocationIconRole,
		EventIconRole,
		TypeIconRole,
		/** The graphic of the event: its CharSet file, empty for none */
		CharsetFileRole,
		/** The QRect of the frame shown in the CharSet */
		CharsetFrameRole,
		/** The name of the CharSet, empty for none */
		CharsetNameRole,
	};

	explicit SearchResultsModel(QObject* parent = nullptr);
	~SearchResultsModel() override;

	/** The results of a new search */
	void setResults(std::vector<SearchResult> results);
	Q_INVOKABLE void clear();
	const std::vector<SearchResult>& results() const;
	/** The result of a row, nullptr for none */
	const SearchResult* result(int row) const;
	int count() const;

	// QAbstractItemModel
	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	int columnCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
	QHash<int, QByteArray> roleNames() const override;

signals:
	void countChanged();

private:
	std::vector<SearchResult> m_results;
};
