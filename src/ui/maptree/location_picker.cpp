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

#include "location_picker.h"
#include "common/dbstring.h"
#include "common/search_match.h"
#include "core.h"
#include "controller/map_tabs_controller.h"
#include "controller/project_controller.h"
#include "controller/shell_controller.h"
#include "model/project.h"
#include "model/project_data.h"
#include "qmlbinding/qml_host.h"
#include "ui/shell/editor_window.h"
#include "ui/picker/pickers.h"
#include <QGuiApplication>
#include <QHash>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScopeGuard>
#include <QSet>
#include <algorithm>

namespace {

// the shell of the main window: the map of its tab shown
ShellController* shell() {
	EditorWindow* window = EditorWindow::main();
	return window ? window->shell() : nullptr;
}

}

/*
 * The list of the maps
 */

LocationMapList::LocationMapList(const lcf::rpg::TreeMap& tree, const QString& root_name, QObject* parent) :
	QAbstractListModel(parent)
{
	Node root;
	root.name = root_name;
	root.type = lcf::rpg::TreeMap::MapType_root;
	root.expanded = true;
	m_nodes.append(root);
	QHash<int, int> index;
	for (const auto& info : tree.maps) {
		if (info.ID <= 0 || index.contains(info.ID))
			continue;
		Node node;
		node.id = info.ID;
		node.name = ToQString(info.name);
		node.type = info.type;
		node.expanded = info.expanded_node;
		index.insert(info.ID, static_cast<int>(m_nodes.size()));
		m_nodes.append(node);
	}
	// attached in the order of the tree, maps missing from it last
	QSet<int> attached;
	auto attach = [&](int id) {
		if (!index.contains(id) || attached.contains(id))
			return;
		const auto it = std::find_if(tree.maps.begin(), tree.maps.end(),
			[id](const lcf::rpg::MapInfo& info) { return info.ID == id; });
		const int parent_id = it != tree.maps.end() ? it->parent_map : 0;
		const int parent = parent_id != id && index.contains(parent_id) ? index.value(parent_id) : 0;
		m_nodes[parent].children.append(index.value(id));
		m_nodes[index.value(id)].parent = parent;
		attached.insert(id);
	};
	for (int id : tree.tree_order)
		attach(id);
	for (const auto& info : tree.maps)
		attach(info.ID);
	// depths, from the project down; a node met twice (a loop) is not
	QList<int> stack{0};
	QSet<int> seen{0};
	while (!stack.isEmpty()) {
		const int i = stack.takeLast();
		for (int child : m_nodes[i].children) {
			if (seen.contains(child))
				continue;
			seen.insert(child);
			m_nodes[child].depth = m_nodes[i].depth + 1;
			stack.append(child);
		}
	}
	update();
}

QString LocationMapList::filter() const
{
	return m_filter;
}

void LocationMapList::setFilter(const QString& filter)
{
	if (filter == m_filter)
		return;
	m_filter = filter;
	update();
	emit filterChanged();
}

bool LocationMapList::collect(int index, bool open, QList<int>& shown) const
{
	const Node& node = m_nodes[index];
	const qsizetype at = shown.size();
	if (open)
		shown.append(index);
	const bool searched = !m_filter.trimmed().isEmpty();
	bool matches = index == 0 || !searched
		|| SearchMatch::matches(QStringLiteral("%1: %2").arg(node.id, 4, 10, QChar('0')).arg(node.name), node.id, m_filter);
	// searched: every match shows, else what the expanded nodes hold
	for (int child : node.children)
		matches = collect(child, open && (searched || node.expanded), shown) || matches;
	if (!matches)
		shown.resize(at);
	return matches;
}

void LocationMapList::update()
{
	beginResetModel();
	m_shown.clear();
	collect(0, true, m_shown);
	endResetModel();
}

void LocationMapList::setExpanded(int row, bool expanded)
{
	if (row < 0 || row >= m_shown.size() || m_nodes[m_shown[row]].children.isEmpty()
			|| m_nodes[m_shown[row]].expanded == expanded)
		return;
	m_nodes[m_shown[row]].expanded = expanded;
	update();
}

void LocationMapList::reveal(int map_id)
{
	const auto it = std::find_if(m_nodes.begin(), m_nodes.end(), [map_id](const Node& node) { return node.id == map_id; });
	if (it == m_nodes.end())
		return;
	bool changed = false;
	// a loop of parents stops at the node met again
	QSet<int> seen;
	for (int parent = it->parent; parent >= 0 && !seen.contains(parent); parent = m_nodes[parent].parent) {
		seen.insert(parent);
		changed = changed || !m_nodes[parent].expanded;
		m_nodes[parent].expanded = true;
	}
	if (changed)
		update();
}

int LocationMapList::rowOf(int map_id) const
{
	for (qsizetype row = 0; row < m_shown.size(); ++row)
		if (m_nodes[m_shown[row]].id == map_id)
			return static_cast<int>(row);
	return -1;
}

int LocationMapList::mapIdAt(int row) const
{
	return row >= 0 && row < m_shown.size() ? m_nodes[m_shown[row]].id : 0;
}

bool LocationMapList::selectableAt(int row) const
{
	return row >= 0 && row < m_shown.size() && m_nodes[m_shown[row]].type == lcf::rpg::TreeMap::MapType_map;
}

int LocationMapList::firstSelectable() const
{
	for (int row = 0; row < m_shown.size(); ++row)
		if (selectableAt(row))
			return row;
	return -1;
}

bool LocationMapList::isMap(int map_id) const
{
	return map_id > 0 && std::any_of(m_nodes.cbegin(), m_nodes.cend(), [map_id](const Node& node) {
		return node.id == map_id && node.type == lcf::rpg::TreeMap::MapType_map;
	});
}

int LocationMapList::rowCount(const QModelIndex& parent) const
{
	return parent.isValid() ? 0 : static_cast<int>(m_shown.size());
}

QVariant LocationMapList::data(const QModelIndex& index, int role) const
{
	if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
		return QVariant();
	const Node& node = m_nodes[m_shown[index.row()]];
	switch (role) {
	case Qt::DisplayRole:
		return node.name;
	case IdRole:
		return node.id;
	case DepthRole:
		return node.depth;
	case TypeRole:
		return node.type;
	case SelectableRole:
		return node.type == lcf::rpg::TreeMap::MapType_map;
	case HasChildrenRole:
		return !node.children.isEmpty();
	case ExpandedRole:
		return node.expanded || !m_filter.trimmed().isEmpty();
	default:
		return QVariant();
	}
}

QHash<int, QByteArray> LocationMapList::roleNames() const
{
	return {{Qt::DisplayRole, "name"}, {IdRole, "mapId"}, {DepthRole, "depth"}, {TypeRole, "type"},
		{SelectableRole, "selectable"}, {HasChildrenRole, "hasChildren"}, {ExpandedRole, "expanded"}};
}

/*
 * The picker
 */

LocationPicker::LocationPicker(ProjectData& project, QObject* parent) :
	QObject(parent),
	m_project(project),
	m_maps(new LocationMapList(project.treeMap(),
		core().project() ? core().project()->gameTitle() : QCoreApplication::translate("MapLocationDialog", "Project"), this))
{
}

LocationMapList* LocationPicker::maps() const
{
	return m_maps;
}

MapDocument* LocationPicker::document() const
{
	return m_document;
}

int LocationPicker::mapId() const
{
	return m_document ? m_document->id() : 0;
}

QString LocationPicker::mapName() const
{
	if (!m_document)
		return QString();
	for (const auto& info : m_project.treeMap().maps)
		if (info.ID == m_document->id())
			return ToQString(info.name);
	return m_document->name();
}

int LocationPicker::columns() const
{
	return m_document && m_document->hasMap() ? std::max(m_document->map().width, 1) : 0;
}

int LocationPicker::rows() const
{
	return m_document && m_document->hasMap() ? std::max(m_document->map().height, 1) : 0;
}

bool LocationPicker::showMap(int map_id)
{
	if (m_document && m_document->id() == map_id && map_id > 0)
		return true;
	if (m_owned && m_document)
		m_document->deleteLater();
	m_document = nullptr;
	m_owned = false;
	// the map of the open tab as edited, the others as saved
	if (MapDocument* open = openDocument(); open && open->id() == map_id && open->hasMap()) {
		m_document = open;
	} else if (map_id > 0 && m_project.hasProject()) {
		if (auto map = m_project.project().loadMap(map_id)) {
			auto* document = new MapDocument(m_project, map_id, this);
			document->setMap(std::move(map));
			m_document = document;
			m_owned = true;
		}
	}
	emit mapChanged();
	return m_document != nullptr;
}

int LocationPicker::openMapId()
{
	ShellController* controller = shell();
	return controller ? controller->tabs()->currentMap() : 0;
}

MapDocument* LocationPicker::openDocument()
{
	ShellController* controller = shell();
	const int id = controller ? controller->tabs()->currentMap() : 0;
	return id > 0 ? controller->project()->document(id) : nullptr;
}

/*
 * Pickers::pickLocation
 */

std::optional<Pickers::Location> Pickers::pickLocation(QObject* from, const LocationRequest& request)
{
	if (!core().project())
		return std::nullopt;
	auto* picker = new LocationPicker(core().project()->projectData());
	const auto cleanup = qScopeGuard([picker] { picker->deleteLater(); });
	int map_id = request.current.map;
	if (request.fixedMap) {
		// a cell of this map only
		if (!picker->showMap(map_id))
			return std::nullopt;
	} else {
		// a location without a map (a new command) starts on the open map
		if (!picker->maps()->isMap(map_id))
			map_id = openMapId();
		picker->showMap(map_id);
	}
	Location chosen = request.current;
	const int result = QmlHost::exec(QStringLiteral("MapLocationPicker"), windowOf(from), {
		{QStringLiteral("picker"), QVariant::fromValue(picker)},
		{QStringLiteral("fixedMap"), request.fixedMap},
		{QStringLiteral("startX"), request.current.x},
		{QStringLiteral("startY"), request.current.y},
		// RPG Maker 2003's Character Facing: 0 retain, 1 up, 2 right, 3 down, 4 left
		{QStringLiteral("facing"), request.facing ? std::clamp(request.current.direction, 0, 4) : -1},
		{QStringLiteral("customTitle"), request.title},
	}, [&chosen, picker](QQuickWindow* window) {
		chosen.map = picker->mapId();
		chosen.x = window->property("cellX").toInt();
		chosen.y = window->property("cellY").toInt();
		chosen.direction = std::max(0, window->property("facing").toInt());
	});
	if (result != 1 || chosen.map <= 0)
		return std::nullopt;
	if (!request.facing)
		chosen.direction = request.current.direction;
	return chosen;
}

int Pickers::openMapId()
{
	return LocationPicker::openMapId();
}

QVariant PickersBridge::pickLocation(const QVariantMap& request, QQuickItem* from)
{
	Pickers::LocationRequest location;
	location.current.map = request.value(QStringLiteral("map")).toInt();
	location.current.x = request.value(QStringLiteral("x")).toInt();
	location.current.y = request.value(QStringLiteral("y")).toInt();
	const QVariant direction = request.value(QStringLiteral("direction"));
	location.current.direction = direction.toInt();
	// the Character Facing is asked when told, or given a direction
	location.facing = request.contains(QStringLiteral("facing"))
		? request.value(QStringLiteral("facing")).toBool() : direction.isValid() && !direction.isNull();
	location.fixedMap = request.value(QStringLiteral("fixedMap")).toBool();
	location.title = request.value(QStringLiteral("title")).toString();
	const auto chosen = Pickers::pickLocation(from, location);
	if (!chosen)
		return QVariant();
	return QVariantMap{
		{QStringLiteral("map"), chosen->map},
		{QStringLiteral("x"), chosen->x},
		{QStringLiteral("y"), chosen->y},
		{QStringLiteral("direction"), chosen->direction},
	};
}

int PickersBridge::openMapId() const
{
	return Pickers::openMapId();
}
