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
#include <QtQmlIntegration>

#include "json.h"

class JsonListView;

/**
 * A Json seen from a pointer (pathPrefix): its pointers are relative to it
 * ("name" in the view of "/actors/0" is "/actors/0/name"). Json::subtree()
 * makes them, one per pointer.
 *
 * valueChanged(ptr): something at or under ptr changed, ptr relative to the
 * view ("/name"); "" when it is the whole view (a change above it: the list
 * holding the entry pasted, the database merged).
 */
class JsonView : public QObject {
	Q_OBJECT
	QML_ELEMENT
	QML_UNCREATABLE("Views are made by Json.subtree()")
	Q_PROPERTY(QString pathPrefix READ pathPrefix CONSTANT)
	/** Whether the prefix points at something (an entry beyond the end of its list does not) */
	Q_PROPERTY(bool valid READ isValid NOTIFY validChanged)

public:
	JsonView(Json* json, const QString& prefix);

	// documentation: see json.h; the pointers are relative to the view

	Q_INVOKABLE QString str(QString jsonPtr) const;
	Q_INVOKABLE int num(QString jsonPtr) const;
	Q_INVOKABLE bool boolean(QString jsonPtr) const;
	Q_INVOKABLE QVariant value(QString jsonPtr) const;
	Q_INVOKABLE bool set(QString jsonPtr, const QVariant& value);
	Q_INVOKABLE bool bit(QString jsonPtr, int index, bool missing = false) const;
	Q_INVOKABLE bool setBit(QString jsonPtr, int index, bool on, bool missing = false);
	Q_INVOKABLE int size(QString jsonPtr) const;
	Q_INVOKABLE bool exists(QString jsonPtr) const;
	Q_INVOKABLE JsonView* subtree(QString jsonPtr);
	Q_INVOKABLE JsonListView* list(QString jsonPtr);
	Q_INVOKABLE JsonListView* choices(QString jsonPtr, QString noneText, int noneValue = 0);
	Q_INVOKABLE QString toJson(QString jsonPtr) const;
	Q_INVOKABLE void beginBatch();
	Q_INVOKABLE void endBatch();

	/** The pointer of the view, normalized ("/actors/0", "" for the root) */
	QString pathPrefix() const {
		return m_pathPrefix;
	}
	/** The pointer of a pointer relative to the view */
	QString path(const QString& jsonPtr) const {
		return m_pathPrefix + Json::normalize(jsonPtr);
	}
	Json* json() const {
		return m_json;
	}
	bool isValid() const;

signals:
	void valueChanged(QString jsonPtr);
	/** A change at or above the view: it may point at something now, or no more */
	void validChanged();

private:
	void onValueChanged(const QString& jsonPtr);

	Json* m_json;
	QString m_pathPrefix;
};
