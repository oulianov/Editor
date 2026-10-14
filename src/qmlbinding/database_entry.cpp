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

#include "database_entry.h"
#include "database_session.h"
#include "json_view.h"

DatabaseEntry DatabaseEntry::of(const JsonView* view)
{
	DatabaseEntry entry;
	if (!view) {
		return entry;
	}
	entry.session = DatabaseSession::owning(view->json());
	entry.project = entry.session ? &entry.session->copy() : nullptr;
	// "/troops/2": the list, then the index
	const QStringList parts = view->pathPrefix().split(QLatin1Char('/'), Qt::SkipEmptyParts);
	if (parts.size() == 2) {
		bool ok = false;
		const int index = parts[1].toInt(&ok);
		if (ok) {
			entry.list = parts[0];
			entry.index = index;
		}
	}
	return entry;
}

void DatabaseEntry::changed(const QString& path) const
{
	if (!session) {
		return;
	}
	QString pointer = QStringLiteral("/%1/%2").arg(list).arg(index);
	if (!path.isEmpty()) {
		pointer += QLatin1Char('/') + path;
	}
	session->json().notifyChanged(pointer);
}
