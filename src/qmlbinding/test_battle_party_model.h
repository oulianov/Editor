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
#include <QPointer>
#include <QStringList>
#include <QtQmlIntegration>
#include <memory>
#include "model/test_battle.h"

class DatabaseSession;
class ProjectDataGadget;

/**
 * The party of the battle tests for QML (Ez.TestBattlePartyView): a row
 * for each of the 4 members of TestBattleParty, whose rules it follows.
 * The party is that of the database of projectData (the project, as the
 * Run Game window edits it) or, when set, of session (a database window's
 * copy); save() writes it there.
 *
 *   Ez.TestBattlePartyModel { id: party; projectData: Ez.ProjectData }
 *   Ez.TestBattlePartyView { party: party }
 *
 * The choices are given as texts and indexes: setHero(member, index of
 * heroes), setEquipment(member, slot, index of slotChoices[slot]).
 */
class TestBattlePartyModel : public QAbstractListModel
{
	Q_OBJECT
	QML_ELEMENT
	Q_PROPERTY(ProjectDataGadget* projectData READ projectData WRITE setProjectData NOTIFY sourceChanged)
	Q_PROPERTY(DatabaseSession* session READ session WRITE setSession NOTIFY sourceChanged)
	/** The members in the party (0 to 4) */
	Q_PROPERTY(int size READ size WRITE setSize NOTIFY sizeChanged)
	/** The heroes of the database ("0001: Alex"), the choices of a member */
	Q_PROPERTY(QStringList heroes READ heroes NOTIFY sourceChanged)

public:
	enum Role {
		/** The index of the member's hero in heroes, -1 when unknown */
		HeroIndexRole = Qt::UserRole + 1,
		LevelRole,
		MaxLevelRole,
		/** A weapon in the shield slot */
		TwoWeaponsRole,
		/** For each slot (TestBattleParty::Slot), the texts of the items it can hold, "<None>" first */
		SlotChoicesRole,
		/** For each slot, the index of its item in its choices */
		SlotIndexesRole,
		/** The parameters (TestBattleParty::Parameter) at the level, with the equipment */
		ParametersRole,
		/** One of the first size members */
		InPartyRole,
	};

	explicit TestBattlePartyModel(QObject* parent = nullptr);
	~TestBattlePartyModel() override;

	ProjectDataGadget* projectData() const;
	void setProjectData(ProjectDataGadget* project_data);
	DatabaseSession* session() const;
	void setSession(DatabaseSession* session);

	int size() const;
	void setSize(int size);
	QStringList heroes() const;

	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	QHash<int, QByteArray> roleNames() const override;

	Q_INVOKABLE void setHero(int member, int hero_index);
	Q_INVOKABLE void setLevel(int member, int level);
	Q_INVOKABLE void setEquipment(int member, int slot, int choice);
	/** The level and equipment the hero starts the game with */
	Q_INVOKABLE void initialStats(int member);

	/** Reads the party of the database again */
	Q_INVOKABLE void load();
	/** Writes the party to the database */
	Q_INVOKABLE void save();

	/** The party edited, nullptr without a database (C++) */
	TestBattleParty* party() const;

signals:
	void sourceChanged();
	void sizeChanged();
	/** A member, or the size, changed by the user */
	void edited();

private:
	lcf::rpg::Database* database() const;
	/** A party over the database of the source */
	void reset();
	/** The items of each choice of a slot, the item of the member last when it cannot hold it */
	std::vector<int> slotItems(int member, int slot) const;
	void memberChanged(int member);

	QPointer<ProjectDataGadget> m_projectData;
	QPointer<DatabaseSession> m_session;
	std::unique_ptr<TestBattleParty> m_party;
};
