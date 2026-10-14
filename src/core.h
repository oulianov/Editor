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
#include <QMap>
#include <QTimer>
#include <memory>
#include <optional>
#include <QVariantMap>
#include <lcf/rpg/map.h>
#include <lcf/rpg/chipset.h>
#include "model/autotile.h"
#include "model/editor_state.h"
#include "model/project.h"
#include "model/project_watcher.h"
#include "render/chipset_tiles.h"
#include "render/tile_painter.h"


/**
 * The project open and what the whole Editor shares: its paths, the QML
 * engine, the editing state (EditorState), the tiles of the chipset of the
 * map edited (ChipsetTiles) and the painter drawing with them, and the
 * clock of the animated tiles.
 */
class Core : public QObject
{
	Q_OBJECT

public:
	// the layers, tools and clipboard of the editing state, in EditorState
	using Layer = EditorState::Layer;
	using enum EditorState::Layer;
	using Tool = EditorState::Tool;
	using enum EditorState::Tool;
	using TileClipboard = EditorState::TileClipboard;
	// the start positions drawn on the map, in TilePainter
	using StartMarker = TilePainter::StartMarker;
	using enum TilePainter::StartMarker;

	Core();

	static Core* getCore();

	void LoadChipset(int n_chipsetid);
	/** The id of the chipset loaded, 0 for none */
	int chipsetId() const;
	/** The tiles of the chipset loaded (no tiles without one) */
	const std::shared_ptr<const ChipsetTiles>& chipsetTiles() const;
	/** The tiles of a chipset of the project: those loaded when it is the
	 *  same chipset, else new ones that Core does not keep (map previews) */
	std::shared_ptr<const ChipsetTiles> loadChipsetTiles(int chipset_id);
	void LoadBackground(QString name);
	/** The panorama of this name, as LoadBackground loads it (magenta for none) */
	QPixmap panorama(const QString& name) const;
	/** The panorama drawn behind the lower tiles; each map keeps its own */
	const QPixmap& background() const;
	/** Where background() is drawn: the place and zoom of the last map drawn */
	const QTransform& backgroundTransform() const;
	/** The panorama drawn under empty tiles; transform: where its top-left
	 *  corner is and how much it is zoomed, in the picture painted */
	void setBackground(const QPixmap& background, const QTransform& transform = QTransform());

	int tileSize();
	void setTileSize(int tileSize);

	QString rtpPath(const QString &folder, const QString &filename = QString()) const;

	QString gameTitle();
	void setGameTitle(const QString &gameTitle);

	/** Animate water and the animated tiles, as the Player does (on by default) */
	void setAnimationEnabled(bool enabled);
	bool animationEnabled() const;
	/** The frames renderTile draws: water (0-2) and the animated tiles (0-3) */
	int waterFrame() const;
	int animatedTileFrame() const;

	QString defDir() const;
	void setDefDir(const QString &defDir);

	void setRtpDir(const QString &n_path);

	/** The layer, tool, tiles picked and clipboards, shared by all maps */
	EditorState& editorState();

	/** The events of the map edited, kept by EditorState; also loads their
	 *  graphics, for renderEvent */
	void setCurrentMapEvents(QMap<int, lcf::rpg::Event *> *events);

	std::shared_ptr<Project>& project();
	const std::shared_ptr<Project>& project() const;

	/** Reports project files changed by other programs */
	ProjectWatcher& watcher();


	/*
	 * Forwarders to the new homes of what Core did, for the callers that
	 * still go through core()
	 */

	// moved to Autotile (model/autotile.h)
	short translate(int terrain_id, int _code = 0, int _scode = 0) { return Autotile::translate(terrain_id, _code, _scode); }
	int translate(short tile_id) { return Autotile::translate(tile_id); }
	bool isWater(int terrain_id) { return Autotile::isWater(terrain_id); }
	bool isABWater(int terrain_id) { return Autotile::isABWater(terrain_id); }
	bool isDWater(int terrain_id) { return Autotile::isDWater(terrain_id); }
	bool isAnimation(int terrain_id) { return Autotile::isAnimation(terrain_id); }
	bool isDblock(int terrain_id) { return Autotile::isDblock(terrain_id); }
	bool isEblock(int terrain_id) { return Autotile::isEblock(terrain_id); }
	bool isFblock(int terrain_id) { return Autotile::isFblock(terrain_id); }
	/** Water and animated tiles (lower tiles below 3150) change with the frame */
	static bool isAnimatedTile(short tile_id) { return Autotile::isAnimatedTile(tile_id); }

	// moved to EditorState (model/editor_state.h)
	Layer layer() { return m_editorState.layer(); }
	void setLayer(const Layer &layer) { m_editorState.setLayer(layer); }
	Tool tool() { return m_editorState.tool(); }
	void setTool(const Tool &tool) { m_editorState.setTool(tool); }
	short selection(int off_x, int off_y, std::optional<Layer> layer = std::nullopt) { return m_editorState.selection(off_x, off_y, layer); }
	int selWidth() { return m_editorState.selWidth(); }
	int selHeight() { return m_editorState.selHeight(); }
	void setSelection(std::vector<short> n_sel, int n_w, int n_h) { m_editorState.setSelection(std::move(n_sel), n_w, n_h); }
	/** All drawing controls, including the inactive layers' brushes. */
	QVariantMap editingState() const { return m_editorState.editingState(); }
	void restoreEditingState(const QVariantMap& state) { m_editorState.restoreEditingState(state); }
	/** The map region copied by the selection tool, shared by all maps */
	const TileClipboard& tileClipboard() const { return m_editorState.tileClipboard(); }
	void setTileClipboard(const TileClipboard& clipboard) { m_editorState.setTileClipboard(clipboard); }
	/** The event copied on a map, shared by all maps (nullptr when empty) */
	const lcf::rpg::Event* eventClipboard() const { return m_editorState.eventClipboard(); }
	void setEventClipboard(const lcf::rpg::Event& event) { m_editorState.setEventClipboard(event); }
	lcf::rpg::Event *currentMapEvent(int eventID) { return m_editorState.currentMapEvent(eventID); }
	/** The events of the map being edited, by id (nullptr without a map) */
	const QMap<int, lcf::rpg::Event *>* currentMapEvents() const { return m_editorState.currentMapEvents(); }

	// moved to TilePainter (render/tile_painter.h), drawing with the tiles
	// of the chipset loaded
	void beginPainting(QPixmap &dest) { m_tilePainter.begin(dest); }
	void renderTile(const short &tile_id, const QRect &dest_rect) { m_tilePainter.renderTile(tile_id, dest_rect); }
	void renderEvent(const lcf::rpg::Event& event, const QRect &dest_rect);
	/** A start position, drawn like RPG Maker 2003's markers on the map, with its letter */
	void renderMarker(StartMarker marker, const QString& letter, const QRect &dest_rect) { m_tilePainter.renderMarker(marker, letter, dest_rect); }
	void endPainting() { m_tilePainter.end(); }
	/** The tiles of a layer (LOWER or UPPER) of the chipset, 6 per row,
	 *  size x size each, in the order of the palette */
	QPixmap paletteTiles(Layer layer, int size = 32) { return m_tilePainter.paletteTiles(layer, size); }
	QPixmap createDummyPixmap(int width, int height) { return TilePainter::createDummyPixmap(width, height); }
	/** Gray squares behind transparent pictures and tiles */
	static QBrush transparencyBrush() { return TilePainter::transparencyBrush(); }
	/** The eraser row of the palette (6 cells of 32 pixels) */
	static QPixmap eraserRow() { return TilePainter::eraserRow(); }

	// moved to ChipsetTiles (render/chipset_tiles.h)
	QColor keycolor() { return m_chipsetTiles->keyColor(); }
	inline bool chipsetIsNull() { return m_chipsetTiles->isNull(); }

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
	const Project* m_chipsetProject = nullptr;
	int m_tileSize;
	QString m_defDir;
	QString m_rtpDir;
	EditorState m_editorState;
	std::shared_ptr<const ChipsetTiles> m_chipsetTiles;
	// draws with m_chipsetTiles, over the panorama and in the frames of the
	// animation
	TilePainter m_tilePainter;
	QTimer* m_animationTimer = nullptr;
	unsigned m_animationTick = 0;
	void nextAnimationFrame();
	QMap<int, QPixmap> m_eventCache;
	/** Charset images by name: events are redrawn on every scroll */
	QHash<QString, QPixmap> m_charsetCache;
	const Project* m_cacheProject = nullptr;
	QPixmap charsetImage(const QString& name);
	QMap<int, lcf::rpg::Map> m_maps;
	static Core *core_instance;
	std::shared_ptr<Project> m_project;
	ProjectWatcher m_watcher;
};

Core& core();
