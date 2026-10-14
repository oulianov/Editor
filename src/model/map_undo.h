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
#include <QUndoCommand>
#include <lcf/rpg/event.h>
#include <lcf/rpg/map.h>
#include <lcf/rpg/mapinfo.h>
#include <lcf/rpg/start.h>
#include "map_document.h"

/**
 * A change of the map, pushed once it is made. It keeps the other version of
 * what it changed: undo and redo both exchange it with the map's.
 */
class SwapCommand : public QUndoCommand
{
public:
	using QUndoCommand::QUndoCommand;

	void undo() override { swap(); }
	// the push redoes it: the change is already made
	void redo() override { if (m_pushed) swap(); m_pushed = true; }

protected:
	virtual void swap() = 0;

private:
	bool m_pushed = false;
};

/** A layer of tiles */
class UndoDraw : public SwapCommand
{
public:
	UndoDraw(MapDocument::Layer layer, std::vector<short> before, MapDocument *document);

protected:
	void swap() override;

private:
	MapDocument::Layer m_layer;
	std::vector<short> m_data;
	MapDocument* m_document;
};

/** The whole map and its info (Map Properties, Shift Map) */
class UndoMap : public SwapCommand
{
public:
	UndoMap(const lcf::rpg::Map &before, const lcf::rpg::MapInfo &info_before, MapDocument *document);

protected:
	void swap() override;

private:
	lcf::rpg::Map m_map;
	lcf::rpg::MapInfo m_info;
	MapDocument* m_document;
};

/** The start positions of the party and vehicles (they are in the map tree) */
class UndoStart : public SwapCommand
{
public:
	UndoStart(const lcf::rpg::Start &before, MapDocument *document);

protected:
	void swap() override;

private:
	lcf::rpg::Start m_start;
	MapDocument* m_document;
};

/** An event added, changed or removed */
class UndoEvent : public SwapCommand
{
public:
	/** before: the event as it was, none when it was added */
	UndoEvent(int id, std::optional<lcf::rpg::Event> before, MapDocument *document);

protected:
	void swap() override;

private:
	int m_id;
	std::optional<lcf::rpg::Event> m_data;
	MapDocument* m_document;
};
