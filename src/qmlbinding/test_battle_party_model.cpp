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

#include "test_battle_party_model.h"
#include "common/dbstring.h"
#include "database_session.h"
#include "project_data_gadget.h"
#include <QCoreApplication>
#include <algorithm>

// The texts are translated in the context TestBattlePartyWidget, as in
// TestBattlePartyView.qml

namespace {
QString entryText(int id, const lcf::DBString& name) {
	return QStringLiteral("%1: %2").arg(id, 4, 10, QChar('0')).arg(ToQString(name));
}
}

TestBattlePartyModel::TestBattlePartyModel(QObject* parent) :
	QAbstractListModel(parent)
{
}

TestBattlePartyModel::~TestBattlePartyModel() = default;

ProjectDataGadget* TestBattlePartyModel::projectData() const
{
	return m_projectData;
}

void TestBattlePartyModel::setProjectData(ProjectDataGadget* project_data)
{
	if (project_data == m_projectData) {
		return;
	}
	if (m_projectData) {
		disconnect(m_projectData, nullptr, this, nullptr);
	}
	m_projectData = project_data;
	if (m_projectData) {
		// another project opened
		connect(m_projectData, &ProjectDataGadget::projectDataChanged, this, &TestBattlePartyModel::reset);
	}
	reset();
}

DatabaseSession* TestBattlePartyModel::session() const
{
	return m_session;
}

void TestBattlePartyModel::setSession(DatabaseSession* session)
{
	if (session == m_session) {
		return;
	}
	if (m_session) {
		disconnect(m_session, nullptr, this, nullptr);
	}
	m_session = session;
	if (m_session) {
		connect(m_session, &DatabaseSession::projectDataChanged, this, &TestBattlePartyModel::reset);
	}
	reset();
}

lcf::rpg::Database* TestBattlePartyModel::database() const
{
	if (m_session) {
		return &m_session->copy().database();
	}
	ProjectData* data = m_projectData ? m_projectData->projectData() : nullptr;
	return data ? &data->database() : nullptr;
}

void TestBattlePartyModel::reset()
{
	beginResetModel();
	lcf::rpg::Database* db = database();
	m_party = db ? std::make_unique<TestBattleParty>(*db) : nullptr;
	endResetModel();
	emit sourceChanged();
	emit sizeChanged();
}

TestBattleParty* TestBattlePartyModel::party() const
{
	return m_party.get();
}

int TestBattlePartyModel::size() const
{
	return m_party ? m_party->size() : 0;
}

void TestBattlePartyModel::setSize(int size)
{
	if (!m_party || size == m_party->size()) {
		return;
	}
	m_party->setSize(size);
	emit dataChanged(index(0), index(TestBattleParty::MaxMembers - 1), {InPartyRole});
	emit sizeChanged();
	emit edited();
}

QStringList TestBattlePartyModel::heroes() const
{
	QStringList names;
	if (const lcf::rpg::Database* db = database(); db && m_party) {
		for (const auto& actor : db->actors) {
			names << entryText(actor.ID, actor.name);
		}
	}
	return names;
}

int TestBattlePartyModel::rowCount(const QModelIndex& parent) const
{
	return parent.isValid() || !m_party ? 0 : TestBattleParty::MaxMembers;
}

std::vector<int> TestBattlePartyModel::slotItems(int member, int slot) const
{
	std::vector<int> items{0};
	const std::vector<int> equipment = m_party->equipment(member, slot);
	items.insert(items.end(), equipment.begin(), equipment.end());
	const int id = TestBattleParty::slotData(m_party->member(member), slot);
	if (std::find(items.begin(), items.end(), id) == items.end()) {
		items.push_back(id);
	}
	return items;
}

QVariant TestBattlePartyModel::data(const QModelIndex& index, int role) const
{
	if (!m_party || !checkIndex(index, CheckIndexOption::IndexIsValid)) {
		return {};
	}
	const int member = index.row();
	const auto& battler = m_party->member(member);
	switch (role) {
		case HeroIndexRole: {
			const auto& actors = database()->actors;
			const auto found = std::find_if(actors.begin(), actors.end(), [&](const auto& actor) {
				return actor.ID == battler.actor_id;
			});
			return found == actors.end() ? -1 : static_cast<int>(found - actors.begin());
		}
		case LevelRole:
			return battler.level;
		case MaxLevelRole:
			return m_party->maxLevel(member);
		case TwoWeaponsRole:
			return m_party->twoWeapons(member);
		case SlotChoicesRole: {
			const auto& items = database()->items;
			QVariantList choices;
			for (int slot = 0; slot < TestBattleParty::SlotCount; ++slot) {
				QStringList texts;
				for (int id : slotItems(member, slot)) {
					if (id == 0) {
						texts << QCoreApplication::translate("TestBattlePartyWidget", "<None>");
					} else if (id >= 1 && id <= static_cast<int>(items.size())) {
						texts << entryText(id, items[static_cast<size_t>(id) - 1].name);
					} else {
						texts << QCoreApplication::translate("TestBattlePartyWidget", "<%1?>").arg(id);
					}
				}
				choices << texts;
			}
			return choices;
		}
		case SlotIndexesRole: {
			QVariantList indexes;
			for (int slot = 0; slot < TestBattleParty::SlotCount; ++slot) {
				const std::vector<int> items = slotItems(member, slot);
				const auto found = std::find(items.begin(), items.end(), TestBattleParty::slotData(battler, slot));
				indexes << static_cast<int>(found - items.begin());
			}
			return indexes;
		}
		case ParametersRole: {
			QVariantList values;
			for (int value : m_party->parameters(member)) {
				values << value;
			}
			return values;
		}
		case InPartyRole:
			return member < m_party->size();
		default:
			return {};
	}
}

QHash<int, QByteArray> TestBattlePartyModel::roleNames() const
{
	return {
		{HeroIndexRole, "heroIndex"},
		{LevelRole, "level"},
		{MaxLevelRole, "maxLevel"},
		{TwoWeaponsRole, "twoWeapons"},
		{SlotChoicesRole, "slotChoices"},
		{SlotIndexesRole, "slotIndexes"},
		{ParametersRole, "parameters"},
		{InPartyRole, "inParty"},
	};
}

void TestBattlePartyModel::memberChanged(int member)
{
	emit dataChanged(index(member), index(member));
	emit edited();
}

void TestBattlePartyModel::setHero(int member, int hero_index)
{
	const lcf::rpg::Database* db = database();
	if (!m_party || member < 0 || member >= TestBattleParty::MaxMembers
			|| hero_index < 0 || hero_index >= static_cast<int>(db->actors.size())) {
		return;
	}
	const int id = db->actors[static_cast<size_t>(hero_index)].ID;
	if (id != m_party->member(member).actor_id) {
		m_party->setHero(member, id);
		memberChanged(member);
	}
}

void TestBattlePartyModel::setLevel(int member, int level)
{
	if (m_party && member >= 0 && member < TestBattleParty::MaxMembers && level != m_party->member(member).level) {
		m_party->setLevel(member, level);
		memberChanged(member);
	}
}

void TestBattlePartyModel::setEquipment(int member, int slot, int choice)
{
	if (!m_party || member < 0 || member >= TestBattleParty::MaxMembers || slot < 0 || slot >= TestBattleParty::SlotCount) {
		return;
	}
	const std::vector<int> items = slotItems(member, slot);
	if (choice >= 0 && choice < static_cast<int>(items.size())
			&& items[static_cast<size_t>(choice)] != TestBattleParty::slotData(m_party->member(member), slot)) {
		m_party->setEquipment(member, slot, items[static_cast<size_t>(choice)]);
		memberChanged(member);
	}
}

void TestBattlePartyModel::initialStats(int member)
{
	if (m_party && member >= 0 && member < TestBattleParty::MaxMembers) {
		m_party->initialStats(member);
		memberChanged(member);
	}
}

void TestBattlePartyModel::load()
{
	if (m_party) {
		m_party->load();
		emit dataChanged(index(0), index(TestBattleParty::MaxMembers - 1));
		emit sizeChanged();
	}
}

void TestBattlePartyModel::save()
{
	if (m_party) {
		m_party->save();
	}
}
