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
#include <QHash>
#include <QMap>
#include <QModelIndex>
#include <QObject>
#include <QPointer>
#include <QRect>
#include <QSettings>
#include <QTimer>
#include <QVariantList>
#include <QtQmlIntegration>
#include <lcf/rpg/event.h>
#include <memory>
#include "controller/workspace_store.h"
#include "editor_window.h"

class ActionCatalog;
class EditorState;
class MapEditor;
class MapTabsController;
class MapTreeModel;
class ProjectController;
class QQuickWindow;
class QmlShellDialogService;
class QmlShellDialogs;
class SearchWindowHost;
class ShellController;

Q_MOC_INCLUDE("controller/action_catalog.h")
Q_MOC_INCLUDE("controller/map_tabs_controller.h")
Q_MOC_INCLUDE("controller/shell_controller.h")
Q_MOC_INCLUDE("model/editor_state.h")
Q_MOC_INCLUDE("model/map_tree_model.h")
Q_MOC_INCLUDE("ui/map/map_editor.h")

/**
 * The main window of the Editor (MainWindow.qml), its C++ side: it holds what the
 * window does (ShellController: the project, its map tabs, the actions),
 * runs the actions of the menus and toolbars (trigger), makes the editing
 * of each map open in a tab (a MapEditor for its MapCanvas), opens the
 * windows the maps ask for (an event, a teleport, a test play), and keeps
 * the workspace of the projects: the tabs, their zoom and scroll, the map
 * tree, the palette and the docks (KDDockWidgets' layout).
 *
 * Its questions go over the window in front.
 */
class QmlShell : public QObject, public EditorWindow
{
	Q_OBJECT
	QML_NAMED_ELEMENT(Shell)
	QML_UNCREATABLE("The main window's shell is made by the Editor")
	Q_PROPERTY(ShellController* controller READ controller CONSTANT)
	Q_PROPERTY(ActionCatalog* actions READ actions CONSTANT)
	Q_PROPERTY(MapTabsController* tabs READ tabs CONSTANT)
	Q_PROPERTY(MapTreeModel* tree READ tree CONSTANT)
	Q_PROPERTY(EditorState* editorState READ editorState CONSTANT)
	/** The editing of the map shown, null for none */
	Q_PROPERTY(MapEditor* currentEditor READ currentEditor NOTIFY currentEditorChanged)
	Q_PROPERTY(bool hasProject READ hasProject NOTIFY projectChanged)
	/** A note of the project for the status bar (a file another program
	 *  changed...), empty once its time is over */
	Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
	/** The window has the keyboard: its keys are its own. A window over it
	 *  (a modal one) keeps them, while Qt deems the main window active */
	Q_PROPERTY(bool windowFocused READ windowFocused NOTIFY windowFocusedChanged)

public:
	explicit QmlShell(QObject* parent = nullptr);
	~QmlShell() override;

	/** Makes the window (MainWindow.qml) and shows it, its layout as last
	 *  time; false when it cannot be made */
	bool show();
	QQuickWindow* window() const;

	ShellController* controller() const;
	ActionCatalog* actions() const;
	MapTabsController* tabs() const;
	MapTreeModel* tree() const;
	EditorState* editorState() const;
	MapEditor* currentEditor() const;
	bool hasProject() const;
	QString statusMessage() const;
	bool windowFocused() const;

	/*
	 * What main.cpp and the scripts ask (EditorWindow)
	 */

	QWindow* handle() const override;
	ShellController* shell() const override;
	ProjectController* projectController() const override;
	/** The Settings window when no RTP folder is set */
	void askForRtp();
	void loadLastProject();
	void LoadProject(QString folder) override;
	bool hasMap(int id) const override;
	void openScene(int id) override;
	const lcf::rpg::Map* openMapData(int id) override;
	bool openEvent(int map, int event, int page = 0, int line = 0) override;
	/** The map tree's node selected, as a click on it (it is shown);
	 *  focus: the tree takes the keys */
	bool selectNode(int id, bool focus = false);
	/** The event window of an event of a map, its map shown first (modal);
	 *  false without such an event */
	bool editMapEvent(int map, int event);
	/** A note in the status bar for timeout ms (0: until the next one) */
	void showStatusMessage(const QString& text, int timeout);

	/*
	 * For the window (QML)
	 */

	/** The editing of the map of a tab, null for none */
	Q_INVOKABLE MapEditor* editorOf(int id) const;
	/** An action of the catalog was chosen (a menu, a button, its keys);
	 *  checked: the state a checkable one has now */
	Q_INVOKABLE void trigger(const QString& name, bool checked = false);
	/** A node of the map tree is shown: its map in a tab (keep: a kept tab,
	 *  else the temporary one), an area on its map */
	Q_INVOKABLE void openNode(int id, bool keep);
	/** The nodes of the map tree, depth first, the project first */
	Q_INVOKABLE QList<int> treeIds() const;
	Q_INVOKABLE QModelIndex treeIndex(int id) const;
	Q_INVOKABLE int treeId(const QModelIndex& index) const;
	/** The name of a node of the map tree, as it shows it */
	Q_INVOKABLE QString treeName(int id) const;
	/** lcf::rpg::TreeMap::MapType of a node, -1 for none */
	Q_INVOKABLE int treeType(int id) const;
	/** The user shows or hides the children of a node (the map tree's
	 *  data keeps it) */
	Q_INVOKABLE void setExpanded(int id, bool expanded);
	/** Whether the tree takes the map id dropped above (0), below (1) or on
	 *  (2) the node target (MapTreeDrop) */
	Q_INVOKABLE bool canDrop(int id, int target, int position) const;
	/** Moves the map dropped there, and writes the tree; false when refused */
	Q_INVOKABLE bool dropMap(int id, int target, int position);
	/** The rpgsync entries of the tree's right-click menu on a node:
	 *  {text, toolTip, enabled, location} (greyed without rpgsync) */
	Q_INVOKABLE QVariantList scriptItems(int id) const;
	Q_INVOKABLE void openScript(const QStringList& location);
	/** The window is closed: false when it stays (unsaved maps, Cancel) */
	Q_INVOKABLE bool requestClose();
	/** Writes the workspace of the project open and the window's layout */
	Q_INVOKABLE void saveWorkspace();

signals:
	void currentEditorChanged();
	void projectChanged();
	void statusMessageChanged();
	void windowFocusedChanged();
	/** The View menu shows or hides a dock (dockPalette, dockMapTree) */
	void dockRequested(const QString& dock, bool open);
	/** The workspace of the project opened: the window shows its view
	 *  (expanded: the ids, or null for the map tree's own; treeScroll;
	 *  paletteX, paletteY) */
	void viewRestoreRequested(const QVariantMap& view);
	/** Back to the default view (Settings' reset of the local state) */
	void viewResetRequested();
	/** The tree shows a node selected (expanded to it, scrolled to it) */
	void treeSelectRequested(int id);
	/** The view of a map dashes the cells of an area, centered */
	void showAreaRequested(int map, const QRect& cells);
	/** The view of a map scrolls a cell to its middle */
	void centerRequested(int map, int x, int y);

private:
	void connectController();
	void connectTabs();
	/** The editing of the map of a tab opened, before its view is made */
	void createEditor(int id);
	void destroyEditor(int id);
	void onCurrentMapChanged();
	/** The events and the chipset of the map shown, for the windows that
	 *  list them (core().currentMapEvents...) */
	void followCurrentMap();
	/** Which ratio of Map Zoom the map shown is at */
	void updateZoomActions();
	void setStatusMessage(const QString& text, int timeout);
	/** The event window of an event of a map (a new one: not on the map
	 *  yet), over the window (modal), on a page (from 1; 0: the one shown
	 *  last) and a line of it (from 1); what it applies goes to the map,
	 *  with undo */
	void editEvent(int map, const lcf::rpg::Event& event, int page = 0, int line = 0);
	/** Place Teleport Event on a cell of a map: RPG Maker's Select Location,
	 *  then the event */
	void placeTeleport(int map, const QPoint& cell);

	// the actions
	void projectSettings();
	void exportGame();
	void openDatabase();
	void openSearch();
	void testPlayOptions();
	void quit();

	// the workspace
	void onProjectOpened();
	void onProjectClosing();
	void restoreWorkspace();
	void resetLocalState();
	/** The nodes the tree shows expanded put into the map tree's data */
	bool syncExpandedFromView();
	/** What only the window knows: {expanded, treeScroll, paletteX, paletteY} */
	QVariantMap viewState() const;
	QByteArray dockLayout() const;
	void restoreDockLayout(const QByteArray& layout);

	ShellController* m_shell;
	ProjectController* m_project;
	MapTabsController* m_tabs;
	ActionCatalog* m_actions;
	std::unique_ptr<QmlShellDialogService> m_dialogService;
	std::unique_ptr<QmlShellDialogs> m_dialogs;
	QPointer<QQuickWindow> m_window;
	QHash<int, MapEditor*> m_editors;
	/** The views of the workspace restored, applied when their tab is made */
	QHash<int, Workspace::MapView> m_pendingViews;
	/** The events of the map shown by id (core().currentMapEvents()) */
	QMap<int, lcf::rpg::Event*> m_events;
	QSettings m_settings;
	WorkspaceStore m_workspaces{&m_settings};
	/** The view of the workspace restored before the window was made */
	QVariantMap m_pendingView;
	/** The layout of the docks as the window made them, for a reset */
	QByteArray m_defaultLayout;
	QString m_statusMessage;
	QTimer m_statusTimer;
	QPointer<SearchWindowHost> m_search;
	bool m_closing = false;
};
