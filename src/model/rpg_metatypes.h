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

#include <QMetaType>
#include <lcf/rpg/database.h>

/** The database entries in QVariants and in signals: declared before any
 *  use (a signal of a widget, the binding of its fields) */
Q_DECLARE_METATYPE(lcf::rpg::Actor*)
Q_DECLARE_METATYPE(lcf::rpg::Skill*)
Q_DECLARE_METATYPE(lcf::rpg::Enemy*)
Q_DECLARE_METATYPE(lcf::rpg::Item*)
Q_DECLARE_METATYPE(lcf::rpg::Troop*)
Q_DECLARE_METATYPE(lcf::rpg::Terrain*)
Q_DECLARE_METATYPE(lcf::rpg::Attribute*)
Q_DECLARE_METATYPE(lcf::rpg::State*)
Q_DECLARE_METATYPE(lcf::rpg::Animation*)
Q_DECLARE_METATYPE(lcf::rpg::Chipset*)
Q_DECLARE_METATYPE(lcf::rpg::CommonEvent*)
Q_DECLARE_METATYPE(lcf::rpg::Class*)
Q_DECLARE_METATYPE(lcf::rpg::BattlerAnimation*)
Q_DECLARE_METATYPE(lcf::rpg::Switch*)
Q_DECLARE_METATYPE(lcf::rpg::Variable*)
