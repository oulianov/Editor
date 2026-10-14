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

#include "troop_editor.h"
#include "model/project_data.h"
#include "model/troop_layout.h"
#include <lcf/rpg/troop.h>
#include <algorithm>
#include <optional>

namespace {
/** The page copied, for every window */
std::optional<lcf::rpg::TroopPage>& pageClipboard()
{
	static std::optional<lcf::rpg::TroopPage> clipboard;
	return clipboard;
}

void renumber(std::vector<lcf::rpg::TroopPage>& pages)
{
	for (size_t i = 0; i < pages.size(); ++i) {
		pages[i].ID = static_cast<int>(i) + 1;
	}
}
}

TroopEditor::TroopEditor(QObject* parent) : QObject(parent)
{
	connect(this, &TroopEditor::sourceChanged, this, &TroopEditor::changed);
}

JsonView* TroopEditor::database() const
{
	return m_database;
}

void TroopEditor::setDatabase(JsonView* database)
{
	if (m_database == database) {
		return;
	}
	disconnect(m_connection);
	m_database = database;
	if (database && database->json()) {
		m_connection = connect(database->json(), &Json::valueChanged, this, &TroopEditor::onDataChanged);
	}
	emit sourceChanged();
}

int TroopEditor::troop() const
{
	return m_troop;
}

void TroopEditor::setTroop(int troop)
{
	if (m_troop != troop) {
		m_troop = troop;
		emit sourceChanged();
	}
}

DatabaseSession* TroopEditor::session() const
{
	return m_database ? DatabaseSession::owning(m_database->json()) : nullptr;
}

lcf::rpg::Troop* TroopEditor::findTroop() const
{
	DatabaseSession* owner = session();
	if (!owner || m_troop < 0 || m_troop >= static_cast<int>(owner->copy().database().troops.size())) {
		return nullptr;
	}
	return &owner->copy().database().troops[static_cast<size_t>(m_troop)];
}

bool TroopEditor::valid() const
{
	return findTroop() != nullptr;
}

int TroopEditor::memberCount() const
{
	const auto* troop = findTroop();
	return troop ? static_cast<int>(troop->members.size()) : 0;
}

int TroopEditor::pageCount() const
{
	const auto* troop = findTroop();
	return troop ? static_cast<int>(troop->pages.size()) : 0;
}

bool TroopEditor::canPastePage() const
{
	return pageClipboard().has_value();
}

void TroopEditor::notify(const QString& path)
{
	if (m_database && m_database->json()) {
		m_database->json()->notifyChanged(QStringLiteral("/troops/%1/%2").arg(m_troop).arg(path));
	}
}

void TroopEditor::onDataChanged(const QString& jsonPtr)
{
	// the group changed elsewhere (a paste, Array Size, a merge)
	if (Json::affects(jsonPtr, QStringLiteral("/troops/%1").arg(m_troop))) {
		emit changed();
		// above the group: its vectors may have moved
		if (!jsonPtr.startsWith(QStringLiteral("/troops/%1/").arg(m_troop))) {
			emit pagesMoved();
		}
	}
}

int TroopEditor::addMember(int enemy_id)
{
	auto* troop = findTroop();
	const int index = troop ? TroopLayout::addMember(*troop, enemy_id) : -1;
	if (index >= 0) {
		notify(QStringLiteral("members"));
	}
	return index;
}

bool TroopEditor::removeMember(int index)
{
	auto* troop = findTroop();
	if (!troop || !TroopLayout::removeMember(*troop, index)) {
		return false;
	}
	notify(QStringLiteral("members"));
	return true;
}

void TroopEditor::removeAllMembers()
{
	if (auto* troop = findTroop()) {
		TroopLayout::removeAllMembers(*troop);
		notify(QStringLiteral("members"));
	}
}

void TroopEditor::autoAlign()
{
	if (auto* troop = findTroop()) {
		TroopLayout::autoAlign(*troop);
		notify(QStringLiteral("members"));
	}
}

bool TroopEditor::memberInvisible(int index) const
{
	const auto* troop = findTroop();
	return troop && index >= 0 && index < static_cast<int>(troop->members.size())
		&& troop->members[static_cast<size_t>(index)].invisible;
}

void TroopEditor::setMemberInvisible(int index, bool invisible)
{
	auto* troop = findTroop();
	if (troop && index >= 0 && index < static_cast<int>(troop->members.size())) {
		TroopLayout::setInvisible(*troop, index, invisible);
		notify(QStringLiteral("members/%1/invisible").arg(index));
	}
}

QString TroopEditor::autoName() const
{
	const auto* troop = findTroop();
	DatabaseSession* owner = session();
	return troop && owner ? TroopLayout::autoName(*troop, owner->copy().database().enemies) : QString();
}

bool TroopEditor::terrainAllowed(int index) const
{
	const auto* troop = findTroop();
	return troop && TroopLayout::terrainAllowed(*troop, index);
}

void TroopEditor::setTerrainAllowed(int index, bool allowed)
{
	auto* troop = findTroop();
	if (!troop || index < 0) {
		return;
	}
	const int terrains = static_cast<int>(session()->copy().database().terrains.size());
	TroopLayout::setTerrainAllowed(*troop, index, allowed, terrains);
	notify(QStringLiteral("terrain_set"));
}

int TroopEditor::pageId(int index) const
{
	const auto* troop = findTroop();
	return troop && index >= 0 && index < static_cast<int>(troop->pages.size()) ? troop->pages[static_cast<size_t>(index)].ID : 0;
}

int TroopEditor::newPage()
{
	auto* troop = findTroop();
	if (!troop) {
		return -1;
	}
	troop->pages.emplace_back();
	renumber(troop->pages);
	notify(QStringLiteral("pages"));
	emit pagesMoved();
	return static_cast<int>(troop->pages.size()) - 1;
}

void TroopEditor::copyPage(int index)
{
	const auto* troop = findTroop();
	if (troop && index >= 0 && index < static_cast<int>(troop->pages.size())) {
		pageClipboard() = troop->pages[static_cast<size_t>(index)];
		emit clipboardChanged();
	}
}

int TroopEditor::pastePage(int index)
{
	auto* troop = findTroop();
	if (!troop || !pageClipboard()) {
		return -1;
	}
	// after the current page
	auto& pages = troop->pages;
	const int at = std::clamp(index + 1, 0, static_cast<int>(pages.size()));
	pages.insert(pages.begin() + at, *pageClipboard());
	renumber(pages);
	notify(QStringLiteral("pages"));
	emit pagesMoved();
	return at;
}

int TroopEditor::deletePage(int index)
{
	auto* troop = findTroop();
	if (!troop || index < 0 || index >= static_cast<int>(troop->pages.size())) {
		return -1;
	}
	// a group keeps one page: the last one is emptied instead
	auto& pages = troop->pages;
	if (pages.size() > 1) {
		pages.erase(pages.begin() + index);
	} else {
		pages[0] = lcf::rpg::TroopPage();
	}
	renumber(pages);
	notify(QStringLiteral("pages"));
	emit pagesMoved();
	return std::min(index, static_cast<int>(pages.size()) - 1);
}
