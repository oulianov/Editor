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

#include "database_windows.h"
#include <QQuickWindow>
#include <QVariantMap>
#include "core.h"
#include "editor_window.h"
#include "qmlbinding/qml_host.h"

namespace {

/** The pages of the targets, numbered as DatabasePage numbers them */
constexpr int troops_page = 5;
constexpr int common_events_page = 16;

/** The target as DatabasePage.showEntry reads it (empty: as last closed) */
QVariantMap startOf(const DatabaseTarget& target)
{
	if (target.page == DatabaseTarget::Page::Last)
		return {};
	const bool troops = target.page == DatabaseTarget::Page::Troops;
	QVariantMap start{
		{QStringLiteral("page"), troops ? troops_page : common_events_page},
		{QStringLiteral("id"), target.id},
		{QStringLiteral("line"), target.line - 1}};
	if (troops)
		start.insert(QStringLiteral("battlePage"), std::max(1, target.troopPage));
	return start;
}

}

void DatabaseWindows::open(EditorWindow& main, const DatabaseTarget& target)
{
	if (!core().project())
		return;
	const QVariantMap start = startOf(target);
	// one window: the open one comes to the front, at the target
	for (QQuickWindow* window : QmlHost::windows()) {
		if (window->objectName() != QLatin1String("DatabaseDialog"))
			continue;
		if (!start.isEmpty())
			QMetaObject::invokeMethod(window, "showEntry", QVariant(start));
		window->raise();
		window->requestActivate();
		return;
	}
	QVariantMap properties;
	if (!start.isEmpty())
		properties.insert(QStringLiteral("start"), start);
	QmlHost::exec(QStringLiteral("DatabaseWindow"), main.handle(), properties);
}
