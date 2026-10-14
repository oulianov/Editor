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

#include "json_field.h"
#include "json.h"
#include "json_list_view.h"
#include "json_view.h"

JsonField::JsonField(QObject* parent) : QObject(parent) {}

void JsonField::setSource(QObject* source) {
	if (m_source == source) {
		return;
	}
	m_source = source;
	bind();
	emit sourceChanged();
}

void JsonField::setPath(const QString& path) {
	if (m_path == path) {
		return;
	}
	m_path = path;
	bind();
	emit pathChanged();
}

void JsonField::bind() {
	QObject::disconnect(m_connection);
	Json* json = nullptr;
	QString pointer;
	if (auto* view = qobject_cast<JsonView*>(m_source)) {
		json = view->json();
		pointer = view->path(m_path);
	} else if (auto* list = qobject_cast<JsonListView*>(m_source)) {
		json = list->json();
		pointer = Json::join(list->pathPrefix(), m_path);
	} else if ((json = qobject_cast<Json*>(m_source))) {
		pointer = Json::normalize(m_path);
	}
	m_json = json;
	if (json) {
		m_connection = connect(json, &Json::valueChanged, this, &JsonField::onValueChanged);
	}
	if (m_pointer != pointer) {
		m_pointer = pointer;
		emit jsonPointerChanged();
	}
	refresh();
}

void JsonField::refresh() {
	const bool valid = m_json && m_json->exists(m_pointer);
	const QVariant value = valid ? m_json->value(m_pointer) : QVariant();
	const bool changed = value != m_value || value.typeId() != m_value.typeId();
	m_value = value;
	if (valid != m_valid) {
		m_valid = valid;
		emit validChanged();
	}
	if (changed) {
		emit valueChanged();
	}
}

void JsonField::setValue(const QVariant& value) {
	if (!m_json || !m_valid) {
		return;
	}
	if (!m_json->set(m_pointer, value)) {
		// the control shows the data's value again
		emit valueChanged();
		return;
	}
	// the data changed (refresh() tells it), or kept its value: a value
	// converted to it (rounded, clamped) is shown as the data has it
	const QVariant before = m_value;
	refresh();
	if (m_value == before && value != m_value) {
		emit valueChanged();
	}
}

void JsonField::onValueChanged(const QString& jsonPtr) {
	if (Json::affects(jsonPtr, m_pointer)) {
		refresh();
	}
}
