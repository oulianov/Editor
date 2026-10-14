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

#include "json.h"
#include "json_list_view.h"
#include "json_view.h"

#include <QDebug>
#include <QQmlEngine>
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace {

/** What a reader gets for a pointer that points at nothing (upstream's) */
const QString BadPointer = QStringLiteral("!BAD POINTER!");
constexpr int BadNumber = -0xEA51;

/** The pointer holding both (normalized) */
QString commonAncestor(const QString& a, const QString& b) {
	if (Json::affects(a, b)) {
		return a.size() <= b.size() ? a : b;
	}
	const auto left = QStringView(a).split(u'/');
	const auto right = QStringView(b).split(u'/');
	QString common;
	// [0] is the empty piece before the first slash
	for (qsizetype i = 1; i < left.size() && i < right.size() && left[i] == right[i]; ++i) {
		common += u'/' + left[i].toString();
	}
	return common;
}

} // namespace

Json::Json(QObject* parent) : QObject(parent) {}

Json::~Json() = default;

QString Json::normalize(const QString& jsonPtr) {
	qsizetype begin = 0;
	qsizetype end = jsonPtr.size();
	while (begin < end && jsonPtr[begin] == u'/') {
		++begin;
	}
	while (end > begin && jsonPtr[end - 1] == u'/') {
		--end;
	}
	if (begin == end) {
		return {};
	}
	return u'/' + jsonPtr.mid(begin, end - begin);
}

QString Json::join(const QString& base, const QString& name) {
	return normalize(base) + normalize(name);
}

bool Json::affects(const QString& changed, const QString& ptr) {
	if (changed.size() == ptr.size()) {
		return changed == ptr;
	}
	// one is the other or under it: "/actors" and "/actors/1", not "/actors1"
	const QString& shorter = changed.size() < ptr.size() ? changed : ptr;
	const QString& longer = changed.size() < ptr.size() ? ptr : changed;
	return longer.startsWith(shorter) && longer[shorter.size()] == u'/';
}

bool Json::isRpg2k3() const {
	return false;
}

JsonNode Json::found(const QString& jsonPtr) const {
	return node(normalize(jsonPtr));
}

QString Json::str(QString jsonPtr) const {
	const JsonNode found = this->found(jsonPtr);
	if (found.exists() && !found.isBit() && found.type->kind == JsonNodeType::Kind::String) {
		return found.type->read(found.data).toString();
	}
	qDebug() << "Json::str: Not pointing to String:" << jsonPtr;
	return BadPointer;
}

int Json::num(QString jsonPtr) const {
	const JsonNode found = this->found(jsonPtr);
	if (found.isBit()) {
		return found.type->bit(found.data, found.bit, false) ? 1 : 0;
	}
	if (found.exists()) {
		switch (found.type->kind) {
			case JsonNodeType::Kind::Integer:
				// the widest (uint32_t) wraps as before
				return static_cast<int>(found.type->read(found.data).toLongLong());
			case JsonNodeType::Kind::Bool:
				return found.type->read(found.data).toBool() ? 1 : 0;
			case JsonNodeType::Kind::Floating: {
				const double d = std::round(found.type->read(found.data).toDouble());
				return static_cast<int>(std::clamp(d, double(std::numeric_limits<int>::min()), double(std::numeric_limits<int>::max())));
			}
			default:
				break;
		}
	}
	qDebug() << "Json::num: Not pointing to a number:" << jsonPtr;
	return BadNumber;
}

bool Json::boolean(QString jsonPtr) const {
	const JsonNode found = this->found(jsonPtr);
	if (found.isBit()) {
		return found.type->bit(found.data, found.bit, false);
	}
	if (found.exists()) {
		switch (found.type->kind) {
			case JsonNodeType::Kind::Bool:
				return found.type->read(found.data).toBool();
			case JsonNodeType::Kind::Integer:
				return found.type->read(found.data).toLongLong() != 0;
			default:
				break;
		}
	}
	qDebug() << "Json::boolean: Not pointing to bool:" << jsonPtr;
	return false;
}

QVariant Json::value(QString jsonPtr) const {
	const JsonNode found = this->found(jsonPtr);
	if (found.isBit()) {
		return found.type->bit(found.data, found.bit, false);
	}
	return found.exists() ? found.type->read(found.data) : QVariant();
}

bool Json::set(QString jsonPtr, const QVariant& value) {
	const QString ptr = normalize(jsonPtr);
	const JsonNode found = node(ptr);
	bool ok = false;
	bool changed = false;
	if (found.isBit()) {
		ok = value.canConvert<bool>() && found.type->setBit(found.data, found.bit, value.toBool(), false, &changed);
	} else if (found.exists() && found.type->write) {
		ok = found.type->write(found.data, value, &changed);
	}
	if (!ok) {
		qDebug() << QStringLiteral("Json::set: Assignment of type %1 failed for path %2").arg(QString::fromLatin1(value.typeName()), jsonPtr);
		return false;
	}
	if (changed) {
		emitChanged(ptr);
	}
	return true;
}

bool Json::bit(QString jsonPtr, int index, bool missing) const {
	const JsonNode found = this->found(jsonPtr);
	if (!found.exists() || found.isBit() || found.type->kind != JsonNodeType::Kind::Bits) {
		qDebug() << "Json::bit: Not pointing to a bit array:" << jsonPtr;
		return missing;
	}
	return found.type->bit(found.data, index, missing);
}

bool Json::setBit(QString jsonPtr, int index, bool on, bool missing) {
	const QString ptr = normalize(jsonPtr);
	const JsonNode found = node(ptr);
	bool changed = false;
	if (!found.exists() || found.isBit() || !found.type->setBit(found.data, index, on, missing, &changed)) {
		qDebug() << "Json::setBit: Not pointing to a bit array:" << jsonPtr << index;
		return false;
	}
	if (changed) {
		// the array: it may have grown
		emitChanged(ptr);
	}
	return true;
}

int Json::size(QString jsonPtr) const {
	const JsonNode found = this->found(jsonPtr);
	if (!found.exists() || found.isBit()) {
		return -1;
	}
	return static_cast<int>(found.type->size(found.data));
}

bool Json::exists(QString jsonPtr) const {
	return found(jsonPtr).exists();
}

QVariant Json::subtree(QString jsonPtr) {
	return QVariant::fromValue(view(jsonPtr));
}

QVariant Json::list(QString jsonPtr) {
	return QVariant::fromValue(listView(jsonPtr));
}

QVariant Json::choices(QString jsonPtr, QString noneText, int noneValue) {
	return QVariant::fromValue(choiceList(jsonPtr, noneText, noneValue));
}

QString Json::toJson(QString jsonPtr) const {
	const JsonNode found = this->found(jsonPtr);
	if (found.isBit()) {
		return found.type->bit(found.data, found.bit, false) ? QStringLiteral("true") : QStringLiteral("false");
	}
	if (!found.exists()) {
		qDebug() << "Json::toJson: Invalid pointer:" << jsonPtr;
		return BadPointer;
	}
	return QString::fromStdString(found.type->json(found.data));
}

JsonView* Json::view(const QString& jsonPtr) {
	const QString ptr = normalize(jsonPtr);
	JsonView*& view = m_views[ptr];
	if (!view) {
		view = new JsonView(this, ptr);
		// owned here, whoever asks for it (QML would collect what its
		// functions return)
		QQmlEngine::setObjectOwnership(view, QQmlEngine::CppOwnership);
	}
	return view;
}

JsonListView* Json::listView(const QString& jsonPtr) {
	const QString ptr = normalize(jsonPtr);
	if (JsonListView* list = m_lists.value(ptr)) {
		return list;
	}
	const JsonNode found = node(ptr);
	if (!found.exists() || found.isBit() || !found.type->makeList) {
		qDebug() << "Json::list: Not pointing to a list of objects:" << jsonPtr;
		return nullptr;
	}
	JsonListView* list = found.type->makeList(this, ptr);
	QQmlEngine::setObjectOwnership(list, QQmlEngine::CppOwnership);
	m_lists.insert(ptr, list);
	return list;
}

JsonListView* Json::choiceList(const QString& jsonPtr, const QString& noneText, int noneValue) {
	const QString ptr = normalize(jsonPtr);
	const QString key = ptr + u'\n' + noneText + u'\n' + QString::number(noneValue);
	if (JsonListView* list = m_choices.value(key)) {
		return list;
	}
	const JsonNode found = node(ptr);
	if (!found.exists() || found.isBit() || !found.type->makeList) {
		qDebug() << "Json::choices: Not pointing to a list of objects:" << jsonPtr;
		return nullptr;
	}
	JsonListView* list = found.type->makeList(this, ptr);
	QQmlEngine::setObjectOwnership(list, QQmlEngine::CppOwnership);
	list->setFallbackValue(noneValue);
	list->setFallbackString(noneText);
	m_choices.insert(key, list);
	return list;
}

void Json::beginBatch() {
	++m_batch;
}

void Json::endBatch() {
	if (m_batch == 0) {
		qWarning() << "Json::endBatch without beginBatch";
		return;
	}
	if (--m_batch > 0 || !m_batchChanged) {
		return;
	}
	m_batchChanged = false;
	emit valueChanged(std::exchange(m_batchPtr, QString()));
}

void Json::notifyChanged(const QString& jsonPtr) {
	emitChanged(normalize(jsonPtr));
}

void Json::emitChanged(const QString& jsonPtr) {
	// the lists find their vector again (the change may have moved it)
	++m_generation;
	if (m_batch > 0) {
		m_batchPtr = m_batchChanged ? commonAncestor(m_batchPtr, jsonPtr) : jsonPtr;
		m_batchChanged = true;
		return;
	}
	emit valueChanged(jsonPtr);
}
