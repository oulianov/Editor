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

#include <QPoint>
#include <QPointF>
#include <QString>
#include <vector>
#include <lcf/rpg/enemy.h>
#include <lcf/rpg/troop.h>

/**
 * The members of a monster group on the battle screen (320 x 240), as
 * RPG Maker's Monster Group page places them: a member's position is the
 * center of the monster, as the Player uses it. Also the group's name and
 * terrains, which the page edits with them.
 */
namespace TroopLayout {

inline constexpr int ScreenWidth = 320;
inline constexpr int ScreenHeight = 240;
/** RPG Maker's limit of monsters in a group */
inline constexpr int MaxMembers = 8;

/** A position kept on the battle screen, in whole pixels */
QPoint clamp(const QPointF& pos);
/** Moves a member, kept on the battle screen */
void moveMember(lcf::rpg::Troop& troop, int index, const QPointF& pos);

/** Adds a monster in the middle of the screen; its index, -1 when the group is full */
int addMember(lcf::rpg::Troop& troop, int enemy_id);
/** Removes a member (-1: the last one), the others numbered again; false when there is none */
bool removeMember(lcf::rpg::Troop& troop, int index = -1);
void removeAllMembers(lcf::rpg::Troop& troop);
/** RPG Maker's Auto Align: one row, or two staggered rows from five monsters on */
void autoAlign(lcf::rpg::Troop& troop);

/** Hides a member at the start of the battle, until an event shows it */
void setInvisible(lcf::rpg::Troop& troop, int index, bool invisible);
/** How opaque a member is drawn: hidden ones faint, like in RPG Maker,
 *  and the monsters drawn transparent in battle */
qreal opacity(const lcf::rpg::TroopMember& member, bool transparent);

/** RPG Maker's Auto Name: "Slime*2, Bat", each monster once, in the order of the group */
QString autoName(const lcf::rpg::Troop& troop, const std::vector<lcf::rpg::Enemy>& enemies);

/** Whether the group meets the party on a terrain (index from 0): those
 *  past the end of its set are allowed (RPG_RT leaves them out) */
bool terrainAllowed(const lcf::rpg::Troop& troop, int index);
/** Allows a terrain or not; terrain_count: the terrains of the database,
 *  to which a shorter set grows */
void setTerrainAllowed(lcf::rpg::Troop& troop, int index, bool allowed, int terrain_count);

}
