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

#include <QObject>
#include <QPointer>
#include <QVariant>
#include <QtQmlIntegration>

class Json;

/**
 * One value of a Json, bound both ways: it follows the data (valueChanged
 * of the Json at, above or under its pointer) and writes what it is given.
 *
 *   Ez.JsonField { id: name; source: page.jsonData; path: "name" }
 *   TextField { text: name.value; onTextEdited: name.value = text }
 *
 * source: a Json, a JsonView (path relative to it) or a JsonListView.
 * A value the data cannot take (a text for a number) is not written: value
 * says the data's again.
 */
class JsonField : public QObject {
	Q_OBJECT
	QML_ELEMENT
	Q_PROPERTY(QObject* source READ source WRITE setSource NOTIFY sourceChanged)
	Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged)
	Q_PROPERTY(QVariant value READ value WRITE setValue NOTIFY valueChanged)
	/** The pointer points at a value */
	Q_PROPERTY(bool valid READ valid NOTIFY validChanged)
	/** The pointer in the Json, normalized ("/actors/0/name") */
	Q_PROPERTY(QString jsonPointer READ jsonPointer NOTIFY jsonPointerChanged)

public:
	explicit JsonField(QObject* parent = nullptr);

	QObject* source() const {
		return m_source;
	}
	void setSource(QObject* source);
	QString path() const {
		return m_path;
	}
	void setPath(const QString& path);
	QVariant value() const {
		return m_value;
	}
	void setValue(const QVariant& value);
	bool valid() const {
		return m_valid;
	}
	QString jsonPointer() const {
		return m_pointer;
	}

	/** Reads the value from the data again */
	Q_INVOKABLE void refresh();

signals:
	void sourceChanged();
	void pathChanged();
	void valueChanged();
	void validChanged();
	void jsonPointerChanged();

private:
	void bind();
	void onValueChanged(const QString& jsonPtr);

	QPointer<QObject> m_source;
	QPointer<Json> m_json;
	QMetaObject::Connection m_connection;
	QString m_path;
	QString m_pointer;
	QVariant m_value;
	bool m_valid = false;
};
