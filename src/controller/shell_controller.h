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

#include <QDir>
#include <QList>
#include <QObject>
#include <QRect>
#include <QString>
#include <functional>
#include "model/editor_state.h"

class ActionCatalog;
class MapTabsController;
class ProjectController;
class ShellDialogs;

/**
 * What the main window does, apart from how it is shown (MainWindow.qml,
 * QmlShell). It holds the project open
 * (ProjectController), its map tabs (MapTabsController) and the actions
 * (ActionCatalog, their enabled and checked states kept up to date), and
 * does what the actions of the menus ask: opening, importing, converting
 * and closing projects, the map tree's actions on the node selected, Save,
 * Revert, the layers and the tools, the test play options. The status bar
 * texts and the window title are its.
 *
 * It asks through DialogService and opens its editing windows through the
 * ShellDialogs of the interface. What only the views of a map can do (the
 * tiles floating over it, an area shown) is asked by its signals.
 */
class ShellController : public QObject
{
	Q_OBJECT
	Q_PROPERTY(QString windowTitle READ windowTitle NOTIFY windowTitleChanged)
	Q_PROPERTY(QString editModeText READ editModeText NOTIFY statusChanged)
	Q_PROPERTY(QString mapText READ mapText NOTIFY statusChanged)
	Q_PROPERTY(int selectedMap READ selectedMap WRITE setSelectedMap NOTIFY selectedMapChanged)

public:
	explicit ShellController(QObject* parent = nullptr);
	~ShellController() override;

	ProjectController* project() const;
	MapTabsController* tabs() const;
	ActionCatalog* actions() const;

	/** The editing windows of the interface (nothing is edited without) */
	void setDialogs(ShellDialogs* dialogs);

	/*
	 * The map tree: its node selected (0 the project, -1 none)
	 */

	int selectedMap() const;
	/** Set by the view of the tree (QML: its current row) */
	void setSelectedMap(int id);
	/** The view of the tree tells its selection when asked instead (a view
	 *  that keeps its own) */
	void setSelectionSource(std::function<int()> source);

	/*
	 * The project
	 */

	/** Opens the project of a folder; the project open is closed first,
	 *  once the folder was read (asking about its maps' changes): false
	 *  when it stays, or the folder cannot be read */
	bool openProject(const QString& folder);
	/** The project open last, if any */
	void openLastProject();
	/** Close Project: false when cancelled (its maps' changes) */
	bool closeProject();
	/** New Project, Open Project, Import Project: their windows first */
	void newProject();
	void openProjectWindow();
	void importProjectWindow();
	/** Writes the RPG Maker 2000/2003 project of src_dir to target_dir, and
	 *  opens it */
	void importProject(const QDir& src_dir, QDir& target_dir, bool convert_xyz);
	/** Convert to RPG Maker 2003: the project is closed, converted from its
	 *  files and opened again */
	void convertProject();

	/*
	 * The map tree's actions, on the node selected
	 */

	/** Shows a map in its tab (kept, or the temporary one); an area on its
	 *  map; true when a map is shown */
	bool openNode(int id, bool keep);
	void newMap();
	void copyMap();
	void pasteMap();
	void deleteMap();
	/** The node selected, else the map shown */
	void mapProperties();
	void generateDungeon();
	void shiftMap();
	void newArea();
	void areaProperties();
	void deleteArea();
	/** Maps dragged in the tree: under parent at row (counted without them) */
	struct MapMove {
		int id;
		int parent;
		int row;
	};
	/** Moves them in the map tree, with what the view shows expanded, and
	 *  writes it when it changed */
	void moveMaps(const QList<MapMove>& moves);

	/*
	 * Going to a map
	 */

	/** The map of the party's start position, 0 when it is unset or not a
	 *  map of the tree */
	int startMap() const;
	/** A map of the tree, for Go to Map */
	struct MapEntry {
		int id;
		QString name;
	};
	/** The maps of the tree by id, without the areas */
	QList<MapEntry> maps() const;
	/** Selects a map in the tree and shows it in a kept tab, as Return on
	 *  it in the tree; false when it is not a map */
	bool goToMap(int id);
	/** Open Start Map */
	bool openStartMap();

	/*
	 * The map shown
	 */

	void saveMap();
	void saveAllMaps();
	/** Asks first */
	void revertMap();
	void setLayer(EditorState::Layer layer);
	void setTool(EditorState::Tool tool);
	/** The ratios of Map Zoom (actionZoom1_1...) as scales: 4, 2, 1, 0.5 */
	static double zoomRatio(const QString& action);
	/** The ratio the map shown is at (0: none of them): its view tells */
	void setZoomRatio(double ratio);

	/*
	 * Test play options (remembered)
	 */

	void setTestPlayFullScreen(bool on);
	void setTestPlayShowTitle(bool on);

	/*
	 * What the window shows
	 */

	/** The enabled and checked states of the actions (ActionCatalog), from
	 *  the project, the node selected and the map shown */
	void updateActions();
	/** "EasyRPG Editor - <game title>" */
	QString windowTitle() const;
	/** RPG Maker's edit mode and tool ("Lower Layer Editing Mode - Pen Tool") */
	QString editModeText() const;
	/** The id and name of the map shown ("0001: name"), empty for none */
	QString mapText() const;

	/** The settings of the Settings window, again: the RTP, the animation,
	 *  the watcher, the language */
	void applySettings();

signals:
	/** A project was opened: its windows, its workspace */
	void projectOpened();
	/** The project is about to close (its changes saved or dropped): its
	 *  workspace is saved, its windows close */
	void projectClosing();
	/** The map tree was read again: the tree shown is a new one. The tabs
	 *  of the maps removed close after it */
	void treeMapReloaded();
	void windowTitleChanged();
	void statusChanged();
	void selectedMapChanged(int id);

	/** The view of the tree selects a node */
	void selectMapRequested(int id);
	/** The view of a map shows the cells of an area */
	void showAreaRequested(int map, const QRect& cells);
	/** The views of an open map put down the tiles floating over it (its
	 *  properties are edited) */
	void floatingCommitRequested(int map);
	/** The views of an open map drop the tiles floating over it (it is
	 *  read again) */
	void floatingCancelRequested(int map);
	/** An open map is shifted by its view (with undo, as its other edits) */
	void shiftRequested(int map, int dx, int dy);

protected:
	bool eventFilter(QObject* watched, QEvent* event) override;

private:
	/** A map or area added: selected, a map opened */
	void showNewNode(int id, bool open);
	void updateLayerChecks();
	void updateToolChecks();
	/** The node's name, as the tree shows it */
	QString nodeName(int id) const;

	ProjectController* m_project;
	MapTabsController* m_tabs;
	ActionCatalog* m_actions;
	ShellDialogs* m_dialogs = nullptr;
	int m_selectedMap = -1;
	std::function<int()> m_selectionSource;
	double m_zoomRatio = 0;
	/** The language seen last: a change switches it */
	QString m_language;
	bool m_languageKnown = false;
};
