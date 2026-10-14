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

// Do not include this file! Only json_t_impl.h does

#include <QVariantList>
#include <cmath>
#include <concepts>
#include <limits>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include "common/dbstring.h"
#include "json_list_view.h"
#include "json_node.h"
#include "lcf_glaze.h"

/**
 * The JsonNodeType of every C++ type of the data: how a value is read for
 * QML, written from it (converted and clamped), measured and serialized
 */
namespace JsonNodes {

template<class T>
concept Integer = std::integral<T> && !std::same_as<T, bool>;

template<class T>
concept Text = std::same_as<T, lcf::DBString> || std::same_as<T, std::string>;

template<class T>
concept BitArray = std::same_as<T, lcf::DBBitArray> || std::same_as<T, std::vector<bool>>;

/** What glaze hands out for a single bit: no reference into the data */
template<class T>
concept BitProxy = std::same_as<T, lcf::DBBitProxy> || std::same_as<T, std::vector<bool>::reference>;

template<class T>
concept Scalar = std::same_as<T, bool> || Integer<T> || std::floating_point<T> || Text<T>;

template<class T>
concept Range = !Text<T> && requires(const T& t) {
	t.begin();
	t.end();
	t.size();
};

template<class T>
using ElementOf = std::remove_cvref_t<decltype(*std::declval<const T&>().begin())>;

template<class T>
struct IsStdVector : std::false_type {};
template<class E, class A>
struct IsStdVector<std::vector<E, A>> : std::true_type {};

/** A vector of structs (actors, event commands...): JsonListView shows it */
template<class T>
concept ObjectVector = IsStdVector<T>::value && std::is_class_v<typename T::value_type>
	&& !Text<typename T::value_type> && !std::same_as<typename T::value_type, bool>;

template<class T>
concept ScalarRange = Range<T> && !BitArray<T> && Scalar<ElementOf<T>>;

/*
 * Values for QML
 */

template<Integer T>
QVariant integerVariant(T value) {
	// what fits an int stays one: QML gets the same numbers as from num()
	if constexpr (std::is_signed_v<T> && sizeof(T) <= sizeof(int)) {
		return QVariant(static_cast<int>(value));
	} else if constexpr (std::is_unsigned_v<T> && sizeof(T) < sizeof(int)) {
		return QVariant(static_cast<int>(value));
	} else if constexpr (std::is_unsigned_v<T> && sizeof(T) <= sizeof(uint)) {
		return QVariant(static_cast<uint>(value));
	} else if constexpr (std::is_signed_v<T>) {
		return QVariant(static_cast<qint64>(value));
	} else {
		return QVariant(static_cast<quint64>(value));
	}
}

template<Scalar T>
QVariant scalarVariant(const T& value) {
	if constexpr (std::same_as<T, bool>) {
		return QVariant(value);
	} else if constexpr (Integer<T>) {
		return integerVariant(value);
	} else if constexpr (std::floating_point<T>) {
		return QVariant(static_cast<double>(value));
	} else {
		return QVariant(ToQString(value));
	}
}

/*
 * Values from QML, converted to the type of the data
 */

template<Integer T>
std::optional<T> toInteger(const QVariant& value) {
	static constexpr T lowest = std::numeric_limits<T>::min();
	static constexpr T highest = std::numeric_limits<T>::max();
	const auto clamped = [](auto n) -> T {
		if (std::cmp_less(n, lowest)) {
			return lowest;
		}
		if (std::cmp_greater(n, highest)) {
			return highest;
		}
		return static_cast<T>(n);
	};
	// numbers of JavaScript are doubles: rounded, then clamped
	const auto fromDouble = [](double d) -> std::optional<T> {
		if (std::isnan(d)) {
			return std::nullopt;
		}
		d = std::round(d);
		if (d <= static_cast<double>(lowest)) {
			return lowest;
		}
		if (d >= static_cast<double>(highest)) {
			return highest;
		}
		return static_cast<T>(d);
	};

	bool ok = false;
	switch (value.typeId()) {
		case QMetaType::Bool:
			return static_cast<T>(value.toBool());
		case QMetaType::Double:
		case QMetaType::Float:
			return fromDouble(value.toDouble());
		case QMetaType::UChar:
		case QMetaType::UShort:
		case QMetaType::UInt:
		case QMetaType::ULong:
		case QMetaType::ULongLong:
			return clamped(value.toULongLong());
		case QMetaType::Char:
		case QMetaType::SChar:
		case QMetaType::Short:
		case QMetaType::Int:
		case QMetaType::Long:
		case QMetaType::LongLong:
			return clamped(value.toLongLong());
		default: {
			// a text holding a number
			const qlonglong n = value.toLongLong(&ok);
			if (ok) {
				return clamped(n);
			}
			const double d = value.toDouble(&ok);
			if (ok) {
				return fromDouble(d);
			}
			return std::nullopt;
		}
	}
}

inline std::optional<bool> toBool(const QVariant& value) {
	if (!value.isValid()) {
		return std::nullopt;
	}
	if (value.typeId() == QMetaType::QString) {
		const QString text = value.toString().trimmed().toLower();
		if (text == QLatin1String("true") || text == QLatin1String("1")) {
			return true;
		}
		if (text == QLatin1String("false") || text == QLatin1String("0") || text.isEmpty()) {
			return false;
		}
		return std::nullopt;
	}
	if (!value.canConvert<bool>()) {
		return std::nullopt;
	}
	return value.toBool();
}

inline std::optional<QString> toText(const QVariant& value) {
	if (!value.isValid() || value.typeId() == QMetaType::QVariantList || !value.canConvert<QString>()) {
		return std::nullopt;
	}
	return value.toString();
}

template<Scalar T>
std::optional<T> toScalar(const QVariant& value) {
	if constexpr (std::same_as<T, bool>) {
		return toBool(value);
	} else if constexpr (Integer<T>) {
		return toInteger<T>(value);
	} else if constexpr (std::floating_point<T>) {
		bool ok = false;
		const double d = value.toDouble(&ok);
		if (!ok || std::isnan(d)) {
			return std::nullopt;
		}
		return static_cast<T>(d);
	} else {
		const auto text = toText(value);
		if (!text) {
			return std::nullopt;
		}
		if constexpr (std::same_as<T, lcf::DBString>) {
			return ToDBString(*text);
		} else {
			return text->toStdString();
		}
	}
}

/** A whole array from a list of QML: every element converted */
template<class T>
std::optional<T> toArray(const QVariant& value) {
	if (!value.canConvert<QVariantList>() || value.typeId() == QMetaType::QString) {
		return std::nullopt;
	}
	const QVariantList list = value.toList();
	using E = std::conditional_t<BitArray<T>, bool, ElementOf<T>>;
	std::vector<E> elements;
	elements.reserve(static_cast<size_t>(list.size()));
	for (const QVariant& v : list) {
		auto element = toScalar<E>(v);
		if (!element) {
			return std::nullopt;
		}
		elements.push_back(std::move(*element));
	}
	if constexpr (std::same_as<T, std::vector<E>>) {
		return elements;
	} else if constexpr (std::is_constructible_v<T, typename std::vector<E>::iterator, typename std::vector<E>::iterator>) {
		// DBArray, DBBitArray: made at their size
		return T(elements.begin(), elements.end());
	} else {
		// a fixed size (std::array): the same number of elements
		T array{};
		if (elements.size() != array.size()) {
			return std::nullopt;
		}
		std::copy(elements.begin(), elements.end(), array.begin());
		return array;
	}
}

/*
 * The functions of the table
 */

template<class T>
QVariant read(const void* data) {
	const T& value = *static_cast<const T*>(data);
	if constexpr (Scalar<T>) {
		return scalarVariant(value);
	} else if constexpr (BitArray<T>) {
		QVariantList list;
		list.reserve(static_cast<qsizetype>(value.size()));
		for (size_t i = 0; i < value.size(); ++i) {
			list.append(static_cast<bool>(value[i]));
		}
		return list;
	} else if constexpr (ScalarRange<T>) {
		QVariantList list;
		list.reserve(static_cast<qsizetype>(value.size()));
		for (const auto& element : value) {
			list.append(scalarVariant(element));
		}
		return list;
	} else {
		// objects and their lists: subtree() and list()
		return {};
	}
}

template<class T>
bool write(void* data, const QVariant& value, bool* changed) {
	T& target = *static_cast<T*>(data);
	std::optional<T> converted;
	if constexpr (Scalar<T>) {
		converted = toScalar<T>(value);
	} else {
		converted = toArray<T>(value);
	}
	if (!converted) {
		return false;
	}
	*changed = !(target == *converted);
	if (*changed) {
		target = std::move(*converted);
	}
	return true;
}

template<class T>
qsizetype size(const void* data) {
	if constexpr (Range<T>) {
		return static_cast<qsizetype>(static_cast<const T*>(data)->size());
	} else {
		return -1;
	}
}

template<class T>
bool bit(const void* data, qsizetype index, bool missing) {
	if constexpr (BitArray<T>) {
		const T& bits = *static_cast<const T*>(data);
		return index >= 0 && static_cast<size_t>(index) < bits.size() ? static_cast<bool>(bits[static_cast<size_t>(index)]) : missing;
	} else {
		return missing;
	}
}

/** Bit arrays grow up to this many bits (RPG Maker's lists have 5000 entries) */
constexpr qsizetype MaxBits = 1 << 20;

template<class T>
bool setBit(void* data, qsizetype index, bool on, bool missing, bool* changed) {
	if constexpr (BitArray<T>) {
		T& bits = *static_cast<T*>(data);
		if (index < 0 || index >= MaxBits) {
			return false;
		}
		const auto i = static_cast<size_t>(index);
		*changed = false;
		if (i >= bits.size()) {
			// the bits it did not have keep the value they were read with
			if constexpr (std::same_as<T, std::vector<bool>>) {
				bits.resize(i + 1, missing);
			} else {
				lcf::DBBitArray grown(i + 1, missing);
				for (size_t j = 0; j < bits.size(); ++j) {
					grown[j] = static_cast<bool>(bits[j]);
				}
				bits = std::move(grown);
			}
			*changed = true;
		}
		if (static_cast<bool>(bits[i]) != on) {
			bits[i] = on;
			*changed = true;
		}
		return true;
	} else {
		return false;
	}
}

template<class T>
std::string json(void* data) {
	std::string buffer;
	if (glz::write_json(*static_cast<T*>(data), buffer)) {
		return {};
	}
	return buffer;
}

template<ObjectVector T>
JsonListView* makeList(Json* owner, const QString& path) {
	return new JsonListViewT<typename T::value_type>(owner, path);
}

template<class T>
constexpr JsonNodeType::Kind kindOf() {
	using Kind = JsonNodeType::Kind;
	if constexpr (std::same_as<T, bool>) {
		return Kind::Bool;
	} else if constexpr (Integer<T>) {
		return Kind::Integer;
	} else if constexpr (std::floating_point<T>) {
		return Kind::Floating;
	} else if constexpr (Text<T>) {
		return Kind::String;
	} else if constexpr (BitArray<T>) {
		return Kind::Bits;
	} else if constexpr (Range<T>) {
		return Kind::Array;
	} else {
		return Kind::Object;
	}
}

/** Numbers, texts and arrays of them are written; objects only through their fields */
template<class T>
constexpr auto writerOf() -> bool (*)(void*, const QVariant&, bool*) {
	if constexpr (Scalar<T> || BitArray<T> || ScalarRange<T>) {
		return &write<T>;
	} else {
		return nullptr;
	}
}

template<class T>
constexpr auto listMakerOf() -> JsonListView* (*)(Json*, const QString&) {
	if constexpr (ObjectVector<T>) {
		return &makeList<T>;
	} else {
		return nullptr;
	}
}

template<class T>
inline constexpr JsonNodeType typeOf = {
	kindOf<T>(),
	&jsonTypeTag<T>,
	&read<T>,
	writerOf<T>(),
	&size<T>,
	&bit<T>,
	&setBit<T>,
	&json<T>,
	listMakerOf<T>(),
};

} // namespace JsonNodes
