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

#include <QByteArray>
#include <QList>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QVariantMap>
#include <optional>

class QSettings;

/**
 * How the main window showed a project, apart from its data: it comes back
 * when the project is opened again. Nothing here is game data.
 */
struct Workspace
{
	/** A map open in a tab */
	struct MapView
	{
		int id = -1;
		/** Its scale (2: the 1/2 ratio maps open at) */
		double zoom = 2.0;
		/** The cell selected on the event layer, (-1, -1) for none */
		QPoint cell{-1, -1};
		/** Where its view was scrolled to, in pixels of the view; none: where
		 *  its map info says (the map tree's scroll position) */
		std::optional<QPoint> scroll;

		/** As saved: id, zoom, cell, scroll {x, y} */
		QVariantMap toVariant() const;
		static MapView fromVariant(const QVariantMap& state);
		bool operator==(const MapView&) const = default;
	};

	/** The tabs in their order; none (never saved): the map tree's active map opens */
	std::optional<QList<MapView>> maps;
	/** The map of the tab shown, -1 for none */
	int activeMap = -1;
	/** The node selected in the map tree (0: the project), -1 for none */
	int selectedMap = -1;
	/** The nodes of the map tree expanded; none: as the map tree file says */
	std::optional<QList<int>> expanded;
	int treeScroll = 0;
	/** The layer, the tool and the brushes (EditorState::editingState()) */
	QVariantMap editing;
	QPoint paletteScroll;
	/** The layout of the main window of older versions (docks,
	 *  splitters, headers), kept as it was saved */
	QVariantMap layout;
	/** The layout of the QML window's docks (KDDockWidgets::LayoutSaver::
	 *  serializeLayout()), empty until it has one */
	QByteArray dockLayout;

	bool operator==(const Workspace&) const = default;
};

/**
 * The workspaces of the projects, in the settings: "project_workspace/<the
 * SHA-256 of the project folder>", as the Editor always saved them, and the
 * window's own state used without a project (main_window_geometry,
 * main_window_layout). What older versions saved is read too.
 */
class WorkspaceStore
{
public:
	/** In settings; nullptr: the application's settings */
	explicit WorkspaceStore(QSettings* settings = nullptr);

	/** The settings key of the workspace of a project folder */
	static QString key(const QString& project_dir);

	bool contains(const QString& project_dir) const;
	/** The workspace of a project; an empty one (maps and expanded unset)
	 *  when none was saved. Without its own layout, the window's. */
	Workspace load(const QString& project_dir) const;
	void save(const QString& project_dir, const Workspace& workspace);

	/** Written as saved, and read back from its settings value: older ones
	 *  kept their layout as "panels" (the docks), and their editing state
	 *  at the top level (layer, tool, tiles...) */
	static QVariantMap toVariant(const Workspace& workspace);
	static Workspace fromVariant(const QVariantMap& value, const QVariantMap& window_layout = {});

	// The window, project or not
	QByteArray windowGeometry() const;
	void setWindowGeometry(const QByteArray& geometry);
	QVariantMap windowLayout() const;
	void setWindowLayout(const QVariantMap& layout);
	/** The QML window's docks without a project (LayoutSaver) */
	QByteArray windowDockLayout() const;
	void setWindowDockLayout(const QByteArray& layout);

	/** Where a window of the QML interface was and its size, by its name
	 *  ("window_states/qml/<name>", reset with the other window states);
	 *  invalid: none saved */
	QRect dialogGeometry(const QString& name) const;
	void setDialogGeometry(const QString& name, const QRect& geometry);

	/** Written to disk now (the Editor can quit next) */
	void sync();

private:
	QVariant value(const QString& key) const;
	void setValue(const QString& key, const QVariant& value);

	QSettings* m_settings;
};
