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
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <cstdint>
#include <optional>
#include <vector>
#include <lcf/rpg/event.h>
#include <lcf/rpg/fwd.h>
#include <lcf/rpg/trooppagecondition.h>
#include "controller/edit_locks.h"

/**
 * The settings of an event page, left of its commands in the event window:
 * the conditions, the graphic, the movement, the trigger, the layer and the
 * animation, with the rules of RPG Maker 2003's window on what can be
 * changed. It edits the page in place (the working copy of the event
 * editor); every change emits changed().
 *
 * The field of a condition is enabled while its condition is on (switchA
 * enables switchAId...); the other rules are the *Enabled properties.
 */
class EventPageSettings : public QObject
{
	Q_OBJECT
	Q_PROPERTY(bool valid READ hasPage NOTIFY changed)
	Q_PROPERTY(bool rpg2k3 READ rpg2k3 WRITE setRpg2k3 NOTIFY changed)

	// the conditions
	Q_PROPERTY(bool switchA READ switchA WRITE setSwitchA NOTIFY changed)
	Q_PROPERTY(int switchAId READ switchAId WRITE setSwitchAId NOTIFY changed)
	Q_PROPERTY(bool switchB READ switchB WRITE setSwitchB NOTIFY changed)
	Q_PROPERTY(int switchBId READ switchBId WRITE setSwitchBId NOTIFY changed)
	Q_PROPERTY(bool variable READ variable WRITE setVariable NOTIFY changed)
	Q_PROPERTY(int variableId READ variableId WRITE setVariableId NOTIFY changed)
	Q_PROPERTY(int compareOperator READ compareOperator WRITE setCompareOperator NOTIFY changed)
	Q_PROPERTY(int variableValue READ variableValue WRITE setVariableValue NOTIFY changed)
	Q_PROPERTY(bool item READ item WRITE setItem NOTIFY changed)
	Q_PROPERTY(int itemId READ itemId WRITE setItemId NOTIFY changed)
	Q_PROPERTY(bool actor READ actor WRITE setActor NOTIFY changed)
	Q_PROPERTY(int actorId READ actorId WRITE setActorId NOTIFY changed)
	Q_PROPERTY(bool timer READ timer WRITE setTimer NOTIFY changed)
	Q_PROPERTY(int timerSeconds READ timerSeconds WRITE setTimerSeconds NOTIFY changed)
	Q_PROPERTY(bool timer2 READ timer2 WRITE setTimer2 NOTIFY changed)
	Q_PROPERTY(int timer2Seconds READ timer2Seconds WRITE setTimer2Seconds NOTIFY changed)

	// the graphic: setCharacter(), setTile(), clearGraphic()
	Q_PROPERTY(QString characterName READ characterName NOTIFY changed)
	Q_PROPERTY(int characterIndex READ characterIndex NOTIFY changed)
	Q_PROPERTY(int direction READ direction NOTIFY changed)
	Q_PROPERTY(int pattern READ pattern NOTIFY changed)
	Q_PROPERTY(bool hasCharacter READ hasCharacter NOTIFY changed)
	Q_PROPERTY(bool tile READ isTile NOTIFY changed)
	Q_PROPERTY(bool translucent READ translucent WRITE setTranslucent NOTIFY changed)

	// the movement and the rest
	Q_PROPERTY(int moveType READ moveType WRITE setMoveType NOTIFY changed)
	Q_PROPERTY(int moveFrequency READ moveFrequency WRITE setMoveFrequency NOTIFY changed)
	Q_PROPERTY(int moveSpeed READ moveSpeed WRITE setMoveSpeed NOTIFY changed)
	Q_PROPERTY(int trigger READ trigger WRITE setTrigger NOTIFY changed)
	Q_PROPERTY(int layer READ layer WRITE setLayer NOTIFY changed)
	Q_PROPERTY(bool overlapForbidden READ overlapForbidden WRITE setOverlapForbidden NOTIFY changed)
	Q_PROPERTY(int animationType READ animationType WRITE setAnimationType NOTIFY changed)

	// the rules
	Q_PROPERTY(int variableMinimum READ variableMinimum NOTIFY changed)
	Q_PROPERTY(int variableMaximum READ variableMaximum NOTIFY changed)
	Q_PROPERTY(bool compareOperatorEnabled READ compareOperatorEnabled NOTIFY changed)
	Q_PROPERTY(bool timer2Allowed READ timer2Allowed NOTIFY changed)
	Q_PROPERTY(bool moveFrequencyEnabled READ moveFrequencyEnabled NOTIFY changed)
	Q_PROPERTY(bool moveRouteEnabled READ moveRouteEnabled NOTIFY changed)
	Q_PROPERTY(bool animationTypeEnabled READ animationTypeEnabled NOTIFY changed)
	Q_PROPERTY(bool animates READ animates NOTIFY changed)

public:
	/** The lists of choices of the page */
	enum List { CompareOperators, MoveTypes, MoveFrequencies, Triggers, Layers, AnimationTypes, MoveSpeeds };
	Q_ENUM(List)

	explicit EventPageSettings(QObject* parent = nullptr);

	/** The page edited, nullptr for none */
	void setPage(lcf::rpg::EventPage* page);
	lcf::rpg::EventPage* page() const;
	bool hasPage() const;

	/** RPG Maker 2003: the comparison of the variable, the second timer
	 *  and the larger values */
	bool rpg2k3() const;
	void setRpg2k3(bool rpg2k3);

	bool switchA() const;
	void setSwitchA(bool on);
	int switchAId() const;
	void setSwitchAId(int id);
	bool switchB() const;
	void setSwitchB(bool on);
	int switchBId() const;
	void setSwitchBId(int id);
	bool variable() const;
	void setVariable(bool on);
	int variableId() const;
	void setVariableId(int id);
	/** lcf::rpg::EventPageCondition::Comparison */
	int compareOperator() const;
	void setCompareOperator(int comparison);
	int variableValue() const;
	void setVariableValue(int value);
	bool item() const;
	void setItem(bool on);
	int itemId() const;
	void setItemId(int id);
	bool actor() const;
	void setActor(bool on);
	int actorId() const;
	void setActorId(int id);
	bool timer() const;
	void setTimer(bool on);
	/** The whole time, minutes * 60 + seconds */
	int timerSeconds() const;
	void setTimerSeconds(int seconds);
	bool timer2() const;
	void setTimer2(bool on);
	int timer2Seconds() const;
	void setTimer2Seconds(int seconds);

	QString characterName() const;
	int characterIndex() const;
	/** lcf::rpg::EventPage::Direction */
	int direction() const;
	/** lcf::rpg::EventPage::Frame */
	int pattern() const;
	/** A character of a charset (else a tile of the upper layer, or nothing) */
	bool hasCharacter() const;
	/** A tile of the upper layer; nothing is tile 0 */
	bool isTile() const;
	bool translucent() const;
	void setTranslucent(bool translucent);

	/** A character, its facing and its frame (Select Event Graphic) */
	Q_INVOKABLE void setCharacter(const QString& name, int index, int direction, int pattern);
	/** A tile of the upper layer. passable: the chipset's flags of the tile
	 *  (upperTileFlags()), which give the layer; -1 keeps the layer */
	Q_INVOKABLE void setTile(int tile, int passable = -1);
	/** No graphic */
	Q_INVOKABLE void clearGraphic();
	/** The passability flags of a tile of the upper layer in the chipset
	 *  (all directions past the data), -1 when there is no such chipset */
	static int upperTileFlags(const std::vector<lcf::rpg::Chipset>& chipsets, int chipset_id, int tile);

	/** lcf::rpg::EventPage::MoveType */
	int moveType() const;
	void setMoveType(int type);
	int moveFrequency() const;
	void setMoveFrequency(int frequency);
	/** lcf::rpg::EventPage::MoveSpeed (1 to 6) */
	int moveSpeed() const;
	void setMoveSpeed(int speed);
	/** The custom route (Custom Pattern) */
	lcf::rpg::MoveRoute moveRoute() const;
	void setMoveRoute(const lcf::rpg::MoveRoute& route);
	/** lcf::rpg::EventPage::Trigger */
	int trigger() const;
	void setTrigger(int trigger);
	/** lcf::rpg::EventPage::Layers */
	int layer() const;
	void setLayer(int layer);
	bool overlapForbidden() const;
	void setOverlapForbidden(bool forbidden);
	/** lcf::rpg::EventPage::AnimType */
	int animationType() const;
	void setAnimationType(int type);

	/** The value of the variable, by version */
	int variableMinimum() const;
	int variableMaximum() const;
	/** The comparison is RPG Maker 2003's (2000 compares with >=) */
	bool compareOperatorEnabled() const;
	/** The second timer is RPG Maker 2003's (its seconds: timer2) */
	bool timer2Allowed() const;
	/** Not for an event standing still */
	bool moveFrequencyEnabled() const;
	/** The route of Custom Pattern */
	bool moveRouteEnabled() const;
	/** A tile or nothing does not animate (RPG Maker greys the list) */
	bool animationTypeEnabled() const;
	/** The graphic moves standing still: steps (continuous) or turns (spin) */
	bool animates() const;
	/** EasyRPG Player (Game_Character::UpdateAnimation) for an event
	 *  standing still: frames (at 60 a second) per step or quarter turn, by
	 *  move speed 1 to 6 */
	static int stepFrames(int move_speed);
	static int turnFrames(int move_speed);

	/** The items of a list as the window shows them: {text, value} */
	Q_INVOKABLE static QVariantList items(List list);

signals:
	void changed();

private:
	/** Sets a field of the page, changed() when it differs */
	template<typename T, typename V>
	void set(T& field, V value);

	lcf::rpg::EventPage* m_page = nullptr;
	bool m_rpg2k3 = true;
};

/**
 * The event window of a map event (RPG Maker's event editor) without its
 * QML view (EventEditorWindow.qml): a working copy of the event with its pages, the page shown, New,
 * Copy, Paste, Delete and Clear Page, and Apply, OK and Cancel.
 *
 * Apply puts the event on the map right away (applied(), which the map
 * commits: MapDocument::commitEvent); OK gives result() Ok when the event
 * changed, and accepted() (QmlShell commits resultEvent() once the window
 * is closed instead). Later changes are compared to what was applied. A new event
 * (New Event, not on the map yet) is put on the map by OK or Apply even
 * unchanged, like RPG Maker.
 *
 * While open it holds an EditLock::Event on its map: the maps can be read
 * again while it is open (reloadEvent()), not the database or the map tree.
 *
 * The commands of each page are edited by an EventCommandListModel on
 * commands(); it normalizes their indents when it is given them: call
 * resetModified() once every page has its list.
 */
class EventEditorController : public QObject
{
	Q_OBJECT
	Q_PROPERTY(int mapId READ mapId WRITE setMapId NOTIFY mapIdChanged)
	Q_PROPERTY(bool rpg2k3 READ rpg2k3 WRITE setRpg2k3 NOTIFY rpg2k3Changed)
	Q_PROPERTY(int eventId READ eventId NOTIFY eventChanged)
	Q_PROPERTY(bool newEvent READ isNewEvent NOTIFY eventChanged)
	Q_PROPERTY(QString title READ title NOTIFY eventChanged)
	Q_PROPERTY(int x READ x NOTIFY eventChanged)
	Q_PROPERTY(int y READ y NOTIFY eventChanged)
	Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
	Q_PROPERTY(int pageCount READ pageCount NOTIFY pagesChanged)
	Q_PROPERTY(int currentPage READ currentPage WRITE setCurrentPage NOTIFY currentPageChanged)
	Q_PROPERTY(EventPageSettings* page READ pageSettings CONSTANT)
	Q_PROPERTY(bool canNewPage READ canNewPage NOTIFY pageActionsChanged)
	Q_PROPERTY(bool canPastePage READ canPastePage NOTIFY pageActionsChanged)
	Q_PROPERTY(bool canDeletePage READ canDeletePage NOTIFY pageActionsChanged)
	Q_PROPERTY(bool modified READ isModified NOTIFY edited)
	Q_PROPERTY(Result result READ result NOTIFY resultChanged)
	Q_PROPERTY(bool open READ isOpen WRITE setOpen NOTIFY openChanged)
	Q_PROPERTY(bool reloadPending READ isReloadPending NOTIFY reloadPendingChanged)

public:
	/** RPG Maker's limit of pages in an event */
	static constexpr int MaxPages = 100;

	/** How the window was closed, what the map takes from it */
	enum Result { Cancel, Apply, Ok };
	Q_ENUM(Result)
	/** What reloadEvent() did */
	enum Reload {
		/** The new version is shown */
		Reloaded,
		/** The event was changed here: the interface asks, then
		 *  acceptReload() or rejectReload() */
		AskFirst,
		/** The event is gone: the window closes (result Cancel) */
		Removed,
	};
	Q_ENUM(Reload)

	explicit EventEditorController(int map_id = 0, QObject* parent = nullptr);
	~EventEditorController() override;

	int mapId() const;
	void setMapId(int map_id);
	bool rpg2k3() const;
	void setRpg2k3(bool rpg2k3);

	/** The event edited: a copy, its pages numbered from 1 in order (as
	 *  RPG Maker saves them); the result starts over. is_new: not on the
	 *  map yet (New Event) */
	void setEvent(const lcf::rpg::Event& event, bool is_new = false);
	/** Not on the map yet: OK and Apply put it there, even unchanged */
	bool isNewEvent() const;
	/** The working copy */
	const lcf::rpg::Event& event() const;
	/** What the window gives: the event applied (Apply), the working copy
	 *  (Ok), else the event as it was */
	lcf::rpg::Event resultEvent() const;

	int eventId() const;
	/** The title of the window: "EV: 1" */
	QString title() const;
	int x() const;
	int y() const;
	QString name() const;
	void setName(const QString& name);

	int pageCount() const;
	/** The page shown (from 0), -1 for none */
	int currentPage() const;
	void setCurrentPage(int index);
	/** The previous (-1) or next (1) page, kept in the pages */
	Q_INVOKABLE void switchPage(int step);
	/** A page of the working copy, nullptr when out of range; the pointers
	 *  change with the pages (pagesChanged) */
	lcf::rpg::EventPage* page(int index);
	std::vector<lcf::rpg::EventCommand>* commands(int index);
	/** The settings of the page shown */
	EventPageSettings* pageSettings() const;
	/** The script of a page for rpgsync: --map, --event, --page */
	Q_INVOKABLE QStringList scriptLocation(int index) const;

	/*
	 * The pages: each returns the page to show, -1 when nothing was done
	 */

	bool canNewPage() const;
	bool canPastePage() const;
	bool canDeletePage() const;
	/** After the page shown, with its look and movement and new conditions,
	 *  as RPG Maker; a new page when there is none */
	Q_INVOKABLE int newPage();
	/** The page shown goes to the clipboard of every event, like RPG Maker */
	Q_INVOKABLE bool copyPage();
	/** The page copied, after the page shown */
	Q_INVOKABLE int pastePage();
	/** The page shown, while there are others: the next one takes its
	 *  place (the last one at the end) */
	Q_INVOKABLE int deletePage();
	/** The page shown starts over (keeping its number) */
	Q_INVOKABLE int clearPage();
	static bool hasPageClipboard();

	/*
	 * Apply, OK and Cancel
	 */

	Result result() const;
	/** Changed since it was shown, or last applied; a new event until it is applied */
	bool isModified() const;
	/** The event as it is now is the reference: modified() is false (but
	 *  for a new event) */
	Q_INVOKABLE void resetModified();
	/** The commands of a page changed: modified() may have */
	Q_INVOKABLE void markEdited();
	/** Puts the event on the map now (applied()); false when unchanged */
	Q_INVOKABLE bool apply();
	/** OK: the result, Ok when the event changed (accepted()), else Cancel */
	Q_INVOKABLE Result ok();

	/** The event was changed by another program (nullptr: it was removed).
	 *  Shows the new version, on the same page, unless it was changed here */
	Reload reloadEvent(const lcf::rpg::Event* event);
	bool isReloadPending() const;
	/** Shows the new version waiting, the changes made here are lost */
	Q_INVOKABLE void acceptReload();
	/** Keeps the version edited */
	Q_INVOKABLE void rejectReload();

	/** While open it holds the EditLock of its map */
	bool isOpen() const;
	void setOpen(bool open);

	/** The same event (all but the numbers of the pages) */
	static bool equalEvents(const lcf::rpg::Event& e1, const lcf::rpg::Event& e2);

signals:
	void mapIdChanged();
	void rpg2k3Changed();
	/** Another event, or another version of it */
	void eventChanged();
	void nameChanged();
	/** Pages added, removed or replaced: their pointers changed */
	void pagesChanged();
	void currentPageChanged(int index);
	void pageActionsChanged();
	/** Anything edited (modified() may have changed) */
	void edited();
	void resultChanged();
	void openChanged();
	void reloadPendingChanged();
	/** Apply: the event as it is now, to put on the map */
	void applied(const lcf::rpg::Event& event);
	/** OK with changes: the event to put on the map */
	void accepted(const lcf::rpg::Event& event);
	/** The event was removed by another program: the window closes */
	void removed();

private:
	void setResult(Result result);
	/** The pages numbered again; pagesChanged, the settings follow */
	void pagesEdited(int current);
	void updatePageSettings();

	int m_mapId = 0;
	bool m_rpg2k3 = true;
	lcf::rpg::Event m_event;
	/** What modified() compares to: as shown, or as last applied */
	lcf::rpg::Event m_reference;
	/** The event last applied */
	lcf::rpg::Event m_applied;
	Result m_result = Cancel;
	bool m_new = false;
	int m_currentPage = -1;
	EventPageSettings* m_pageSettings = nullptr;
	std::optional<lcf::rpg::Event> m_pendingReload;
	EditLocks::Lock m_lock;
	/** Copy Page: shared by the events of every map, like RPG Maker */
	static std::optional<lcf::rpg::EventPage> s_pageClipboard;
};

/**
 * The trigger conditions of a battle event page (TroopPageCondition) as
 * RPG Maker's "Define Battle Event Triggers" window shows them: a row per
 * condition, whose check box enables its fields. The conditions of RPG
 * Maker 2003 stay visible (and keep their data) in 2000 games, locked.
 * A copy is edited: condition() is written back on OK.
 */
class BattlePageConditions : public QObject
{
	Q_OBJECT
	Q_PROPERTY(bool rpg2k3 READ rpg2k3 CONSTANT)
	Q_PROPERTY(int variableMaximum READ variableMaximum CONSTANT)

	// the rows checked (isChecked) and whose fields can be changed (fieldsEnabled), by Row
	Q_PROPERTY(QVariantList checkedRows READ checkedRows NOTIFY changed)
	Q_PROPERTY(QVariantList enabledRows READ enabledRows NOTIFY changed)
	Q_PROPERTY(QVariantList allowedRows READ allowedRows CONSTANT)

	// the fields of the rows
	Q_PROPERTY(int switchAId READ switchAId WRITE setSwitchAId NOTIFY changed)
	Q_PROPERTY(int switchBId READ switchBId WRITE setSwitchBId NOTIFY changed)
	Q_PROPERTY(int variableId READ variableId WRITE setVariableId NOTIFY changed)
	Q_PROPERTY(int variableValue READ variableValue WRITE setVariableValue NOTIFY changed)
	Q_PROPERTY(int turnA READ turnA WRITE setTurnA NOTIFY changed)
	Q_PROPERTY(int turnB READ turnB WRITE setTurnB NOTIFY changed)
	Q_PROPERTY(int fatigueMin READ fatigueMin WRITE setFatigueMin NOTIFY changed)
	Q_PROPERTY(int fatigueMax READ fatigueMax WRITE setFatigueMax NOTIFY changed)
	Q_PROPERTY(int enemyId READ enemyId WRITE setEnemyId NOTIFY changed)
	Q_PROPERTY(int enemyHpMin READ enemyHpMin WRITE setEnemyHpMin NOTIFY changed)
	Q_PROPERTY(int enemyHpMax READ enemyHpMax WRITE setEnemyHpMax NOTIFY changed)
	Q_PROPERTY(int actorId READ actorId WRITE setActorId NOTIFY changed)
	Q_PROPERTY(int actorHpMin READ actorHpMin WRITE setActorHpMin NOTIFY changed)
	Q_PROPERTY(int actorHpMax READ actorHpMax WRITE setActorHpMax NOTIFY changed)
	Q_PROPERTY(int turnEnemyId READ turnEnemyId WRITE setTurnEnemyId NOTIFY changed)
	Q_PROPERTY(int turnEnemyA READ turnEnemyA WRITE setTurnEnemyA NOTIFY changed)
	Q_PROPERTY(int turnEnemyB READ turnEnemyB WRITE setTurnEnemyB NOTIFY changed)
	Q_PROPERTY(int turnActorId READ turnActorId WRITE setTurnActorId NOTIFY changed)
	Q_PROPERTY(int turnActorA READ turnActorA WRITE setTurnActorA NOTIFY changed)
	Q_PROPERTY(int turnActorB READ turnActorB WRITE setTurnActorB NOTIFY changed)
	Q_PROPERTY(int commandActorId READ commandActorId WRITE setCommandActorId NOTIFY changed)
	Q_PROPERTY(int commandId READ commandId WRITE setCommandId NOTIFY changed)

	/** The monsters of the group ({text: "1:Slime", value: its place from 0}) and
	 *  the battle commands ({text: "0001:Attack", value: ID}), with the
	 *  values of the condition they lack (kept as they are); see setChoices */
	Q_PROPERTY(QVariantList members READ members CONSTANT)
	Q_PROPERTY(QVariantList battleCommands READ battleCommands CONSTANT)

public:
	/** The rows of the window, in its order (and the flags') */
	enum Row { SwitchA, SwitchB, Variable, Turn, Fatigue, EnemyHp, ActorHp, TurnEnemy, TurnActor, Command, RowCount };
	Q_ENUM(Row)

	BattlePageConditions(const lcf::rpg::TroopPageCondition& condition, bool rpg2k3, QObject* parent = nullptr);

	lcf::rpg::TroopPageCondition& condition();
	const lcf::rpg::TroopPageCondition& condition() const;
	bool rpg2k3() const;
	/** The value of the variable is between -max and max */
	int variableMaximum() const;

	Q_INVOKABLE bool isChecked(int row) const;
	Q_INVOKABLE void setChecked(int row, bool checked);
	/** The check box can be used: RPG Maker 2003's rows only in its games */
	Q_INVOKABLE bool isAllowed(int row) const;
	/** The fields of the row: allowed and checked */
	Q_INVOKABLE bool fieldsEnabled(int row) const;
	static bool allowed(int row, bool rpg2k3);
	QVariantList checkedRows() const;
	QVariantList enabledRows() const;
	QVariantList allowedRows() const;

	int switchAId() const;
	void setSwitchAId(int id);
	int switchBId() const;
	void setSwitchBId(int id);
	int variableId() const;
	void setVariableId(int id);
	int variableValue() const;
	void setVariableValue(int value);
	/** Turns a * n + b (b alone when a is 0) */
	int turnA() const;
	void setTurnA(int turns);
	int turnB() const;
	void setTurnB(int turns);
	int fatigueMin() const;
	void setFatigueMin(int percent);
	int fatigueMax() const;
	void setFatigueMax(int percent);
	/** A monster of the group by its place in it (from 0) */
	int enemyId() const;
	void setEnemyId(int index);
	int enemyHpMin() const;
	void setEnemyHpMin(int percent);
	int enemyHpMax() const;
	void setEnemyHpMax(int percent);
	int actorId() const;
	void setActorId(int id);
	int actorHpMin() const;
	void setActorHpMin(int percent);
	int actorHpMax() const;
	void setActorHpMax(int percent);
	int turnEnemyId() const;
	void setTurnEnemyId(int index);
	int turnEnemyA() const;
	void setTurnEnemyA(int turns);
	int turnEnemyB() const;
	void setTurnEnemyB(int turns);
	int turnActorId() const;
	void setTurnActorId(int id);
	int turnActorA() const;
	void setTurnActorA(int turns);
	int turnActorB() const;
	void setTurnActorB(int turns);
	int commandActorId() const;
	void setCommandActorId(int id);
	int commandId() const;
	void setCommandId(int id);

	/** The lists of the window from the database and the monsters of the group */
	void setChoices(const lcf::rpg::Database& database, const lcf::rpg::Troop& troop);
	QVariantList members() const;
	QVariantList battleCommands() const;

	/** The "Trigger" line of a battle page: "Switch [0001:xxx] ON, Turns [3x+1]" */
	static QString text(const lcf::rpg::Database& database, const lcf::rpg::Troop& troop,
		const lcf::rpg::TroopPageCondition& condition);
	/** A monster of the group by its place in it (from 0, as the Player
	 *  reads them): "1:Slime" */
	static QString memberName(const lcf::rpg::Database& database, const lcf::rpg::Troop& troop, int index);
	/** "0001:name" */
	static QString idName(const QString& name, int id);

signals:
	void changed();

private:
	/** Sets a field of the condition, changed() when it differs */
	void set(int32_t& field, int value);

	lcf::rpg::TroopPageCondition m_condition;
	bool m_rpg2k3;
	QVariantList m_members;
	QVariantList m_battleCommands;
};
