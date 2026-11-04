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
 * Maniac Patch expressions one after the other (the parameters of Call
 * Command, from parameter 5): their bytes follow each other, a Null byte
 * (or the end) after the last one, as EasyRPG Player reads them
 * (ManiacPatch::ParseExpressions). Each one is a ManiacExpression.
 */
namespace ManiacExpressionList {
	/** The texts of the expressions of the words; false if one could not be read */
	bool decode(std::span<const int32_t> words, std::vector<std::string>& texts);
	/** The words of the expressions; false with the expression in error and the offset of the error in its text */
	bool encode(const std::vector<std::string>& texts, std::vector<int32_t>& words, size_t& expression, size_t& error);
}
