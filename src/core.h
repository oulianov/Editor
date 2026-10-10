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

#include "defines.h"

#ifdef Q_OS_WIN
#define PLAYER "Player.exe"
#else
#define PLAYER "easyrpg-player"
#endif

#include <QHash>
#include <QBrush>
#include <QPixmap>
#include <QTransform>
#include <QPainter>
#include <QListWidget>
#include <QTimer>
#include <optional>
#include <QVariantMap>
#include <QQmlApplicationEngine>
#include <lcf/rpg/map.h>
#include <lcf/rpg/chipset.h>
#include "ui/other/run_game_dialog.h"
#include "model/project.h"
#include "model/project_watcher.h"

class Core : public QObject
{
	Q_OBJECT

public:

	enum Layer
	{
		LOWER,
		UPPER,
		EVENT
	};

	enum Tool
	{
		ZOOM,
		PENCIL,
		RECTANGLE,
		CIRCLE,
		FILL,
		SELECT
	};

	/** A rectangle of map tiles (both layers) copied by the selection tool */
	struct TileClipboard
	{
		int width = 0;
		int height = 0;
		std::vector<short> lower;
		std::vector<short> upper;
	};

	Core();

	static Core* getCore();

	void LoadChipset(int n_chipsetid);
	/** The id of the chipset loaded, 0 for none */
	int chipsetId() const;
	void LoadBackground(QString name);
	/** The panorama drawn behind the lower tiles; each map keeps its own */
	const QPixmap& background() const;
	/** The panorama drawn under empty tiles; transform: where its top-left
	 *  corner is and how much it is zoomed, in the picture painted */
	void setBackground(const QPixmap& background, const QTransform& transform = QTransform());

	int tileSize();
	void setTileSize(int tileSize);

	QString rtpPath(const QString &folder, const QString &filename = QString()) const;

	Layer layer();
	void setLayer(const Layer &layer);

	Tool tool();
	void setTool(const Tool &tool);

	QString gameTitle();
	void setGameTitle(const QString &gameTitle);

	void beginPainting(QPixmap &dest);
	void renderTile(const short &tile_id, const QRect &dest_rect);
	void renderEvent(const lcf::rpg::Event& event, const QRect &dest_rect);
	/** A start position: the charset (facing down) with its label, or the label alone */
	void renderMarker(const QString& charset_name, int charset_index, const QString& label, const QRect &dest_rect);
	void endPainting();

	QColor keycolor();

	short translate(int terrain_id, int _code = 0, int _scode = 0);
	int translate(short tile_id);

	/** Animate water and the animated tiles, as the Player does (on by default) */
	void setAnimationEnabled(bool enabled);
	bool animationEnabled() const;
	/** The frames renderTile draws: water (0-2) and the animated tiles (0-3) */
	int waterFrame() const;
	int animatedTileFrame() const;
	/** Water and animated tiles (lower tiles below 3150) change with the frame */
	static bool isAnimatedTile(short tile_id);

	inline bool chipsetIsNull() {return m_tileCache.value(0).isNull();}
	/** The tiles of a layer (LOWER or UPPER) of the chipset, 6 per row,
	 *  size x size each, in the order of the palette */
	QPixmap paletteTiles(Layer layer, int size = 32);
	QPixmap createDummyPixmap(int width, int height);
	/** Gray squares behind transparent pictures and tiles */
	static QBrush transparencyBrush();
	/** The eraser row of the palette (6 cells of 32 pixels) */
	static QPixmap eraserRow();

	bool isWater(int terrain_id);
	bool isABWater(int terrain_id);
	bool isDWater(int terrain_id);
	bool isAnimation(int terrain_id);
	bool isDblock(int terrain_id);
	bool isEblock(int terrain_id);
	bool isFblock(int terrain_id);

	QString defDir() const;
	void setDefDir(const QString &defDir);

	void setRtpDir(const QString &n_path);

	short selection(int off_x, int off_y, std::optional<Layer> layer = std::nullopt);
	int selWidth();
	int selHeight();
	void setSelection(std::vector<short> n_sel, int n_w, int n_h);
	/** All drawing controls, including the inactive layers' brushes. */
	QVariantMap editingState() const;
	void restoreEditingState(const QVariantMap& state);

	/** The map region copied by the selection tool, shared by all maps */
	const TileClipboard& tileClipboard() const;
	void setTileClipboard(const TileClipboard& clipboard);
	/** The event copied on a map, shared by all maps (nullptr when empty) */
	const lcf::rpg::Event* eventClipboard() const;
	void setEventClipboard(const lcf::rpg::Event& event);

	lcf::rpg::Event *currentMapEvent(int eventID);
	/** The events of the map being edited, by id (nullptr without a map) */
	const QMap<int, lcf::rpg::Event *>* currentMapEvents() const;
	void setCurrentMapEvents(QMap<int, lcf::rpg::Event *> *events);

	void runGame();
	void runGameHere(int map_id, int x, int y);
	void runBattleTest(int troop_id);

	std::shared_ptr<Project>& project();
	const std::shared_ptr<Project>& project() const;

	QQmlApplicationEngine* qmlEngine();

	/** Reports project files changed by other programs */
	ProjectWatcher& watcher();

signals:
	void toolChanged();

	void layerChanged();

	void chipsetChanged();

	/** The tiles of the current layer were picked (palette or eyedropper) */
	void selectionChanged();

	/** The water or the animated tiles show another frame */
	void animationFrameChanged();

private:
	lcf::rpg::Map *m_map;
	lcf::rpg::Chipset m_chipset;
	const Project* m_chipsetProject = nullptr;
	int m_tileSize;
	QPainter m_painter;
	QString m_defDir;
	QString m_rtpDir;
	QColor m_keycolor;
	Layer m_layer;
	Tool m_tool;
	QPixmap m_background;
	QTransform m_backgroundTransform;
	QMap<int, QPixmap> m_tileCache;
	// frames 1 to 3 of the animated tiles; m_tileCache has the frame 0
	QMap<int, QPixmap> m_frameCache[3];
	QTimer* m_animationTimer = nullptr;
	unsigned m_animationTick = 0;
	int m_waterFrame = 0;
	int m_animatedTileFrame = 0;
	void nextAnimationFrame();
	QMap<int, QPixmap> m_eventCache;
	/** Charset images by name: events are redrawn on every scroll */
	QHash<QString, QPixmap> m_charsetCache;
	const Project* m_cacheProject = nullptr;
	QPixmap charsetImage(const QString& name);
	TileClipboard m_tileClipboard;
	lcf::rpg::Event m_eventClipboard;
	bool m_eventClipboardSet = false;
	QMap<int, short> m_dictionary;
	QMap<int, lcf::rpg::Map> m_maps;
	QMap<int, QWidget*> m_mapTabs;
	std::vector<short> m_lowerSel;
	std::vector<short> m_upperSel;
	short m_eventSel;
	int m_lowerSelW;
	int m_lowerSelH;
	int m_upperSelW;
	int m_upperSelH;
	static Core *core_instance;
	QMap<int, lcf::rpg::Event*> *m_currentMapEvents;
	RunGameDialog *m_runGameDialog;
	std::shared_ptr<Project> m_project;
	QQmlApplicationEngine* m_qml_engine = nullptr;
	ProjectWatcher m_watcher;
};

Core& core();
