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

#include "file_sort.h"
#include <QSettings>
#include <QStringList>
#include <iterator>

namespace {
	const char* const kColumnNames[] = { "name", "format", "source" };
	const char DESCENDING[] = "descending";

	/** -1 when it is not a column's */
	int columnOf(const QString& name) {
		for (int i = 0; i < static_cast<int>(std::size(kColumnNames)); ++i) {
			if (name == QLatin1String(kColumnNames[i])) {
				return i;
			}
		}
		return -1;
	}
}

QString FileSort::columnName() const
{
	return QLatin1String(kColumnNames[static_cast<int>(m_column)]);
}

FileSort FileSort::clicked(const QString& column) const
{
	const int found = columnOf(column);
	if (found < 0) {
		return *this;
	}
	const Column chosen = static_cast<Column>(found);
	return chosen == m_column ? FileSort(m_column, !m_ascending) : FileSort(chosen, true);
}

FileSort FileSort::load(const QString& key)
{
	const QStringList words = QSettings().value(key).toString().split(QLatin1Char(' '), Qt::SkipEmptyParts);
	const int found = columnOf(words.value(0));
	if (found < 0) {
		return FileSort();
	}
	return FileSort(static_cast<Column>(found), words.value(1) != QLatin1String(DESCENDING));
}

void FileSort::save(const QString& key) const
{
	QSettings().setValue(key, m_ascending ? columnName() : columnName() + QLatin1Char(' ') + QLatin1String(DESCENDING));
}
