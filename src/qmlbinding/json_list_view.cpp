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

#include "json_list_view.h"
#include "json_view.h"

JsonListView::JsonListView(Json* json, const QString& path) :
	QAbstractListModel(json), m_json(json), m_path(Json::normalize(path))
{
	m_nested = m_path.count(u'/') > 1;
	connect(json, &Json::valueChanged, this, &JsonListView::onValueChanged);
	// another list, or a copy of the database put back
	// (DatabaseListNotifier::assign), changed a list: this one, or the one
	// holding it
	connect(&DatabaseListNotifier::instance(), &DatabaseListNotifier::listChanged, this, [this](const void* list, QObject* source) {
		if (source != this && (m_nested || list == vectorAddress())) {
			sync();
		}
	});
}

JsonListView::~JsonListView() = default;

QHash<int, QByteArray> JsonListView::roleNames() const {
	QHash<int, QByteArray> roles;
	roles[Qt::DisplayRole] = "text";
	roles[NameRole] = "name";
	roles[TitleRole] = "title";
	roles[IdRole] = "value";
	roles[IndexRole] = "index";
	return roles;
}

int JsonListView::rowCount(const QModelIndex& parent) const {
	return parent.isValid() ? 0 : static_cast<int>(m_rows) + offset();
}

QVariant JsonListView::data(const QModelIndex& index, int role) const {
	if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
		return {};
	}
	if (role == IndexRole) {
		return index.row();
	}
	if (hasFallback() && index.row() == 0) {
		switch (role) {
			case Qt::DisplayRole:
			case NameRole:
				return m_fallbackString;
			case IdRole:
				return m_fallbackValue;
			default:
				return {};
		}
	}
	return entryData(index.row() - offset(), role);
}

void JsonListView::sync() {
	const qsizetype rows = entryCount();
	const qsizetype known = m_rows;
	// rows inserted or removed instead of a reset: the views keep their current entry
	if (rows > m_rows) {
		beginInsertRows(QModelIndex(), static_cast<int>(m_rows) + offset(), static_cast<int>(rows) - 1 + offset());
		m_rows = rows;
		endInsertRows();
	} else if (rows < m_rows) {
		beginRemoveRows(QModelIndex(), static_cast<int>(rows) + offset(), static_cast<int>(m_rows) - 1 + offset());
		m_rows = rows;
		endRemoveRows();
	}
	if (rowCount() > 0) {
		emit dataChanged(index(0), index(rowCount() - 1));
	}
	if (known != m_rows) {
		emit countChanged();
	}
}

void JsonListView::onValueChanged(const QString& jsonPtr) {
	if (!Json::affects(jsonPtr, m_path)) {
		return;
	}
	if (jsonPtr.size() <= m_path.size()) {
		// the list or what holds it
		sync();
		return;
	}
	// an entry: "/3/name" under the list
	const QStringView rest = QStringView(jsonPtr).mid(m_path.size() + 1);
	const qsizetype slash = rest.indexOf(u'/');
	bool ok = false;
	const int row = rest.left(slash < 0 ? rest.size() : slash).toInt(&ok);
	const QModelIndex changed = ok ? index(row + offset()) : QModelIndex();
	if (changed.isValid()) {
		emit dataChanged(changed, changed);
	}
}

void JsonListView::changed() {
	DatabaseListNotifier::notify(vectorAddress(), this);
	m_json->notifyChanged(m_path);
}

bool JsonListView::validEntries(int row, int count) const {
	return row >= 0 && count > 0 && row + count <= m_rows && m_rows == entryCount();
}

QString JsonListView::str(QString jsonPtr) const {
	return m_json->str(Json::join(m_path, jsonPtr));
}

int JsonListView::num(QString jsonPtr) const {
	return m_json->num(Json::join(m_path, jsonPtr));
}

bool JsonListView::boolean(QString jsonPtr) const {
	return m_json->boolean(Json::join(m_path, jsonPtr));
}

QVariant JsonListView::value(QString jsonPtr) const {
	return m_json->value(Json::join(m_path, jsonPtr));
}

bool JsonListView::set(QString jsonPtr, const QVariant& value) {
	return m_json->set(Json::join(m_path, jsonPtr), value);
}

JsonView* JsonListView::subtree(QString jsonPtr) {
	return m_json->view(Json::join(m_path, jsonPtr));
}

JsonListView* JsonListView::list(QString jsonPtr) {
	return m_json->listView(Json::join(m_path, jsonPtr));
}

QString JsonListView::toJson(QString jsonPtr) const {
	return m_json->toJson(Json::join(m_path, jsonPtr));
}

JsonView* JsonListView::entry(int row) {
	return m_json->view(m_path + u'/' + QString::number(row));
}

JsonView* JsonListView::view() const {
	return m_json->view(m_path);
}

void JsonListView::setFallbackValue(int value) {
	if (m_fallbackValue == value) {
		return;
	}
	m_fallbackValue = value;
	if (hasFallback()) {
		emit dataChanged(index(0), index(0));
	}
	emit fallbackValueChanged();
}

void JsonListView::setFallbackString(const QString& text) {
	if (m_fallbackString == text) {
		return;
	}
	if (m_fallbackString.isEmpty()) {
		beginInsertRows(QModelIndex(), 0, 0);
		m_fallbackString = text;
		endInsertRows();
	} else if (text.isEmpty()) {
		beginRemoveRows(QModelIndex(), 0, 0);
		m_fallbackString.clear();
		endRemoveRows();
	} else {
		m_fallbackString = text;
		emit dataChanged(index(0), index(0));
	}
	emit fallbackStringChanged();
}

bool JsonListView::resize(int count) {
	sync();
	if (count < 1 || count == m_rows || !vectorAddress()) {
		return false;
	}
	if (count > m_rows) {
		beginInsertRows(QModelIndex(), static_cast<int>(m_rows) + offset(), count - 1 + offset());
		resizeEntries(count);
		m_rows = count;
		endInsertRows();
	} else {
		beginRemoveRows(QModelIndex(), count + offset(), static_cast<int>(m_rows) - 1 + offset());
		resizeEntries(count);
		m_rows = count;
		endRemoveRows();
	}
	emit countChanged();
	changed();
	return true;
}

bool JsonListView::rename(int row, const QString& name) {
	sync();
	if (row < 0 || row >= m_rows || !m_json->set(Json::join(m_path, QString::number(row) + QStringLiteral("/name")), name)) {
		return false;
	}
	// the views of the same list through another Json (the project's, a command editor's)
	DatabaseListNotifier::notify(vectorAddress(), this);
	return true;
}

void JsonListView::copy(int row, int count) {
	if (row < 0 || row >= m_rows || count < 1) {
		return;
	}
	copyEntries(row, std::min<qsizetype>(count, m_rows - row));
}

int JsonListView::clipboardSize() const {
	return static_cast<int>(clipboardEntries());
}

int JsonListView::paste(int row) {
	if (!validEntries(row, 1)) {
		return 0;
	}
	const qsizetype pasted = pasteEntries(row);
	if (pasted > 0) {
		changed();
	}
	return static_cast<int>(pasted);
}

bool JsonListView::clear(int row) {
	if (!canClear(row)) {
		return false;
	}
	clearEntry(row);
	changed();
	return true;
}

bool JsonListView::canClear(int row) const {
	return validEntries(row, 1) && clearable(row);
}

bool JsonListView::isEmpty(int row) const {
	return validEntries(row, 1) && entryIsEmpty(row);
}

QStringList JsonListView::entriesWithData(int row) const {
	QStringList texts;
	for (qsizetype i = std::max(row, 0); i < m_rows && i < entryCount(); ++i) {
		if (!entryIsEmpty(i)) {
			texts << entryData(i, Qt::DisplayRole).toString();
		}
	}
	return texts;
}

bool JsonListView::insertEntries(int row, int count) {
	return insertRows(row + offset(), count);
}

bool JsonListView::removeEntries(int row, int count) {
	return removeRows(row + offset(), count);
}

bool JsonListView::moveEntry(int from, int to) {
	// to: where the entry is once moved
	if (from == to || !validEntries(from, 1) || !validEntries(to, 1)) {
		return false;
	}
	return moveRows(QModelIndex(), from + offset(), 1, QModelIndex(), (to > from ? to + 1 : to) + offset());
}

bool JsonListView::insertRows(int row, int count, const QModelIndex& parent) {
	const int entry = row - offset();
	if (parent.isValid() || entry < 0 || entry > m_rows || count < 1 || m_rows != entryCount() || !vectorAddress()) {
		return false;
	}
	beginInsertRows(QModelIndex(), row, row + count - 1);
	insertEntriesAt(entry, count);
	m_rows += count;
	endInsertRows();
	renumber(entry);
	emit countChanged();
	changed();
	return true;
}

bool JsonListView::removeRows(int row, int count, const QModelIndex& parent) {
	const int entry = row - offset();
	if (parent.isValid() || !validEntries(entry, count)) {
		return false;
	}
	beginRemoveRows(QModelIndex(), row, row + count - 1);
	removeEntriesAt(entry, count);
	m_rows -= count;
	endRemoveRows();
	renumber(entry);
	emit countChanged();
	changed();
	return true;
}

bool JsonListView::moveRows(const QModelIndex& sourceParent, int sourceRow, int count,
		const QModelIndex& destinationParent, int destinationChild) {
	const int entry = sourceRow - offset();
	const int destination = destinationChild - offset();
	if (sourceParent.isValid() || destinationParent.isValid() || !validEntries(entry, count)
			|| destination < 0 || destination > m_rows || (destination >= entry && destination <= entry + count)) {
		return false;
	}
	if (!beginMoveRows(QModelIndex(), sourceRow, sourceRow + count - 1, QModelIndex(), destinationChild)) {
		return false;
	}
	moveEntriesAt(entry, count, destination);
	endMoveRows();
	renumber(std::min(entry, destination));
	changed();
	return true;
}
