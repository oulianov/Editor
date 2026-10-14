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

#include <QObject>
#include <QRect>
#include <QtQmlIntegration>
#include <cstdint>
#include <vector>
#include <lcf/rpg/map.h>
#include <lcf/rpg/mapinfo.h>
#include "model/dungeon_generator.h"
#include "model/encounter_list_model.h"
#include "model/map_document.h"
#include "qmlbinding/json_t.h"

class ProjectData;
class QQuickItem;

/**
 * Map Properties (MapPropertiesWindow.qml): copies of the map and of its
 * entry in the map tree, which the window edits through the JSON binding;
 * the caller takes them on OK (MapTreeDialogs::mapProperties).
 */
class MapPropertiesForm : public QObject
{
	Q_OBJECT
	QML_ELEMENT
	QML_UNCREATABLE("Map Properties is opened by MapTreeDialogs")
	Q_PROPERTY(Json* map READ map CONSTANT)
	Q_PROPERTY(Json* info READ info CONSTANT)
	Q_PROPERTY(EncounterListModel* encounters READ encounters CONSTANT)
	Q_PROPERTY(int mapId READ mapId CONSTANT)
	/** A map at the top of the tree has no parent: "Same as parent" is greyed */
	Q_PROPERTY(bool hasParent READ hasParent CONSTANT)
	/** Its size when the window opened */
	Q_PROPERTY(int oldWidth READ oldWidth CONSTANT)
	Q_PROPERTY(int oldHeight READ oldHeight CONSTANT)

public:
	MapPropertiesForm(ProjectData& project, const lcf::rpg::MapInfo& info, const lcf::rpg::Map& map,
		QObject* parent = nullptr);

	Json* map() const;
	Json* info() const;
	EncounterListModel* encounters() const;
	int mapId() const;
	bool hasParent() const;
	int oldWidth() const;
	int oldHeight() const;

	/** The tile ids a box of the dungeon generator shows (MapTools::GeneratorBox):
	 *  one, or 2 x 2 for an obstacle (-1: an empty cell) */
	Q_INVOKABLE QList<int> generatorTiles(int box) const;
	/** The palette cells of a box, "x,y wxh" (scripts) */
	Q_INVOKABLE QString generatorCellsText(int box) const;
	/** Select Tile for a box, over the window of from, in the chipset chosen */
	Q_INVOKABLE void pickGeneratorTiles(int box, QQuickItem* from);

	/** The map as OK writes it: the panorama off clears its settings, a
	 *  new size cuts or extends the layers (MapTools::resizeMap) */
	lcf::rpg::Map resultMap() const;
	lcf::rpg::MapInfo resultInfo() const;

signals:
	/** A box of the dungeon generator got other tiles */
	void generatorChanged();

private:
	lcf::rpg::Map m_map;
	lcf::rpg::MapInfo m_info;
	JsonT<lcf::rpg::Map>* m_mapJson;
	JsonT<lcf::rpg::MapInfo>* m_infoJson;
	EncounterListModel* m_encounters;
	int m_oldWidth;
	int m_oldHeight;
	std::vector<int16_t> m_generatorTiles;
	std::vector<uint32_t> m_generatorX;
	std::vector<uint32_t> m_generatorY;
	/** Written back only when a tile was chosen: maps keep what they had */
	bool m_generatorChanged = false;
};

/**
 * Area Properties (AreaPropertiesWindow.qml): a copy of the area's entry in
 * the map tree; its boundaries in cells of the map it is on.
 */
class AreaPropertiesForm : public QObject
{
	Q_OBJECT
	QML_ELEMENT
	QML_UNCREATABLE("Area Properties is opened by MapTreeDialogs")
	Q_PROPERTY(Json* info READ info CONSTANT)
	Q_PROPERTY(EncounterListModel* encounters READ encounters CONSTANT)
	Q_PROPERTY(int mapWidth READ mapWidth CONSTANT)
	Q_PROPERTY(int mapHeight READ mapHeight CONSTANT)
	/** x, y, width, height in cells; the rectangle is stored with exclusive
	 *  right and bottom edges */
	Q_PROPERTY(QRect area READ area WRITE setArea NOTIFY areaChanged)

public:
	AreaPropertiesForm(ProjectData& project, const lcf::rpg::MapInfo& info, const QSize& map_size,
		QObject* parent = nullptr);

	Json* info() const;
	EncounterListModel* encounters() const;
	int mapWidth() const;
	int mapHeight() const;
	QRect area() const;
	void setArea(const QRect& area);

	/** Define Area Boundaries over the window of from, on the area's map:
	 *  the box dragged, an empty one when cancelled */
	Q_INVOKABLE QRect defineBoundaries(const QRect& area, QQuickItem* from);

	lcf::rpg::MapInfo resultInfo() const;

signals:
	void areaChanged();

private:
	ProjectData& m_project;
	lcf::rpg::MapInfo m_info;
	JsonT<lcf::rpg::MapInfo>* m_infoJson;
	EncounterListModel* m_encounters;
	QSize m_mapSize;
	QRect m_area;
};

/**
 * Generate Dungeon (GenerateDungeonWindow.qml): a new map with the settings
 * of a map using the Random Dungeon Generator, its own tileset and size,
 * and a dungeon, shown as it is generated (preview).
 */
class DungeonForm : public QObject
{
	Q_OBJECT
	QML_ELEMENT
	QML_UNCREATABLE("Generate Dungeon is opened by MapTreeDialogs")
	/** The map the dungeon would be (MapPreview.qml) */
	Q_PROPERTY(MapDocument* preview READ preview CONSTANT)
	Q_PROPERTY(int sourceChipset READ sourceChipset CONSTANT)
	Q_PROPERTY(int sourceWidth READ sourceWidth CONSTANT)
	Q_PROPERTY(int sourceHeight READ sourceHeight CONSTANT)

public:
	DungeonForm(ProjectData& project, const lcf::rpg::Map& source, QObject* parent = nullptr);

	MapDocument* preview() const;
	int sourceChipset() const;
	int sourceWidth() const;
	int sourceHeight() const;

	/** Random by default; fixed by the tests */
	void setSeed(uint32_t seed);
	/** Another dungeon with the same settings */
	Q_INVOKABLE void reseed();

	/**
	 * Generates the dungeon into preview: the source map without its
	 * events, this tileset and size; the entrance and the exit (unless at
	 * 0, 0) kept on the floor and reachable
	 */
	Q_INVOKABLE void generate(int chipset_id, int width, int height, int entrance_x, int entrance_y,
		int exit_x, int exit_y);

	/** The dungeon generated last */
	const lcf::rpg::Map& map() const;

private:
	lcf::rpg::Map m_source;
	uint32_t m_seed;
	MapDocument* m_preview;
	// the settings of the dungeon generated last, for reseed()
	QList<int> m_settings;
};
