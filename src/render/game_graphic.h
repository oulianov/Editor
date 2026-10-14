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

#include <QImage>
#include <QString>

class ProjectData;

namespace GameGraphic {
	/**
	 * A picture of the game (Battle, BattleCharSet, Monster...) as the
	 * Player draws it: the first color of its palette transparent. Null
	 * when it has no name or is neither in the game nor the RTP.
	 */
	QImage load(const ProjectData& project, const QString& dir, const QString& name);
}
