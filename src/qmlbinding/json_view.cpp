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

#include "json_view.h"
#include "json_list_view.h"

JsonView::JsonView(Json* json, const QString& prefix) :
	QObject(json), m_json(json), m_pathPrefix(Json::normalize(prefix))
{
	// every change of the data: the slot keeps those of the view
	connect(json, &Json::valueChanged, this, &JsonView::onValueChanged);
}

QString JsonView::str(QString jsonPtr) const {
	return m_json->str(path(jsonPtr));
}

int JsonView::num(QString jsonPtr) const {
	return m_json->num(path(jsonPtr));
}

bool JsonView::boolean(QString jsonPtr) const {
	return m_json->boolean(path(jsonPtr));
}

QVariant JsonView::value(QString jsonPtr) const {
	return m_json->value(path(jsonPtr));
}

bool JsonView::set(QString jsonPtr, const QVariant& value) {
	return m_json->set(path(jsonPtr), value);
}

bool JsonView::bit(QString jsonPtr, int index, bool missing) const {
	return m_json->bit(path(jsonPtr), index, missing);
}

bool JsonView::setBit(QString jsonPtr, int index, bool on, bool missing) {
	return m_json->setBit(path(jsonPtr), index, on, missing);
}

int JsonView::size(QString jsonPtr) const {
	return m_json->size(path(jsonPtr));
}

bool JsonView::exists(QString jsonPtr) const {
	return m_json->exists(path(jsonPtr));
}

JsonView* JsonView::subtree(QString jsonPtr) {
	return m_json->view(path(jsonPtr));
}

JsonListView* JsonView::list(QString jsonPtr) {
	return m_json->listView(path(jsonPtr));
}

JsonListView* JsonView::choices(QString jsonPtr, QString noneText, int noneValue) {
	return m_json->choiceList(path(jsonPtr), noneText, noneValue);
}

QString JsonView::toJson(QString jsonPtr) const {
	return m_json->toJson(path(jsonPtr));
}

void JsonView::beginBatch() {
	m_json->beginBatch();
}

void JsonView::endBatch() {
	m_json->endBatch();
}

bool JsonView::isValid() const {
	return m_json->exists(m_pathPrefix);
}

void JsonView::onValueChanged(const QString& jsonPtr) {
	if (!Json::affects(jsonPtr, m_pathPrefix)) {
		return;
	}
	// under the view: relative to it; at or above it: all of it
	if (jsonPtr.size() > m_pathPrefix.size()) {
		emit valueChanged(jsonPtr.mid(m_pathPrefix.size()));
		return;
	}
	emit validChanged();
	emit valueChanged(QString());
}
