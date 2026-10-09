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

#include "maniac_expression.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <memory>

namespace {
// the operation codes of EasyRPG Player (maniac_patch.cpp, Op)
enum Op {
	Null = 0, U8, U16, S32, UX8, UX16, SX32,
	Var = 8, Switch, VarIndirect = 13, SwitchIndirect,
	Array = 19, Negate = 24, Not, Flip,
	AssignFirst = 34, AssignLast = 44,
	BinaryFirst = 48, BinaryLast = 65,
	Range = 66, Subscript, Ternary = 72, Function = 78
};

// operator precedence, lowest first
enum Prec { Assign = 1, TernaryPrec, RangePrec, LogicalOr, LogicalAnd, BitOr, BitXor, BitAnd,
	Equality, Relation, Shift, Additive, Multiplicative, Unary, Postfix, Atom };

struct Operator {
	int op;
	const char* text;
	int prec;
};

constexpr std::array<Operator, 18> binaries = {{
	{48, "+", Additive}, {49, "-", Additive}, {50, "*", Multiplicative}, {51, "/", Multiplicative},
	{52, "%", Multiplicative}, {53, "|", BitOr}, {54, "&", BitAnd}, {55, "^", BitXor},
	{56, "<<", Shift}, {57, ">>", Shift}, {58, "==", Equality}, {59, ">=", Relation},
	{60, "<=", Relation}, {61, ">", Relation}, {62, "<", Relation}, {63, "!=", Equality},
	{64, "||", LogicalOr}, {65, "&&", LogicalAnd}
}};

constexpr std::array<const char*, 11> assignments = {"=", "+=", "-=", "*=", "/=", "%=", "|=", "&=", "^=", "<<=", ">>="};

struct Builtin {
	const char* name;
	int arguments;
};

// Fn of EasyRPG Player, with the number of arguments it requires
constexpr std::array<Builtin, 23> functions = {{
	{"rand", 2}, {"item", 2}, {"event", 2}, {"actor", 2}, {"party", 2}, {"enemy", 2}, {"misc", 1},
	{"pow", 2}, {"sqrt", 2}, {"sin", 3}, {"cos", 3}, {"atan2", 3}, {"min", 2}, {"max", 2}, {"abs", 1},
	{"clamp", 3}, {"muldiv", 3}, {"divmul", 3}, {"between", 3}, {"lerp", 4},
	{"arraysum", 2}, {"arraymin", 2}, {"arraymax", 2}
}};

constexpr int maxDepth = 256;

std::string hex(uint32_t value) {
	char buffer[16];
	std::snprintf(buffer, sizeof(buffer), "0x%X", value);
	return buffer;
}

// Decoding: each node prints itself with its precedence, the parent adds parentheses

struct Text {
	std::string text;
	int prec = Atom;
	// the elements of an array of two or more, printed without brackets in V[...]
	std::string list;
	bool literal = false;
};

class Decoder {
public:
	explicit Decoder(std::span<const int32_t> words) {
		for (int32_t word : words) {
			const auto value = static_cast<uint32_t>(word);
			for (int shift = 0; shift < 32; shift += 8) {
				m_bytes.push_back(static_cast<int>((value >> shift) & 0xFF));
			}
		}
	}

	bool node(Text& out, int depth = 0) {
		if (depth > maxDepth || m_pos >= m_bytes.size()) {
			return false;
		}
		const int op = m_bytes[m_pos++];
		switch (op) {
			case U8:
			case UX8:
			case U16:
			case UX16:
			case S32:
			case SX32: {
				const int size = op == U8 || op == UX8 ? 1 : op == U16 || op == UX16 ? 2 : 4;
				uint32_t value = 0;
				for (int i = 0; i < size; ++i) {
					int byte;
					if (!next(byte)) {
						return false;
					}
					value |= static_cast<uint32_t>(byte) << (8 * i);
				}
				const auto number = static_cast<int32_t>(value);
				const bool isHex = op >= UX8;
				out.literal = true;
				out.prec = number < 0 ? Unary : Atom;
				out.text = number < 0 ? "-" + (isHex ? hex(0u - value) : std::to_string(-static_cast<int64_t>(number)))
					: isHex ? hex(value) : std::to_string(number);
				return true;
			}
			case Var:
			case Switch:
			case VarIndirect:
			case SwitchIndirect: {
				Text inner;
				if (!node(inner, depth + 1)) {
					return false;
				}
				const std::string content = inner.list.empty() ? inner.text : inner.list;
				const std::string name = op == Var || op == VarIndirect ? "V" : "S";
				out.text = op == Var || op == Switch ? name + "[" + content + "]" : name + "[V[" + content + "]]";
				return true;
			}
			case Array: {
				int count;
				if (!next(count)) {
					return false;
				}
				if (count & 0x80) {
					uint32_t value = 0;
					for (int i = 0; i < 4; ++i) {
						int byte;
						if (!next(byte)) {
							return false;
						}
						value |= static_cast<uint32_t>(byte) << (8 * i);
					}
					// more elements than bytes: the Player would read past the end
					if (value > m_bytes.size()) {
						return false;
					}
					count = static_cast<int>(value);
				}
				std::string list;
				for (int i = 0; i < count; ++i) {
					Text element;
					if (!node(element, depth + 1)) {
						return false;
					}
					list += (i ? ", " : "") + wrap(element, Assign);
				}
				out.text = "[" + list + "]";
				if (count >= 2) {
					out.list = list;
				}
				return true;
			}
			case Negate:
			case Not:
			case Flip: {
				Text operand;
				if (!node(operand, depth + 1)) {
					return false;
				}
				std::string text = wrap(operand, Unary);
				// "-5" is a negative number, -(5) the negation of 5
				if ((op == Negate && operand.literal) || text[0] == '-') {
					text = "(" + operand.text + ")";
				}
				out.text = std::string(op == Negate ? "-" : op == Not ? "!" : "~") + text;
				out.prec = Unary;
				return true;
			}
			case Range:
			case Subscript:
			case Ternary:
			case Function:
				return special(op, out, depth);
			default:
				break;
		}
		if (op >= AssignFirst && op <= AssignLast) {
			Text target, value;
			if (!node(target, depth + 1) || !node(value, depth + 1)) {
				return false;
			}
			out.text = wrap(target, TernaryPrec) + " " + assignments[op - AssignFirst] + " " + wrap(value, Assign);
			out.prec = Assign;
			return true;
		}
		if (op >= BinaryFirst && op <= BinaryLast) {
			const auto& binary = binaries[op - BinaryFirst];
			Text left, right;
			if (!node(left, depth + 1) || !node(right, depth + 1)) {
				return false;
			}
			out.text = wrap(left, binary.prec) + " " + binary.text + " " + wrap(right, binary.prec + 1);
			out.prec = binary.prec;
			return true;
		}
		return false;
	}

private:
	bool next(int& value) {
		if (m_pos >= m_bytes.size()) {
			return false;
		}
		value = m_bytes[m_pos++];
		return true;
	}

	static std::string wrap(const Text& text, int prec) {
		return text.prec < prec ? "(" + text.text + ")" : text.text;
	}

	bool special(int op, Text& out, int depth) {
		switch (op) {
			case Range: {
				Text first, last;
				if (!node(first, depth + 1) || !node(last, depth + 1)) {
					return false;
				}
				out.text = wrap(first, LogicalOr) + ".." + wrap(last, LogicalOr);
				out.prec = RangePrec;
				return true;
			}
			case Subscript: {
				Text array, index;
				if (!node(array, depth + 1) || !node(index, depth + 1)) {
					return false;
				}
				out.text = wrap(array, Postfix) + "[" + index.text + "]";
				out.prec = Postfix;
				return true;
			}
			case Ternary: {
				Text condition, yes, no;
				if (!node(condition, depth + 1) || !node(yes, depth + 1) || !node(no, depth + 1)) {
					return false;
				}
				out.text = wrap(condition, RangePrec) + " ? " + wrap(yes, Assign) + " : " + wrap(no, TernaryPrec);
				out.prec = TernaryPrec;
				return true;
			}
			case Function: {
				int id, count;
				if (!next(id) || !next(count) || (count & 0x80)) {
					return false;
				}
				// the Player skips the arguments of a known function called
				// with another count: what follows would be misread
				const bool known = id < static_cast<int>(functions.size());
				if (known && functions[id].arguments != count) {
					return false;
				}
				std::string arguments;
				for (int i = 0; i < count; ++i) {
					Text argument;
					if (!node(argument, depth + 1)) {
						return false;
					}
					arguments += (i ? ", " : "") + argument.text;
				}
				out.text = (known ? std::string(functions[id].name) : "fn" + std::to_string(id)) + "(" + arguments + ")";
				return true;
			}
		}
		return false;
	}

	std::vector<int> m_bytes;
	size_t m_pos = 0;
};

// Encoding: tokens, a tree, then the bytes in prefix order

struct Token {
	enum Kind { End, Number, Name, Symbol } kind = End;
	std::string text;
	int64_t value = 0;
	bool hex = false;
	size_t offset = 0;
};

struct Node {
	int op = Null;
	int64_t value = 0;
	bool hex = false;
	std::vector<std::unique_ptr<Node>> children;
};

using NodePtr = std::unique_ptr<Node>;

NodePtr make(int op) {
	auto node = std::make_unique<Node>();
	node->op = op;
	return node;
}

NodePtr make(int op, NodePtr first, NodePtr second = nullptr) {
	auto node = make(op);
	node->children.push_back(std::move(first));
	if (second) {
		node->children.push_back(std::move(second));
	}
	return node;
}

struct Failure {
	size_t offset;
};

class Parser {
public:
	explicit Parser(const std::string& text) : m_text(text) {
		advance();
	}

	NodePtr parse() {
		auto node = assignment(0);
		if (m_token.kind != Token::End) {
			fail();
		}
		return node;
	}

private:
	[[noreturn]] void fail() {
		throw Failure{m_token.offset};
	}

	bool is(const char* symbol) const {
		return m_token.kind == Token::Symbol && m_token.text == symbol;
	}

	void expect(const char* symbol) {
		if (!is(symbol)) {
			fail();
		}
		advance();
	}

	void advance() {
		while (m_pos < m_text.size() && std::isspace(static_cast<unsigned char>(m_text[m_pos]))) {
			++m_pos;
		}
		m_token = Token();
		m_token.offset = m_pos;
		if (m_pos >= m_text.size()) {
			return;
		}
		const char c = m_text[m_pos];
		if (std::isdigit(static_cast<unsigned char>(c))) {
			m_token.kind = Token::Number;
			int base = 10;
			if (c == '0' && m_pos + 1 < m_text.size() && (m_text[m_pos + 1] == 'x' || m_text[m_pos + 1] == 'X')) {
				base = 16;
				m_pos += 2;
				m_token.hex = true;
			}
			const size_t start = m_pos;
			while (m_pos < m_text.size() && std::isxdigit(static_cast<unsigned char>(m_text[m_pos]))) {
				const char digit = m_text[m_pos];
				if (base == 10 && !std::isdigit(static_cast<unsigned char>(digit))) {
					break;
				}
				m_token.value = m_token.value * base + (std::isdigit(static_cast<unsigned char>(digit))
					? digit - '0' : std::tolower(static_cast<unsigned char>(digit)) - 'a' + 10);
				if (m_token.value > 0xFFFFFFFFll) {
					fail();
				}
				++m_pos;
			}
			if (m_pos == start || (m_pos < m_text.size() && std::isalnum(static_cast<unsigned char>(m_text[m_pos])))) {
				fail();
			}
			return;
		}
		if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
			m_token.kind = Token::Name;
			while (m_pos < m_text.size() && (std::isalnum(static_cast<unsigned char>(m_text[m_pos])) || m_text[m_pos] == '_')) {
				m_token.text += static_cast<char>(std::tolower(static_cast<unsigned char>(m_text[m_pos++])));
			}
			return;
		}
		static const char* symbols[] = {"<<=", ">>=", "..", "<<", ">>", "<=", ">=", "==", "!=", "&&", "||",
			"+=", "-=", "*=", "/=", "%=", "|=", "&=", "^=",
			"+", "-", "*", "/", "%", "|", "&", "^", "<", ">", "=", "!", "~", "?", ":", ",", "[", "]", "(", ")"};
		for (const char* symbol : symbols) {
			if (m_text.compare(m_pos, std::char_traits<char>::length(symbol), symbol) == 0) {
				m_token.kind = Token::Symbol;
				m_token.text = symbol;
				m_pos += m_token.text.size();
				return;
			}
		}
		fail();
	}

	void enter(int depth) {
		if (depth > maxDepth) {
			fail();
		}
	}

	NodePtr assignment(int depth) {
		enter(depth);
		auto target = ternary(depth + 1);
		for (size_t i = 0; i < assignments.size(); ++i) {
			if (is(assignments[i])) {
				advance();
				return make(AssignFirst + static_cast<int>(i), std::move(target), assignment(depth + 1));
			}
		}
		return target;
	}

	NodePtr ternary(int depth) {
		enter(depth);
		auto condition = range(depth + 1);
		if (!is("?")) {
			return condition;
		}
		advance();
		auto yes = assignment(depth + 1);
		expect(":");
		auto node = make(Ternary, std::move(condition), std::move(yes));
		node->children.push_back(ternary(depth + 1));
		return node;
	}

	NodePtr range(int depth) {
		enter(depth);
		auto first = binary(LogicalOr, depth + 1);
		if (!is("..")) {
			return first;
		}
		advance();
		return make(Range, std::move(first), binary(LogicalOr, depth + 1));
	}

	NodePtr binary(int prec, int depth) {
		enter(depth);
		auto left = unary(depth + 1);
		for (;;) {
			const Operator* found = nullptr;
			for (const auto& candidate : binaries) {
				if (candidate.prec >= prec && is(candidate.text)) {
					found = &candidate;
					break;
				}
			}
			if (!found) {
				return left;
			}
			advance();
			left = make(found->op, std::move(left), binary(found->prec + 1, depth + 1));
		}
	}

	NodePtr unary(int depth) {
		enter(depth);
		if (is("-")) {
			advance();
			if (m_token.kind == Token::Number) {
				const Token digits = m_token;
				advance();
				if (!is("[")) {
					// a negative number, written in 32 bits like the Player reads it
					auto literal = number(-digits.value, digits.hex, digits.offset);
					literal->op = digits.hex ? SX32 : S32;
					return literal;
				}
				// -5[0] is -(5[0]), as the decoder writes it
				return make(Negate, postfix(number(digits.value, digits.hex, digits.offset), depth + 1));
			}
			return make(Negate, unary(depth + 1));
		}
		if (is("!") || is("~")) {
			const int op = is("!") ? Not : Flip;
			advance();
			return make(op, unary(depth + 1));
		}
		return postfix(primary(depth + 1), depth + 1);
	}

	NodePtr postfix(NodePtr node, int depth) {
		while (is("[")) {
			advance();
			node = make(Subscript, std::move(node), assignment(depth + 1));
			expect("]");
		}
		return node;
	}

	NodePtr number(int64_t value, bool hex, size_t offset) {
		// decimal numbers are signed, hexadecimal ones any 32 bits
		if (value < INT32_MIN || value > (hex ? 0xFFFFFFFFll : INT32_MAX)) {
			throw Failure{offset};
		}
		auto node = make(Null);
		node->value = static_cast<int32_t>(static_cast<uint32_t>(value));
		node->hex = hex;
		const auto unsigned_value = static_cast<uint32_t>(node->value);
		node->op = unsigned_value <= 0xFF ? (node->hex ? UX8 : U8)
			: unsigned_value <= 0xFFFF ? (node->hex ? UX16 : U16) : (node->hex ? SX32 : S32);
		return node;
	}

	/** Elements separated by commas, up to close */
	std::vector<NodePtr> list(const char* close, int depth) {
		std::vector<NodePtr> elements;
		if (!is(close)) {
			elements.push_back(assignment(depth + 1));
			while (is(",")) {
				advance();
				elements.push_back(assignment(depth + 1));
			}
		}
		expect(close);
		return elements;
	}

	NodePtr array(std::vector<NodePtr> elements) {
		auto node = make(Array);
		node->value = static_cast<int64_t>(elements.size());
		node->children = std::move(elements);
		return node;
	}

	NodePtr primary(int depth) {
		enter(depth);
		if (m_token.kind == Token::Number) {
			auto node = number(m_token.value, m_token.hex, m_token.offset);
			advance();
			return node;
		}
		if (is("(")) {
			advance();
			auto node = assignment(depth + 1);
			expect(")");
			return node;
		}
		if (is("[")) {
			advance();
			return array(list("]", depth + 1));
		}
		if (m_token.kind != Token::Name) {
			fail();
		}
		const Token name = m_token;
		advance();
		if ((name.text == "v" || name.text == "s") && is("[")) {
			advance();
			auto elements = list("]", depth + 1);
			if (elements.empty()) {
				fail();
			}
			auto inner = elements.size() == 1 ? std::move(elements.front()) : array(std::move(elements));
			const bool variable = name.text == "v";
			// V[V[x]] and S[V[x]] have their own operation
			if (inner->op == Var) {
				return make(variable ? VarIndirect : SwitchIndirect, std::move(inner->children.front()));
			}
			return make(variable ? Var : Switch, std::move(inner));
		}
		int id = -1;
		for (size_t i = 0; i < functions.size(); ++i) {
			if (name.text == functions[i].name) {
				id = static_cast<int>(i);
			}
		}
		if (id < 0 && name.text.size() > 2 && name.text.compare(0, 2, "fn") == 0
			&& std::all_of(name.text.begin() + 2, name.text.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)); })) {
			id = std::stoi(name.text.substr(2));
		}
		if (id < 0 || id > 255 || !is("(")) {
			throw Failure{name.offset};
		}
		advance();
		auto node = make(Function);
		node->value = id;
		node->children = list(")", depth + 1);
		const size_t count = node->children.size();
		if (count >= 0x80 || (id < static_cast<int>(functions.size()) && static_cast<int>(count) != functions[id].arguments)) {
			throw Failure{name.offset};
		}
		return node;
	}

	const std::string& m_text;
	size_t m_pos = 0;
	Token m_token;
};

void emit(const Node& node, std::vector<uint8_t>& bytes) {
	bytes.push_back(static_cast<uint8_t>(node.op));
	const auto value = static_cast<uint32_t>(node.value);
	switch (node.op) {
		case U8: case UX8: case U16: case UX16: case S32: case SX32: {
			const int size = node.op == U8 || node.op == UX8 ? 1 : node.op == U16 || node.op == UX16 ? 2 : 4;
			for (int i = 0; i < size; ++i) {
				bytes.push_back(static_cast<uint8_t>(value >> (8 * i)));
			}
			return;
		}
		case Array:
			if (value < 0x80) {
				bytes.push_back(static_cast<uint8_t>(value));
			} else {
				bytes.push_back(0x80);
				for (int i = 0; i < 4; ++i) {
					bytes.push_back(static_cast<uint8_t>(value >> (8 * i)));
				}
			}
			break;
		case Function:
			bytes.push_back(static_cast<uint8_t>(value));
			bytes.push_back(static_cast<uint8_t>(node.children.size()));
			break;
		default:
			break;
	}
	for (const auto& child : node.children) {
		emit(*child, bytes);
	}
}
}

bool ManiacExpression::decode(std::span<const int32_t> words, std::string& text) {
	Decoder decoder(words);
	Text result;
	if (!decoder.node(result)) {
		return false;
	}
	text = result.text;
	return true;
}

bool ManiacExpression::encode(const std::string& text, std::vector<int32_t>& words, size_t& error) {
	NodePtr tree;
	try {
		tree = Parser(text).parse();
	} catch (const Failure& failure) {
		error = failure.offset;
		return false;
	}
	std::vector<uint8_t> bytes;
	emit(*tree, bytes);
	words.assign((bytes.size() + 3) / 4, 0);
	for (size_t i = 0; i < bytes.size(); ++i) {
		words[i / 4] |= static_cast<int32_t>(static_cast<uint32_t>(bytes[i]) << (8 * (i % 4)));
	}
	return true;
}
