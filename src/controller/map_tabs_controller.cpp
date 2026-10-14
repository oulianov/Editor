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

#include "map_tabs_controller.h"
#include <QCoreApplication>
#include <QScopedValueRollback>
#include "core.h"
#include "dialog_service.h"
#include "project_controller.h"
#include "common/dbstring.h"
#include "model/map_document.h"
#include "model/project.h"

using lcf::rpg::TreeMap;

MapTabsController::MapTabsController(ProjectController* project, QObject* parent) :
	QAbstractListModel(parent),
	m_project(project)
{
	connect(m_project, &ProjectController::documentClosing, this, &MapTabsController::onDocumentClosing);
	// saving them all goes in the order of the tabs
	m_project->setDocumentOrder([this] { return m_ids; });
}

int MapTabsController::rowCount(const QModelIndex& parent) const
{
	return parent.isValid() ? 0 : static_cast<int>(m_ids.size());
}

QVariant MapTabsController::data(const QModelIndex& index, int role) const
{
	if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
		return {};
	const int id = m_ids[index.row()];
	const MapDocument* doc = m_project->document(id);
	switch (role) {
	case MapIdRole:
		return id;
	case NameRole:
		return doc ? doc->name() : QString();
	case Qt::DisplayRole:
	case TitleRole:
		return title(id);
	case Qt::ToolTipRole:
	case ToolTipRole:
		return toolTip(id);
	case ModifiedRole:
		return doc && doc->isModified();
	case TemporaryRole:
		return id == m_temporary;
	case DeletedRole:
		return core().project() && !m_project->treeModel()->contains(id);
	}
	return {};
}

QHash<int, QByteArray> MapTabsController::roleNames() const
{
	return {
		{MapIdRole, "mapId"},
		{NameRole, "name"},
		{TitleRole, "title"},
		{ToolTipRole, "toolTip"},
		{ModifiedRole, "modified"},
		{TemporaryRole, "temporary"},
		{DeletedRole, "deleted"},
	};
}

QList<int> MapTabsController::mapIds() const
{
	return m_ids;
}

int MapTabsController::count() const
{
	return static_cast<int>(m_ids.size());
}

int MapTabsController::indexOf(int id) const
{
	return static_cast<int>(m_ids.indexOf(id));
}

int MapTabsController::mapAt(int index) const
{
	return m_ids.value(index, 0);
}

bool MapTabsController::isOpen(int id) const
{
	return m_ids.contains(id);
}

int MapTabsController::currentMap() const
{
	return m_current;
}

int MapTabsController::currentIndex() const
{
	return indexOf(m_current);
}

MapDocument* MapTabsController::currentDocument() const
{
	return m_current ? m_project->document(m_current) : nullptr;
}

int MapTabsController::temporaryMap() const
{
	return m_temporary;
}

QString MapTabsController::title(int id) const
{
	const MapDocument* doc = m_project->document(id);
	if (!doc)
		return {};
	// another program deleted it while it was edited: saving asks
	const bool deleted = core().project() && !m_project->treeModel()->contains(id);
	const QString name = deleted ? QCoreApplication::translate("MainWindow", "%1 (deleted)").arg(doc->name()) : doc->name();
	return name + (doc->isModified() ? QStringLiteral(" *") : QString());
}

QString MapTabsController::toolTip(int id) const
{
	const bool deleted = core().project() && m_project->document(id) && !m_project->treeModel()->contains(id);
	return deleted ? QCoreApplication::translate("MainWindow", "Another program deleted this map. Saving it asks "
		"whether to add it back to the map tree or to discard your changes.") : QString();
}

bool MapTabsController::openMap(int id, bool keep)
{
	if (!core().project() || id <= 0 || m_project->treeModel()->type(id) != TreeMap::MapType_map)
		return false;
	if (!isOpen(id) && !core().project()->loadMap(id)) {
		const lcf::rpg::MapInfo* info = m_project->treeModel()->mapInfo(id);
		DialogService::instance().warning(QCoreApplication::translate("MainWindow", "Open Map"),
			QCoreApplication::translate("MainWindow", "The file of the map %1 (%2) is missing or cannot be read.")
				.arg(info ? ToQString(info->name) : QString(), ProjectController::mapFileName(id)));
		return false;
	}
	// Like a code editor: a map only looked at opens in the temporary tab,
	// which the next one replaces; it stays when kept (a double-click, an
	// edit to save)
	if (!isOpen(id)) {
		int index = count();
		const MapDocument* temporary = m_temporary ? m_project->document(m_temporary) : nullptr;
		if (temporary && !temporary->isModified()) {
			index = indexOf(m_temporary);
			// the map shown at once (setCurrentMap below), not the tab
			// beside it first: it would be loaded for nothing
			QScopedValueRollback<bool> replacing(m_replacing, true);
			closeMap(m_temporary);
		}
		setTemporaryMap(0);
		m_project->openDocument(id);
		// the view of the tab is made now: it reads the document
		{
			QScopedValueRollback<bool> changing(m_rowsChanging, true);
			beginInsertRows(QModelIndex(), index, index);
			m_ids.insert(index, id);
			endInsertRows();
		}
		emit countChanged();
		connectDocument(id);
		if (!keep)
			setTemporaryMap(id);
	} else if (keep) {
		keepMap(id);
	}
	setCurrentMap(id);
	return true;
}

QList<int> MapTabsController::restoreMaps(const QList<int>& ids)
{
	QList<int> opened;
	if (!core().project())
		return opened;
	for (int id : ids) {
		// removed maps, areas and unreadable files cannot be restored as tabs
		if (m_project->treeModel()->type(id) != TreeMap::MapType_map || isOpen(id) || !core().project()->loadMap(id))
			continue;
		m_project->openDocument(id);
		{
			QScopedValueRollback<bool> changing(m_rowsChanging, true);
			beginInsertRows(QModelIndex(), count(), count());
			m_ids.append(id);
			endInsertRows();
		}
		emit countChanged();
		connectDocument(id);
		opened.append(id);
	}
	return opened;
}

void MapTabsController::connectDocument(int id)
{
	MapDocument* doc = m_project->document(id);
	if (!doc)
		return;
	connect(doc, &MapDocument::edited, this, [this, id] { onEdited(id); });
	connect(doc, &MapDocument::loaded, this, [this, id] { onSavedOrReverted(id); });
	connect(doc, &MapDocument::saved, this, [this, id] { onSavedOrReverted(id); });
	// undoing Map Properties can rename it
	connect(doc, &MapDocument::mapReplaced, this, [this, id] { updateTab(id); });
	connect(doc, &MapDocument::eventsChanged, this, [this, id] { updateTab(id); });
}

void MapTabsController::onEdited(int id)
{
	// an edit to save keeps a temporary map open (looking does not)
	if (const MapDocument* doc = m_project->document(id); doc && doc->isModified())
		keepMap(id);
	updateTab(id);
}

void MapTabsController::onSavedOrReverted(int id)
{
	updateTab(id);
	// it wrote its copy of its tree entry: the tree keeps its order
	m_project->keepTreeAfterMapSave();
}

void MapTabsController::updateTab(int id)
{
	const int row = indexOf(id);
	if (row >= 0)
		emit dataChanged(index(row), index(row), {NameRole, TitleRole, ToolTipRole, ModifiedRole, DeletedRole});
}

void MapTabsController::setTemporaryMap(int id)
{
	if (m_temporary == id)
		return;
	const int old = m_temporary;
	m_temporary = id;
	for (int changed : {old, id}) {
		if (const int row = indexOf(changed); row >= 0)
			emit dataChanged(index(row), index(row), {TemporaryRole});
	}
	emit temporaryMapChanged(id);
}

void MapTabsController::keepMap(int id)
{
	if (id != 0 && id == m_temporary)
		setTemporaryMap(0);
}

void MapTabsController::setCurrentMap(int id)
{
	if (id != 0 && !isOpen(id))
		return;
	if (m_current == id)
		return;
	m_current = id;
	emit currentMapChanged(id);
}

void MapTabsController::setCurrentIndex(int index)
{
	// a tab bar moves its index with the rows: the user did not choose it
	if (m_rowsChanging)
		return;
	setCurrentMap(mapAt(index));
}

void MapTabsController::moveTab(int from, int to)
{
	if (from == to || from < 0 || to < 0 || from >= count() || to >= count())
		return;
	QScopedValueRollback<bool> changing(m_rowsChanging, true);
	beginMoveRows(QModelIndex(), from, from, QModelIndex(), to > from ? to + 1 : to);
	m_ids.move(from, to);
	endMoveRows();
}

bool MapTabsController::requestClose(int id)
{
	MapDocument* doc = m_project->document(id);
	if (!isOpen(id) || !doc)
		return false;
	if (doc->isModified()) {
		const auto answer = DialogService::instance().question(
			QCoreApplication::translate("MainWindow", "Save map changes"),
			QCoreApplication::translate("MainWindow", "%1 has unsaved changes.\n"
				"Do you want to save them before closing it?").arg(doc->name()),
			DialogService::Buttons::YesNoCancel);
		if (answer == DialogService::Answer::Cancel)
			return false;
		// not saved (cancelled, or failed): the tab stays open
		if (answer == DialogService::Answer::Yes && !doc->save())
			return false;
	}
	closeMap(id);
	return true;
}

void MapTabsController::removeTab(int id)
{
	const int row = indexOf(id);
	if (row < 0)
		return;
	if (id == m_temporary)
		setTemporaryMap(0);
	// the tab shown goes: the one on its right is shown, else on its left
	// (as a tab bar does)
	const bool current = id == m_current;
	{
		QScopedValueRollback<bool> changing(m_rowsChanging, true);
		beginRemoveRows(QModelIndex(), row, row);
		m_ids.removeAt(row);
		endRemoveRows();
	}
	emit countChanged();
	emit mapClosing(id);
	if (current && !m_replacing)
		setCurrentMap(m_ids.value(std::min(row, static_cast<int>(m_ids.size()) - 1), 0));
}

void MapTabsController::closeMap(int id)
{
	if (!isOpen(id))
		return;
	removeTab(id);
	// its views let it go
	m_project->closeDocument(id);
}

void MapTabsController::closeAll()
{
	while (m_current != 0)
		closeMap(m_current);
	while (!m_ids.isEmpty())
		closeMap(m_ids.last());
}

void MapTabsController::onDocumentClosing(int id)
{
	// closing it here (closeMap) removed the tab first
	removeTab(id);
}

void MapTabsController::followTreeMap()
{
	// Close the maps that were removed (one with changes says it was
	// deleted), update the names of the others
	for (int i = count() - 1; i >= 0; i--) {
		const int id = m_ids[i];
		MapDocument* doc = m_project->document(id);
		if (!doc)
			continue;
		if (!m_project->treeModel()->contains(id) && !doc->isModified()) {
			closeMap(id);
			continue;
		}
		doc->refreshMapInfo();
		updateTab(id);
	}
}
