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

#include <QCollator>
#include <QString>
#include <algorithm>

/**
 * The order of a list of resource files (Resource Manager, Jukebox,
 * pickers): by one of its columns, then by name (natural, any case, as the
 * file windows of the systems). The default is RPG Maker 2003's: the game's
 * own files, then the RTP's. A column reversed keeps the names of each of
 * its groups in order; only the Name column reverses the names.
 */
class FileSort
{
public:
	enum class Column { Name, Format, Source };

	FileSort() = default;
	FileSort(Column column, bool ascending) : m_column(column), m_ascending(ascending) {}

	Column column() const { return m_column; }
	bool ascending() const { return m_ascending; }
	/** "name", "format" or "source": the sort keys of the lists' columns */
	QString columnName() const;
	/** A click on the title of a column: the same column reversed, else that column in order */
	FileSort clicked(const QString& column) const;
	bool operator==(const FileSort&) const = default;

	/** The order kept in a settings key ("source", "name descending"); the default when none */
	static FileSort load(const QString& key);
	void save(const QString& key) const;

	/**
	 * Sorts the lines from begin to end
	 *
	 * @param name A line's name
	 * @param format Its format (PNG, MIDI...)
	 * @param source Where it comes from: 0 the game, 1 the RTP (2: none...)
	 */
	template<typename It, typename Name, typename Format, typename Source>
	void sort(It begin, It end, Name name, Format format, Source source) const {
		QCollator collator;
		collator.setNumericMode(true);
		collator.setCaseSensitivity(Qt::CaseInsensitive);
		const int sign = m_ascending ? 1 : -1;
		std::stable_sort(begin, end, [&](const auto& a, const auto& b) {
			int order = 0;
			if (m_column == Column::Format) {
				order = collator.compare(format(a), format(b));
			} else if (m_column == Column::Source) {
				order = source(a) - source(b);
			}
			if (order != 0) {
				return order * sign < 0;
			}
			order = collator.compare(name(a), name(b));
			return (m_column == Column::Name ? order * sign : order) < 0;
		});
	}

private:
	Column m_column = Column::Source;
	bool m_ascending = true;
};
