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
#include <QObject>
#include <QStringList>
#include <QtQmlIntegration>
#include <algorithm>
#include <concepts>
#include <type_traits>
#include <vector>
#include <lcf/rpg/state.h>

#include "common/dbstring.h"
#include "model/database_list_data.h"
#include "json.h"

class JsonView;

/**
 * A vector of objects of a Json (the actors, the pages of an event...) as a
 * list model: Json::list() makes them, one per pointer. The rows show the
 * entries ("0001: Alex"); an optional first row stands for none
 * (fallbackString, fallbackValue), for the combo boxes.
 *
 * The list follows its data: the changes of the Json at or above its
 * pointer (a merge, the list itself) and DatabaseListNotifier (the other
 * lists, a copy of the database put back) tell it to find its vector again and count the entries; the
 * changes in an entry repaint its row.
 *
 * The entry operations (resize, copy, paste, clear...) are those of the
 * database lists of RPG Maker. Their rows are the indexes of the entries,
 * without the fallback row; the list model rows count it.
 */
class JsonListView : public QAbstractListModel {
	Q_OBJECT
	QML_ELEMENT
	QML_UNCREATABLE("Lists are made by Json.list()")
	Q_PROPERTY(int fallbackValue READ fallbackValue WRITE setFallbackValue NOTIFY fallbackValueChanged)
	Q_PROPERTY(QString fallbackString READ fallbackString WRITE setFallbackString NOTIFY fallbackStringChanged)
	Q_PROPERTY(QString pathPrefix READ pathPrefix CONSTANT)
	/** The entries the views know (the fallback row not counted) */
	Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
	enum RoleNames {
		NameRole = Qt::UserRole + 1,
		TitleRole,
		IndexRole,
		IdRole
	};

	JsonListView(Json* json, const QString& path);
	~JsonListView() override;

	// pointers relative to the list ("/0/name"); documentation: see json.h
	Q_INVOKABLE QString str(QString jsonPtr) const;
	Q_INVOKABLE int num(QString jsonPtr) const;
	Q_INVOKABLE bool boolean(QString jsonPtr) const;
	Q_INVOKABLE QVariant value(QString jsonPtr) const;
	Q_INVOKABLE bool set(QString jsonPtr, const QVariant& value);
	Q_INVOKABLE JsonView* subtree(QString jsonPtr);
	Q_INVOKABLE JsonListView* list(QString jsonPtr);
	Q_INVOKABLE QString toJson(QString jsonPtr) const;

	/** The view of an entry ("name" in it is the entry's name) */
	Q_INVOKABLE JsonView* entry(int row);

	/**
	 * Array Size: entries added at the end as RPG Maker makes them, or
	 * removed from the end (entriesWithData() says what would be lost)
	 *
	 * @return false for less than one entry, or the same number
	 */
	Q_INVOKABLE bool resize(int count);
	/**
	 * The name of the entry at row ("name"), as the entry picker renames it:
	 * the views of the list in the other windows read it again
	 *
	 * @return false for no such entry
	 */
	Q_INVOKABLE bool rename(int row, const QString& name);
	/** Copy, Multiple Copy: count entries from row on, into the clipboard of their type */
	Q_INVOKABLE void copy(int row, int count = 1);
	/** Entries in the clipboard of the type of the list */
	Q_INVOKABLE int clipboardSize() const;
	/**
	 * Paste: the clipboard over the entries from row on, which keep their
	 * IDs (as RPG Maker; the list does not grow)
	 *
	 * @return The entries pasted
	 */
	Q_INVOKABLE int paste(int row);
	/** Clear: the entry as a new one, keeping its ID */
	Q_INVOKABLE bool clear(int row);
	/** RPG Maker keeps the first condition (the engine gives it at 0 HP) */
	Q_INVOKABLE bool canClear(int row) const;
	/** The entry is as a new one */
	Q_INVOKABLE bool isEmpty(int row) const;
	/** The texts of the entries holding data from row on (the warning of a smaller Array Size) */
	Q_INVOKABLE QStringList entriesWithData(int row) const;
	/** Entries inserted or removed in the middle (the pages of an event,
	 *  the members of a troop): the IDs follow the rows */
	Q_INVOKABLE bool insertEntries(int row, int count = 1);
	Q_INVOKABLE bool removeEntries(int row, int count = 1);
	Q_INVOKABLE bool moveEntry(int from, int to);

	// QAbstractListModel
	QHash<int, QByteArray> roleNames() const override;
	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	bool insertRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;
	bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;
	bool moveRows(const QModelIndex& sourceParent, int sourceRow, int count,
		const QModelIndex& destinationParent, int destinationChild) override;

	Json* json() const {
		return m_json;
	}
	/** The pointer of the list, normalized */
	QString pathPrefix() const {
		return m_path;
	}
	/** The view of the list */
	JsonView* view() const;

	int count() const {
		return static_cast<int>(m_rows);
	}
	int fallbackValue() const {
		return m_fallbackValue;
	}
	void setFallbackValue(int value);
	QString fallbackString() const {
		return m_fallbackString;
	}
	void setFallbackString(const QString& text);
	bool hasFallback() const {
		return !m_fallbackString.isEmpty();
	}

	/** Counts the entries of the vector again and repaints the rows */
	void sync();

signals:
	void fallbackValueChanged();
	void fallbackStringChanged();
	void countChanged();

protected:
	// the work on the vector: JsonListViewT

	/** Entries in the vector (0 when it is gone) */
	virtual qsizetype entryCount() const = 0;
	/** The vector (its address, as DatabaseListNotifier knows it) */
	virtual const void* vectorAddress() const = 0;
	virtual QVariant entryData(qsizetype row, int role) const = 0;
	virtual void insertEntriesAt(qsizetype row, qsizetype count) = 0;
	virtual void removeEntriesAt(qsizetype row, qsizetype count) = 0;
	/** Moves count entries from row to before destination (in the old indexes) */
	virtual void moveEntriesAt(qsizetype row, qsizetype count, qsizetype destination) = 0;
	/** The IDs follow the rows from row on (RPG Maker numbers the entries):
	 *  once the rows moved, a change of their data of its own */
	virtual void renumber(qsizetype row) = 0;
	virtual void resizeEntries(qsizetype count) = 0;
	virtual void copyEntries(qsizetype row, qsizetype count) = 0;
	virtual qsizetype clipboardEntries() const = 0;
	virtual qsizetype pasteEntries(qsizetype row) = 0;
	virtual void clearEntry(qsizetype row) = 0;
	virtual bool clearable(qsizetype row) const = 0;
	virtual bool entryIsEmpty(qsizetype row) const = 0;

	/** The data changed: the views of the Json and the other lists hear it */
	void changed();
	/** The fallback row first */
	int offset() const {
		return hasFallback() ? 1 : 0;
	}

	Json* m_json;
	QString m_path;

private:
	void onValueChanged(const QString& jsonPtr);
	bool validEntries(int row, int count) const;

	/** The entries the views know (the vector may already have more or less) */
	qsizetype m_rows = 0;
	/** The pointer goes through another list: that one can move the vector */
	bool m_nested = false;
	int m_fallbackValue = 0;
	QString m_fallbackString;
};

namespace JsonLists {

template<class T>
concept HasName = requires(const T& t) { ToQString(t.name); };
template<class T>
concept HasTitle = requires(const T& t) { ToQString(t.title); };
template<class T>
concept HasId = requires(T& t) { t.ID = 1; };

} // namespace JsonLists

/** The list of a std::vector<LCFTYPE>: made by the type table of the Json (json_node_impl.h) */
template<typename LCFTYPE>
class JsonListViewT : public JsonListView {
public:
	JsonListViewT(Json* json, const QString& path) : JsonListView(json, path) {
		sync();
	}

	/** The vector, found again after a change of the Json (nullptr: gone) */
	std::vector<LCFTYPE>* entries() const;

protected:
	qsizetype entryCount() const override {
		const auto* data = entries();
		return data ? static_cast<qsizetype>(data->size()) : 0;
	}
	const void* vectorAddress() const override {
		return entries();
	}
	QVariant entryData(qsizetype row, int role) const override;
	void insertEntriesAt(qsizetype row, qsizetype count) override;
	void removeEntriesAt(qsizetype row, qsizetype count) override;
	void moveEntriesAt(qsizetype row, qsizetype count, qsizetype destination) override;
	void resizeEntries(qsizetype count) override;
	void copyEntries(qsizetype row, qsizetype count) override;
	qsizetype clipboardEntries() const override {
		return static_cast<qsizetype>(DatabaseList::clipboard<LCFTYPE>().size());
	}
	qsizetype pasteEntries(qsizetype row) override;
	void clearEntry(qsizetype row) override;
	bool clearable(qsizetype row) const override {
		return !std::is_same_v<LCFTYPE, lcf::rpg::State> || row > 0;
	}
	bool entryIsEmpty(qsizetype row) const override;
	void renumber(qsizetype row) override;

private:
	LCFTYPE newEntry(qsizetype row) const {
		return DatabaseList::defaultEntry<LCFTYPE>(m_json->isRpg2k3(), static_cast<int>(row + 1));
	}

	mutable std::vector<LCFTYPE>* m_entries = nullptr;
	mutable quint64 m_generation = 0;
	mutable bool m_found = false;
};

template<typename LCFTYPE>
std::vector<LCFTYPE>* JsonListViewT<LCFTYPE>::entries() const {
	if (!m_found || m_generation != m_json->generation()) {
		const JsonNode found = m_json->node(m_path);
		m_entries = found.template is<std::vector<LCFTYPE>>() ? static_cast<std::vector<LCFTYPE>*>(found.data) : nullptr;
		m_generation = m_json->generation();
		m_found = true;
	}
	return m_entries;
}

template<typename LCFTYPE>
QVariant JsonListViewT<LCFTYPE>::entryData(qsizetype row, int role) const {
	const auto* data = entries();
	if (!data || row < 0 || row >= static_cast<qsizetype>(data->size())) {
		return {};
	}
	const LCFTYPE& entry = (*data)[static_cast<size_t>(row)];
	switch (role) {
		case NameRole:
			if constexpr (JsonLists::HasName<LCFTYPE>) {
				return ToQString(entry.name);
			}
			return {};
		case TitleRole:
			if constexpr (JsonLists::HasTitle<LCFTYPE>) {
				return ToQString(entry.title);
			}
			return {};
		case IdRole:
			if constexpr (JsonLists::HasId<LCFTYPE>) {
				return entry.ID;
			}
			return {};
		case Qt::DisplayRole:
			if constexpr (JsonLists::HasId<LCFTYPE> && JsonLists::HasName<LCFTYPE>) {
				return QStringLiteral("%1: %2").arg(entry.ID, 4, 10, QChar(u'0')).arg(ToQString(entry.name));
			} else if constexpr (JsonLists::HasName<LCFTYPE>) {
				return ToQString(entry.name);
			} else if constexpr (JsonLists::HasId<LCFTYPE>) {
				return QStringLiteral("%1").arg(entry.ID, 4, 10, QChar(u'0'));
			}
			return {};
		default:
			return {};
	}
}

template<typename LCFTYPE>
void JsonListViewT<LCFTYPE>::renumber(qsizetype row) {
	if constexpr (JsonLists::HasId<LCFTYPE>) {
		auto& data = *entries();
		for (size_t i = static_cast<size_t>(std::max<qsizetype>(row, 0)); i < data.size(); ++i) {
			data[i].ID = static_cast<int>(i + 1);
		}
	}
}

template<typename LCFTYPE>
void JsonListViewT<LCFTYPE>::insertEntriesAt(qsizetype row, qsizetype count) {
	auto& data = *entries();
	data.insert(data.begin() + row, static_cast<size_t>(count), newEntry(row));
}

template<typename LCFTYPE>
void JsonListViewT<LCFTYPE>::removeEntriesAt(qsizetype row, qsizetype count) {
	auto& data = *entries();
	data.erase(data.begin() + row, data.begin() + row + count);
}

template<typename LCFTYPE>
void JsonListViewT<LCFTYPE>::moveEntriesAt(qsizetype row, qsizetype count, qsizetype destination) {
	auto& data = *entries();
	const auto first = data.begin() + row;
	const auto last = first + count;
	if (destination < row) {
		std::rotate(data.begin() + destination, first, last);
	} else {
		std::rotate(first, last, data.begin() + destination);
	}
}

template<typename LCFTYPE>
void JsonListViewT<LCFTYPE>::resizeEntries(qsizetype count) {
	auto& data = *entries();
	const size_t previous = data.size();
	data.resize(static_cast<size_t>(count));
	for (size_t i = previous; i < data.size(); ++i) {
		data[i] = newEntry(static_cast<qsizetype>(i));
	}
}

template<typename LCFTYPE>
void JsonListViewT<LCFTYPE>::copyEntries(qsizetype row, qsizetype count) {
	const auto& data = *entries();
	DatabaseList::clipboard<LCFTYPE>().assign(data.begin() + row, data.begin() + row + count);
}

template<typename LCFTYPE>
qsizetype JsonListViewT<LCFTYPE>::pasteEntries(qsizetype row) {
	auto& data = *entries();
	const auto& clipboard = DatabaseList::clipboard<LCFTYPE>();
	qsizetype pasted = 0;
	for (; pasted < static_cast<qsizetype>(clipboard.size()) && row + pasted < static_cast<qsizetype>(data.size()); ++pasted) {
		auto& target = data[static_cast<size_t>(row + pasted)];
		if constexpr (JsonLists::HasId<LCFTYPE>) {
			const int id = target.ID;
			target = clipboard[static_cast<size_t>(pasted)];
			target.ID = id;
		} else {
			target = clipboard[static_cast<size_t>(pasted)];
		}
	}
	return pasted;
}

template<typename LCFTYPE>
void JsonListViewT<LCFTYPE>::clearEntry(qsizetype row) {
	auto& target = (*entries())[static_cast<size_t>(row)];
	if constexpr (JsonLists::HasId<LCFTYPE>) {
		LCFTYPE entry = newEntry(row);
		entry.ID = target.ID;
		target = std::move(entry);
	} else {
		target = newEntry(row);
	}
}

template<typename LCFTYPE>
bool JsonListViewT<LCFTYPE>::entryIsEmpty(qsizetype row) const {
	const auto& entry = (*entries())[static_cast<size_t>(row)];
	LCFTYPE empty = newEntry(row);
	if constexpr (JsonLists::HasId<LCFTYPE>) {
		empty.ID = entry.ID;
	}
	return entry == empty;
}
