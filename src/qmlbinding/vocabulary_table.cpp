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

#include "vocabulary_table.h"
#include "model/vocabulary_fields.h"

#include <QVariantMap>

VocabularyTable::VocabularyTable(QObject* parent) : QObject(parent) {}

void VocabularyTable::setRpg2k3(bool on) {
	if (m_rpg2k3 != on) {
		m_rpg2k3 = on;
		emit pagesChanged();
	}
}

QVariantList VocabularyTable::pages() const {
	using namespace VocabularyFields;
	auto text = [](const char* source) {
		return source ? QString::fromUtf8(source) : QString();
	};
	QVariantList pages;
	for (const Page& page : VocabularyFields::pages()) {
		QVariantList groups;
		for (const Group& group : page.groups) {
			QVariantList rows;
			for (const Row& row : group.rows) {
				if (!shown(row, m_rpg2k3)) {
					continue;
				}
				rows.append(QVariantMap{
					{QStringLiteral("label"), text(row.label)},
					{QStringLiteral("term"), text(row.term)},
					{QStringLiteral("limit"), limit(row.term)},
					{QStringLiteral("hint"), text(row.hint)},
					{QStringLiteral("second"), text(row.second)},
					{QStringLiteral("secondLimit"), row.second ? limit(row.second) : 0},
				});
			}
			// the groups of the other engine only are not shown
			if (!rows.isEmpty()) {
				groups.append(QVariantMap{
					{QStringLiteral("title"), text(group.title)},
					{QStringLiteral("column"), group.column},
					{QStringLiteral("pairs"), group.pairs},
					{QStringLiteral("rows"), rows},
				});
			}
		}
		if (!groups.isEmpty()) {
			pages.append(QVariantMap{{QStringLiteral("title"), text(page.title)}, {QStringLiteral("groups"), groups}});
		}
	}
	return pages;
}
