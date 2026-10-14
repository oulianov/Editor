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

#include <QBrush>
#include <QPainter>
#include <QPixmap>
#include <QRect>
#include <QString>
#include <QTransform>
#include <memory>
#include <lcf/rpg/event.h>
#include "model/editor_state.h"
#include "render/chipset_tiles.h"

/**
 * Draws a map with the tiles of a chipset, on a pixmap: begin(), the
 * tiles, events and start markers, end(). The lower tiles are drawn over
 * the panorama, the water and the animated tiles in the frames set.
 */
class TilePainter
{
public:
	/** The start positions of the party and of the vehicles, in RPG Maker's order */
	enum StartMarker
	{
		PARTY,
		BOAT,
		SHIP,
		AIRSHIP
	};

	explicit TilePainter(std::shared_ptr<const ChipsetTiles> tiles = nullptr);
	TilePainter(const TilePainter&) = delete;
	TilePainter& operator=(const TilePainter&) = delete;

	const std::shared_ptr<const ChipsetTiles>& tiles() const;
	void setTiles(std::shared_ptr<const ChipsetTiles> tiles);

	/** The panorama drawn under the lower tiles */
	const QPixmap& background() const;
	/** Where the top-left corner of the panorama is and how much it is
	 *  zoomed, in the picture painted */
	const QTransform& backgroundTransform() const;
	void setBackground(const QPixmap& background, const QTransform& transform = QTransform());

	/** The frames renderTile draws: water (0-2) and the animated tiles (0-3) */
	int waterFrame() const;
	int animatedTileFrame() const;
	void setFrames(int water_frame, int animated_tile_frame);

	void begin(QPixmap &dest);
	void renderTile(short tile_id, const QRect &dest_rect);
	/** An event: the graphic of its first page in a frame. character: the
	 *  24 x 32 cell of its charset, nullptr when it is not loaded (the
	 *  frame alone) */
	void renderEvent(const lcf::rpg::Event& event, const QRect &dest_rect, const QPixmap* character);
	/** A start position, drawn like RPG Maker 2003's markers on the map, with its letter */
	void renderMarker(StartMarker marker, const QString& letter, const QRect &dest_rect);
	void end();

	/** The tiles of a layer (LOWER, else UPPER) of the chipset, 6 per row,
	 *  size x size each, in the order of the palette */
	QPixmap paletteTiles(EditorState::Layer layer, int size = 32);

	/** Gray squares behind transparent pictures and tiles */
	static QBrush transparencyBrush();
	/** The eraser row of the palette (6 cells of 32 pixels) */
	static QPixmap eraserRow();
	/** Gray squares in place of a picture that is missing */
	static QPixmap createDummyPixmap(int width, int height);

private:
	/** The frame of an event's graphic, also around the start markers; returns the inside */
	QRect renderEventFrame(const QRect &dest_rect);

	std::shared_ptr<const ChipsetTiles> m_tiles;
	QPainter m_painter;
	QPixmap m_background;
	QTransform m_backgroundTransform;
	int m_waterFrame = 0;
	int m_animatedTileFrame = 0;
};
