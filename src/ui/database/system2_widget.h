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

#include <QWidget>

class ProjectData;

/**
 * The System 2 page of the database: the menu and graphics of RPG Maker 2003
 * (disabled in RPG Maker 2000 games) and the test battle settings.
 * Edits the system of the given project data.
 */
class System2Widget : public QWidget
{
	Q_OBJECT

public:
	explicit System2Widget(ProjectData& project, QWidget *parent = nullptr);
};
