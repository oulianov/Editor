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
#include <QString>
#include <QtQmlIntegration>

#include "qmlbinding/database_session.h"
#include "qmlbinding/project_data_gadget.h"

class ProjectData;
class QQuickItem;

namespace lcf::rpg {
	class Troop;
	class TroopPage;
}

/**
 * A battle page of a troop for BattlePageEditor.qml: its trigger as the
 * line of RPG Maker ("Switch [0001:xxx] ON, Turns [3x+1]"), and the window
 * that defines it (BattleConditionDialog.qml). The page is found by the
 * index of its troop and its own, in a database window's copy (session)
 * or in the project.
 */
class BattlePage : public QObject
{
	Q_OBJECT
	QML_ELEMENT
	Q_PROPERTY(ProjectDataGadget* projectData READ projectData WRITE setProjectData NOTIFY sourceChanged)
	Q_PROPERTY(DatabaseSession* session READ session WRITE setSession NOTIFY sourceChanged)
	Q_PROPERTY(int troop READ troop WRITE setTroop NOTIFY sourceChanged)
	Q_PROPERTY(int page READ page WRITE setPage NOTIFY sourceChanged)
	/** There is such a page */
	Q_PROPERTY(bool valid READ valid NOTIFY changed)
	Q_PROPERTY(QString triggerText READ triggerText NOTIFY changed)

public:
	explicit BattlePage(QObject* parent = nullptr);

	ProjectDataGadget* projectData() const;
	void setProjectData(ProjectDataGadget* gadget);
	DatabaseSession* session() const;
	void setSession(DatabaseSession* session);
	int troop() const;
	void setTroop(int troop);
	int page() const;
	void setPage(int page);

	bool valid() const;
	QString triggerText() const;

	/** Define Battle Event Triggers, over the window of from; false when cancelled */
	Q_INVOKABLE bool editTrigger(QQuickItem* from);
	/** Reads the page again (the monsters of the troop changed) */
	Q_INVOKABLE void refresh();

signals:
	void sourceChanged();
	void changed();
	/** The trigger was changed */
	void edited();

private:
	ProjectData* project() const;
	lcf::rpg::Troop* findTroop() const;
	lcf::rpg::TroopPage* findPage() const;

	QPointer<ProjectDataGadget> m_gadget;
	QPointer<DatabaseSession> m_session;
	int m_troop = -1;
	int m_page = -1;
};
