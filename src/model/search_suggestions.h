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
#include <QString>
#include <vector>

/**
 * The entries the search of the project suggests while its text is typed
 * (variables, switches, items, common events): "12: Variable — Name", those
 * whose number or name has the text, the closest first (the number, the
 * name, then the start of the number or name, then anywhere in them).
 * Choosing one searches it with its type.
 */
class SearchSuggestions : public QAbstractListModel
{
	Q_OBJECT
	Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
	enum Role {
		/** The number of the entry */
		IdRole = Qt::UserRole + 1,
		/** ProjectSearch::Type: Variables, Switches, Items or EventNames (common events) */
		TypeRole,
		NameRole,
		/** "12: Variable — Name" (DisplayRole too) */
		LabelRole,
		/** Of resources/icons: variable, switch, resources, code */
		IconRole,
	};

	struct Entry {
		int id = 0;
		int type = 0;
		QString name;
		QString label;
		QString icon;
	};

	explicit SearchSuggestions(QObject* parent = nullptr);

	/** The entries to suggest from */
	void setEntries(std::vector<Entry> entries);
	/** The text typed: the entries suggested for it (none for an empty text) */
	Q_INVOKABLE void setQuery(const QString& text);
	QString query() const { return m_query; }
	/** The entry of a row, nullptr for none */
	const Entry* entry(int row) const;
	/** The text and the type of the entry of a row (empty, -1 for none) */
	Q_INVOKABLE QString labelAt(int row) const;
	Q_INVOKABLE int typeAt(int row) const;

	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	QHash<int, QByteArray> roleNames() const override;

signals:
	void countChanged();

private:
	/** How close an entry is to the query: 0 its number... 6 none */
	int rank(const Entry& entry) const;
	void update();

	std::vector<Entry> m_entries;
	/** The entries shown, the closest first */
	std::vector<int> m_shown;
	QString m_query;
	int m_id = -1;
};
