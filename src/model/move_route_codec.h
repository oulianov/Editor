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

#include <lcf/rpg/eventcommand.h>
#include <lcf/rpg/moveroute.h>
#include <string>

/**
 * The Move Event command stores its route in its parameters:
 * target, frequency, repeat, skippable, then the moves. A move is its code,
 * followed for some by a switch, or by a text (length then bytes in the
 * encoding of the game) and numbers; numbers are 7 bits per value, highest
 * bits first, like in EasyRPG Player.
 */
namespace MoveRouteCodec {
	struct MoveEvent {
		int target = 10005;  // this event
		int frequency = 6;
		lcf::rpg::MoveRoute route;
	};

	MoveEvent decode(const lcf::rpg::EventCommand& cmd, const std::string& encoding);
	void encode(const MoveEvent& move, lcf::rpg::EventCommand& cmd, const std::string& encoding);
}
