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

class ProjectData;
class QQmlApplicationEngine;

namespace QmlEngine {
	/** The engine of the QML interface, made on first use, with the docks of
	 *  KDDockWidgets set up for it; gone with the application */
	QQmlApplicationEngine* instance();

	/** The project the QML interface shows (Ez.ProjectData); nullptr: none */
	void setProject(ProjectData* data);
	/** Its data was read again (another program changed the files): the views read it again */
	void projectReloaded();
}
