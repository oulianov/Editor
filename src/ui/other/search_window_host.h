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

#include <QObject>
#include <QPointer>

class EditorWindow;
class ProjectData;
class ProjectSearch;
class QQuickWindow;

/**
 * The Search window (SearchWindow.qml) of the main window: the search of
 * the open project (ProjectSearch), kept between two openings with its
 * results, and the results it opens: the map and its event at the page
 * and line, the common event or the monster group in the database window.
 */
class SearchWindowHost : public QObject
{
	Q_OBJECT

public:
	/** window: the main window, which shows the results (parent: its QObject) */
	SearchWindowHost(ProjectData& project, EditorWindow* window, QObject* parent);
	/** The window closes first: the search is the project's */
	~SearchWindowHost() override;

	/** Shows the window (in front when it is open) */
	void show();
	/** The database may have changed: the suggestions are made again */
	void updateUI();
	/** The Search cache setting */
	void enableCache(bool enable);

private:
	void openResult(int mapID, int eventID, int pageID, int line);

	ProjectData& m_project;
	EditorWindow* m_main;
	ProjectSearch* m_search;
	QPointer<QQuickWindow> m_window;
};
