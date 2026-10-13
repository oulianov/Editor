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

// The texts of a Move Event route (a charset, a sound) are in the code page
// of the game in the command, and in UTF-8 in the Editor, like the other
// strings liblcf reads: an accented name survives an edit of the route

#include "model/move_route_codec.h"
#include <algorithm>
#include <cstdio>
#include <vector>

namespace {
int failures = 0;

void check(bool condition, const char* what) {
	if (!condition) {
		std::printf("FAILED: %s\n", what);
		++failures;
	}
}

lcf::rpg::MoveCommand command(lcf::rpg::MoveCommand::Code code, const char* text) {
	lcf::rpg::MoveCommand m;
	m.command_id = static_cast<int>(code);
	m.parameter_string = lcf::DBString(text);
	return m;
}
}

int main() {
	// UTF-8, as the Editor has them: "Héros" and "Épée"
	const char* hero = "H\xC3\xA9ros";
	const char* sword = "\xC3\x89p\xC3\xA9" "e";

	MoveRouteCodec::MoveEvent move;
	auto graphic = command(lcf::rpg::MoveCommand::Code::change_graphic, hero);
	graphic.parameter_a = 3;
	auto sound = command(lcf::rpg::MoveCommand::Code::play_sound_effect, sword);
	sound.parameter_a = 90;
	sound.parameter_b = 100;
	sound.parameter_c = 50;
	move.route.move_commands = {graphic, sound};

	lcf::rpg::EventCommand cmd;
	MoveRouteCodec::encode(move, cmd, "1252");

	// in the command, Windows-1252: one byte a letter, é is 0xE9
	const std::vector<int32_t> written(cmd.parameters.begin(), cmd.parameters.end());
	const std::vector<int32_t> hero_1252 = {'H', 0xE9, 'r', 'o', 's'};
	check(written.size() > 11, "the route is written");
	if (written.size() > 11) {
		check(written[4] == static_cast<int>(lcf::rpg::MoveCommand::Code::change_graphic), "the first move is the graphic");
		check(written[5] == 5, "the name has 5 bytes in the code page");
		check(std::equal(hero_1252.begin(), hero_1252.end(), written.begin() + 6), "\"Héros\" is written in Windows-1252");
	}

	// read back: UTF-8 again, the numbers unchanged
	const auto back = MoveRouteCodec::decode(cmd, "1252");
	check(back.route.move_commands.size() == 2, "two moves read back");
	if (back.route.move_commands.size() == 2) {
		const auto& g = back.route.move_commands[0];
		const auto& s = back.route.move_commands[1];
		check(std::string(g.parameter_string) == hero, "the graphic name read back in UTF-8");
		check(g.parameter_a == 3, "the graphic index read back");
		check(std::string(s.parameter_string) == sword, "the sound name read back in UTF-8");
		check(s.parameter_a == 90 && s.parameter_b == 100 && s.parameter_c == 50, "the sound settings read back");
	}

	if (failures == 0) {
		std::printf("move route codec: all passed\n");
	}
	return failures == 0 ? 0 : 1;
}
