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
#include <QMap>
#include <QObject>
#include <QSize>
#include <QStringList>
#include <functional>
#include <memory>
#include <optional>
#include <lcf/rpg/map.h>
#include <lcf/rpg/mapinfo.h>
#include "controller/database_apply.h"
#include "model/map_tree_model.h"
#include "model/project_convert.h"
#include "model/project_watcher.h"
#include "model/stale_choice.h"

class DialogService;
class MapDocument;
class Project;
class QFile;

/**
 * What the Editor does with the project open (core().project()), apart from
 * how it is shown: opening, importing, converting and closing it, the maps
 * open (their MapDocument, by map id) and saving them, the map tree (its
 * model, its file, the maps and areas added, copied, moved and removed),
 * and the files other programs change (ProjectWatcher, held back by the
 * EditLocks of the editors open).
 *
 * It asks the user through DialogService::instance(). The dialogs that edit
 * (map properties, areas, dungeons...) are the interface's: it gets their
 * data before them and takes their result after.
 */
class ProjectController : public QObject
{
	Q_OBJECT

public:
	explicit ProjectController(QObject* parent = nullptr);
	~ProjectController() override;

	/** The map tree of the project open (empty without one) */
	MapTreeModel* treeModel();

	/** The folders of the resources of a game */
	static const QStringList& resourceDirs();
	/** The file of a map, as Project::loadMap names it */
	static QString mapFileName(int id);
	/** The name RPG Maker gives a new map */
	static QString defaultMapName(int id);
	/** The database file of the project open, for the OK and Apply of the
	 *  database windows (DatabaseApply): the watcher knows what was written */
	static DatabaseApply::File databaseFile();

	/** The order the interface shows the open maps in (its tabs): saving
	 *  them all goes in this order. Without it, by id. */
	void setDocumentOrder(std::function<QList<int>()> order);
	/** Puts what the interface's tree shows (the nodes expanded) into the
	 *  tree before a map or an area is added or removed and the tree
	 *  written; true when it changed */
	void setViewSync(std::function<bool()> sync);
	/** Puts what the interface's tree shows into the tree now; true when
	 *  it changed */
	bool syncView();

	/*
	 * The project
	 */

	/** Opens the project of folder as core().project(); false (an error
	 *  shown) when it cannot be read */
	bool open(const QString& folder);
	/** Reads the project of folder, its database and map tree, not opened
	 *  yet; nullptr (an error shown) when it cannot be read */
	std::shared_ptr<Project> read(const QString& folder);
	/** Opens a project read as core().project() */
	void open(const std::shared_ptr<Project>& project);
	/** The project is closed: its maps (their views let them go first),
	 *  its tree, the map copied */
	void close();
	/** Watches the project open for the changes of other programs when the
	 *  setting asks (rpgsync mode), else nothing */
	void updateWatcher();

	/** The progress of an import, as a progress dialog shows it */
	struct ImportProgress {
		std::function<void(const QString& label, int maximum)> begin;
		std::function<void(const QString& label)> label;
		std::function<void(int maximum)> maximum;
		std::function<void(int value)> value;
	};
	enum class ImportResult { Imported, Failed, CopyFailed };
	/** Writes the RPG Maker 2000/2003 project of src_dir to target_dir as
	 *  an EasyRPG project (its XYZ images as PNG when convert_xyz); folder:
	 *  the project imported, to open */
	ImportResult importProject(const QDir& src_dir, QDir& target_dir, bool convert_xyz,
		const ImportProgress& progress, QString* folder);
	/** An XYZ image (RPG Maker 2000's) written as a PNG */
	static bool convertXYZtoPNG(QFile& xyz_file, const QString& out_path);
	/** Converts the project of folder, closed, to RPG Maker 2003; the
	 *  error, empty when it worked */
	QString convertTo2003(const QString& folder, const ProjectConvert::Options& options, QString* backup);

	/*
	 * The open maps
	 */

	/** The document of a map, made unless it is open (its view reads it) */
	MapDocument* openDocument(int id);
	MapDocument* document(int id) const;
	QList<int> documentIds() const;
	/** documentClosing first: the views of the document let it go */
	void closeDocument(int id);
	/** Asks (title, question) to save the modified maps; false when
	 *  cancelled, or a map was not saved */
	bool saveModifiedMaps(const QString& title, const QString& question);
	/** Before the maps are closed (closing the project, quitting) */
	bool saveAll();
	/** The Player reads the files: what is not saved is not tested */
	bool saveBeforeTestPlay();
	/** Save All: the modified maps */
	void saveAllMaps();

	/*
	 * The map tree
	 */

	/** RPG Maker takes the first free id; a map file without its tree entry
	 *  (another program's new map) keeps its id */
	int freeMapId() const;
	/** Reads the map tree again when another program changed it, before an
	 *  action that changes and writes it */
	void refreshTreeMap();
	/** Writes the map tree after an action changed it. What another program
	 *  changed in its file meanwhile (a dialog was open) is merged, a
	 *  conflict asked, never replaced silently. false: not written (the tree
	 *  is then the one of the file, or a write error was shown) */
	bool writeTreeMap();
	/** Writes the map tree after a map saved its entry: what another
	 *  program changed is merged, else the tree of the file is taken */
	void writeTreeMapAfterSave();
	/** Reads the map tree again from its file (treeMapReloaded) */
	bool reloadTreeMap();
	/** A map was saved or read again: its copy of its tree entry was
	 *  written, the tree keeps its parents, its order and the nodes the
	 *  interface shows expanded */
	void keepTreeAfterMapSave();

	/** New Map: the map info of a new map, a child of parent when it is a map */
	lcf::rpg::MapInfo newMapInfo(int parent) const;
	/** Writes a new map with its properties and adds it to the tree; its
	 *  id (another one when another program took it), 0 when not written */
	int addNewMap(lcf::rpg::MapInfo info, lcf::rpg::Map& map);
	bool hasCopiedMap() const;
	/** Copy Map: the map as saved (asking to save its changes first) */
	bool copyMap(int id);
	/** Paste Map: the copy as a child of parent; its id, 0 for none */
	int pasteMap(int parent);
	/** Delete Map: asks, then removes the map with its child maps and
	 *  areas, and their files; name: the map as the interface shows it */
	bool deleteMap(int id, const QString& name);

	/** A map read for its properties, not open */
	struct MapEdit {
		std::unique_ptr<lcf::rpg::Map> map;
		lcf::rpg::MapInfo info;
		/** The version of the file read: another program can write it
		 *  while its properties are edited */
		ProjectWatcher::Stamp stamp;
	};
	std::optional<MapEdit> readMapProperties(int id);
	/** Writes the map and its tree entry, asking when the file changed */
	bool saveMapProperties(int id, const MapEdit& edit);

	/** Generate Dungeon: the map generated from (the one open, unsaved
	 *  changes included) and the info of the new one, a child of it */
	struct DungeonSource {
		std::unique_ptr<lcf::rpg::Map> map;
		lcf::rpg::MapInfo info;
	};
	std::optional<DungeonSource> dungeonSource(int id);
	/** Writes the dungeon generated and adds it to the tree; its id, 0 for none */
	int addDungeon(lcf::rpg::MapInfo info, lcf::rpg::Map& map);
	/** Shift Map of a map not open, on disk, with the start positions on it */
	bool shiftMap(int id, int dx, int dy);

	/** An area, with the size of its map */
	struct AreaEdit {
		lcf::rpg::MapInfo info;
		QSize map_size;
	};
	/** Create Area: a new area of a map */
	std::optional<AreaEdit> newArea(int parent);
	/** Area Properties */
	std::optional<AreaEdit> editArea(int id);
	/** Adds an area to the tree (its map can be gone); its id, 0 for none */
	int addArea(lcf::rpg::MapInfo info);
	void setAreaInfo(int id, const lcf::rpg::MapInfo& info);
	/** Delete Area: asks, then removes it; name: as the interface shows it */
	bool deleteArea(int id, const QString& name);

signals:
	/** Another project is open (core().project()), or none */
	void projectChanged();
	/** A map is closed: its views let its document go */
	void documentClosing(int id);
	/** The database was read again (another program changed it) */
	void databaseReloaded(bool chipsets_changed);
	/** The map tree was read again: the tree shown is a new one */
	void treeMapReloaded();
	/** A short note for the status bar */
	void statusMessage(const QString& text, int timeout);

private:
	void onDatabaseChanged();
	void onMapChanged(int id);
	/** Saving an open map another program deleted: put it back in the
	 *  tree (Overwrite), or drop it with its changes (Reload) */
	StaleChoice askDeletedMap(int id);
	/** The documents in the interface's order */
	QList<int> documentOrder() const;
	/** Adds a map or an area to the tree and writes it */
	void insertMapInfo(const lcf::rpg::MapInfo& info);
	/** Writes a new map (title: of the error) and adds it, its id kept
	 *  unless another program took it meanwhile */
	int addMap(lcf::rpg::MapInfo info, lcf::rpg::Map& map, const QString& title);
	/** Removes a map or an area with what it holds: their documents, their
	 *  files, their tree entries */
	void removeMap(int id);
	QSize mapSize(int id) const;

	MapTreeModel m_tree;
	QMap<int, MapDocument*> m_documents;
	std::function<QList<int>()> m_documentOrder;
	std::function<bool()> m_viewSync;
	// Copy Map takes the map when copied: it can be pasted after a deletion
	std::unique_ptr<lcf::rpg::Map> m_copiedMap;
	lcf::rpg::MapInfo m_copiedMapInfo;
};
