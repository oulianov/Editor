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

#include "event_editor_controller.h"
#include <QCoreApplication>
#include <QVariantMap>
#include <algorithm>
#include <lcf/rpg/database.h>
#include "common/dbstring.h"
#include "model/event_page_defaults.h"

namespace {
using Page = lcf::rpg::EventPage;

QVariantMap choice(const QString& text, int value)
{
	return {{QStringLiteral("text"), text}, {QStringLiteral("value"), value}};
}

/** The texts of the lists are the event window's (context EventPageWidget) */
QString pageText(const char* text)
{
	return QCoreApplication::translate("EventPageWidget", text);
}

/** The texts of the battle triggers are their window's */
QString conditionText(const char* text)
{
	return QCoreApplication::translate("BattlePageConditionDialog", text);
}

template<typename T>
QString nameOf(const std::vector<T>& list, int id)
{
	if (id < 1 || id > static_cast<int>(list.size())) {
		return QStringLiteral("<%1?>").arg(id);
	}
	return ToQString(list[id - 1].name);
}

/** Every a turns from turn b: the forms of RPG Maker, only the parts that are set */
QString turnsText(int a, int b)
{
	if (a == 0) {
		return QString::number(b);
	} else if (b == 0) {
		return QStringLiteral("%1x").arg(a);
	}
	return QStringLiteral("%1x+%2").arg(a).arg(b);
}
}

/*
 * EventPageSettings
 */

EventPageSettings::EventPageSettings(QObject* parent) :
	QObject(parent)
{
}

template<typename T, typename V>
void EventPageSettings::set(T& field, V value)
{
	if (field == static_cast<T>(value)) {
		return;
	}
	field = static_cast<T>(value);
	emit changed();
}

void EventPageSettings::setPage(lcf::rpg::EventPage* page)
{
	m_page = page;
	emit changed();
}

lcf::rpg::EventPage* EventPageSettings::page() const
{
	return m_page;
}

bool EventPageSettings::hasPage() const
{
	return m_page != nullptr;
}

bool EventPageSettings::rpg2k3() const
{
	return m_rpg2k3;
}

void EventPageSettings::setRpg2k3(bool rpg2k3)
{
	if (m_rpg2k3 != rpg2k3) {
		m_rpg2k3 = rpg2k3;
		emit changed();
	}
}

// Without a page the fields read as a new page's and are not written
#define PAGE_FIELD(type, getter, setter, field) \
	type EventPageSettings::getter() const { return m_page ? static_cast<type>(m_page->field) : static_cast<type>(Page().field); } \
	void EventPageSettings::setter(type value) { if (m_page) set(m_page->field, value); }

PAGE_FIELD(bool, switchA, setSwitchA, condition.flags.switch_a)
PAGE_FIELD(int, switchAId, setSwitchAId, condition.switch_a_id)
PAGE_FIELD(bool, switchB, setSwitchB, condition.flags.switch_b)
PAGE_FIELD(int, switchBId, setSwitchBId, condition.switch_b_id)
PAGE_FIELD(bool, variable, setVariable, condition.flags.variable)
PAGE_FIELD(int, variableId, setVariableId, condition.variable_id)
PAGE_FIELD(int, compareOperator, setCompareOperator, condition.compare_operator)
PAGE_FIELD(int, variableValue, setVariableValue, condition.variable_value)
PAGE_FIELD(bool, item, setItem, condition.flags.item)
PAGE_FIELD(int, itemId, setItemId, condition.item_id)
PAGE_FIELD(bool, actor, setActor, condition.flags.actor)
PAGE_FIELD(int, actorId, setActorId, condition.actor_id)
PAGE_FIELD(bool, timer, setTimer, condition.flags.timer)
PAGE_FIELD(int, timerSeconds, setTimerSeconds, condition.timer_sec)
PAGE_FIELD(bool, timer2, setTimer2, condition.flags.timer2)
PAGE_FIELD(int, timer2Seconds, setTimer2Seconds, condition.timer2_sec)
PAGE_FIELD(bool, translucent, setTranslucent, translucent)
PAGE_FIELD(int, moveType, setMoveType, move_type)
PAGE_FIELD(int, moveFrequency, setMoveFrequency, move_frequency)
PAGE_FIELD(int, moveSpeed, setMoveSpeed, move_speed)
PAGE_FIELD(int, trigger, setTrigger, trigger)
PAGE_FIELD(int, layer, setLayer, layer)
PAGE_FIELD(bool, overlapForbidden, setOverlapForbidden, overlap_forbidden)
PAGE_FIELD(int, animationType, setAnimationType, animation_type)

#undef PAGE_FIELD

QString EventPageSettings::characterName() const
{
	return m_page ? ToQString(m_page->character_name) : QString();
}

int EventPageSettings::characterIndex() const
{
	return m_page ? m_page->character_index : 0;
}

int EventPageSettings::direction() const
{
	return m_page ? m_page->character_direction : Page().character_direction;
}

int EventPageSettings::pattern() const
{
	return m_page ? m_page->character_pattern : Page().character_pattern;
}

bool EventPageSettings::hasCharacter() const
{
	return m_page && !m_page->character_name.empty();
}

bool EventPageSettings::isTile() const
{
	return m_page && m_page->character_name.empty() && m_page->character_index > 0;
}

void EventPageSettings::setCharacter(const QString& name, int index, int direction, int pattern)
{
	if (!m_page) {
		return;
	}
	const bool was_character = hasCharacter();
	m_page->character_name = ToDBString(name);
	m_page->character_index = index;
	m_page->character_direction = direction;
	m_page->character_pattern = pattern;
	// RPG Maker's rule: a character goes to the hero's layer, a tile below
	// it, and the animation stops
	EventPageDefaults::graphicChanged(*m_page, was_character);
	emit changed();
}

void EventPageSettings::setTile(int tile, int passable)
{
	if (!m_page) {
		return;
	}
	const bool was_character = hasCharacter();
	m_page->character_name = lcf::DBString();
	m_page->character_index = tile;
	// the layer of what the tile is in the chipset: above the hero (star),
	// blocking (the hero's layer), or walked on (below)
	if (passable >= 0) {
		m_page->layer = EventPageDefaults::tileLayer(passable);
	}
	EventPageDefaults::graphicChanged(*m_page, was_character);
	emit changed();
}

void EventPageSettings::clearGraphic()
{
	if (!m_page) {
		return;
	}
	const bool was_character = hasCharacter();
	m_page->character_name = lcf::DBString();
	m_page->character_index = 0;
	EventPageDefaults::graphicChanged(*m_page, was_character);
	emit changed();
}

int EventPageSettings::upperTileFlags(const std::vector<lcf::rpg::Chipset>& chipsets, int chipset_id, int tile)
{
	if (chipset_id <= 0 || chipset_id > static_cast<int>(chipsets.size())) {
		return -1;
	}
	const auto& passable = chipsets[chipset_id - 1].passable_data_upper;
	return tile >= 0 && tile < static_cast<int>(passable.size()) ? passable[tile] : EventPageDefaults::AllDirections;
}

lcf::rpg::MoveRoute EventPageSettings::moveRoute() const
{
	return m_page ? m_page->move_route : lcf::rpg::MoveRoute();
}

void EventPageSettings::setMoveRoute(const lcf::rpg::MoveRoute& route)
{
	if (m_page && m_page->move_route != route) {
		m_page->move_route = route;
		emit changed();
	}
}

int EventPageSettings::variableMinimum() const
{
	return -variableMaximum();
}

int EventPageSettings::variableMaximum() const
{
	return m_rpg2k3 ? 9999999 : 999999;
}

bool EventPageSettings::compareOperatorEnabled() const
{
	return m_rpg2k3 && variable();
}

bool EventPageSettings::timer2Allowed() const
{
	return m_rpg2k3;
}

bool EventPageSettings::moveFrequencyEnabled() const
{
	return moveType() != Page::MoveType_stationary;
}

bool EventPageSettings::moveRouteEnabled() const
{
	return moveType() == Page::MoveType_custom;
}

bool EventPageSettings::animationTypeEnabled() const
{
	return hasCharacter();
}

bool EventPageSettings::animates() const
{
	const int type = animationType();
	return hasCharacter() && (type == Page::AnimType_continuous || type == Page::AnimType_fixed_continuous
		|| type == Page::AnimType_spin);
}

int EventPageSettings::stepFrames(int move_speed)
{
	static constexpr int steps[] = {16, 12, 10, 8, 7, 6};
	return steps[std::clamp(move_speed, 1, 6) - 1];
}

int EventPageSettings::turnFrames(int move_speed)
{
	static constexpr int turns[] = {24, 16, 12, 8, 6, 4};
	return turns[std::clamp(move_speed, 1, 6) - 1];
}

QVariantList EventPageSettings::items(List list)
{
	using Condition = lcf::rpg::EventPageCondition;
	switch (list) {
		case CompareOperators:
			return {
				choice(pageText(QT_TRANSLATE_NOOP("EventPageWidget", "Equal to")) + " (==)", Condition::Comparison_equal),
				choice(pageText(QT_TRANSLATE_NOOP("EventPageWidget", "Greater or equal to")) + " (>=)", Condition::Comparison_greater_equal),
				choice(pageText(QT_TRANSLATE_NOOP("EventPageWidget", "Lesser or equal to")) + " (<=)", Condition::Comparison_less_equal),
				choice(pageText(QT_TRANSLATE_NOOP("EventPageWidget", "Greater than")) + " (>)", Condition::Comparison_greater),
				choice(pageText(QT_TRANSLATE_NOOP("EventPageWidget", "Lesser than")) + " (<)", Condition::Comparison_less),
				choice(pageText(QT_TRANSLATE_NOOP("EventPageWidget", "Not equal to")) + " (!=)", Condition::Comparison_not_equal),
			};
		case MoveTypes:
			return {
				choice(pageText(QT_TRANSLATE_NOOP("EventPageWidget", "Stationary")), Page::MoveType_stationary),
				choice(pageText(QT_TRANSLATE_NOOP("EventPageWidget", "Random")), Page::MoveType_random),
				choice(pageText(QT_TRANSLATE_NOOP("EventPageWidget", "Vertical")), Page::MoveType_vertical),
				choice(pageText(QT_TRANSLATE_NOOP("EventPageWidget", "Horizontal")), Page::MoveType_horizontal),
				choice(pageText(QT_TRANSLATE_NOOP("EventPageWidget", "Toward Hero")), Page::MoveType_toward),
				choice(pageText(QT_TRANSLATE_NOOP("EventPageWidget", "Away from Hero")), Page::MoveType_away),
				choice(pageText(QT_TRANSLATE_NOOP("EventPageWidget", "Custom Pattern")), Page::MoveType_custom),
			};
		case MoveFrequencies: {
			QVariantList frequencies;
			for (int i = 1; i <= 8; i++) {
				frequencies.append(choice(QString::number(i), i));
			}
			return frequencies;
		}
		case Triggers:
			return {
				choice(pageText(QT_TRANSLATE_NOOP("EventPageWidget", "Action Key")), Page::Trigger_action),
				choice(pageText(QT_TRANSLATE_NOOP("EventPageWidget", "Touched by Hero")), Page::Trigger_touched),
				choice(pageText(QT_TRANSLATE_NOOP("EventPageWidget", "Collision with Hero")), Page::Trigger_collision),
				choice(pageText(QT_TRANSLATE_NOOP("EventPageWidget", "AutoStart")), Page::Trigger_auto_start),
				choice(pageText(QT_TRANSLATE_NOOP("EventPageWidget", "Parallel Process")), Page::Trigger_parallel),
			};
		case Layers:
			return {
				choice(pageText(QT_TRANSLATE_NOOP("EventPageWidget", "Below Hero")), Page::Layers_below),
				choice(pageText(QT_TRANSLATE_NOOP("EventPageWidget", "Same as Hero")), Page::Layers_same),
				choice(pageText(QT_TRANSLATE_NOOP("EventPageWidget", "Above Hero")), Page::Layers_above),
			};
		case AnimationTypes:
			return {
				choice(pageText(QT_TRANSLATE_NOOP("EventPageWidget", "Non-Continuous")), Page::AnimType_non_continuous),
				choice(pageText(QT_TRANSLATE_NOOP("EventPageWidget", "Continuous")), Page::AnimType_continuous),
				choice(pageText(QT_TRANSLATE_NOOP("EventPageWidget", "Fixed Dir/Non-Continuous")), Page::AnimType_fixed_non_continuous),
				choice(pageText(QT_TRANSLATE_NOOP("EventPageWidget", "Fixed Dir/Continuous")), Page::AnimType_fixed_continuous),
				choice(pageText(QT_TRANSLATE_NOOP("EventPageWidget", "Fixed Graphic")), Page::AnimType_fixed_graphic),
				choice(pageText(QT_TRANSLATE_NOOP("EventPageWidget", "Spin Around")), Page::AnimType_spin),
			};
		case MoveSpeeds:
			return {
				choice(pageText(QT_TRANSLATE_NOOP("EventPageWidget", "1: (1/8) Normal")), Page::MoveSpeed_eighth),
				choice(pageText(QT_TRANSLATE_NOOP("EventPageWidget", "2: (1/4) Normal")), Page::MoveSpeed_quarter),
				choice(pageText(QT_TRANSLATE_NOOP("EventPageWidget", "3: (1/2) Normal")), Page::MoveSpeed_half),
				choice(pageText(QT_TRANSLATE_NOOP("EventPageWidget", "4: Normal")), Page::MoveSpeed_normal),
				choice(pageText(QT_TRANSLATE_NOOP("EventPageWidget", "5: Normal x 2")), Page::MoveSpeed_double),
				choice(pageText(QT_TRANSLATE_NOOP("EventPageWidget", "6: Normal x 4")), Page::MoveSpeed_fourfold),
			};
	}
	return {};
}

/*
 * EventEditorController
 */

std::optional<lcf::rpg::EventPage> EventEditorController::s_pageClipboard;

EventEditorController::EventEditorController(int map_id, QObject* parent) :
	QObject(parent), m_mapId(map_id), m_pageSettings(new EventPageSettings(this))
{
	// the settings of the page shown are edits of the event
	connect(m_pageSettings, &EventPageSettings::changed, this, &EventEditorController::edited);
}

EventEditorController::~EventEditorController() = default;

int EventEditorController::mapId() const
{
	return m_mapId;
}

void EventEditorController::setMapId(int map_id)
{
	if (m_mapId == map_id) {
		return;
	}
	m_mapId = map_id;
	if (m_lock.isHeld()) {
		m_lock = EditLocks::Lock({EditLock::Event, m_mapId});
	}
	emit mapIdChanged();
}

bool EventEditorController::rpg2k3() const
{
	return m_rpg2k3;
}

void EventEditorController::setRpg2k3(bool rpg2k3)
{
	if (m_rpg2k3 == rpg2k3) {
		return;
	}
	m_rpg2k3 = rpg2k3;
	m_pageSettings->setRpg2k3(rpg2k3);
	emit rpg2k3Changed();
}

void EventEditorController::setEvent(const lcf::rpg::Event& event, bool is_new)
{
	m_pendingReload.reset();
	m_new = is_new;
	m_event = event;
	for (size_t i = 0; i < m_event.pages.size(); ++i) {
		m_event.pages[i].ID = static_cast<int>(i + 1);
	}
	m_reference = m_event;
	m_applied = m_event;
	setResult(Cancel);
	m_currentPage = m_event.pages.empty() ? -1 : std::clamp(m_currentPage, 0, pageCount() - 1);
	updatePageSettings();
	emit eventChanged();
	emit nameChanged();
	emit pagesChanged();
	emit currentPageChanged(m_currentPage);
	emit pageActionsChanged();
	emit edited();
}

bool EventEditorController::isNewEvent() const
{
	return m_new;
}

const lcf::rpg::Event& EventEditorController::event() const
{
	return m_event;
}

lcf::rpg::Event EventEditorController::resultEvent() const
{
	switch (m_result) {
		case Apply:
			return m_applied;
		case Ok:
			return m_event;
		case Cancel:
			break;
	}
	return m_reference;
}

int EventEditorController::eventId() const
{
	return m_event.ID;
}

QString EventEditorController::title() const
{
	return QStringLiteral("EV: %1").arg(m_event.ID);
}

int EventEditorController::x() const
{
	return m_event.x;
}

int EventEditorController::y() const
{
	return m_event.y;
}

QString EventEditorController::name() const
{
	return ToQString(m_event.name);
}

void EventEditorController::setName(const QString& name)
{
	const lcf::DBString value = ToDBString(name);
	if (m_event.name == value) {
		return;
	}
	m_event.name = value;
	emit nameChanged();
	emit edited();
}

int EventEditorController::pageCount() const
{
	return static_cast<int>(m_event.pages.size());
}

int EventEditorController::currentPage() const
{
	return m_currentPage;
}

void EventEditorController::setCurrentPage(int index)
{
	index = m_event.pages.empty() ? -1 : std::clamp(index, 0, pageCount() - 1);
	if (m_currentPage == index) {
		return;
	}
	m_currentPage = index;
	updatePageSettings();
	emit currentPageChanged(m_currentPage);
}

void EventEditorController::switchPage(int step)
{
	if (!m_event.pages.empty()) {
		setCurrentPage(m_currentPage + step);
	}
}

lcf::rpg::EventPage* EventEditorController::page(int index)
{
	return index >= 0 && index < pageCount() ? &m_event.pages[index] : nullptr;
}

std::vector<lcf::rpg::EventCommand>* EventEditorController::commands(int index)
{
	auto* p = page(index);
	return p ? &p->event_commands : nullptr;
}

EventPageSettings* EventEditorController::pageSettings() const
{
	return m_pageSettings;
}

QStringList EventEditorController::scriptLocation(int index) const
{
	return {QStringLiteral("--map"), QString::number(m_mapId), QStringLiteral("--event"),
		QString::number(m_event.ID), QStringLiteral("--page"), QString::number(index + 1)};
}

bool EventEditorController::canNewPage() const
{
	return pageCount() < MaxPages;
}

bool EventEditorController::canPastePage() const
{
	return canNewPage() && s_pageClipboard.has_value();
}

bool EventEditorController::canDeletePage() const
{
	return pageCount() >= 2;
}

int EventEditorController::newPage()
{
	if (!canNewPage()) {
		return -1;
	}
	const int shown = page(m_currentPage) ? m_currentPage : -1;
	// as RPG Maker: the look and movement of the page shown, new conditions
	m_event.pages.insert(m_event.pages.begin() + shown + 1,
		shown >= 0 ? EventPageDefaults::nextPage(m_event.pages[shown]) : EventPageDefaults::newPage());
	pagesEdited(shown + 1);
	return shown + 1;
}

bool EventEditorController::copyPage()
{
	const auto* shown = page(m_currentPage);
	if (!shown) {
		return false;
	}
	s_pageClipboard = *shown;
	emit pageActionsChanged();
	return true;
}

int EventEditorController::pastePage()
{
	if (!canPastePage()) {
		return -1;
	}
	const int index = page(m_currentPage) ? m_currentPage + 1 : 0;
	m_event.pages.insert(m_event.pages.begin() + index, *s_pageClipboard);
	pagesEdited(index);
	return index;
}

int EventEditorController::deletePage()
{
	if (!canDeletePage() || !page(m_currentPage)) {
		return -1;
	}
	const int index = m_currentPage;
	m_event.pages.erase(m_event.pages.begin() + index);
	// the page that took its place, as RPG Maker (the last one at the end)
	pagesEdited(std::min(index, pageCount() - 1));
	return m_currentPage;
}

int EventEditorController::clearPage()
{
	auto* shown = page(m_currentPage);
	if (!shown) {
		return -1;
	}
	lcf::rpg::EventPage empty = EventPageDefaults::newPage();
	empty.ID = shown->ID;
	*shown = empty;
	pagesEdited(m_currentPage);
	return m_currentPage;
}

bool EventEditorController::hasPageClipboard()
{
	return s_pageClipboard.has_value();
}

void EventEditorController::pagesEdited(int current)
{
	// numbered in order, as RPG Maker saves them (new and pasted pages too)
	for (size_t i = 0; i < m_event.pages.size(); ++i) {
		m_event.pages[i].ID = static_cast<int>(i + 1);
	}
	m_currentPage = m_event.pages.empty() ? -1 : std::clamp(current, 0, pageCount() - 1);
	updatePageSettings();
	emit pagesChanged();
	emit currentPageChanged(m_currentPage);
	emit pageActionsChanged();
	emit edited();
}

void EventEditorController::updatePageSettings()
{
	m_pageSettings->setPage(page(m_currentPage));
}

EventEditorController::Result EventEditorController::result() const
{
	return m_result;
}

void EventEditorController::setResult(Result result)
{
	if (m_result != result) {
		m_result = result;
		emit resultChanged();
	}
}

bool EventEditorController::isModified() const
{
	return m_new || !equalEvents(m_event, m_reference);
}

void EventEditorController::resetModified()
{
	m_reference = m_event;
	emit edited();
}

void EventEditorController::markEdited()
{
	emit edited();
}

bool EventEditorController::apply()
{
	if (!isModified()) {
		return false;
	}
	// Put it on the map now, like RPG Maker; later changes compare to it
	m_applied = m_event;
	m_reference = m_applied;
	m_new = false;
	setResult(Apply);
	emit applied(m_applied);
	emit edited();
	return true;
}

EventEditorController::Result EventEditorController::ok()
{
	// a new event is put on the map unchanged too, like RPG Maker
	if (!isModified()) {
		setResult(Cancel);
	} else {
		setResult(Ok);
		emit accepted(m_event);
	}
	return m_result;
}

EventEditorController::Reload EventEditorController::reloadEvent(const lcf::rpg::Event* event)
{
	if (!event) {
		m_pendingReload.reset();
		setResult(Cancel);
		emit reloadPendingChanged();
		emit removed();
		return Removed;
	}
	m_pendingReload = *event;
	if (isModified()) {
		emit reloadPendingChanged();
		return AskFirst;
	}
	acceptReload();
	return Reloaded;
}

bool EventEditorController::isReloadPending() const
{
	return m_pendingReload.has_value();
}

void EventEditorController::acceptReload()
{
	if (!m_pendingReload) {
		return;
	}
	const int shown = m_currentPage;
	const lcf::rpg::Event event = std::move(*m_pendingReload);
	m_pendingReload.reset();
	// what was applied is replaced too: the map has the new version
	setEvent(event);
	setCurrentPage(std::min(shown, pageCount() - 1));
	emit reloadPendingChanged();
}

void EventEditorController::rejectReload()
{
	if (m_pendingReload) {
		m_pendingReload.reset();
		emit reloadPendingChanged();
	}
}

bool EventEditorController::isOpen() const
{
	return m_lock.isHeld();
}

void EventEditorController::setOpen(bool open)
{
	if (open == isOpen()) {
		return;
	}
	if (open) {
		m_lock = EditLocks::Lock({EditLock::Event, m_mapId});
	} else {
		m_lock.release();
	}
	emit openChanged();
}

bool EventEditorController::equalEvents(const lcf::rpg::Event& e1, const lcf::rpg::Event& e2)
{
#define chk(property) if (e1.property != e2.property) return false
	if (e1.ID != e2.ID) return false; //Prevents warning
	chk(name);
	chk(x);
	chk(y);
	chk(pages.size());
	for (unsigned int i = 0; i < e1.pages.size(); i++)
	{
		chk(pages[i].condition.flags.switch_a);
		chk(pages[i].condition.flags.switch_b);
		chk(pages[i].condition.flags.variable);
		chk(pages[i].condition.flags.item);
		chk(pages[i].condition.flags.actor);
		chk(pages[i].condition.flags.timer);
		chk(pages[i].condition.flags.timer2);
		chk(pages[i].condition.switch_a_id);
		chk(pages[i].condition.switch_b_id);
		chk(pages[i].condition.variable_id);
		chk(pages[i].condition.variable_value);
		chk(pages[i].condition.item_id);
		chk(pages[i].condition.actor_id);
		chk(pages[i].condition.timer_sec);
		chk(pages[i].condition.timer2_sec);
		chk(pages[i].condition.compare_operator);
		chk(pages[i].character_name);
		chk(pages[i].character_index);
		chk(pages[i].character_direction);
		chk(pages[i].character_pattern);
		chk(pages[i].translucent);
		chk(pages[i].move_type);
		chk(pages[i].move_frequency);
		chk(pages[i].trigger);
		chk(pages[i].layer);
		chk(pages[i].overlap_forbidden);
		chk(pages[i].animation_type);
		chk(pages[i].move_speed);
		chk(pages[i].move_route.repeat);
		chk(pages[i].move_route.skippable);
		chk(pages[i].move_route.move_commands.size());
		for (unsigned int j = 0; j < e1.pages[i].move_route.move_commands.size(); j++)
		{
			chk(pages[i].move_route.move_commands[j].command_id);
			chk(pages[i].move_route.move_commands[j].parameter_string);
			chk(pages[i].move_route.move_commands[j].parameter_a);
			chk(pages[i].move_route.move_commands[j].parameter_b);
			chk(pages[i].move_route.move_commands[j].parameter_c);
		}
		chk(pages[i].event_commands.size());
		for (unsigned int j = 0; j < e1.pages[i].event_commands.size(); j++)
		{
			chk(pages[i].event_commands[j].code);
			chk(pages[i].event_commands[j].indent);
			chk(pages[i].event_commands[j].string);
			chk(pages[i].event_commands[j].parameters.size());
			for (unsigned int k = 0; k < e1.pages[i].event_commands[j].parameters.size(); k++)
				chk(pages[i].event_commands[j].parameters[k]);
		}
	}
	return true;
#undef chk
}

/*
 * BattlePageConditions
 */

BattlePageConditions::BattlePageConditions(const lcf::rpg::TroopPageCondition& condition, bool rpg2k3, QObject* parent) :
	QObject(parent), m_condition(condition), m_rpg2k3(rpg2k3)
{
}

lcf::rpg::TroopPageCondition& BattlePageConditions::condition()
{
	return m_condition;
}

const lcf::rpg::TroopPageCondition& BattlePageConditions::condition() const
{
	return m_condition;
}

bool BattlePageConditions::rpg2k3() const
{
	return m_rpg2k3;
}

int BattlePageConditions::variableMaximum() const
{
	return m_rpg2k3 ? 9999999 : 999999;
}

bool BattlePageConditions::isChecked(int row) const
{
	// the rows are in the order of the flags
	return row >= 0 && row < RowCount && m_condition.flags.flags[row];
}

void BattlePageConditions::setChecked(int row, bool checked)
{
	if (row < 0 || row >= RowCount || m_condition.flags.flags[row] == checked) {
		return;
	}
	m_condition.flags.flags[row] = checked;
	emit changed();
}

bool BattlePageConditions::isAllowed(int row) const
{
	return allowed(row, m_rpg2k3);
}

bool BattlePageConditions::fieldsEnabled(int row) const
{
	return isAllowed(row) && isChecked(row);
}

QVariantList BattlePageConditions::checkedRows() const
{
	QVariantList rows;
	for (int row = 0; row < RowCount; ++row) {
		rows.append(isChecked(row));
	}
	return rows;
}

QVariantList BattlePageConditions::enabledRows() const
{
	QVariantList rows;
	for (int row = 0; row < RowCount; ++row) {
		rows.append(fieldsEnabled(row));
	}
	return rows;
}

QVariantList BattlePageConditions::allowedRows() const
{
	QVariantList rows;
	for (int row = 0; row < RowCount; ++row) {
		rows.append(isAllowed(row));
	}
	return rows;
}

void BattlePageConditions::set(int32_t& field, int value)
{
	if (field != value) {
		field = value;
		emit changed();
	}
}

#define CONDITION_FIELD(getter, setter, field) \
	int BattlePageConditions::getter() const { return m_condition.field; } \
	void BattlePageConditions::setter(int value) { set(m_condition.field, value); }

CONDITION_FIELD(switchAId, setSwitchAId, switch_a_id)
CONDITION_FIELD(switchBId, setSwitchBId, switch_b_id)
CONDITION_FIELD(variableId, setVariableId, variable_id)
CONDITION_FIELD(variableValue, setVariableValue, variable_value)
CONDITION_FIELD(turnA, setTurnA, turn_a)
CONDITION_FIELD(turnB, setTurnB, turn_b)
CONDITION_FIELD(fatigueMin, setFatigueMin, fatigue_min)
CONDITION_FIELD(fatigueMax, setFatigueMax, fatigue_max)
CONDITION_FIELD(enemyId, setEnemyId, enemy_id)
CONDITION_FIELD(enemyHpMin, setEnemyHpMin, enemy_hp_min)
CONDITION_FIELD(enemyHpMax, setEnemyHpMax, enemy_hp_max)
CONDITION_FIELD(actorId, setActorId, actor_id)
CONDITION_FIELD(actorHpMin, setActorHpMin, actor_hp_min)
CONDITION_FIELD(actorHpMax, setActorHpMax, actor_hp_max)
CONDITION_FIELD(turnEnemyId, setTurnEnemyId, turn_enemy_id)
CONDITION_FIELD(turnEnemyA, setTurnEnemyA, turn_enemy_a)
CONDITION_FIELD(turnEnemyB, setTurnEnemyB, turn_enemy_b)
CONDITION_FIELD(turnActorId, setTurnActorId, turn_actor_id)
CONDITION_FIELD(turnActorA, setTurnActorA, turn_actor_a)
CONDITION_FIELD(turnActorB, setTurnActorB, turn_actor_b)
CONDITION_FIELD(commandActorId, setCommandActorId, command_actor_id)
CONDITION_FIELD(commandId, setCommandId, command_id)

#undef CONDITION_FIELD

void BattlePageConditions::setChoices(const lcf::rpg::Database& database, const lcf::rpg::Troop& troop)
{
	// a value the lists lack is kept, shown as it is
	auto ensure = [](QVariantList& list, int value, const QString& text) {
		for (const QVariant& item : list) {
			if (item.toMap().value(QStringLiteral("value")).toInt() == value) {
				return;
			}
		}
		list.append(choice(text, value));
	};
	m_members.clear();
	for (int i = 0; i < static_cast<int>(troop.members.size()); ++i) {
		m_members.append(choice(memberName(database, troop, i), i));
	}
	ensure(m_members, m_condition.enemy_id, memberName(database, troop, m_condition.enemy_id));
	if (m_condition.turn_enemy_id != m_condition.enemy_id) {
		ensure(m_members, m_condition.turn_enemy_id, memberName(database, troop, m_condition.turn_enemy_id));
	}
	m_battleCommands.clear();
	for (const auto& command : database.battlecommands.commands) {
		m_battleCommands.append(choice(idName(ToQString(command.name), command.ID), command.ID));
	}
	ensure(m_battleCommands, m_condition.command_id, idName(QStringLiteral("?"), m_condition.command_id));
}

QVariantList BattlePageConditions::members() const
{
	return m_members;
}

QVariantList BattlePageConditions::battleCommands() const
{
	return m_battleCommands;
}

bool BattlePageConditions::allowed(int row, bool rpg2k3)
{
	switch (row) {
		// RPG Maker 2003: the exhaustion of the party, the turns of one
		// battler, and the command a hero chose
		case Fatigue:
		case TurnEnemy:
		case TurnActor:
		case Command:
			return rpg2k3;
		default:
			return row >= 0 && row < RowCount;
	}
}

QString BattlePageConditions::idName(const QString& name, int id)
{
	return QStringLiteral("%1:%2").arg(id, 4, 10, QChar('0')).arg(name);
}

QString BattlePageConditions::memberName(const lcf::rpg::Database& database, const lcf::rpg::Troop& troop, int index)
{
	QString name = index >= 0 && index < static_cast<int>(troop.members.size())
		? nameOf(database.enemies, troop.members[index].enemy_id) : QStringLiteral("?");
	return QStringLiteral("%1:%2").arg(index + 1).arg(name);
}

QString BattlePageConditions::text(const lcf::rpg::Database& db, const lcf::rpg::Troop& troop,
		const lcf::rpg::TroopPageCondition& c)
{
	auto actor = [&](int id) { return idName(nameOf(db.actors, id), id); };
	QStringList parts;
	if (c.flags.switch_a) {
		parts << conditionText(QT_TRANSLATE_NOOP("BattlePageConditionDialog", "Switch [%1] ON")).arg(idName(nameOf(db.switches, c.switch_a_id), c.switch_a_id));
	}
	if (c.flags.switch_b) {
		parts << conditionText(QT_TRANSLATE_NOOP("BattlePageConditionDialog", "Switch [%1] ON")).arg(idName(nameOf(db.switches, c.switch_b_id), c.switch_b_id));
	}
	if (c.flags.variable) {
		parts << conditionText(QT_TRANSLATE_NOOP("BattlePageConditionDialog", "Variable [%1] >= %2")).arg(idName(nameOf(db.variables, c.variable_id), c.variable_id)).arg(c.variable_value);
	}
	if (c.flags.turn) {
		parts << conditionText(QT_TRANSLATE_NOOP("BattlePageConditionDialog", "Turns [%1]")).arg(turnsText(c.turn_a, c.turn_b));
	}
	if (c.flags.fatigue) {
		parts << conditionText(QT_TRANSLATE_NOOP("BattlePageConditionDialog", "Exhaustion [%1-%2]%")).arg(c.fatigue_min).arg(c.fatigue_max);
	}
	if (c.flags.enemy_hp) {
		parts << conditionText(QT_TRANSLATE_NOOP("BattlePageConditionDialog", "Monster [%1] HP [%2-%3]%")).arg(memberName(db, troop, c.enemy_id)).arg(c.enemy_hp_min).arg(c.enemy_hp_max);
	}
	if (c.flags.actor_hp) {
		parts << conditionText(QT_TRANSLATE_NOOP("BattlePageConditionDialog", "Hero [%1] HP [%2-%3]%")).arg(actor(c.actor_id)).arg(c.actor_hp_min).arg(c.actor_hp_max);
	}
	if (c.flags.turn_enemy) {
		parts << conditionText(QT_TRANSLATE_NOOP("BattlePageConditionDialog", "Monster [%1] Turns [%2]")).arg(memberName(db, troop, c.turn_enemy_id), turnsText(c.turn_enemy_a, c.turn_enemy_b));
	}
	if (c.flags.turn_actor) {
		parts << conditionText(QT_TRANSLATE_NOOP("BattlePageConditionDialog", "Hero [%1] Turns [%2]")).arg(actor(c.turn_actor_id), turnsText(c.turn_actor_a, c.turn_actor_b));
	}
	if (c.flags.command_actor) {
		parts << conditionText(QT_TRANSLATE_NOOP("BattlePageConditionDialog", "Hero [%1] uses [%2]")).arg(actor(c.command_actor_id), idName(nameOf(db.battlecommands.commands, c.command_id), c.command_id));
	}
	// The Player never runs a page without a trigger
	return parts.isEmpty() ? conditionText(QT_TRANSLATE_NOOP("BattlePageConditionDialog", "None (the page never runs)")) : parts.join(QStringLiteral(", "));
}
