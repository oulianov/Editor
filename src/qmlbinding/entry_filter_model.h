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

#include <QSet>
#include <QAbstractListModel>
#include <QSortFilterProxyModel>
#include <QVariant>
#include <optional>
#include <QtQmlIntegration>

/**
 * The entries of a list that match a search (filterText), as every
 * searchable list finds them (SearchMatch): for every word,
 * the number of the entry starts with it or its name contains it.
 *
 *   Ez.EntryFilterModel { id: found; sourceModel: database.list("items"); filterText: search.text }
 *
 * The number of an entry is its "value" role (JsonListView), else read from
 * its text ("0012: Potion").
 *
 * ids keeps only the entries of these numbers (the weapons a hero can
 * equip...), whatever is searched; unset (undefined), every entry.
 */
class EntryFilterModel : public QSortFilterProxyModel {
	Q_OBJECT
	QML_ELEMENT
	Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
	/** The role of the numbers (-1: read from the texts) */
	Q_PROPERTY(int idRole READ idRole WRITE setIdRole NOTIFY idRoleChanged)
	Q_PROPERTY(QVariant ids READ ids WRITE setIds NOTIFY idsChanged)

public:
	explicit EntryFilterModel(QObject* parent = nullptr);

	QString filterText() const {
		return m_filterText;
	}
	void setFilterText(const QString& text);
	int idRole() const {
		return m_idRole;
	}
	void setIdRole(int role);
	QVariant ids() const {
		return m_ids;
	}
	void setIds(const QVariant& ids);

	/** The row of the source shown at row (-1: none) */
	Q_INVOKABLE int sourceRow(int row) const;
	/** The row showing the row of the source (-1: filtered out) */
	Q_INVOKABLE int rowOf(int sourceRow) const;
	/** The row to select for the search: the entry of the number typed, else the first one (-1: none) */
	Q_INVOKABLE int bestRow() const;

signals:
	void filterTextChanged();
	void idRoleChanged();
	void idsChanged();

protected:
	bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
	int entryId(const QModelIndex& index) const;

	QString m_filterText;
	int m_idRole;
	QVariant m_ids;
	/** m_ids as numbers; none set: every entry */
	std::optional<QSet<int>> m_idSet;
};

/**
 * A list of choices of QML ([{text, value}]) as a model, its roles the
 * ones of a database list (JsonListView: "text", "value"), so a list that is
 * not of the database can be searched (Ez.RefComboBox's choices)
 */
class ChoiceListModel : public QAbstractListModel {
	Q_OBJECT
	QML_ELEMENT
	Q_PROPERTY(QVariantList entries READ entries WRITE setEntries NOTIFY entriesChanged)

public:
	explicit ChoiceListModel(QObject* parent = nullptr);

	QVariantList entries() const {
		return m_entries;
	}
	void setEntries(const QVariantList& entries);

	int rowCount(const QModelIndex& parent = {}) const override;
	QVariant data(const QModelIndex& index, int role) const override;
	QHash<int, QByteArray> roleNames() const override;

signals:
	void entriesChanged();

private:
	QVariantList m_entries;
};
