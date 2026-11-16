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

// The profile of the game open in the QML module, registered from here:
// editor_core does not link QtQml.

#include <QJSEngine>
#include <QObject>
#include <QtQmlIntegration>
#include "model/game_profile.h"

/** The profile of the game open (GameProfile), the singleton Ez.GameProfile:
 *  Ez.GameProfile.chips, Ez.GameProfile.text */
struct GameProfileForeign
{
	Q_GADGET
	QML_FOREIGN(GameProfile)
	QML_NAMED_ELEMENT(GameProfile)
	QML_SINGLETON

public:
	static GameProfile* create(QQmlEngine*, QJSEngine*)
	{
		// the Editor's, not the engine's
		QJSEngine::setObjectOwnership(&GameProfile::instance(), QJSEngine::CppOwnership);
		return &GameProfile::instance();
	}
};
