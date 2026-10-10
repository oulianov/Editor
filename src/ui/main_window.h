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

#include <optional>

#include <QFile>
#include <QMainWindow>
#include <QSettings>
#include <QTreeWidgetItem>
#include <memory>
#include <lcf/rpg/map.h>
#include <lcf/rpg/mapinfo.h>
#include "ui/other/resource_manager_dialog.h"
#include "ui/database/database_dialog.h"
#include "ui/other/search_dialog.h"
#include "ui/map/map_scene.h"
#include "ui/common/palette_scene.h"

class QActionGroup;
class QLabel;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit MainWindow(QWidget *parent = nullptr);
	~MainWindow();

	/** Opens the Settings window when no RTP folder is set */
	void AskForRtp();
	void LoadLastProject();
	void LoadProject(QString foldername);
	void ImportProject(const QDir& src_dir, QDir& target_dir, bool convert_xyz);

	MapScene *currentScene();
	MapScene *getScene(int id);
	void openScene(int mapID);
	bool hasMap(int mapID) const;
	void selectTile(int x, int y);

private slots:
	void update_actions();

	void on_actionQuit_triggered();

	void on_actionPaletteToggle_triggered(bool checked);

	void on_actionMapTreeToggle_triggered(bool checked);

	void on_actionResourceManager_triggered();

	void on_actionDatabase_triggered();


	void on_actionProjectNew_triggered();

	void on_actionProjectClose_triggered();

	void on_actionProjectOpen_triggered();

	void on_actionJukebox_triggered(bool disconnect = false);

	void on_actionLayerLower_triggered();

	void on_actionLayerUpper_triggered();

	void on_actionLayerEvents_triggered();

	void on_actionZoomIn_triggered();

	void on_actionZoomOut_triggered();

	void on_actionZoom100_triggered();

	void on_treeMap_itemDoubleClicked(QTreeWidgetItem *item, int column);

	void on_tabMap_tabCloseRequested(int index);

	void on_tabMap_currentChanged(int index);

	void on_actionProjectImport_triggered();


	void on_actionZoom_triggered();

	void on_actionDrawPen_triggered();

	void on_actionDrawRectangle_triggered();

	void on_actionDrawCircle_triggered();
	void on_actionDrawSelect_triggered();

	void on_actionDrawFill_triggered();

	void updateLayerActions();

	void updateToolActions();

	/** Check the zoom ratio of the map shown (none at other scales) */
	void updateZoomActions();

	/** The edit mode, the map and the cell in the status bar, as RPG Maker */
	void updateStatusBar();

	void updateSearchUI();

	void on_actionPlayTest_triggered();

	void on_mapChanged();

	void on_mapUnchanged();

	void on_actionUndo_triggered();
	void on_actionRedo_triggered();

	void on_actionMapSave_triggered();

	void on_actionMapRevert_triggered();

	void on_treeMap_currentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);

	void on_actionMapNew_triggered();

	void on_actionMapCopy_triggered();

	void on_actionMapPaste_triggered();

	void on_actionMapDelete_triggered();

	void on_actionMapProperties_triggered();

	void on_actionMapShift_triggered();
	void on_actionMapGenerateDungeon_triggered();
	void on_actionSettings_triggered();
	void on_actionProjectRename_triggered();
	void on_actionProjectConvert2003_triggered();
	void on_actionGameDiskCreate_triggered();
	void on_actionMapSaveAll_triggered();
	/** Applies the options of the Settings window */
	void applySettings();
	void setPlatformShortcuts();
	/** Shows the map (or the area on its map) of a tree item */
	void openTreeItem(QTreeWidgetItem* item);
	/** Shows the map of item in its tab; keep: a kept tab, else the
	 *  temporary one (italic), replaced by the next map opened */
	void openMap(QTreeWidgetItem* item, bool keep);
	void setTemporaryMap(int id);
	/** The temporary tab of id becomes a kept one */
	void keepMap(int id);

	void on_actionAreaNew_triggered();

	void on_actionAreaProperties_triggered();

	void on_actionAreaDelete_triggered();

	void on_actionPlayTestOptions_triggered();


	void on_actionFullScreenToggle_toggled(bool checked);

	void on_actionTitleBackgroundToggle_toggled(bool checked);

	void onMapTreeContextMenu(const QPoint& pos);

	void onMapTreeDropped();

	void on_actionSearch_triggered();



	void onDatabaseFileChanged();

	void onTreeMapFileChanged();

	void onMapFileChanged(int id);

	void onMapReloaded();

private:
	bool canReload(int kind) const;
	void updateWatcher();
	void populateMapTree(bool open_active_map);
	void refreshIcons();
	void closeEvent(QCloseEvent *event) override;
	void showEvent(QShowEvent* event) override;
	void changeEvent(QEvent* event) override;
	bool saveAll();
	void saveWorkspace();
	void restoreWorkspace();
	void restoreWorkspaceLayout();
	void resetLocalState();
	QGraphicsView *getView(int id);
	QGraphicsView *getTabView(int index);
	MapScene *getTabScene(int index);
	void removeView(int id);
	void removeMap(const int id);
	bool eventFilter(QObject* watched, QEvent* event) override;
	/** Id of the map tree's current item, -1 without one */
	int currentMapId() const;
	/** TreeMap::MapType of a map tree entry, -1 if there is none */
	int mapType(int id) const;
	lcf::rpg::MapInfo* mapInfo(int id);
	int freeMapId() const;
	QTreeWidgetItem* addTreeItem(const lcf::rpg::MapInfo& info);
	/** Adds a map or area to the map tree (data, item, order) and saves the tree */
	QTreeWidgetItem* insertMapInfo(const lcf::rpg::MapInfo& info);
	/** Copies the tree (parents, depth, order, expanded) into the map tree data, true if it changed */
	bool syncTreeOrder();
	/** Width and height of a map, from its file */
	QSize mapSize(int id);
	bool saveBeforeTestPlay();
	/** Reads the map tree again when another program changed it, before an
	 *  action that changes and writes it */
	void refreshTreeMap();
	/** Asks to save the modified maps; false when cancelled */
	bool saveModifiedMaps(const QString& title, const QString& question);
	MapScene* senderScene();
	void setTabText(MapScene* scene);
	bool convertXYZtoPNG(QFile &xyz_file, QString out_path);

	Ui::MainWindow *ui;
	ResourceManagerDialog *dlg_resource;
	QSettings m_settings;
	PaletteScene *m_paletteScene;
	QMap<int,QGraphicsView*> m_views;
	QMap<int,QTreeWidgetItem*> m_treeItems;
	// Copy Map takes the map when copied: it can be pasted after a deletion
	std::unique_ptr<lcf::rpg::Map> m_copiedMap;
	lcf::rpg::MapInfo m_copiedMapInfo;
	SearchDialog *searchdialog = nullptr;
	/** The language setting seen last: a change switches the language */
	std::optional<QString> m_languageSetting;
	/** The map of the temporary tab (0: none) */
	int m_temporaryMap = 0;
	/** openTreeItem is running (opening an area selects items again) */
	bool m_openingTreeItem = false;
	bool m_windowGeometryRestored = false;
	QSize m_defaultWindowSize;
	QVariantMap m_defaultLayoutState;
	QVariantMap m_pendingWorkspace;
	QActionGroup *m_zoomGroup;
	QLabel *m_editModeLabel;
	QLabel *m_mapLabel;
	QLabel *m_cellLabel;
};
