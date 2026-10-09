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

#include "move_route_codec.h"
#include <lcf/encoder.h>
#include <vector>

namespace {
using It = lcf::DBArray<int32_t>::const_iterator;

int decodeInt(It& it, It end) {
	int value = 0;
	while (it != end) {
		int x = *it++;
		value = (value << 7) | (x & 0x7F);
		if (!(x & 0x80)) {
			break;
		}
	}
	return value;
}

std::string decodeString(It& it, It end, const std::string& encoding) {
	int length = decodeInt(it, end);
	std::string text;
	for (int i = 0; i < length && it != end; ++i) {
		text.push_back(static_cast<char>(*it++));
	}
	lcf::Encoder encoder(encoding);
	if (encoder.IsOk()) {
		encoder.Decode(text);
	}
	return text;
}

void encodeInt(std::vector<int32_t>& out, int value) {
	// 7 bits per value, the highest first, all but the last with bit 8 set
	std::vector<int32_t> groups;
	do {
		groups.push_back(value & 0x7F);
		value >>= 7;
	} while (value > 0);
	for (size_t i = groups.size(); i-- > 0;) {
		out.push_back(groups[i] | (i > 0 ? 0x80 : 0));
	}
}

void encodeString(std::vector<int32_t>& out, std::string text, const std::string& encoding) {
	lcf::Encoder encoder(encoding);
	if (encoder.IsOk()) {
		encoder.Encode(text);
	}
	encodeInt(out, static_cast<int>(text.size()));
	for (unsigned char c : text) {
		out.push_back(c);
	}
}
}

MoveRouteCodec::MoveEvent MoveRouteCodec::decode(const lcf::rpg::EventCommand& cmd, const std::string& encoding) {
	MoveEvent move;
	const auto& p = cmd.parameters;
	if (p.size() > 0) move.target = p[0];
	if (p.size() > 1) move.frequency = p[1];
	if (p.size() > 2) move.route.repeat = p[2] & 1;
	if (p.size() > 3) move.route.skippable = p[3] != 0;

	It it = p.begin() + std::min<size_t>(4, p.size());
	const It end = p.end();
	while (it != end) {
		lcf::rpg::MoveCommand m;
		m.command_id = *it++;
		switch (static_cast<lcf::rpg::MoveCommand::Code>(m.command_id)) {
			case lcf::rpg::MoveCommand::Code::switch_on:
			case lcf::rpg::MoveCommand::Code::switch_off:
				m.parameter_a = decodeInt(it, end);
				break;
			case lcf::rpg::MoveCommand::Code::change_graphic:
				m.parameter_string = lcf::DBString(decodeString(it, end, encoding));
				m.parameter_a = decodeInt(it, end);
				break;
			case lcf::rpg::MoveCommand::Code::play_sound_effect:
				m.parameter_string = lcf::DBString(decodeString(it, end, encoding));
				m.parameter_a = decodeInt(it, end);
				m.parameter_b = decodeInt(it, end);
				m.parameter_c = decodeInt(it, end);
				break;
			default:
				break;
		}
		move.route.move_commands.push_back(m);
	}
	return move;
}

void MoveRouteCodec::encode(const MoveEvent& move, lcf::rpg::EventCommand& cmd, const std::string& encoding) {
	// keep the other bits of the repeat parameter (Maniac Patch flags)
	const int flags = cmd.parameters.size() > 2 ? (cmd.parameters[2] & ~1) : 0;
	std::vector<int32_t> out = {move.target, move.frequency, flags | (move.route.repeat ? 1 : 0), move.route.skippable ? 1 : 0};
	for (const auto& m : move.route.move_commands) {
		out.push_back(m.command_id);
		switch (static_cast<lcf::rpg::MoveCommand::Code>(m.command_id)) {
			case lcf::rpg::MoveCommand::Code::switch_on:
			case lcf::rpg::MoveCommand::Code::switch_off:
				encodeInt(out, m.parameter_a);
				break;
			case lcf::rpg::MoveCommand::Code::change_graphic:
				encodeString(out, std::string(m.parameter_string), encoding);
				encodeInt(out, m.parameter_a);
				break;
			case lcf::rpg::MoveCommand::Code::play_sound_effect:
				encodeString(out, std::string(m.parameter_string), encoding);
				encodeInt(out, m.parameter_a);
				encodeInt(out, m.parameter_b);
				encodeInt(out, m.parameter_c);
				break;
			default:
				break;
		}
	}
	cmd.parameters = lcf::DBArray<int32_t>(out.begin(), out.end());
}
