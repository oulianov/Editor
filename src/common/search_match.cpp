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

#include "search_match.h"

#include <QRegularExpression>

namespace {

/** "0012: Potion": the number, then the name */
const QRegularExpression& entryPattern() {
	static const QRegularExpression pattern(QStringLiteral("^\\s*(\\d+)\\s*:\\s*(.*)$"),
		QRegularExpression::DotMatchesEverythingOption);
	return pattern;
}

} // namespace

QString SearchMatch::number(const QString& word) {
	if (word.isEmpty()) {
		return {};
	}
	for (const QChar c : word) {
		if (c < u'0' || c > u'9') {
			return {};
		}
	}
	QString number = word;
	while (number.size() > 1 && number.startsWith(u'0')) {
		number.remove(0, 1);
	}
	return number;
}

bool SearchMatch::matches(const QString& text, int id, const QString& search) {
	QString name = text;
	const auto match = entryPattern().match(text);
	if (match.hasMatch()) {
		if (id < 0) {
			id = match.captured(1).toInt();
		}
		name = match.captured(2);
	}
	// only an entry written with its number has one: "Hero" (10001) does not
	const QString id_text = match.hasMatch() && id >= 0 ? QString::number(id) : QString();
	for (const QString& word : search.split(u' ', Qt::SkipEmptyParts)) {
		const QString digits = number(word);
		if (!digits.isEmpty() && !id_text.isEmpty() && id_text.startsWith(digits)) {
			continue;
		}
		if (!name.contains(word, Qt::CaseInsensitive)) {
			return false;
		}
	}
	return true;
}

int SearchMatch::entryId(const QString& text) {
	const auto match = entryPattern().match(text);
	return match.hasMatch() ? match.captured(1).toInt() : -1;
}
