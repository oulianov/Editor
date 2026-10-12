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

#include "ini_file.h"

#include <QFile>
#include <algorithm>
#include <tuple>

namespace {

QList<QByteArray> readLines(const QString& path, bool* exists = nullptr) {
	QFile file(path);
	if (exists) {
		*exists = file.exists();
	}
	if (!file.open(QIODevice::ReadOnly)) {
		return {};
	}
	const QByteArray content = file.readAll();
	return content.isEmpty() ? QList<QByteArray>() : content.split('\n');
}

/** The name of a section header line ("[Patch]": "Patch"), else empty */
QByteArray sectionName(const QByteArray& line) {
	const QByteArray text = line.trimmed();
	const int close = text.indexOf(']');
	return text.startsWith('[') && close > 0 ? text.mid(1, close - 1).trimmed() : QByteArray();
}

/** The key of a "key=value" line, else empty (blank lines, comments) */
QByteArray keyOf(const QByteArray& line) {
	const QByteArray text = line.trimmed();
	if (text.isEmpty() || text.startsWith(';') || text.startsWith('#') || text.startsWith('[')) {
		return {};
	}
	const int equal = text.indexOf('=');
	return equal > 0 ? text.left(equal).trimmed() : QByteArray();
}

bool sameName(const QByteArray& a, const QByteArray& b) {
	return a.compare(b, Qt::CaseInsensitive) == 0;
}

/** The lines of the section after its header: [first, end); first -1 when there is none */
std::pair<int, int> sectionLines(const QList<QByteArray>& lines, const QByteArray& section) {
	int first = -1;
	for (int i = 0; i < lines.size(); ++i) {
		const QByteArray name = sectionName(lines[i]);
		if (name.isEmpty()) {
			continue;
		}
		if (first >= 0) {
			return {first, i};
		}
		if (sameName(name, section)) {
			first = i + 1;
		}
	}
	return {first, static_cast<int>(lines.size())};
}

} // namespace

QList<IniFile::Entry> IniFile::read(const QString& path, const QByteArray& section) {
	const QList<QByteArray> lines = readLines(path);
	const auto [first, end] = sectionLines(lines, section);
	QList<Entry> entries;
	for (int i = std::max(first, 0); first >= 0 && i < end; ++i) {
		const QByteArray key = keyOf(lines[i]);
		if (!key.isEmpty()) {
			const QByteArray text = lines[i].trimmed();
			entries.append({key, text.mid(text.indexOf('=') + 1).trimmed()});
		}
	}
	return entries;
}

bool IniFile::write(const QString& path, const QByteArray& section, const std::vector<Change>& changes,
		bool replace, bool crlf) {
	bool exists = false;
	QList<QByteArray> lines = readLines(path, &exists);
	if (exists && lines.isEmpty() && QFile(path).size() > 0) {
		return false;
	}
	bool has_crlf = crlf;
	for (const auto& line : lines) {
		if (!line.isEmpty()) {
			has_crlf = line.endsWith('\r');
			break;
		}
	}
	const QByteArray eol = has_crlf ? "\r" : "";
	auto change = [&](const QByteArray& key) {
		return std::find_if(changes.begin(), changes.end(), [&](const Change& c) { return sameName(c.first, key); });
	};

	// the keys of the section: changed in place, or removed
	std::vector<bool> done(changes.size(), false);
	bool removed = false;
	auto [first, end] = sectionLines(lines, section);
	for (int i = end - 1; first >= 0 && i >= first; --i) {
		const QByteArray key = keyOf(lines[i]);
		if (key.isEmpty()) {
			continue;
		}
		const auto it = change(key);
		if (it != changes.end() && !done[static_cast<size_t>(it - changes.begin())] && it->second) {
			lines[i] = key + '=' + *it->second + eol;
			done[static_cast<size_t>(it - changes.begin())] = true;
		} else if (it != changes.end() || replace) {
			lines.removeAt(i);
			removed = true;
		}
	}

	// the new keys: after the last line of the section, else in a new one at the end
	QList<QByteArray> added;
	for (size_t i = 0; i < changes.size(); ++i) {
		if (!done[i] && changes[i].second) {
			added.append(changes[i].first + '=' + *changes[i].second + eol);
		}
	}
	std::tie(first, end) = sectionLines(lines, section);
	if (!added.isEmpty()) {
		if (first < 0) {
			// after the last line written, with a blank line between sections
			while (!lines.isEmpty() && lines.last().trimmed().isEmpty()) {
				lines.removeLast();
			}
			if (!lines.isEmpty()) {
				// the last line had no line end of its own
				if (!eol.isEmpty() && !lines.last().endsWith('\r')) {
					lines.last().append(eol);
				}
				lines.append(eol);
			}
			lines.append('[' + section + ']' + eol);
			lines.append(added);
			lines.append(QByteArray());
		} else {
			int at = end;
			while (at > first && lines[at - 1].trimmed().isEmpty()) {
				--at;
			}
			if (at > first && !eol.isEmpty() && !lines[at - 1].endsWith('\r')) {
				lines[at - 1].append(eol);
			}
			for (const auto& line : added) {
				lines.insert(at++, line);
			}
			// the file ends with a line end
			if (at == lines.size()) {
				lines.append(QByteArray());
			}
		}
	} else if (removed && first >= 0) {
		// a section left without keys goes, with its blank lines
		bool empty = true;
		for (int i = first; i < end; ++i) {
			if (!lines[i].trimmed().isEmpty()) {
				empty = false;
			}
		}
		if (empty) {
			int from = first - 1;
			int to = end;
			// the blank line before it too, unless it is the first one
			if (to == lines.size()) {
				while (from > 0 && lines[from - 1].trimmed().isEmpty()) {
					--from;
				}
				lines.erase(lines.begin() + from, lines.begin() + to);
				if (!lines.isEmpty()) {
					lines.append(QByteArray());
				}
			} else {
				lines.erase(lines.begin() + from, lines.begin() + to);
			}
		}
	}

	if (!exists && lines.isEmpty()) {
		return true;
	}
	QFile file(path);
	return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(lines.join('\n')) >= 0;
}
