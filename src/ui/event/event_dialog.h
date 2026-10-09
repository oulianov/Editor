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

#include <QDialog>
#include <QVariantMap>
#include <optional>
#include <lcf/rpg/event.h>
#include <lcf/rpg/database.h>

class ProjectData;

namespace Ui {
class EventDialog;
}

class EventDialog : public QDialog
{
	Q_OBJECT

public:
	explicit EventDialog(ProjectData& project, QWidget *parent = nullptr, int mapID = 0);
	~EventDialog();

	static bool equalEvents(const lcf::rpg::Event &e1,
							const lcf::rpg::Event &e2);

	lcf::rpg::Event getEvent() const;
	void setEvent(const lcf::rpg::Event& event);

	/** Cancel, Apply or Ok: how the dialog was closed */
	int editResult() const;

	/** Whether the user changed the event since it was shown */
	bool isModified() const;

	/** The event was changed by another program (nullptr: it was removed).
	 *  Shows the new version, asking first when the user changed it. */
	void reloadEvent(const lcf::rpg::Event* event);

	/** Shows a page (by its ID) and selects a line of it (1: the first, 0: none) */
	void showCommand(int pageID, int line);

signals:
	/** Apply was clicked: the event as it is now, to put on the map */
	void applied(const lcf::rpg::Event& event);

private slots:
	void apply();

	void ok();

	void on_pushNewPage_clicked();

	void on_pushCopyPage_clicked();

	void on_pushPastePage_clicked();

	void on_pushDeletePage_clicked();

	void on_pushClearPage_clicked();
	void on_tabEventPages_currentChanged(int index);

private:
	Ui::EventDialog *ui;
	lcf::rpg::Event m_event;
	lcf::rpg::Event r_event;
	lcf::rpg::Event a_event;
	int lst_result;
	ProjectData& m_project;
	/** Copy Page: shared by the events of every map, like RPG Maker */
	static std::optional<lcf::rpg::EventPage> s_pageClipboard;
	int m_mapID;
	QString m_viewStateKey;
	std::vector<QVariantMap> m_pageViewStates;
	int m_activePage = -1;
	bool m_refreshingPages = false;

	void refreshEventPageTabs();
	/** New and Paste Page while there are fewer than 100 pages */
	void updatePageButtons();
	/** The previous (-1) or next (1) page */
	void switchPage(int step);
};
