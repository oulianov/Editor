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

#include "battle_page.h"
#include "command_editing.h"
#include "model/event_editor_controller.h"
#include "model/project_data.h"
#include "qmlbinding/database_session.h"
#include "qmlbinding/project_data_gadget.h"
#include "ui/picker/pickers.h"
#include <QQuickItem>
#include <lcf/rpg/troop.h>

BattlePage::BattlePage(QObject* parent) :
	QObject(parent)
{
	connect(this, &BattlePage::sourceChanged, this, &BattlePage::changed);
}

ProjectDataGadget* BattlePage::projectData() const
{
	return m_gadget;
}

void BattlePage::setProjectData(ProjectDataGadget* gadget)
{
	if (m_gadget != gadget) {
		m_gadget = gadget;
		emit sourceChanged();
	}
}

DatabaseSession* BattlePage::session() const
{
	return m_session;
}

void BattlePage::setSession(DatabaseSession* session)
{
	if (m_session != session) {
		m_session = session;
		emit sourceChanged();
	}
}

int BattlePage::troop() const
{
	return m_troop;
}

void BattlePage::setTroop(int troop)
{
	if (m_troop != troop) {
		m_troop = troop;
		emit sourceChanged();
	}
}

int BattlePage::page() const
{
	return m_page;
}

void BattlePage::setPage(int page)
{
	if (m_page != page) {
		m_page = page;
		emit sourceChanged();
	}
}

ProjectData* BattlePage::project() const
{
	if (m_session) {
		return &m_session->copy();
	}
	return m_gadget ? m_gadget->projectData() : nullptr;
}

lcf::rpg::Troop* BattlePage::findTroop() const
{
	ProjectData* data = project();
	if (!data || m_troop < 0 || m_troop >= static_cast<int>(data->database().troops.size())) {
		return nullptr;
	}
	return &data->database().troops[static_cast<size_t>(m_troop)];
}

lcf::rpg::TroopPage* BattlePage::findPage() const
{
	lcf::rpg::Troop* troop = findTroop();
	if (!troop || m_page < 0 || m_page >= static_cast<int>(troop->pages.size())) {
		return nullptr;
	}
	return &troop->pages[static_cast<size_t>(m_page)];
}

bool BattlePage::valid() const
{
	return findPage() != nullptr;
}

QString BattlePage::triggerText() const
{
	const lcf::rpg::TroopPage* page = findPage();
	return page ? BattlePageConditions::text(project()->database(), *findTroop(), page->condition) : QString();
}

bool BattlePage::editTrigger(QQuickItem* from)
{
	lcf::rpg::TroopPage* page = findPage();
	if (!page) {
		return false;
	}
	lcf::rpg::TroopPageCondition condition = page->condition;
	if (!CommandEditing::editBattleTrigger(Pickers::windowOf(from), *project(), *findTroop(), condition)) {
		return false;
	}
	// the page may have gone meanwhile
	page = findPage();
	if (!page) {
		return false;
	}
	page->condition = condition;
	if (m_session) {
		// the copy of the window changed
		m_session->json().notifyChanged(QStringLiteral("/troops/%1/pages/%2/condition").arg(m_troop).arg(m_page));
	}
	emit changed();
	emit edited();
	return true;
}

void BattlePage::refresh()
{
	emit changed();
}
