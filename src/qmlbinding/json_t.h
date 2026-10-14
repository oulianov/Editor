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

#include "json.h"

#include <lcf/rpg/database.h>
#include <lcf/rpg/map.h>
#include <lcf/rpg/mapinfo.h>
#include <lcf/rpg/treemap.h>

/**
 * Json over live liblcf data: the pointers are found with glaze
 * (json_internals/json_t_impl.h, instantiated once per type in its own file:
 * the reflection of a whole database is slow to compile)
 */
template<typename LCFTYPE>
class JsonT : public Json {
public:
	explicit JsonT(QObject* parent = nullptr);

	JsonNode node(const QString& jsonPtr) const override;
	bool isRpg2k3() const override;

	/** The data shown (not owned): the views show it at once */
	void setData(LCFTYPE* data);
	LCFTYPE* data() const {
		return m_data;
	}

private:
	LCFTYPE* m_data = nullptr;
};

extern template class JsonT<lcf::rpg::Database>;
extern template class JsonT<lcf::rpg::TreeMap>;
extern template class JsonT<lcf::rpg::Map>;
extern template class JsonT<lcf::rpg::MapInfo>;
