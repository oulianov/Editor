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

#include <QVariant>
#include <string>

class Json;
class JsonListView;

/** Identity of a C++ type without RTTI: the address differs for every T */
template<class T>
inline constexpr char jsonTypeTag = 0;

/**
 * What the Json binding can do with one C++ type of the data (an int16_t,
 * a DBString, a std::vector<Actor>...). JsonT finds the value a JSON
 * pointer points at with a single glaze seek; everything else goes through
 * this table, written once for all the types (json_internals/json_node_impl.h)
 * instead of one glaze seek per operation and type.
 */
struct JsonNodeType {
	enum class Kind {
		Bool,
		Integer,
		Floating,
		/** lcf::DBString and std::string */
		String,
		/** lcf::DBBitArray and std::vector<bool> */
		Bits,
		/** Vectors and arrays of anything else */
		Array,
		/** A struct of liblcf */
		Object,
	};

	Kind kind;
	/** The C++ type: &jsonTypeTag<T> */
	const void* tag;
	/** The value for QML: a number, bool or string; a list of them for an
	 *  array of them; invalid for objects and arrays of objects */
	QVariant (*read)(const void* data);
	/** Writes a value converted to the type (numbers clamped to its range);
	 *  false when it cannot be converted. changed: the data differs now */
	bool (*write)(void* data, const QVariant& value, bool* changed);
	/** Elements of an array, -1 for anything else */
	qsizetype (*size)(const void* data);
	/** Bit i of a bit array; missing beyond its end */
	bool (*bit)(const void* data, qsizetype index, bool missing);
	/** Sets bit i of a bit array, growing it with missing bits; false for others */
	bool (*setBit)(void* data, qsizetype index, bool on, bool missing, bool* changed);
	/** The value as JSON */
	std::string (*json)(void* data);
	/** A list model of a vector of objects (nullptr for anything else) */
	JsonListView* (*makeList)(Json* json, const QString& path);
};

/** The value a JSON pointer points at: what JsonT::node() found */
struct JsonNode {
	const JsonNodeType* type = nullptr;
	void* data = nullptr;
	/** A single bit of a bit array (data, type: the array): its index, else -1 */
	qsizetype bit = -1;

	bool exists() const {
		return type != nullptr;
	}
	bool isBit() const {
		return type && bit >= 0;
	}
	template<class T>
	bool is() const {
		return type && bit < 0 && type->tag == &jsonTypeTag<T>;
	}
};
