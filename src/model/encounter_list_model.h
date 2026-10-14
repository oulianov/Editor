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

#include <QAbstractListModel>
#include <vector>
#include <lcf/rpg/encounter.h>

class ProjectData;

/**
 * The "Enemy Encounters" of Map Properties and Area Properties: the monster
 * groups met there, a line each ("0001: Slime"). Cut, Copy and Paste share
 * one clipboard for the map and area windows, as RPG Maker's.
 */
class EncounterListModel : public QAbstractListModel
{
	Q_OBJECT
	Q_PROPERTY(int count READ count NOTIFY countChanged)
	Q_PROPERTY(bool canPaste READ canPaste NOTIFY clipboardChanged)

public:
	enum Role {
		TroopRole = Qt::UserRole + 1,
	};

	EncounterListModel(const ProjectData& project, QObject* parent = nullptr);

	void setEncounters(const std::vector<lcf::rpg::Encounter>& encounters);
	/** The encounters, their IDs counted from 1 in their order */
	std::vector<lcf::rpg::Encounter> encounters() const;

	int count() const;
	/** The monster group of a line, 0 for none */
	Q_INVOKABLE int troopId(int row) const;
	/** "0001: Slime", the ID of a deleted monster group kept */
	Q_INVOKABLE QString text(int row) const;
	/** The monster groups of the database (none: nothing can be added) */
	Q_INVOKABLE int troopCount() const;

	/** Inserts a line before row (past the end: the last); its row */
	Q_INVOKABLE int insert(int row, int troop_id);
	Q_INVOKABLE void setTroop(int row, int troop_id);
	Q_INVOKABLE void remove(int row);
	Q_INVOKABLE void copy(int row);
	Q_INVOKABLE void cut(int row);
	/** After row, as a paste in a text (-1: at the end); the row of the
	 *  last line pasted, -1 when nothing was */
	Q_INVOKABLE int paste(int row);
	bool canPaste() const;

	// QAbstractListModel
	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	QHash<int, QByteArray> roleNames() const override;

signals:
	void countChanged();
	void clipboardChanged();

private:
	const ProjectData& m_project;
	std::vector<int> m_troops;
};
