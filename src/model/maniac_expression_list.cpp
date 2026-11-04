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

#include "maniac_expression_list.h"
#include "maniac_expression.h"

namespace {
std::vector<uint8_t> bytesOf(std::span<const int32_t> words) {
	std::vector<uint8_t> bytes;
	for (int32_t word : words) {
		const auto value = static_cast<uint32_t>(word);
		for (int shift = 0; shift < 32; shift += 8) {
			bytes.push_back(static_cast<uint8_t>(value >> shift));
		}
	}
	return bytes;
}

std::vector<int32_t> wordsOf(const std::vector<uint8_t>& bytes, size_t from, size_t to) {
	std::vector<int32_t> words((to - from + 3) / 4, 0);
	for (size_t i = from; i < to; ++i) {
		words[(i - from) / 4] |= static_cast<int32_t>(static_cast<uint32_t>(bytes[i]) << (8 * ((i - from) % 4)));
	}
	return words;
}

/**
 * Where the expression at pos ends, as the Player reads its bytes (the
 * operation codes of maniac_patch.cpp): false past the end of the bytes
 */
bool skip(const std::vector<uint8_t>& bytes, size_t& pos, int depth = 0) {
	if (depth > 256 || pos >= bytes.size()) {
		return false;
	}
	const int op = bytes[pos++];
	auto children = [&](int count) {
		for (int i = 0; i < count; ++i) {
			if (!skip(bytes, pos, depth + 1)) {
				return false;
			}
		}
		return true;
	};
	auto take = [&](size_t count) {
		pos += count;
		return pos <= bytes.size();
	};
	switch (op) {
		// numbers: 1, 2 or 4 bytes (decimal or hexadecimal)
		case 1: case 4: return take(1);
		case 2: case 5: return take(2);
		case 3: case 6: return take(4);
		// a variable, a switch (and through a variable)
		case 8: case 9: case 13: case 14: return children(1);
		// an array: its count, on 4 more bytes from 0x80
		case 19: {
			if (pos >= bytes.size()) {
				return false;
			}
			size_t count = bytes[pos++];
			if (count & 0x80) {
				if (pos + 4 > bytes.size()) {
					return false;
				}
				count = 0;
				for (int i = 0; i < 4; ++i) {
					count |= static_cast<size_t>(bytes[pos++]) << (8 * i);
				}
				if (count > bytes.size()) {
					return false;
				}
			}
			return children(static_cast<int>(count));
		}
		// negation, not, flip
		case 24: case 25: case 26: return children(1);
		// range, subscript
		case 66: case 67: return children(2);
		case 72: return children(3);
		// a function: its number, the count of its arguments
		case 78: {
			if (pos + 2 > bytes.size() || (bytes[pos + 1] & 0x80)) {
				return false;
			}
			const int count = bytes[pos + 1];
			pos += 2;
			return children(count);
		}
		default:
			// assignments and binary operations
			if ((op >= 34 && op <= 44) || (op >= 48 && op <= 65)) {
				return children(2);
			}
			return false;
	}
}
}

bool ManiacExpressionList::decode(std::span<const int32_t> words, std::vector<std::string>& texts) {
	texts.clear();
	const std::vector<uint8_t> bytes = bytesOf(words);
	size_t pos = 0;
	while (pos < bytes.size() && bytes[pos] != 0) {
		const size_t start = pos;
		if (!skip(bytes, pos)) {
			return false;
		}
		const std::vector<int32_t> one = wordsOf(bytes, start, pos);
		std::string text;
		if (!ManiacExpression::decode(one, text)) {
			return false;
		}
		texts.push_back(text);
	}
	return true;
}

bool ManiacExpressionList::encode(const std::vector<std::string>& texts, std::vector<int32_t>& words, size_t& expression,
	size_t& error) {
	std::vector<uint8_t> bytes;
	for (size_t i = 0; i < texts.size(); ++i) {
		std::vector<int32_t> one;
		if (!ManiacExpression::encode(texts[i], one, error)) {
			expression = i;
			return false;
		}
		// without the zeros completing its last word
		const std::vector<uint8_t> own = bytesOf(one);
		size_t end = 0;
		if (!skip(own, end)) {
			expression = i;
			error = 0;
			return false;
		}
		bytes.insert(bytes.end(), own.begin(), own.begin() + static_cast<std::ptrdiff_t>(end));
	}
	words = wordsOf(bytes, 0, bytes.size());
	return true;
}
