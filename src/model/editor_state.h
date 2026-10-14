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

#include <QMap>
#include <QObject>
#include <QVariantMap>
#include <optional>
#include <vector>
#include <lcf/rpg/event.h>

/**
 * What the map editing works with, shared by all maps: the layer and the
 * tool, the tiles picked for each layer, the clipboards, and the events of
 * the map being edited.
 */
class EditorState : public QObject
{
	Q_OBJECT
	Q_PROPERTY(Layer layer READ layer WRITE setLayer NOTIFY layerChanged)
	Q_PROPERTY(Tool tool READ tool WRITE setTool NOTIFY toolChanged)

public:
	enum Layer
	{
		LOWER,
		UPPER,
		EVENT
	};
	Q_ENUM(Layer)

	enum Tool
	{
		ZOOM,
		PENCIL,
		RECTANGLE,
		CIRCLE,
		FILL,
		SELECT
	};
	Q_ENUM(Tool)

	/** A rectangle of map tiles (both layers) copied by the selection tool */
	struct TileClipboard
	{
		int width = 0;
		int height = 0;
		std::vector<short> lower;
		std::vector<short> upper;
	};

	explicit EditorState(QObject* parent = nullptr);

	Layer layer() const;
	void setLayer(Layer layer);

	Tool tool() const;
	void setTool(Tool tool);

	/** The tile picked at off_x, off_y of the selection of a layer (the
	 *  current one by default), repeated past its size */
	short selection(int off_x, int off_y, std::optional<Layer> layer = std::nullopt) const;
	int selWidth() const;
	int selHeight() const;
	/** The tiles of the current layer, n_w x n_h of them */
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

	/** An event of the map being edited; a placeholder named <?> when there is none */
	lcf::rpg::Event* currentMapEvent(int eventID);
	/** The events of the map being edited, by id (nullptr without a map) */
	const QMap<int, lcf::rpg::Event *>* currentMapEvents() const;
	void setCurrentMapEvents(QMap<int, lcf::rpg::Event *> *events);

signals:
	void toolChanged();

	void layerChanged();

	/** The tiles of the current layer were picked (palette or eyedropper) */
	void selectionChanged();

private:
	Layer m_layer = LOWER;
	Tool m_tool = PENCIL;
	std::vector<short> m_lowerSel{0};
	std::vector<short> m_upperSel{10000};
	short m_eventSel = 10000;
	int m_lowerSelW = 1;
	int m_lowerSelH = 1;
	int m_upperSelW = 1;
	int m_upperSelH = 1;
	TileClipboard m_tileClipboard;
	lcf::rpg::Event m_eventClipboard;
	bool m_eventClipboardSet = false;
	QMap<int, lcf::rpg::Event*> *m_currentMapEvents = nullptr;
};
