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

#include <QString>
#include <optional>
#include "model/project_convert.h"

class Project;
class QWindow;

/**
 * The windows of Game Project that choose a project (New, Open, Import,
 * Convert to RPG Maker 2003), in QML, over a window: each waits for its
 * answer and gives what was chosen, nothing when cancelled. Making,
 * opening, copying or converting the project is the caller's.
 */
namespace ProjectWindows {
	/** New Project */
	struct NewProject {
		/** The name of its folder, in the folder of the games */
		QString folder;
		QString title;
		/** Project::Format */
		int format = 0;
		int tileSize = 16;
		/** The folder of the games chosen (ends with "/") */
		QString defDir;
	};
	std::optional<NewProject> newProject(QWindow* over, const QString& def_dir);

	/** Open Project: a project chosen, or Import asked for */
	struct OpenProject {
		enum class Choice { Cancelled, Open, Import };
		Choice choice = Choice::Cancelled;
		QString folder;
		/** The folder of the games shown last that exists (not a path typed that is none), even when cancelled */
		QString defDir;
	};
	/** open_path: the project open in the Editor (not changed from there) */
	OpenProject openProject(QWindow* over, const QString& def_dir, const QString& open_path = QString());

	/** Import Project: an RPG Maker game copied as a new project */
	struct ImportProject {
		bool accepted = false;
		QString sourceFolder;
		/** The name of its folder, in defDir */
		QString folder;
		QString defDir;
		bool convertXyz = false;
	};
	ImportProject importProject(QWindow* over, const QString& def_dir);

	/** Convert to RPG Maker 2003: its options */
	std::optional<ProjectConvert::Options> convertProject(QWindow* over, const Project& project);
	/** What the conversion did: error empty when it worked */
	void convertResult(QWindow* over, const QString& error, const QString& backup);

	/** Project Settings of a project that is not open (Open Project): its errors, empty when done or cancelled */
	QString editSettings(QWindow* over, Project& project);
}
