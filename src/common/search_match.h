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

#include <QString>

/**
 * The search of the database entries ("0012: Potion") of the searchable
 * lists, whatever the interface: an entry matches when, for every word
 * typed, its number starts with the word (digits, leading zeros ignored)
 * or its name contains it.
 */
namespace SearchMatch {

/** Whether the entry of this text (and number, -1: read from the text) matches the search */
bool matches(const QString& text, int id, const QString& search);

/** The number of an entry text ("0012: Potion": 12), -1 without one */
int entryId(const QString& text);

/** The digits of a word typed without its leading zeros ("0012": "12"), empty if it is not a number */
QString number(const QString& word);

} // namespace SearchMatch
