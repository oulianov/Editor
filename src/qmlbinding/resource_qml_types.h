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

// The models of the Resource Manager, the Jukebox and the project windows
// in the QML module, registered from here: editor_core does not link QtQml

#include <QObject>
#include <QtQmlIntegration>
#include "common/audio_player.h"
#include "model/audio_library.h"
#include "model/project_list.h"
#include "model/resource_library.h"
#include "model/transparent_color.h"

/** The files of the folders of the game and the RTP (Resource Manager) */
struct ResourceLibraryModelForeign
{
	Q_GADGET
	QML_FOREIGN(ResourceLibraryModel)
	QML_NAMED_ELEMENT(ResourceLibraryModel)
};

struct ResourceFolderModelForeign
{
	Q_GADGET
	QML_FOREIGN(ResourceFolderModel)
	QML_NAMED_ELEMENT(ResourceFolderModel)
	QML_UNCREATABLE("The folders are a library's")
};

/** What a file operation did: ok, cancelled, error, notice, name */
struct ResourceResultForeign
{
	Q_GADGET
	QML_FOREIGN(ResourceResult)
	QML_VALUE_TYPE(resourceResult)
};

/** The music and sounds of the game and the RTP (Jukebox) */
struct AudioLibraryModelForeign
{
	Q_GADGET
	QML_FOREIGN(AudioLibraryModel)
	QML_NAMED_ELEMENT(AudioLibraryModel)
};

struct AudioListModelForeign
{
	Q_GADGET
	QML_FOREIGN(AudioListModel)
	QML_NAMED_ELEMENT(AudioListModel)
	QML_UNCREATABLE("The lists are a library's")
};

/** Plays a music or sound file with RPG Maker's settings */
struct AudioPlayerForeign
{
	Q_GADGET
	QML_FOREIGN(AudioPlayer)
	QML_NAMED_ELEMENT(AudioPlayer)
};

/** The transparent color of an image imported (Import Image) */
struct TransparentColorChoiceForeign
{
	Q_GADGET
	QML_FOREIGN(TransparentColorChoice)
	QML_NAMED_ELEMENT(TransparentColorChoice)
};

/** The projects of the folder of the games (Open Project) */
struct ProjectListModelForeign
{
	Q_GADGET
	QML_FOREIGN(ProjectListModel)
	QML_NAMED_ELEMENT(ProjectListModel)
};
