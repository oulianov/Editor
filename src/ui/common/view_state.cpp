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

#include "view_state.h"

#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QComboBox>
#include <QHash>
#include <QLayout>
#include <QScrollBar>
#include <QTabWidget>
#include <QTimer>
#include <QWidget>
#include <algorithm>

namespace {

QList<std::pair<QString, QAbstractScrollArea*>> scrollAreas(QWidget* page) {
	QList<std::pair<QString, QAbstractScrollArea*>> areas;
	QHash<QString, int> occurrences;
	for (auto* area : page->findChildren<QAbstractScrollArea*>()) {
		if (!area->isVisibleTo(page)) continue;
		bool popup = false;
		for (QWidget* parent = area->parentWidget(); parent && parent != page; parent = parent->parentWidget()) {
			if (qobject_cast<QComboBox*>(parent)) popup = true;
		}
		if (popup) continue;
		const QString name = area->objectName().isEmpty() ? QString::fromLatin1(area->metaObject()->className()) : area->objectName();
		areas.append({QStringLiteral("%1/%2").arg(name).arg(occurrences[name]++), area});
	}
	return areas;
}

void restoreSelection(QAbstractItemView* view, const QVariantMap& area) {
	if (!view || !view->model() || !area.contains(QStringLiteral("rows"))) return;
	QModelIndex index;
	for (const auto& row : area.value(QStringLiteral("rows")).toList()) {
		const int count = view->model()->rowCount(index);
		if (count == 0) { index = {}; break; }
		index = view->model()->index(std::clamp(row.toInt(), 0, count - 1), 0, index);
	}
	if (index.isValid()) {
		const int columns = view->model()->columnCount(index.parent());
		index = index.siblingAtColumn(std::clamp(area.value(QStringLiteral("column")).toInt(), 0, std::max(0, columns - 1)));
		view->setCurrentIndex(index);
	}
}

}

QVariantMap ViewState::capture(QWidget* page) {
	if (!page) return {};
	// A page switched again before its first paint still has its saved position.
	if (page->property("view_state_pending").toBool()) return page->property("view_state").toMap();
	QVariantMap state;
	const auto areas = scrollAreas(page);
	for (auto it = areas.begin(); it != areas.end(); ++it) {
		QVariantMap area{{QStringLiteral("x"), it->second->horizontalScrollBar()->value()},
			{QStringLiteral("y"), it->second->verticalScrollBar()->value()}};
		if (auto* view = qobject_cast<QAbstractItemView*>(it->second)) {
			QVariantList rows;
			for (QModelIndex index = view->currentIndex(); index.isValid(); index = index.parent()) rows.prepend(index.row());
			area.insert(QStringLiteral("rows"), rows);
			area.insert(QStringLiteral("column"), view->currentIndex().column());
		}
		state.insert(it->first, area);
	}
	QVariantMap tabs;
	for (auto* tab : page->findChildren<QTabWidget*>()) {
		if (tab->isVisibleTo(page)) tabs.insert(tab->objectName(), tab->currentIndex());
	}
	state.insert(QStringLiteral("tabs"), tabs);
	return state;
}

void ViewState::restore(QWidget* page, const QVariantMap& state) {
	if (!page || state.isEmpty()) return;
	page->setProperty("view_state", state);
	page->setProperty("view_state_pending", true);
	QTimer::singleShot(0, page, [page, state] {
		if (page->property("view_state").toMap() != state) return;
		// Restore entry selections first: they populate the corresponding inspector.
		for (const auto& [key, area] : scrollAreas(page)) {
			bool nestedPage = false;
			for (QWidget* parent = area->parentWidget(); parent && parent != page; parent = parent->parentWidget())
				if (qobject_cast<QTabWidget*>(parent)) nestedPage = true;
			if (!nestedPage) restoreSelection(qobject_cast<QAbstractItemView*>(area), state.value(key).toMap());
		}
		const auto tabs = state.value(QStringLiteral("tabs")).toMap();
		for (auto* tab : page->findChildren<QTabWidget*>()) {
			if (tabs.contains(tab->objectName()) && tab->count() > 0)
				tab->setCurrentIndex(std::clamp(tabs.value(tab->objectName()).toInt(), 0, tab->count() - 1));
		}
		// Choosing an entry can rebuild its battle event pages, so inspect the new widgets.
		const auto areas = scrollAreas(page);
		for (const auto& [key, area] : areas)
			restoreSelection(qobject_cast<QAbstractItemView*>(area), state.value(key).toMap());
		if (page->layout()) page->layout()->activate();
		for (auto it = areas.begin(); it != areas.end(); ++it) {
			if (!state.contains(it->first)) continue;
			if (auto* view = qobject_cast<QAbstractItemView*>(it->second)) view->doItemsLayout();
			const auto area = state.value(it->first).toMap();
			it->second->horizontalScrollBar()->setValue(area.value(QStringLiteral("x")).toInt());
			it->second->verticalScrollBar()->setValue(area.value(QStringLiteral("y")).toInt());
		}
		page->setProperty("view_state_pending", false);
	});
}
