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

#include "search_window_host.h"
#include "common/editor_settings.h"
#include "controller/map_tabs_controller.h"
#include "controller/shell_controller.h"
#include "model/project_data.h"
#include "model/project_search.h"
#include "qmlbinding/qml_host.h"
#include "ui/shell/database_windows.h"
#include "ui/shell/editor_window.h"

#include <QGuiApplication>
#include <QQuickWindow>
#include <QTimer>
#include <algorithm>

SearchWindowHost::SearchWindowHost(ProjectData& project, EditorWindow* window, QObject* parent) :
	QObject(parent), m_project(project), m_main(window), m_search(new ProjectSearch(project, this))
{
	// the maps open in tabs, as they are now
	m_search->setMapSource([this](int mapID) -> std::shared_ptr<const lcf::rpg::Map> {
		const lcf::rpg::Map* map = m_main->openMapData(mapID);
		return map ? std::make_shared<const lcf::rpg::Map>(*map) : nullptr;
	});
	m_search->setCurrentMapSource([this] {
		return m_main->shell()->tabs()->currentMap();
	});
	connect(m_search, &ProjectSearch::openRequested, this, &SearchWindowHost::openResult);
	connect(&EditorSettings::instance(), &EditorSettings::localStateReset, m_search, &ProjectSearch::reset);
}

SearchWindowHost::~SearchWindowHost()
{
	delete m_window.data();
}

void SearchWindowHost::show()
{
	m_search->refresh();
	if (m_window) {
		m_window->show();
		m_window->raise();
		m_window->requestActivate();
		return;
	}
	m_window = QmlHost::open(QStringLiteral("SearchWindow"), m_main->handle(),
		{{QStringLiteral("search"), QVariant::fromValue(m_search)}});
}

void SearchWindowHost::updateUI()
{
	m_search->refresh();
}

void SearchWindowHost::enableCache(bool enable)
{
	m_search->setCacheEnabled(enable);
}

void SearchWindowHost::openResult(int mapID, int eventID, int pageID, int line)
{
	// the window goes; the results stay for the next time
	auto hide = [this] {
		if (m_window) {
			m_window->close();
		}
	};
	// a common event, a monster group: in the database window
	if (mapID == ProjectSearch::CommonEventResults || mapID == ProjectSearch::TroopResults) {
		const bool troop = mapID == ProjectSearch::TroopResults;
		const bool found = troop
			? std::ranges::any_of(m_project.database().troops, [&](const auto& entry) { return entry.ID == eventID; })
			: std::ranges::any_of(m_project.database().commonevents, [&](const auto& entry) { return entry.ID == eventID; });
		if (!found) return;
		hide();
		DatabaseWindows::open(*m_main, {troop ? DatabaseTarget::Page::Troops : DatabaseTarget::Page::CommonEvents,
			eventID, pageID, line});
		return;
	}
	if (!m_main->hasMap(mapID)) return;
	hide();
	// the map, its event selected, then the event at the page and the line
	m_main->openEvent(mapID, eventID, pageID, line);
}
