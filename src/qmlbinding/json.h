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

#include <QHash>
#include <QObject>
#include <QtQmlIntegration>
#include <QVariant>

#include "json_node.h"

class JsonView;
class JsonListView;

/**
 * Base class holding a LCF Object for use with the Glaze JSON API.
 * The implementing template class is JsonT.
 * This is required because the Qt MOC preprocessor does not support templates.
 *
 * JSON pointers can be written with or without the leading and trailing
 * slashes ("actors/0/name", "/actors/0/name/"); "" and "/" are the root.
 *
 * valueChanged(ptr) means "something at or under ptr changed" (ptr
 * normalized: "/actors/0/name", "" for everything). Between beginBatch()
 * and endBatch() the changes are collected and endBatch() sends one signal
 * for the closest pointer holding all of them (a merge, a paste of many
 * entries).
 */
class Json : public QObject {
	Q_OBJECT
	QML_ELEMENT
	QML_UNCREATABLE("Json is an abstract base class")

public:
	explicit Json(QObject* parent = nullptr);
	~Json() override;

	/**
	 * @param jsonPtr JSON Pointer to read from
	 * @return String at the given pointer location
	 */
	Q_INVOKABLE QString str(QString jsonPtr) const;

	/**
	 * @param jsonPtr JSON Pointer to read from
	 * @return Integer at the given pointer location (any width, bool, a
	 *  double rounded)
	 */
	Q_INVOKABLE int num(QString jsonPtr) const;

	/**
	 * @param jsonPtr JSON Pointer to read from
	 * @return bool at the given pointer location (an integer: not 0; a bit
	 *  of a bit array: "/items/0/state_set/3")
	 */
	Q_INVOKABLE bool boolean(QString jsonPtr) const;

	/**
	 * @param jsonPtr JSON Pointer to read from
	 * @return The value: a number, bool or string, a list for an array of
	 *  them; invalid for objects (use subtree() and list())
	 */
	Q_INVOKABLE QVariant value(QString jsonPtr) const;

	/**
	 * Writes value to the given pointer location, converted to the type of
	 * the data: numbers rounded and clamped to the range of the field,
	 * strings for the texts, lists for arrays of numbers or bits.
	 *
	 * @param jsonPtr JSON Pointer to write to
	 * @param value Value to write
	 * @return Whether it could be written (the same value: true, no signal)
	 */
	Q_INVOKABLE bool set(QString jsonPtr, const QVariant& value);

	/**
	 * @param jsonPtr JSON Pointer of a bit array (DBBitArray, std::vector<bool>)
	 * @param index The bit
	 * @param missing The value of the bits beyond the end of the array
	 * @return Whether the bit is set
	 */
	Q_INVOKABLE bool bit(QString jsonPtr, int index, bool missing = false) const;

	/**
	 * Sets a bit of a bit array, growing the array when it is shorter (the
	 * new bits get missing)
	 *
	 * @return Whether it could be written
	 */
	Q_INVOKABLE bool setBit(QString jsonPtr, int index, bool on, bool missing = false);

	/**
	 * @param jsonPtr JSON Pointer of an array (vector, bit array...)
	 * @return Number of elements, -1 when it is no array
	 */
	Q_INVOKABLE int size(QString jsonPtr) const;

	/**
	 * @param jsonPtr JSON Pointer
	 * @return Whether the pointer points at a value
	 */
	Q_INVOKABLE bool exists(QString jsonPtr) const;

	/**
	 * @param jsonPtr JSON Pointer
	 * @return JsonView rooted at the given pointer location: the same object
	 *  for the same pointer, owned by this object
	 */
	Q_INVOKABLE QVariant subtree(QString jsonPtr);

	/**
	 * Access std::vector<> data at the given pointer location.
	 *
	 * @param jsonPtr JSON Pointer
	 * @return JsonListView rooted at the given pointer location: the same
	 *  object for the same pointer, owned by this object; null when it is no
	 *  vector of objects
	 */
	Q_INVOKABLE QVariant list(QString jsonPtr);

	/**
	 * The entries of a list for a combo box: a list of its own with a
	 * first row standing for none (the list of list() has none: the entry
	 * lists show it)
	 *
	 * @param noneText The text of the first row ("(None)")
	 * @param noneValue Its value (the ID meaning none, 0)
	 * @return The same object for the same pointer and row, owned by this
	 *  object; null when it is no vector of objects
	 */
	Q_INVOKABLE QVariant choices(QString jsonPtr, QString noneText, int noneValue = 0);

	/**
	 * Dump the data given at the pointer location as raw JSON data.
	 * Only use this as a last resort or for debugging purposes.
	 *
	 * @param jsonPtr JSON Pointer
	 * @return JSON string
	 */
	Q_INVOKABLE QString toJson(QString jsonPtr) const;

	/** Collects the changes until endBatch(), which signals them once (nested) */
	Q_INVOKABLE void beginBatch();
	Q_INVOKABLE void endBatch();

	/** The pointer written the same way as the signals do ("/actors/0", "" for the root) */
	static QString normalize(const QString& jsonPtr);
	/** The pointer of name under base (both normalized or not) */
	static QString join(const QString& base, const QString& name);
	/** A change at changed (normalized) can change the value at ptr: it is the same, above or below */
	static bool affects(const QString& changed, const QString& ptr);

	/** The value at the pointer (normalized), found in the data */
	virtual JsonNode node(const QString& jsonPtr) const = 0;
	/** The database is RPG Maker 2003's (new heroes get its stat curves) */
	virtual bool isRpg2k3() const;

	/** The cached view of subtree() */
	JsonView* view(const QString& jsonPtr);
	/** The cached list of list(); nullptr when it is no vector of objects */
	JsonListView* listView(const QString& jsonPtr);
	/** The cached list of choices() */
	JsonListView* choiceList(const QString& jsonPtr, const QString& noneText, int noneValue);

	/**
	 * Tells the views that the data at or under jsonPtr changed (by C++
	 * code: set() and the list operations tell them themselves). In a batch,
	 * the signal waits for endBatch().
	 */
	void notifyChanged(const QString& jsonPtr);
	/** Grows on every change of the data: the lists find their vector again */
	quint64 generation() const {
		return m_generation;
	}

signals:
	void valueChanged(QString jsonPtr);

private:
	JsonNode found(const QString& jsonPtr) const;
	void emitChanged(const QString& jsonPtr);

	// the views stay while this object does: one whose data is gone (an
	// entry beyond the end of a shorter list) reads nothing until it is back
	QHash<QString, JsonView*> m_views;
	QHash<QString, JsonListView*> m_lists;
	QHash<QString, JsonListView*> m_choices;
	int m_batch = 0;
	bool m_batchChanged = false;
	QString m_batchPtr;
	quint64 m_generation = 0;
};
