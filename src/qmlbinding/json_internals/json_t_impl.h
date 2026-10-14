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

// Do not include this file! Only the json_t_*.cpp files do, one per type:
// glaze reflects the whole type there

#include <type_traits>

#include "json_node_impl.h"
#include "json_t.h"

template<typename LCFTYPE>
JsonT<LCFTYPE>::JsonT(QObject* parent) : Json(parent) {}

template<typename LCFTYPE>
JsonNode JsonT<LCFTYPE>::node(const QString& jsonPtr) const {
	JsonNode found;
	if (!m_data) {
		return found;
	}
	glz::seek([&](auto&& value) {
		using T = std::remove_cvref_t<decltype(value)>;
		// a bit of a bit array: a proxy, found below from its array
		if constexpr (!JsonNodes::BitProxy<T>) {
			found.type = &JsonNodes::typeOf<T>;
			found.data = const_cast<void*>(static_cast<const void*>(&value));
		}
	}, *m_data, jsonPtr.toStdString());
	if (found.exists()) {
		return found;
	}

	// "/items/0/state_set/3": a bit, also beyond the end of the array (it
	// reads as missing, and grows the array when set)
	const qsizetype slash = jsonPtr.lastIndexOf(u'/');
	if (slash < 0) {
		return found;
	}
	bool ok = false;
	const qsizetype index = jsonPtr.mid(slash + 1).toLongLong(&ok);
	if (!ok || index < 0) {
		return found;
	}
	JsonNode array = node(jsonPtr.left(slash));
	if (array.exists() && !array.isBit() && array.type->kind == JsonNodeType::Kind::Bits) {
		array.bit = index;
		return array;
	}
	return found;
}

template<typename LCFTYPE>
bool JsonT<LCFTYPE>::isRpg2k3() const {
	if constexpr (std::is_same_v<LCFTYPE, lcf::rpg::Database>) {
		return m_data && m_data->system.ldb_id == 2003;
	} else {
		return false;
	}
}

template<typename LCFTYPE>
void JsonT<LCFTYPE>::setData(LCFTYPE* data) {
	m_data = data;
	// everything changed: the views read the new data
	notifyChanged(QString());
}
