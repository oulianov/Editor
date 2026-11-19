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

#include <QStringList>
#include <QVariantMap>
#include <string_view>

VocabularyTable::VocabularyTable(QObject* parent) : QObject(parent) {}

void VocabularyTable::setRpg2k3(bool on) {
	if (m_rpg2k3 != on) {
		m_rpg2k3 = on;
		emit pagesChanged();
	}
}

void VocabularyTable::setBattle2k(bool on) {
	if (m_battle2k != on) {
		m_battle2k = on;
		emit pagesChanged();
	}
}

void VocabularyTable::setManiac(bool on) {
	if (m_maniac != on) {
		m_maniac = on;
		emit pagesChanged();
	}
}

QVariantList VocabularyTable::pages() const {
	using namespace VocabularyFields;
	auto text = [](const char* source) {
		return source ? QString::fromUtf8(source) : QString();
	};
	// own: RPG Maker's (or EasyRPG Player's) term of the row, not a part the Maniac Patch adds
	auto termPart = [&](const char* term, const Row* main, bool own = true) {
		QVariantMap part{{QStringLiteral("term"), text(term)}, {QStringLiteral("limit"), limit(term)},
			{QStringLiteral("own"), own}, {QStringLiteral("origin"), own ? QString() : QStringLiteral("maniac")}};
		// the term of the row itself, left to EasyRPG Player's text
		if (main && main->fallback) {
			part.insert(QStringLiteral("fallback"), text(main->fallback));
			part.insert(QStringLiteral("fallbackHint"), main->fallbackHint);
		}
		return part;
	};
	auto hintPart = [&](const char* hint) {
		return QVariantMap{{QStringLiteral("hint"), text(hint)}};
	};
	// the fields of a row and the texts between them, in the order of the
	// message: RPG Maker's, or the Maniac Patch's (maniac)
	auto lines = [&](const Row& row, bool maniac) {
		QVariantList lines;
		QVariantList parts;
		if (maniac) {
			for (const Part& part : row.maniac) {
				if (part.term) {
					const bool own = part.term == std::string_view(row.term);
					parts.append(termPart(part.term, own ? &row : nullptr, own));
				} else if (part.hint) {
					parts.append(hintPart(part.hint));
				} else {
					lines.append(QVariant(parts));
					parts.clear();
				}
			}
		} else if (row.second) {
			parts = {termPart(row.term, &row), hintPart(row.hint), termPart(row.second, nullptr)};
		} else if (row.hint) {
			parts = {hintPart(row.hint), termPart(row.term, &row)};
		} else {
			parts = {termPart(row.term, &row)};
		}
		lines.append(QVariant(parts));
		return lines;
	};
	QVariantList pages;
	for (const Page& page : VocabularyFields::pages()) {
		QVariantList groups;
		for (const Group& group : page.groups) {
			QVariantList rows;
			for (const Row& row : group.rows) {
				if (!shown(row, m_rpg2k3, m_battle2k)) {
					continue;
				}
				QVariantMap shownRow{
					{QStringLiteral("label"), text(row.label)},
					{QStringLiteral("legend"), text(row.term ? legend(row.term) : nullptr)},
					{QStringLiteral("lines"), lines(row, false)},
				};
				// the Maniac Patch's message, and the terms it adds
				if (!row.maniac.empty()) {
					QStringList added;
					for (const Part& part : row.maniac) {
						if (part.term && part.term != std::string_view(row.term)) {
							added << text(part.term);
						}
					}
					shownRow.insert(QStringLiteral("maniacLines"), lines(row, true));
					shownRow.insert(QStringLiteral("maniacTerms"), added);
				}
				rows.append(shownRow);
			}
			// the groups of the other engine only are not shown
			if (!rows.isEmpty()) {
				// who reads the group's terms besides RPG Maker: EasyRPG Player
				// in every game, the Maniac Patch too in its games
				QString origin;
				if (group.extension == Extension::EasyRpg
						|| (group.extension == Extension::EasyRpgManiac && !m_maniac)) {
					origin = QStringLiteral("player");
				} else if (group.extension == Extension::EasyRpgManiac) {
					origin = QStringLiteral("maniac");
				}
				groups.append(QVariantMap{
					{QStringLiteral("title"), text(group.title)},
					{QStringLiteral("column"), group.column},
					{QStringLiteral("pairs"), group.pairs},
					{QStringLiteral("labelsBeside"), page.labelsBeside},
					{QStringLiteral("origin"), origin},
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
