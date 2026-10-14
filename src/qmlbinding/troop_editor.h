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
#include "qmlbinding/json_view.h"

namespace lcf::rpg {
	class Troop;
}

/**
 * What the Monster Groups page of a database window does to a group in the
 * window's copy, by RPG Maker's rules (TroopLayout): its members (added,
 * removed, aligned, hidden), its name made from them, the terrains where
 * it is met, and its battle pages (new, copied, pasted after the current
 * one, deleted: a group keeps one page). Each change tells the views.
 *
 *   Ez.TroopEditor { id: troop; database: page.database; troop: 0 }
 *   BattlePageEditor { session: troop.session; troopIndex: 0; pageIndex: 0 }
 */
class TroopEditor : public QObject
{
	Q_OBJECT
	QML_ELEMENT
	/** The root of the window's database */
	Q_PROPERTY(JsonView* database READ database WRITE setDatabase NOTIFY sourceChanged)
	/** The index of the group, -1 for none */
	Q_PROPERTY(int troop READ troop WRITE setTroop NOTIFY sourceChanged)
	/** The window's session (its copy), for the battle pages */
	Q_PROPERTY(DatabaseSession* session READ session NOTIFY sourceChanged)
	Q_PROPERTY(bool valid READ valid NOTIFY changed)
	Q_PROPERTY(int memberCount READ memberCount NOTIFY changed)
	Q_PROPERTY(int pageCount READ pageCount NOTIFY changed)
	/** A page was copied */
	Q_PROPERTY(bool canPastePage READ canPastePage NOTIFY clipboardChanged)

public:
	explicit TroopEditor(QObject* parent = nullptr);

	JsonView* database() const;
	void setDatabase(JsonView* database);
	int troop() const;
	void setTroop(int troop);
	DatabaseSession* session() const;
	bool valid() const;
	int memberCount() const;
	int pageCount() const;
	bool canPastePage() const;

	/** Adds a monster in the middle of the screen; its index, -1 when the group is full */
	Q_INVOKABLE int addMember(int enemy_id);
	/** Removes a member (-1: the last one) */
	Q_INVOKABLE bool removeMember(int index);
	Q_INVOKABLE void removeAllMembers();
	Q_INVOKABLE void autoAlign();
	Q_INVOKABLE bool memberInvisible(int index) const;
	Q_INVOKABLE void setMemberInvisible(int index, bool invisible);
	/** RPG Maker's Auto Name: "Slime*2, Bat" */
	Q_INVOKABLE QString autoName() const;

	/** Whether the group is met on a terrain (index from 0) */
	Q_INVOKABLE bool terrainAllowed(int index) const;
	Q_INVOKABLE void setTerrainAllowed(int index, bool allowed);

	/** The ID of a battle page (its tab) */
	Q_INVOKABLE int pageId(int index) const;
	/** A page at the end; its index */
	Q_INVOKABLE int newPage();
	Q_INVOKABLE void copyPage(int index);
	/** The page copied after index; its index, -1 when nothing was */
	Q_INVOKABLE int pastePage(int index);
	/** Deletes a page (the last one left is emptied); the index to show */
	Q_INVOKABLE int deletePage(int index);

signals:
	void sourceChanged();
	/** The group changed (its members, its pages...) */
	void changed();
	/** Its pages moved: the editors of their commands find them again */
	void pagesMoved();
	void clipboardChanged();

private:
	lcf::rpg::Troop* findTroop() const;
	void notify(const QString& path);
	void onDataChanged(const QString& jsonPtr);

	QPointer<JsonView> m_database;
	QMetaObject::Connection m_connection;
	int m_troop = -1;
};
