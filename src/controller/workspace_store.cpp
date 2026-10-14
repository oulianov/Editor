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

#include "workspace_store.h"
#include <QCryptographicHash>
#include <QSettings>

namespace {

const QString geometry_key = QStringLiteral("main_window_geometry");
const QString layout_key = QStringLiteral("main_window_layout");
const QString dock_layout_key = QStringLiteral("main_window_dock_layout");

}

QVariantMap Workspace::MapView::toVariant() const
{
	QVariantMap state{{"id", id}, {"zoom", zoom}, {"cell", cell}};
	if (scroll)
		state.insert("scroll", QVariantMap{{"x", scroll->x()}, {"y", scroll->y()}});
	return state;
}

Workspace::MapView Workspace::MapView::fromVariant(const QVariantMap& state)
{
	MapView view;
	view.id = state.value("id", -1).toInt();
	view.zoom = state.value("zoom", 2.0).toDouble();
	view.cell = state.value("cell", QPoint(-1, -1)).toPoint();
	if (state.value("scroll").isValid()) {
		const QVariantMap scroll = state.value("scroll").toMap();
		view.scroll = QPoint(scroll.value("x").toInt(), scroll.value("y").toInt());
	}
	return view;
}

WorkspaceStore::WorkspaceStore(QSettings* settings) :
	m_settings(settings)
{
}

QString WorkspaceStore::key(const QString& project_dir)
{
	// the folder can have any characters: a key of its hash
	return QStringLiteral("project_workspace/") + QString::fromLatin1(QCryptographicHash::hash(
		project_dir.toUtf8(), QCryptographicHash::Sha256).toHex());
}

QVariant WorkspaceStore::value(const QString& key) const
{
	return m_settings ? m_settings->value(key) : QSettings().value(key);
}

void WorkspaceStore::setValue(const QString& key, const QVariant& value)
{
	if (m_settings)
		m_settings->setValue(key, value);
	else
		QSettings().setValue(key, value);
}

bool WorkspaceStore::contains(const QString& project_dir) const
{
	return value(key(project_dir)).isValid();
}

Workspace WorkspaceStore::load(const QString& project_dir) const
{
	return fromVariant(value(key(project_dir)).toMap(), windowLayout());
}

void WorkspaceStore::save(const QString& project_dir, const Workspace& workspace)
{
	setValue(key(project_dir), toVariant(workspace));
}

QVariantMap WorkspaceStore::toVariant(const Workspace& workspace)
{
	QVariantMap value{
		{"active_map", workspace.activeMap},
		{"selected_map", workspace.selectedMap},
		{"tree_scroll", workspace.treeScroll},
		{"editing", workspace.editing},
		{"palette_x", workspace.paletteScroll.x()},
		{"palette_y", workspace.paletteScroll.y()},
		{"layout", workspace.layout},
	};
	if (workspace.maps) {
		QVariantList maps;
		for (const Workspace::MapView& map : *workspace.maps)
			maps.append(map.toVariant());
		value.insert("maps", maps);
	}
	if (workspace.expanded) {
		QVariantList expanded;
		for (int id : *workspace.expanded)
			expanded.append(id);
		value.insert("expanded", expanded);
	}
	if (!workspace.dockLayout.isEmpty())
		value.insert("dock_layout", workspace.dockLayout);
	return value;
}

Workspace WorkspaceStore::fromVariant(const QVariantMap& value, const QVariantMap& window_layout)
{
	Workspace workspace;
	if (value.contains("maps")) {
		QList<Workspace::MapView> maps;
		for (const QVariant& map : value.value("maps").toList())
			maps.append(Workspace::MapView::fromVariant(map.toMap()));
		workspace.maps = maps;
	}
	workspace.activeMap = value.value("active_map", -1).toInt();
	workspace.selectedMap = value.value("selected_map", -1).toInt();
	if (value.contains("expanded")) {
		QList<int> expanded;
		for (const QVariant& id : value.value("expanded").toList())
			expanded.append(id.toInt());
		workspace.expanded = expanded;
	}
	workspace.treeScroll = value.value("tree_scroll").toInt();
	// older workspaces kept the editing state at the top
	workspace.editing = value.contains("editing") ? value.value("editing").toMap() : value;
	workspace.paletteScroll = QPoint(value.value("palette_x").toInt(), value.value("palette_y").toInt());
	// older ones kept the docks only (as "panels"); without either, the window's
	if (value.contains("layout"))
		workspace.layout = value.value("layout").toMap();
	else if (value.contains("panels"))
		workspace.layout = {{"docks", value.value("panels")}};
	else
		workspace.layout = window_layout;
	workspace.dockLayout = value.value("dock_layout").toByteArray();
	return workspace;
}

QByteArray WorkspaceStore::windowGeometry() const
{
	return value(geometry_key).toByteArray();
}

void WorkspaceStore::setWindowGeometry(const QByteArray& geometry)
{
	setValue(geometry_key, geometry);
}

QVariantMap WorkspaceStore::windowLayout() const
{
	return value(layout_key).toMap();
}

void WorkspaceStore::setWindowLayout(const QVariantMap& layout)
{
	setValue(layout_key, layout);
}

QByteArray WorkspaceStore::windowDockLayout() const
{
	return value(dock_layout_key).toByteArray();
}

void WorkspaceStore::setWindowDockLayout(const QByteArray& layout)
{
	setValue(dock_layout_key, layout);
}

QRect WorkspaceStore::dialogGeometry(const QString& name) const
{
	return value(QStringLiteral("window_states/qml/") + name).toMap().value("rect").toRect();
}

void WorkspaceStore::setDialogGeometry(const QString& name, const QRect& geometry)
{
	setValue(QStringLiteral("window_states/qml/") + name, QVariantMap{{"rect", geometry}});
}

void WorkspaceStore::sync()
{
	if (m_settings)
		m_settings->sync();
	else
		QSettings().sync();
}
