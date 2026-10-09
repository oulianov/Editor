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

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

/**
 * Maniac Patch expressions (Control Variables, Conditional Branch...): a
 * prefix bytecode, 4 bytes per parameter (lowest byte first), as EasyRPG
 * Player reads it (maniac_patch.cpp, ParseExpression).
 *
 * The text is the Editor's notation, C-like:
 *   V[1] + V[V[2]] * 3, S[4] && !S[5], V[1..5][2], [1, 2, 3][V[1]],
 *   V[1] = 0x10 << 2, V[3] ? rand(1, 6) : -V[2], clamp(V[1], 0, 99)
 * Functions: rand item event actor party enemy misc pow sqrt sin cos atan2
 * min max abs clamp muldiv divmul between lerp arraysum arraymin arraymax,
 * fnN(...) for an unknown function N.
 */
namespace ManiacExpression {
	/** The text of the first expression of the words; false if the Player could not read it */
	bool decode(std::span<const int32_t> words, std::string& text);
	/** The words of text; false with the offset of the error in text */
	bool encode(const std::string& text, std::vector<int32_t>& words, size_t& error);
}
