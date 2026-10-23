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

#include "editor_state.h"
#include <QVariantList>
#include <algorithm>

EditorState::EditorState(QObject* parent) :
	QObject(parent)
{
}

EditorState::Layer EditorState::layer() const
{
	return m_layer;
}

void EditorState::setLayer(Layer current_layer)
{
	m_layer = current_layer;
	emit layerChanged();
}

EditorState::Tool EditorState::tool() const
{
	return m_tool;
}

void EditorState::setTool(Tool current_tool)
{
	m_tool = current_tool;
	emit toolChanged();
}

short EditorState::selection(int off_x, int off_y, std::optional<Layer> layer) const
{
	short result = 0;
	switch(layer.value_or(m_layer))
	{
	case LOWER:
		off_x %= m_lowerSelW;
		off_y %= m_lowerSelH;
		if (off_x < 0)
			off_x += m_lowerSelW;
		if (off_y < 0)
			off_y += m_lowerSelH;
		result = m_lowerSel[static_cast<size_t>(off_x+off_y*m_lowerSelW)];
		break;
	case UPPER:
		off_x %= m_upperSelW;
		off_y %= m_upperSelH;
		if (off_x < 0)
			off_x += m_upperSelW;
		if (off_y < 0)
			off_y += m_upperSelH;
		result = m_upperSel[static_cast<size_t>(off_x+off_y*m_upperSelW)];
		break;
	case EVENT:
		result = m_eventSel;
		break;
	}
	return result;
}

int EditorState::selWidth() const
{
	int w = 1;
	if (m_layer == LOWER)
		w = m_lowerSelW;
	if (m_layer == UPPER)
		w = m_upperSelW;
	return w;
}

int EditorState::selHeight() const
{
	int h = 1;
	if (m_layer == LOWER)
		h = m_lowerSelH;
	if (m_layer == UPPER)
		h = m_upperSelH;
	return h;
}

void EditorState::setSelection(std::vector<short> n_sel, int n_w, int n_h)
{
	if (static_cast<int>(n_sel.size()) != n_w * n_h)
		return;
	switch(m_layer)
	{
	case LOWER:
		m_lowerSel = n_sel;
		m_lowerSelW = n_w;
		m_lowerSelH = n_h;
		break;
	case UPPER:
		m_upperSel = n_sel;
		m_upperSelW = n_w;
		m_upperSelH = n_h;
		break;
	case EVENT:
		m_eventSel = n_sel[0];
		break;
	}
	emit selectionChanged();
}

QVariantMap EditorState::editingState() const
{
	QVariantMap state{{"layer", m_layer}, {"tool", m_tool}, {"event_tile", m_eventSel}};
	for (const auto layer : {LOWER, UPPER}) {
		const auto& selection = layer == LOWER ? m_lowerSel : m_upperSel;
		QVariantList tiles;
		for (short tile : selection) tiles.append(tile);
		state.insert(layer == LOWER ? "lower" : "upper", QVariantMap{{"tiles", tiles},
			{"width", layer == LOWER ? m_lowerSelW : m_upperSelW}, {"height", layer == LOWER ? m_lowerSelH : m_upperSelH}});
	}
	return state;
}

void EditorState::restoreEditingState(const QVariantMap& state)
{
	m_lowerSel = {0};
	m_upperSel = {10000};
	m_lowerSelW = m_lowerSelH = m_upperSelW = m_upperSelH = 1;
	m_eventSel = static_cast<short>(state.value("event_tile", 10000).toInt());
	m_layer = static_cast<Layer>(std::clamp(state.value("layer", LOWER).toInt(), static_cast<int>(LOWER), static_cast<int>(EVENT)));
	m_tool = static_cast<Tool>(std::clamp(state.value("tool", PENCIL).toInt(), static_cast<int>(ZOOM), static_cast<int>(SELECT)));
	for (const auto layer : {LOWER, UPPER}) {
		QVariantMap brush = state.value(layer == LOWER ? "lower" : "upper").toMap();
		// Migrate workspaces that remembered only the active layer's brush.
		if (brush.isEmpty() && m_layer == layer && state.contains("tiles"))
			brush = {{"tiles", state.value("tiles")}, {"width", state.value("tile_width")}, {"height", state.value("tile_height")}};
		const int width = brush.value("width").toInt(), height = brush.value("height").toInt();
		const QVariantList tiles = brush.value("tiles").toList();
		// tiles picked on the map: as large as a map (500 x 500 in RPG Maker)
		if (width <= 0 || width > 500 || height <= 0 || height > 500 || tiles.size() != width * height) continue;
		auto& selection = layer == LOWER ? m_lowerSel : m_upperSel;
		selection.clear();
		for (const auto& tile : tiles) selection.push_back(static_cast<short>(tile.toInt()));
		(layer == LOWER ? m_lowerSelW : m_upperSelW) = width;
		(layer == LOWER ? m_lowerSelH : m_upperSelH) = height;
	}
	emit layerChanged();
	emit toolChanged();
	emit selectionChanged();
}

const EditorState::TileClipboard& EditorState::tileClipboard() const
{
	return m_tileClipboard;
}

void EditorState::setTileClipboard(const TileClipboard& clipboard)
{
	m_tileClipboard = clipboard;
}

const lcf::rpg::Event* EditorState::eventClipboard() const
{
	return m_eventClipboardSet ? &m_eventClipboard : nullptr;
}

void EditorState::setEventClipboard(const lcf::rpg::Event& event)
{
	m_eventClipboard = event;
	m_eventClipboardSet = true;
}

const QMap<int, lcf::rpg::Event *>* EditorState::currentMapEvents() const
{
	return m_currentMapEvents;
}

lcf::rpg::Event *EditorState::currentMapEvent(int eventID)
{
	lcf::rpg::Event *event = nullptr;
	if (m_currentMapEvents)
		event = m_currentMapEvents->value(eventID);
	if (!event)
	{
		// one shared placeholder instead of a new (leaked) event per call
		static lcf::rpg::Event unknown;
		unknown = lcf::rpg::Event();
		unknown.name = "<?>";
		event = &unknown;
	}
	return event;
}

void EditorState::setCurrentMapEvents(QMap<int, lcf::rpg::Event *> *events)
{
	m_currentMapEvents = events;
}
