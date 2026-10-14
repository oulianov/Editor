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

// The event window without its QML view, on events in memory: the pages
// (New, Copy, Paste, Delete, Clear), the settings of a page with the rules
// of RPG Maker's window on them, Apply, OK and Cancel, an event changed on
// disk meanwhile, and the edit lock; a new event put on a map in memory
// (MapDocument) by OK; the conditions of battle pages and
// their Trigger line; the moves of a move route as a list model, with Qt's
// model checks watching every change. The texts of the moves come from a
// stand-in of the Stringizer (the real one reads the project open, core());
// the QML windows are left to the script tests (tests/scripts/event_*.txt).

#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <QTest>
#include <QUndoStack>
#include <memory>
#include <lcf/rpg/database.h>
#include <lcf/rpg/map.h>
#include "common/dbstring.h"
#include "controller/edit_locks.h"
#include "model/event_editor_controller.h"
#include "model/event_page_defaults.h"
#include "model/map_document.h"
#include "model/move_route_model.h"
#include "model/project_data.h"
#include "stringizer.h"

using MoveCode = lcf::rpg::MoveCommand::Code;
using Page = lcf::rpg::EventPage;
using Row = BattlePageConditions::Row;

// The stand-in of the Stringizer: the code of the move, and its switch or graphic
QString Stringizer::moveCommand(int id)
{
	return QStringLiteral("Move %1").arg(id);
}

QString Stringizer::moveCommand(const lcf::rpg::MoveCommand& move)
{
	switch (static_cast<MoveCode>(move.command_id)) {
		case MoveCode::switch_on:
			return QStringLiteral("Switch ON: %1").arg(move.parameter_a);
		case MoveCode::change_graphic:
			return QStringLiteral("Graphic: %1, %2").arg(ToQString(move.parameter_string)).arg(move.parameter_a);
		default:
			return moveCommand(move.command_id);
	}
}

namespace {
lcf::rpg::EventCommand command(lcf::rpg::EventCommand::Code code)
{
	lcf::rpg::EventCommand cmd;
	cmd.code = static_cast<int32_t>(code);
	return cmd;
}

lcf::rpg::MoveCommand move(MoveCode code)
{
	return MoveRouteModel::newMove(static_cast<int>(code));
}

/** EV0003 at 4,5: three pages, each with its own look, trigger and switch,
 *  numbered as a file could have them */
lcf::rpg::Event makeEvent(int pages = 3)
{
	lcf::rpg::Event event;
	event.ID = 3;
	event.name = lcf::DBString("EV0003");
	event.x = 4;
	event.y = 5;
	for (int i = 0; i < pages; ++i) {
		Page page;
		page.ID = 10 + i;
		page.character_name = lcf::DBString("Chara1");
		page.character_index = i;
		page.trigger = i;
		page.move_type = Page::MoveType_random;
		page.condition.flags.switch_a = true;
		page.condition.switch_a_id = i + 1;
		page.event_commands.push_back(command(lcf::rpg::EventCommand::Code::Wait));
		event.pages.push_back(page);
	}
	return event;
}

QList<int> pageIds(const lcf::rpg::Event& event)
{
	QList<int> ids;
	for (const auto& page : event.pages) {
		ids << page.ID;
	}
	return ids;
}

QList<int> triggers(const lcf::rpg::Event& event)
{
	QList<int> list;
	for (const auto& page : event.pages) {
		list << page.trigger;
	}
	return list;
}

QList<int> codes(const MoveRouteModel& model)
{
	QList<int> list;
	for (const auto& m : model.moves()) {
		list << m.command_id;
	}
	return list;
}

int code(MoveCode code)
{
	return static_cast<int>(code);
}
}

class EventEditorTest : public QObject
{
	Q_OBJECT

private slots:
	void showsEvent();
	void newAndDeletePages();
	void copyAndPastePages();
	void clearPage();
	void pageLimit();
	void pageSettingsFollowPage();
	void applyOkCancel();
	void newEventOnMap();
	void reloadEvent();
	void editLock();
	void conditionRules();
	void movementRules();
	void graphic();
	void lists();
	void battleConditionRules();
	void battleConditionText();
	void moveRouteRows();
	void moveRouteInsertAndParameters();
	void moveRouteRemoveCopyPaste();
	void moveRouteMove();
	void moveRouteCommand();
	void moveRouteButtons();
};

void EventEditorTest::showsEvent()
{
	EventEditorController controller(7);
	controller.setEvent(makeEvent());
	QCOMPARE(controller.eventId(), 3);
	QCOMPARE(controller.title(), QStringLiteral("EV: 3"));
	QCOMPARE(controller.name(), QStringLiteral("EV0003"));
	QCOMPARE(controller.x(), 4);
	QCOMPARE(controller.y(), 5);
	// numbered from 1 in order, as RPG Maker saves them
	QCOMPARE(pageIds(controller.event()), QList<int>({1, 2, 3}));
	QCOMPARE(controller.pageCount(), 3);
	QCOMPARE(controller.currentPage(), 0);
	QVERIFY(!controller.isModified());
	QCOMPARE(controller.result(), EventEditorController::Cancel);
	// the commands of each page, for its list
	QCOMPARE(controller.commands(1), &controller.page(1)->event_commands);
	QVERIFY(!controller.commands(3));
	QCOMPARE(controller.scriptLocation(1), QStringList({"--map", "7", "--event", "3", "--page", "2"}));

	// the previous and next pages stop at the ends
	controller.switchPage(-1);
	QCOMPARE(controller.currentPage(), 0);
	controller.switchPage(1);
	controller.switchPage(1);
	controller.switchPage(1);
	QCOMPARE(controller.currentPage(), 2);
	// another event keeps the page shown when it has it
	controller.setEvent(makeEvent(2));
	QCOMPARE(controller.currentPage(), 1);
}

void EventEditorTest::newAndDeletePages()
{
	EventEditorController controller(7);
	controller.setEvent(makeEvent());
	QSignalSpy pages(&controller, &EventEditorController::pagesChanged);

	// New Page: after the page shown, with its look and movement, new
	// conditions and no commands, like RPG Maker
	controller.setCurrentPage(1);
	QCOMPARE(controller.newPage(), 2);
	QCOMPARE(pages.count(), 1);
	QCOMPARE(controller.currentPage(), 2);
	QCOMPARE(pageIds(controller.event()), QList<int>({1, 2, 3, 4}));
	QCOMPARE(triggers(controller.event()), QList<int>({0, 1, 1, 2}));
	const auto& added = *controller.page(2);
	QCOMPARE(added.character_index, 1);
	QCOMPARE(added.move_type, int(Page::MoveType_random));
	QVERIFY(!added.condition.flags.switch_a);
	QVERIFY(added.event_commands.empty());
	QVERIFY(controller.isModified());

	// Delete Page: the next page takes its place, the last one at the end
	QVERIFY(controller.canDeletePage());
	QCOMPARE(controller.deletePage(), 2);
	QCOMPARE(triggers(controller.event()), QList<int>({0, 1, 2}));
	QCOMPARE(pageIds(controller.event()), QList<int>({1, 2, 3}));
	QCOMPARE(controller.deletePage(), 1);
	QCOMPARE(triggers(controller.event()), QList<int>({0, 1}));
	QCOMPARE(controller.deletePage(), 0);
	// an event keeps a page
	QVERIFY(!controller.canDeletePage());
	QCOMPARE(controller.deletePage(), -1);
	QCOMPARE(controller.pageCount(), 1);

	// New Page in an event without pages: a page standing still
	lcf::rpg::Event empty = makeEvent(0);
	controller.setEvent(empty);
	QCOMPARE(controller.currentPage(), -1);
	QCOMPARE(controller.newPage(), 0);
	QCOMPARE(controller.page(0)->move_type, int(Page::MoveType_stationary));
	QCOMPARE(controller.page(0)->ID, 1);
}

void EventEditorTest::copyAndPastePages()
{
	EventEditorController first(7);
	first.setEvent(makeEvent());
	EventEditorController second(8);
	second.setEvent(makeEvent(1));

	// copied from an event, pasted in another (of another map), after the page shown
	first.setCurrentPage(2);
	QSignalSpy actions(&first, &EventEditorController::pageActionsChanged);
	QVERIFY(first.copyPage());
	QCOMPARE(actions.count(), 1);
	QVERIFY(EventEditorController::hasPageClipboard());
	QVERIFY(second.canPastePage());
	// the page as it was copied
	first.page(2)->trigger = 4;
	QCOMPARE(second.pastePage(), 1);
	QCOMPARE(second.currentPage(), 1);
	QCOMPARE(second.page(1)->trigger, 2);
	QCOMPARE(second.page(1)->condition.switch_a_id, 3);
	QCOMPARE(second.page(1)->event_commands.size(), size_t(1));
	QCOMPARE(pageIds(second.event()), QList<int>({1, 2}));

	// pasted after the first page, before the second
	second.setCurrentPage(0);
	QCOMPARE(second.pastePage(), 1);
	QCOMPARE(triggers(second.event()), QList<int>({0, 2, 2}));
}

void EventEditorTest::clearPage()
{
	EventEditorController controller(7);
	controller.setEvent(makeEvent());
	controller.setCurrentPage(1);
	// a new page, keeping its number
	QCOMPARE(controller.clearPage(), 1);
	const auto& cleared = *controller.page(1);
	QCOMPARE(cleared.ID, 2);
	QVERIFY(cleared.character_name.empty());
	QCOMPARE(cleared.move_type, int(Page::MoveType_stationary));
	QVERIFY(!cleared.condition.flags.switch_a);
	QVERIFY(cleared.event_commands.empty());
	QCOMPARE(controller.pageCount(), 3);
	QVERIFY(controller.isModified());
}

void EventEditorTest::pageLimit()
{
	// RPG Maker's limit: 100 pages
	EventEditorController controller(7);
	controller.setEvent(makeEvent(EventEditorController::MaxPages - 1));
	QVERIFY(controller.copyPage());
	QVERIFY(controller.canNewPage());
	QCOMPARE(controller.newPage(), 1);
	QCOMPARE(controller.pageCount(), EventEditorController::MaxPages);
	QVERIFY(!controller.canNewPage());
	QVERIFY(!controller.canPastePage());
	QCOMPARE(controller.newPage(), -1);
	QCOMPARE(controller.pastePage(), -1);
	QCOMPARE(controller.pageCount(), EventEditorController::MaxPages);
	QCOMPARE(controller.page(99)->ID, 100);
}

void EventEditorTest::pageSettingsFollowPage()
{
	EventEditorController controller(7);
	controller.setEvent(makeEvent());
	auto* settings = controller.pageSettings();
	QCOMPARE(settings->page(), controller.page(0));
	controller.setCurrentPage(2);
	QCOMPARE(settings->page(), controller.page(2));
	QCOMPARE(settings->trigger(), 2);
	// a new page moves the pages in memory: the settings follow
	controller.newPage();
	QCOMPARE(settings->page(), controller.page(3));

	// a setting is an edit of the event
	QSignalSpy edited(&controller, &EventEditorController::edited);
	controller.resetModified();
	settings->setTrigger(Page::Trigger_parallel);
	QVERIFY(edited.count() >= 2);
	QCOMPARE(controller.event().pages[3].trigger, int(Page::Trigger_parallel));
	QVERIFY(controller.isModified());

	// the version of the game
	controller.setRpg2k3(false);
	QVERIFY(!settings->rpg2k3());
}

void EventEditorTest::applyOkCancel()
{
	const lcf::rpg::Event original = makeEvent();
	EventEditorController controller(7);
	controller.setEvent(original);
	QList<lcf::rpg::Event> applied;
	QList<lcf::rpg::Event> accepted;
	connect(&controller, &EventEditorController::applied, this, [&](const lcf::rpg::Event& event) { applied << event; });
	connect(&controller, &EventEditorController::accepted, this, [&](const lcf::rpg::Event& event) { accepted << event; });

	// nothing changed: nothing to apply
	QVERIFY(!controller.apply());
	QCOMPARE(applied.count(), 0);
	QCOMPARE(controller.result(), EventEditorController::Cancel);

	// Apply puts the event on the map now
	controller.setName(QStringLiteral("Applied"));
	QVERIFY(controller.isModified());
	QVERIFY(controller.apply());
	QCOMPARE(applied.count(), 1);
	QCOMPARE(ToQString(applied.first().name), QStringLiteral("Applied"));
	QCOMPARE(controller.result(), EventEditorController::Apply);
	// later changes compare to it
	QVERIFY(!controller.isModified());
	controller.pageSettings()->setTrigger(Page::Trigger_auto_start);
	QVERIFY(controller.isModified());
	// Cancel: the map keeps what was applied
	QCOMPARE(ToQString(controller.resultEvent().name), QStringLiteral("Applied"));
	QCOMPARE(controller.resultEvent().pages[0].trigger, 0);

	// OK: the changes since
	QCOMPARE(controller.ok(), EventEditorController::Ok);
	QCOMPARE(accepted.count(), 1);
	QCOMPARE(controller.resultEvent().pages[0].trigger, int(Page::Trigger_auto_start));

	// OK without changes is a Cancel: the event as it was
	EventEditorController other(7);
	other.setEvent(original);
	QCOMPARE(other.ok(), EventEditorController::Cancel);
	QCOMPARE(accepted.count(), 1);
	QVERIFY(EventEditorController::equalEvents(other.resultEvent(), original));

	// the commands are edited by their list: they change the event, and
	// what they set when given the page (indents) does not
	other.commands(0)->push_back(command(lcf::rpg::EventCommand::Code::Wait));
	QVERIFY(other.isModified());
	other.resetModified();
	QVERIFY(!other.isModified());
	QCOMPARE(other.ok(), EventEditorController::Cancel);
}

void EventEditorTest::newEventOnMap()
{
	// a map of 4 x 4 cells, without events
	ProjectData project;
	lcf::rpg::MapInfo root;
	root.ID = 0;
	lcf::rpg::MapInfo info;
	info.ID = 1;
	project.treeMap().maps = {root, info};
	auto map = std::make_unique<lcf::rpg::Map>();
	map->width = 4;
	map->height = 4;
	map->lower_layer.assign(16, 0);
	map->upper_layer.assign(16, 10000);
	MapDocument doc(project, 1);
	doc.setMap(std::move(map));
	// the map takes what Apply and OK give
	auto edit = [&doc](EventEditorController& controller) {
		connect(&controller, &EventEditorController::applied, &doc, &MapDocument::commitEvent);
		connect(&controller, &EventEditorController::accepted, &doc, &MapDocument::commitEvent);
	};

	// New Event, OK without changes: on the map, one undo step (RPG Maker)
	EventEditorController created(1);
	edit(created);
	created.setEvent(doc.newEvent(2, 3), true);
	QVERIFY(created.isNewEvent());
	QVERIFY(created.isModified());
	QCOMPARE(created.ok(), EventEditorController::Ok);
	QVERIFY(doc.eventAt(2, 3));
	QCOMPARE(doc.eventAt(2, 3)->ID, created.eventId());
	QCOMPARE(doc.undoStack()->count(), 1);

	// the event of the map, OK without changes: nothing
	EventEditorController existing(1);
	edit(existing);
	existing.setEvent(*doc.eventAt(2, 3));
	QVERIFY(!existing.isNewEvent());
	QVERIFY(!existing.isModified());
	QCOMPARE(existing.ok(), EventEditorController::Cancel);
	QCOMPARE(doc.undoStack()->count(), 1);

	// New Event, Apply without changes then OK: put on the map once
	EventEditorController applied(1);
	edit(applied);
	applied.setEvent(doc.newEvent(0, 0), true);
	QVERIFY(applied.apply());
	QVERIFY(!applied.isNewEvent());
	QVERIFY(doc.eventAt(0, 0));
	QCOMPARE(applied.ok(), EventEditorController::Cancel);
	QCOMPARE(doc.undoStack()->count(), 2);
	// changed after it: OK replaces it
	applied.setName(QStringLiteral("Door"));
	QCOMPARE(applied.ok(), EventEditorController::Ok);
	QCOMPARE(ToQString(doc.eventAt(0, 0)->name), QStringLiteral("Door"));
	QCOMPARE(doc.undoStack()->count(), 3);

	// New Event, Cancel: nothing
	EventEditorController cancelled(1);
	edit(cancelled);
	cancelled.setEvent(doc.newEvent(1, 1), true);
	QCOMPARE(cancelled.result(), EventEditorController::Cancel);
	QVERIFY(!doc.eventAt(1, 1));
	QCOMPARE(doc.undoStack()->count(), 3);
}

void EventEditorTest::reloadEvent()
{
	EventEditorController controller(7);
	controller.setEvent(makeEvent());
	controller.setCurrentPage(2);

	// not changed here: the new version, on the same page (the last one when it has fewer)
	lcf::rpg::Event disk = makeEvent(2);
	disk.name = lcf::DBString("On disk");
	QCOMPARE(controller.reloadEvent(&disk), EventEditorController::Reloaded);
	QCOMPARE(controller.name(), QStringLiteral("On disk"));
	QCOMPARE(controller.pageCount(), 2);
	QCOMPARE(controller.currentPage(), 1);
	QVERIFY(!controller.isModified());
	QVERIFY(!controller.isReloadPending());

	// changed here: the interface asks first
	controller.setName(QStringLiteral("Mine"));
	disk.name = lcf::DBString("Again");
	QCOMPARE(controller.reloadEvent(&disk), EventEditorController::AskFirst);
	QVERIFY(controller.isReloadPending());
	QCOMPARE(controller.name(), QStringLiteral("Mine"));
	controller.rejectReload();
	QVERIFY(!controller.isReloadPending());
	QCOMPARE(controller.name(), QStringLiteral("Mine"));
	QCOMPARE(controller.reloadEvent(&disk), EventEditorController::AskFirst);
	controller.acceptReload();
	QCOMPARE(controller.name(), QStringLiteral("Again"));
	QVERIFY(!controller.isModified());

	// applied, then replaced on disk: the window gives the new version
	controller.setName(QStringLiteral("Applied"));
	QVERIFY(controller.apply());
	QCOMPARE(controller.reloadEvent(&disk), EventEditorController::Reloaded);
	QCOMPARE(controller.result(), EventEditorController::Cancel);
	QCOMPARE(ToQString(controller.resultEvent().name), QStringLiteral("Again"));

	// removed: the window closes, giving nothing
	QSignalSpy removed(&controller, &EventEditorController::removed);
	controller.setName(QStringLiteral("Applied"));
	QVERIFY(controller.apply());
	QCOMPARE(controller.reloadEvent(nullptr), EventEditorController::Removed);
	QCOMPARE(removed.count(), 1);
	QCOMPARE(controller.result(), EventEditorController::Cancel);
}

void EventEditorTest::editLock()
{
	auto& locks = EditLocks::instance();
	QVERIFY(locks.isEmpty());
	{
		EventEditorController controller(7);
		QVERIFY(!controller.isOpen());
		controller.setOpen(true);
		QVERIFY(controller.isOpen());
		QCOMPARE(locks.locks().size(), 1);
		QCOMPARE(locks.locks().first().kind, EditLock::Event);
		QCOMPARE(locks.locks().first().map_id, 7);
		// the maps can be read again while it is open, not the database
		QVERIFY(locks.canReload(7));
		QVERIFY(!locks.canReload(0));
		// another map: the lock follows
		controller.setMapId(9);
		QCOMPARE(locks.locks().size(), 1);
		QCOMPARE(locks.locks().first().map_id, 9);
		controller.setOpen(false);
		QVERIFY(locks.isEmpty());
		controller.setOpen(true);
		QVERIFY(!locks.isEmpty());
	}
	// released with it
	QVERIFY(locks.isEmpty());
}

void EventEditorTest::conditionRules()
{
	Page page = EventPageDefaults::newPage();
	EventPageSettings settings;
	settings.setRpg2k3(false);
	settings.setPage(&page);
	QSignalSpy changed(&settings, &EventPageSettings::changed);

	// RPG Maker 2000: the variable is compared with >=, one timer, smaller values
	settings.setVariable(true);
	QVERIFY(page.condition.flags.variable);
	QVERIFY(!settings.compareOperatorEnabled());
	QVERIFY(!settings.timer2Allowed());
	QCOMPARE(settings.variableMaximum(), 999999);
	QCOMPARE(settings.variableMinimum(), -999999);
	// RPG Maker 2003
	settings.setRpg2k3(true);
	QVERIFY(settings.compareOperatorEnabled());
	QVERIFY(settings.timer2Allowed());
	QCOMPARE(settings.variableMaximum(), 9999999);
	settings.setVariable(false);
	QVERIFY(!settings.compareOperatorEnabled());

	// the fields write the page
	settings.setSwitchA(true);
	settings.setSwitchAId(12);
	settings.setSwitchB(true);
	settings.setSwitchBId(13);
	settings.setVariableId(4);
	settings.setCompareOperator(lcf::rpg::EventPageCondition::Comparison_not_equal);
	settings.setVariableValue(-50);
	settings.setItem(true);
	settings.setItemId(5);
	settings.setActor(true);
	settings.setActorId(6);
	settings.setTimer(true);
	settings.setTimerSeconds(90);
	settings.setTimer2(true);
	settings.setTimer2Seconds(61);
	const auto& c = page.condition;
	QVERIFY(c.flags.switch_a && c.flags.switch_b && c.flags.item && c.flags.actor && c.flags.timer && c.flags.timer2);
	QCOMPARE(c.switch_a_id, 12);
	QCOMPARE(c.switch_b_id, 13);
	QCOMPARE(c.variable_id, 4);
	QCOMPARE(c.compare_operator, int(lcf::rpg::EventPageCondition::Comparison_not_equal));
	QCOMPARE(c.variable_value, -50);
	QCOMPARE(c.item_id, 5);
	QCOMPARE(c.actor_id, 6);
	QCOMPARE(c.timer_sec, 90);
	QCOMPARE(c.timer2_sec, 61);
	QCOMPARE(settings.timerSeconds(), 90);

	// the same value changes nothing
	changed.clear();
	settings.setSwitchAId(12);
	settings.setTimer(true);
	QCOMPARE(changed.count(), 0);

	// without a page, nothing is written
	settings.setPage(nullptr);
	QVERIFY(!settings.hasPage());
	settings.setSwitchAId(1);
	QCOMPARE(c.switch_a_id, 12);
}

void EventEditorTest::movementRules()
{
	Page page = EventPageDefaults::newPage();
	EventPageSettings settings;
	settings.setPage(&page);

	// standing still: no frequency nor route
	QCOMPARE(settings.moveType(), int(Page::MoveType_stationary));
	QVERIFY(!settings.moveFrequencyEnabled());
	QVERIFY(!settings.moveRouteEnabled());
	settings.setMoveType(Page::MoveType_random);
	QVERIFY(settings.moveFrequencyEnabled());
	QVERIFY(!settings.moveRouteEnabled());
	// the route of Custom Pattern
	settings.setMoveType(Page::MoveType_custom);
	QVERIFY(settings.moveFrequencyEnabled());
	QVERIFY(settings.moveRouteEnabled());
	lcf::rpg::MoveRoute route;
	route.move_commands = {move(MoveCode::move_up)};
	settings.setMoveRoute(route);
	QCOMPARE(page.move_route, route);

	settings.setMoveFrequency(8);
	settings.setMoveSpeed(Page::MoveSpeed_fourfold);
	settings.setTrigger(Page::Trigger_collision);
	settings.setLayer(Page::Layers_above);
	settings.setOverlapForbidden(true);
	settings.setTranslucent(true);
	QCOMPARE(page.move_frequency, 8);
	QCOMPARE(page.move_speed, int(Page::MoveSpeed_fourfold));
	QCOMPARE(page.trigger, int(Page::Trigger_collision));
	QCOMPARE(page.layer, int(Page::Layers_above));
	QVERIFY(page.overlap_forbidden);
	QVERIFY(page.translucent);

	// the frames of a step or a quarter turn, by speed, as the Player
	QCOMPARE(EventPageSettings::stepFrames(1), 16);
	QCOMPARE(EventPageSettings::stepFrames(6), 6);
	QCOMPARE(EventPageSettings::turnFrames(4), 8);
	QCOMPARE(EventPageSettings::stepFrames(0), 16);
}

void EventEditorTest::graphic()
{
	Page page = EventPageDefaults::newPage();
	page.layer = Page::Layers_below;
	page.animation_type = Page::AnimType_continuous;
	EventPageSettings settings;
	settings.setPage(&page);

	// nothing: tile 0, does not animate
	QVERIFY(!settings.hasCharacter());
	QVERIFY(!settings.isTile());
	QVERIFY(!settings.animationTypeEnabled());
	QVERIFY(!settings.animates());

	// a character: on the hero's layer, the animation stops (RPG Maker)
	settings.setCharacter(QStringLiteral("Chara1"), 2, Page::Direction_left, Page::Frame_right);
	QCOMPARE(ToQString(page.character_name), QStringLiteral("Chara1"));
	QCOMPARE(page.character_index, 2);
	QCOMPARE(page.character_direction, int(Page::Direction_left));
	QCOMPARE(page.character_pattern, int(Page::Frame_right));
	QCOMPARE(page.layer, int(Page::Layers_same));
	QCOMPARE(page.animation_type, int(Page::AnimType_non_continuous));
	QVERIFY(settings.animationTypeEnabled());
	QVERIFY(!settings.animates());
	settings.setAnimationType(Page::AnimType_spin);
	QVERIFY(settings.animates());

	// a tile: its layer from the chipset (a star above, a blocking tile on
	// the hero's layer, a tile walked on below)
	std::vector<lcf::rpg::Chipset> chipsets(1);
	auto& passable = chipsets[0].passable_data_upper;
	passable[5] = EventPageDefaults::Above | EventPageDefaults::AllDirections;
	passable[6] = 0;
	passable[7] = EventPageDefaults::AllDirections;
	QCOMPARE(EventPageSettings::upperTileFlags(chipsets, 1, 5), int(passable[5]));
	QCOMPARE(EventPageSettings::upperTileFlags(chipsets, 2, 5), -1);
	QCOMPARE(EventPageSettings::upperTileFlags(chipsets, 1, 1000), int(EventPageDefaults::AllDirections));
	settings.setTile(5, EventPageSettings::upperTileFlags(chipsets, 1, 5));
	QVERIFY(settings.isTile());
	QVERIFY(page.character_name.empty());
	QCOMPARE(page.character_index, 5);
	QCOMPARE(page.layer, int(Page::Layers_above));
	QCOMPARE(page.animation_type, int(Page::AnimType_non_continuous));
	QVERIFY(!settings.animationTypeEnabled());
	settings.setTile(6, EventPageSettings::upperTileFlags(chipsets, 1, 6));
	QCOMPARE(page.layer, int(Page::Layers_same));
	settings.setTile(7, EventPageSettings::upperTileFlags(chipsets, 1, 7));
	QCOMPARE(page.layer, int(Page::Layers_below));
	// no chipset: the layer stays
	settings.setTile(5);
	QCOMPARE(page.layer, int(Page::Layers_below));

	// no graphic: below the hero
	settings.setCharacter(QStringLiteral("Chara1"), 0, Page::Direction_down, Page::Frame_middle);
	QCOMPARE(page.layer, int(Page::Layers_same));
	settings.clearGraphic();
	QVERIFY(!settings.hasCharacter());
	QVERIFY(!settings.isTile());
	QCOMPARE(page.character_index, 0);
	QCOMPARE(page.layer, int(Page::Layers_below));
}

void EventEditorTest::lists()
{
	// the items of the lists, in the order of the window, with their values
	const auto speeds = EventPageSettings::items(EventPageSettings::MoveSpeeds);
	QCOMPARE(speeds.size(), 6);
	QCOMPARE(speeds.first().toMap().value("value").toInt(), int(Page::MoveSpeed_eighth));
	QCOMPARE(speeds.last().toMap().value("value").toInt(), int(Page::MoveSpeed_fourfold));
	const auto comparisons = EventPageSettings::items(EventPageSettings::CompareOperators);
	QCOMPARE(comparisons.size(), 6);
	QCOMPARE(comparisons.first().toMap().value("text").toString(), QStringLiteral("Equal to (==)"));
	QCOMPARE(EventPageSettings::items(EventPageSettings::MoveTypes).size(), 7);
	QCOMPARE(EventPageSettings::items(EventPageSettings::MoveFrequencies).size(), 8);
	QCOMPARE(EventPageSettings::items(EventPageSettings::Triggers).size(), 5);
	QCOMPARE(EventPageSettings::items(EventPageSettings::Layers).size(), 3);
	QCOMPARE(EventPageSettings::items(EventPageSettings::AnimationTypes).size(), 6);
	for (int list = EventPageSettings::CompareOperators; list <= EventPageSettings::MoveSpeeds; ++list) {
		for (const auto& item : EventPageSettings::items(EventPageSettings::List(list))) {
			QVERIFY(!item.toMap().value("text").toString().isEmpty());
		}
	}
}

void EventEditorTest::battleConditionRules()
{
	lcf::rpg::TroopPageCondition condition;
	condition.flags.turn_enemy = true;

	// RPG Maker 2003's conditions stay in 2000 games, locked
	BattlePageConditions conditions(condition, false);
	for (int row : {Row::SwitchA, Row::SwitchB, Row::Variable, Row::Turn, Row::EnemyHp, Row::ActorHp}) {
		QVERIFY(conditions.isAllowed(row));
	}
	for (int row : {Row::Fatigue, Row::TurnEnemy, Row::TurnActor, Row::Command}) {
		QVERIFY(!conditions.isAllowed(row));
	}
	QVERIFY(conditions.isChecked(Row::TurnEnemy));
	QVERIFY(!conditions.fieldsEnabled(Row::TurnEnemy));
	QCOMPARE(conditions.variableMaximum(), 999999);

	// a check box enables its fields
	QSignalSpy changed(&conditions, &BattlePageConditions::changed);
	QVERIFY(!conditions.fieldsEnabled(Row::Turn));
	conditions.setChecked(Row::Turn, true);
	QCOMPARE(changed.count(), 1);
	QVERIFY(conditions.condition().flags.turn);
	QVERIFY(conditions.fieldsEnabled(Row::Turn));
	conditions.setChecked(Row::Turn, true);
	QCOMPARE(changed.count(), 1);
	// a copy: the page's condition changes only when it is written back
	QVERIFY(!condition.flags.turn);

	BattlePageConditions conditions2k3(condition, true);
	QVERIFY(conditions2k3.fieldsEnabled(Row::TurnEnemy));
	QVERIFY(conditions2k3.isAllowed(Row::Command));
	QCOMPARE(conditions2k3.variableMaximum(), 9999999);
	QVERIFY(!conditions2k3.isAllowed(Row::RowCount));
	QVERIFY(!conditions2k3.isChecked(-1));
}

void EventEditorTest::battleConditionText()
{
	lcf::rpg::Database db;
	db.switches.resize(2);
	db.switches[1].name = lcf::DBString("Door");
	db.variables.resize(1);
	db.variables[0].name = lcf::DBString("Gold");
	db.actors.resize(1);
	db.actors[0].name = lcf::DBString("Alex");
	db.enemies.resize(2);
	db.enemies[1].name = lcf::DBString("Bat");
	db.battlecommands.commands.resize(1);
	db.battlecommands.commands[0].ID = 1;
	db.battlecommands.commands[0].name = lcf::DBString("Attack");
	lcf::rpg::Troop troop;
	troop.members.resize(1);
	troop.members[0].enemy_id = 2;

	lcf::rpg::TroopPageCondition c;
	QCOMPARE(BattlePageConditions::text(db, troop, c), QStringLiteral("None (the page never runs)"));

	c.flags.switch_a = true;
	c.switch_a_id = 2;
	c.flags.variable = true;
	c.variable_id = 1;
	c.variable_value = 10;
	c.flags.turn = true;
	c.turn_a = 3;
	c.turn_b = 1;
	c.flags.enemy_hp = true;
	c.enemy_id = 0;
	c.enemy_hp_max = 50;
	QCOMPARE(BattlePageConditions::text(db, troop, c),
		QStringLiteral("Switch [0002:Door] ON, Variable [0001:Gold] >= 10, Turns [3x+1], Monster [1:Bat] HP [0-50]%"));

	// 2003: the turns of a battler, a command; unknown entries are marked
	lcf::rpg::TroopPageCondition c3;
	c3.flags.switch_b = true;
	c3.switch_b_id = 9;
	c3.flags.turn_enemy = true;
	c3.turn_enemy_id = 4;
	c3.turn_enemy_b = 2;
	c3.flags.command_actor = true;
	QCOMPARE(BattlePageConditions::text(db, troop, c3),
		QStringLiteral("Switch [0009:<9?>] ON, Monster [5:?] Turns [2], Hero [0001:Alex] uses [0001:Attack]"));
	QCOMPARE(BattlePageConditions::memberName(db, troop, 0), QStringLiteral("1:Bat"));
}

void EventEditorTest::moveRouteRows()
{
	MoveRouteModel model;
	QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
	// a new route: the end line alone, repeated
	QCOMPARE(model.rowCount(), 1);
	QVERIFY(model.repeat());

	lcf::rpg::MoveRoute route;
	route.move_commands = {move(MoveCode::move_up), move(MoveCode::move_right), move(MoveCode::switch_on)};
	route.repeat = false;
	route.skippable = true;
	model.setRoute(route);
	QCOMPARE(model.rowCount(), 4);
	QCOMPARE(model.moveCount(), 3);
	QVERIFY(!model.repeat());
	QVERIFY(model.skippable());
	QCOMPARE(model.route(), route);

	// the lines as RPG Maker shows them, then the end line, where moves go
	QCOMPARE(model.data(model.index(0)).toString(), QStringLiteral("Move 0"));
	QCOMPARE(model.data(model.index(2)).toString(), QStringLiteral("Switch ON: 1"));
	QCOMPARE(model.data(model.index(3)).toString(), QStringLiteral("<>"));
	QVERIFY(model.data(model.index(3), MoveRouteModel::EndRole).toBool());
	QVERIFY(!model.data(model.index(0), MoveRouteModel::EndRole).toBool());
	QCOMPARE(model.data(model.index(3), MoveRouteModel::CodeRole).toInt(), -1);
	QCOMPARE(model.data(model.index(1), MoveRouteModel::CodeRole).toInt(), code(MoveCode::move_right));
	QVERIFY(model.data(model.index(3), Qt::ForegroundRole).isValid());
	QVERIFY(model.data(model.index(2), MoveRouteModel::HasParametersRole).toBool());
	QVERIFY(!model.data(model.index(0), MoveRouteModel::HasParametersRole).toBool());
	QCOMPARE(model.data(model.index(2), MoveRouteModel::ParameterARole).toInt(), 1);
	QCOMPARE(model.data(model.index(0), MoveRouteModel::IconNameRole).toString(), QStringLiteral("go-up"));
	QVERIFY(model.roleNames().contains(MoveRouteModel::EndRole));

	// the flags
	QSignalSpy repeat(&model, &MoveRouteModel::repeatChanged);
	model.setRepeat(true);
	model.setRepeat(true);
	QCOMPARE(repeat.count(), 1);
	model.setSkippable(false);
	QVERIFY(model.route().repeat);
	QVERIFY(!model.route().skippable);
}

void EventEditorTest::moveRouteInsertAndParameters()
{
	MoveRouteModel model;
	QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
	lcf::rpg::MoveRoute route;
	route.move_commands = {move(MoveCode::move_up), move(MoveCode::move_down)};
	model.setRoute(route);

	// before the line selected, the next line selected after it
	QCOMPARE(model.insertCommand(1, code(MoveCode::play_sound_effect)), 2);
	// a sound at its full volume and tempo, centered, like RPG Maker
	const auto& sound = model.moves()[1];
	QCOMPARE(sound.parameter_a, 100);
	QCOMPARE(sound.parameter_b, 100);
	QCOMPARE(sound.parameter_c, 50);
	// on the end line (or past it): at the end
	QCOMPARE(model.insertCommand(model.rowCount() - 1, code(MoveCode::wait)), 4);
	QCOMPARE(model.insertCommand(99, code(MoveCode::switch_off)), 5);
	QCOMPARE(codes(model), QList<int>({code(MoveCode::move_up), code(MoveCode::play_sound_effect),
		code(MoveCode::move_down), code(MoveCode::wait), code(MoveCode::switch_off)}));
	QCOMPARE(model.moves()[4].parameter_a, 1);

	// the parameters, as their windows give them
	QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
	model.setSwitch(4, 7);
	QCOMPARE(changed.count(), 1);
	QCOMPARE(model.data(model.index(4), MoveRouteModel::ParameterARole).toInt(), 7);
	model.setSound(1, QStringLiteral("Bell"), 80, 120, 30);
	QCOMPARE(model.data(model.index(1), MoveRouteModel::ParameterStringRole).toString(), QStringLiteral("Bell"));
	QCOMPARE(model.data(model.index(1), MoveRouteModel::ParameterBRole).toInt(), 120);
	QCOMPARE(model.data(model.index(1), MoveRouteModel::ParameterCRole).toInt(), 30);
	model.insertCommand(0, code(MoveCode::change_graphic));
	model.setGraphic(0, QStringLiteral("Actor1"), 3);
	QCOMPARE(model.data(model.index(0)).toString(), QStringLiteral("Graphic: Actor1, 3"));
	// the end line has none
	model.setSwitch(model.rowCount() - 1, 3);
	QCOMPARE(model.moveCount(), 6);
}

void EventEditorTest::moveRouteRemoveCopyPaste()
{
	MoveRouteModel model;
	QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
	lcf::rpg::MoveRoute route;
	route.move_commands = {move(MoveCode::move_up), move(MoveCode::move_right), move(MoveCode::move_down),
		move(MoveCode::move_left)};
	model.setRoute(route);
	const int up = code(MoveCode::move_up), right = code(MoveCode::move_right);
	const int down = code(MoveCode::move_down), left = code(MoveCode::move_left);

	// the rows selected, in any order, once each; the end line stays
	QCOMPARE(model.moveRowsOf({4, 2, 0, 2}), QList<int>({0, 2}));
	QCOMPARE(model.removeMoves({4}), -1);
	QCOMPARE(model.removeMoves({2, 0, 2, 4}), 0);
	QCOMPARE(codes(model), QList<int>({right, left}));
	QCOMPARE(model.rowCount(), 3);

	// copied for every route, pasted before the line selected
	model.setRoute(route);
	QVERIFY(!model.copyMoves({4}));
	QSignalSpy clipboard(&model, &MoveRouteModel::clipboardChanged);
	QVERIFY(model.copyMoves({2, 0}));
	QCOMPARE(clipboard.count(), 1);
	QVERIFY(MoveRouteModel::canPaste());
	QCOMPARE(model.paste(1), 3);
	QCOMPARE(codes(model), QList<int>({up, up, down, right, down, left}));
	// cut
	QVERIFY(model.copyMoves({3, 4}, true));
	QCOMPARE(codes(model), QList<int>({up, up, down, left}));
	MoveRouteModel other;
	QAbstractItemModelTester otherTester(&other, QAbstractItemModelTester::FailureReportingMode::QtTest);
	QCOMPARE(other.paste(0), 2);
	QCOMPARE(codes(other), QList<int>({right, down}));

	// Clear List
	model.clear();
	QCOMPARE(model.rowCount(), 1);
	QCOMPARE(model.moveCount(), 0);
	model.clear();
	QCOMPARE(model.paste(5), 2);
}

void EventEditorTest::moveRouteMove()
{
	MoveRouteModel model;
	QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
	lcf::rpg::MoveRoute route;
	route.move_commands = {move(MoveCode::move_up), move(MoveCode::move_right), move(MoveCode::move_down),
		move(MoveCode::move_left)};
	model.setRoute(route);
	const int up = code(MoveCode::move_up), right = code(MoveCode::move_right);
	const int down = code(MoveCode::move_down), left = code(MoveCode::move_left);

	// the first after the third
	QVERIFY(model.moveMoves(0, 1, 3));
	QCOMPARE(codes(model), QList<int>({right, down, up, left}));
	// the last two first
	QVERIFY(model.moveMoves(2, 2, 0));
	QCOMPARE(codes(model), QList<int>({up, left, right, down}));
	// to the end (before the end line)
	QVERIFY(model.moveMoves(0, 1, 4));
	QCOMPARE(codes(model), QList<int>({left, right, down, up}));
	// not moved: in place, the end line, past it
	QVERIFY(!model.moveMoves(0, 1, 1));
	QVERIFY(!model.moveMoves(4, 1, 0));
	QVERIFY(!model.moveMoves(0, 1, 5));
	QVERIFY(!model.moveMoves(0, 0, 2));
	QCOMPARE(codes(model), QList<int>({left, right, down, up}));
}

void EventEditorTest::moveRouteCommand()
{
	// the route of a Move Event command, its target and frequency
	MoveRouteCodec::MoveEvent move_event;
	move_event.target = 10001;
	move_event.frequency = 4;
	move_event.route.repeat = false;
	move_event.route.move_commands = {move(MoveCode::move_up), move(MoveCode::switch_off)};
	move_event.route.move_commands[1].parameter_a = 3;
	auto graphic = move(MoveCode::change_graphic);
	graphic.parameter_string = lcf::DBString("Actor1");
	graphic.parameter_a = 2;
	move_event.route.move_commands.push_back(graphic);
	lcf::rpg::EventCommand cmd;
	cmd.code = static_cast<int32_t>(lcf::rpg::EventCommand::Code::MoveEvent);
	MoveRouteCodec::encode(move_event, cmd, "1252");

	MoveRouteModel model;
	QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
	model.setCommand(cmd, "1252");
	QCOMPARE(model.target(), 10001);
	QCOMPARE(model.frequency(), 4);
	QVERIFY(!model.repeat());
	QCOMPARE(model.route(), move_event.route);

	model.setTarget(10005);
	model.setFrequency(6);
	model.insertCommand(0, code(MoveCode::face_left));
	lcf::rpg::EventCommand written;
	model.writeCommand(written, "1252");
	const auto read = MoveRouteCodec::decode(written, "1252");
	QCOMPARE(read.target, 10005);
	QCOMPARE(read.frequency, 6);
	QCOMPARE(read.route, model.route());
	QCOMPARE(model.moveEvent().route, model.route());
}

void EventEditorTest::moveRouteButtons()
{
	// RPG Maker's buttons: moves, facings, the others, then those with parameters
	const auto& groups = MoveRouteModel::commandGroups();
	QCOMPARE(groups.size(), size_t(4));
	QCOMPARE(groups[0].size(), size_t(12));
	QCOMPARE(groups[1].size(), size_t(11));
	QCOMPARE(groups[2].size(), size_t(9));
	QCOMPARE(groups[3].size(), size_t(10));
	MoveRouteModel model;
	const auto list = model.commandGroupList();
	QCOMPARE(list.size(), 4);
	QCOMPARE(list[3].toList().first().toInt(), code(MoveCode::switch_on));
	// a button asking more says so
	QCOMPARE(MoveRouteModel::buttonText(code(MoveCode::switch_on)), QStringLiteral("Move %1...").arg(code(MoveCode::switch_on)));
	QCOMPARE(MoveRouteModel::buttonText(code(MoveCode::move_up)), QStringLiteral("Move 0"));
	int with_parameters = 0;
	for (const auto& group : groups) {
		for (auto c : group) {
			with_parameters += MoveRouteModel::hasParameters(static_cast<int>(c)) ? 1 : 0;
		}
	}
	QCOMPARE(with_parameters, 4);
}

QTEST_MAIN(EventEditorTest)
#include "event_editor_test.moc"
