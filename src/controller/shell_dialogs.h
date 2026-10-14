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

#include <QPoint>
#include <QSize>
#include <QString>
#include <functional>
#include <optional>
#include <lcf/rpg/map.h>
#include <lcf/rpg/mapinfo.h>
#include "controller/project_controller.h"
#include "model/project_convert.h"

class Project;

/**
 * The windows the main window's actions open to edit something (a new
 * project, the properties of a map...), as the interface makes them
 * (QmlShellDialogs). Each waits for its
 * answer and gives the data chosen, nothing when cancelled. Questions and
 * messages go through DialogService.
 */
class ShellDialogs
{
public:
	virtual ~ShellDialogs() = default;

	/** New Project */
	struct NewProject {
		/** The name of its folder, in the folder of the projects */
		QString folder;
		QString title;
		/** Project::Format */
		int format = 0;
		int tileSize = 16;
		/** The folder of the projects chosen */
		QString defDir;
	};
	virtual std::optional<NewProject> newProject(const QString& def_dir) = 0;

	/** Open Project: a project chosen, or Import asked */
	struct OpenProject {
		enum class Choice { Cancelled, Open, Import };
		Choice choice = Choice::Cancelled;
		QString folder;
		/** The folder of the projects shown last, even when cancelled */
		QString defDir;
	};
	virtual OpenProject openProject(const QString& def_dir) = 0;

	/** Import Project: an RPG Maker project copied as a new one */
	struct ImportProject {
		bool accepted = false;
		QString sourceFolder;
		/** The name of its folder, in defDir */
		QString folder;
		QString defDir;
		bool convertXyz = false;
	};
	virtual ImportProject importProject(const QString& def_dir) = 0;
	/** work runs while a progress window shows what it says */
	virtual void withImportProgress(const std::function<void(const ProjectController::ImportProgress&)>& work) = 0;

	/** Convert to RPG Maker 2003: its options */
	virtual std::optional<ProjectConvert::Options> convertProject(const Project& project) = 0;
	/** What the conversion did: error empty when it worked */
	virtual void convertResult(const QString& error, const QString& backup) = 0;

	/** Map Properties (of a new map too): false when cancelled */
	virtual bool mapProperties(lcf::rpg::MapInfo& info, lcf::rpg::Map& map) = 0;
	/** Create Area and Area Properties, on a map of map_size cells */
	virtual bool areaProperties(lcf::rpg::MapInfo& info, const QSize& map_size) = 0;
	/** Generate Dungeon: the new map and its name, from source */
	struct Dungeon {
		QString name;
		lcf::rpg::Map map;
	};
	virtual std::optional<Dungeon> generateDungeon(const lcf::rpg::Map& source, const QString& name) = 0;
	/** Shift Map: by how many cells */
	virtual std::optional<QPoint> shiftMap() = 0;
};
