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

#include "command_edit_session.h"
#include "common/dbstring.h"
#include "common/exfont.h"
#include "json_view.h"
#include "model/game_patches.h"
#include "model/maniac_expression.h"
#include "model/move_route_model.h"
#include "model/project.h"
#include "model/project_data.h"
#include "ui/common/help.h"
#include "model/map_document.h"
#include "model/map_tools.h"
#include "ui/maptree/location_picker.h"
#include "ui/picker/pickers.h"

#include <QCoreApplication>
#include <QMetaEnum>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScopeGuard>
#include <QSignalBlocker>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>
#include <limits>
#include <lcf/reader_util.h>

#include "qml_host.h"

using namespace CommandSchema;

namespace {

/** RPG Maker's Select Location: the maps, the cell clicked, the facing */
std::optional<CommandEditSession::Location> pickOnMap(QWindow* over, ProjectData&, const CommandEditSession::Location& current) {
	Pickers::LocationRequest request;
	request.current = {current.map, current.x, current.y, std::max(0, current.direction)};
	request.facing = current.direction >= 0;
	request.fixedMap = current.fixed;
	const auto chosen = Pickers::pickLocation(over, request);
	if (!chosen) {
		return std::nullopt;
	}
	return CommandEditSession::Location{chosen->map, chosen->x, chosen->y, request.facing ? chosen->direction : -1, current.fixed};
}

/**
 * Select Tile on the chipset of the map: the 144 tiles a substitution
 * numbers, the lower ones after the autotiles of the palette
 */
std::optional<int> pickInChipset(QWindow* over, ProjectData& project, int map, int layer, int tile) {
	auto* picker = new LocationPicker(project);
	const auto cleanup = qScopeGuard([picker] { picker->deleteLater(); });
	if (!picker->showMap(map) || !picker->document() || !picker->document()->hasMap()) {
		return std::nullopt;
	}
	const bool lower = layer == 0;
	const QList<int> palette = MapTools::paletteTiles(lower);
	const int first = lower ? static_cast<int>(palette.size()) - 144 : 0;
	QVariantList tiles;
	for (qsizetype i = first; i < palette.size(); ++i) {
		tiles.append(palette[i]);
	}
	QRect chosen;
	const QRect current = tile >= 0 && tile < 144 ? QRect(tile % 6, tile / 6, 1, 1) : QRect();
	const int result = QmlHost::exec(QStringLiteral("ChipSelectorWindow"), over, {
		{QStringLiteral("chipsetId"), picker->document()->map().chipset_id},
		{QStringLiteral("tiles"), tiles},
		{QStringLiteral("maxWidth"), 1},
		{QStringLiteral("maxHeight"), 1},
		{QStringLiteral("selection"), current},
	}, [&chosen](QQuickWindow* window) {
		chosen = window->property("selection").toRect();
	});
	if (result != 1 || chosen.isEmpty()) {
		return std::nullopt;
	}
	return chosen.y() * 6 + chosen.x();
}

// Control Variables: the operand of the expression; the operands use the
// parameters up to 10 (the ternary operator)
constexpr int variableExpressionOperand = 21;
constexpr int variableOperandEnd = 11;

CommandEditSession::LocationPicker& locationPicker() {
	static CommandEditSession::LocationPicker picker = pickOnMap;
	return picker;
}

CommandEditSession::TilePicker& tilePicker() {
	static CommandEditSession::TilePicker picker = pickInChipset;
	return picker;
}

uint32_t maskOf(int shift, int bits) {
	return bits >= 32 ? 0xFFFFFFFFu : (((1u << bits) - 1u) << shift);
}

/** A number from QML (a double, a bool, a text), rounded */
std::optional<qint64> toNumber(const QVariant& value) {
	switch (value.typeId()) {
		case QMetaType::Bool:
			return value.toBool() ? 1 : 0;
		case QMetaType::Double:
		case QMetaType::Float: {
			const double d = value.toDouble();
			if (std::isnan(d)) {
				return std::nullopt;
			}
			return static_cast<qint64>(std::clamp(std::round(d), -9e18, 9e18));
		}
		default: {
			bool ok = false;
			const qint64 n = value.toLongLong(&ok);
			if (ok) {
				return n;
			}
			const double d = value.toDouble(&ok);
			if (ok && !std::isnan(d)) {
				return static_cast<qint64>(std::clamp(std::round(d), -9e18, 9e18));
			}
			return std::nullopt;
		}
	}
}

/** The name of a kind ("Number") for QML */
QString kindName(Kind kind) {
	return QString::fromLatin1(QMetaEnum::fromType<Kind>().valueToKey(static_cast<int>(kind)));
}

/** The entry of a database list by ID, "0012: Potion" */
template<class T>
QString entryText(const std::vector<T>& list, int id) {
	const T* entry = lcf::ReaderUtil::GetElement(list, id);
	return entry ? QStringLiteral("%1: %2").arg(id, 4, 10, QChar(u'0')).arg(ToQString(entry->name)) : QString();
}

} // namespace

/*
 * The schema
 */

CommandSchemaView::CommandSchemaView(const Schema* schema, QObject* parent) : QObject(parent), m_schema(schema) {
	if (!schema) {
		return;
	}
	for (const auto& one : schema->fields) {
		m_fields.append(fieldMap(one));
	}

	// the form: sections of rows, the inline fields after the field of their row
	QVariantMap section;
	QVariantList rows;
	auto flush = [&] {
		if (!rows.isEmpty() || section.contains(QStringLiteral("title"))) {
			section.insert(QStringLiteral("rows"), rows);
			m_sections.append(section);
		}
		section.clear();
		rows.clear();
	};
	for (qsizetype i = 0; i < m_fields.size(); ++i) {
		const Field& one = schema->fields[static_cast<size_t>(i)];
		QVariantMap map = m_fields[i].toMap();
		if (one.kind == Kind::Section) {
			flush();
			section.insert(QStringLiteral("title"), label(one.label));
			continue;
		}
		if (one.flags & Flag::Hidden) {
			continue;
		}
		if ((one.flags & Flag::Inline) && !rows.isEmpty()) {
			QVariantMap row = rows.last().toMap();
			const QVariantMap owner = row.value(QStringLiteral("field")).toMap();
			// on the line of the radio button of its mode, else after the field
			bool placed = false;
			if (owner.value(QStringLiteral("radio")).toBool()) {
				const QByteArray ownerId = owner.value(QStringLiteral("id")).toString().toUtf8();
				for (const auto& condition : one.visible) {
					if (condition.negated || condition.values.size() != 1 || ownerId != condition.field) {
						continue;
					}
					QVariantList options = row.value(QStringLiteral("options")).toList();
					for (auto& option : options) {
						QVariantMap optionMap = option.toMap();
						if (optionMap.value(QStringLiteral("item")).toMap().value(QStringLiteral("value")).toInt() == condition.values.front()) {
							QVariantList inlined = optionMap.value(QStringLiteral("inline")).toList();
							map.insert(QStringLiteral("option"), condition.values.front());
							inlined.append(map);
							optionMap.insert(QStringLiteral("inline"), inlined);
							option = optionMap;
							placed = true;
							break;
						}
					}
					row.insert(QStringLiteral("options"), options);
					break;
				}
			}
			if (!placed) {
				QVariantList inlined = row.value(QStringLiteral("inline")).toList();
				inlined.append(map);
				row.insert(QStringLiteral("inline"), inlined);
			}
			rows.last() = row;
			continue;
		}
		QVariantMap row;
		row.insert(QStringLiteral("field"), map);
		row.insert(QStringLiteral("inline"), QVariantList());
		QVariantList options;
		if (map.value(QStringLiteral("radio")).toBool()) {
			for (const auto& item : map.value(QStringLiteral("items")).toList()) {
				options.append(QVariantMap{{QStringLiteral("item"), item}, {QStringLiteral("inline"), QVariantList()}});
			}
		}
		row.insert(QStringLiteral("options"), options);
		rows.append(row);
	}
	flush();
}

bool CommandSchemaView::custom() const {
	return m_schema && m_schema->custom;
}

bool CommandSchemaView::hides() const {
	// the editors of the Maniac Patch show the fields of the mode chosen,
	// the others grey what does not apply (as RPG Maker)
	return m_schema && (m_schema->flags & Flag::Maniac);
}

QVariantMap CommandSchemaView::title() const {
	return m_schema ? label(m_schema->title) : QVariantMap();
}

QVariantMap CommandSchemaView::field(const QString& id) const {
	for (const auto& one : m_fields) {
		const QVariantMap map = one.toMap();
		if (map.value(QStringLiteral("id")).toString() == id) {
			return map;
		}
	}
	return {};
}

QVariantMap CommandSchemaView::label(const Label& label) {
	if (label.isEmpty()) {
		return {};
	}
	return {
		{QStringLiteral("context"), QString::fromLatin1(label.context ? label.context : "")},
		{QStringLiteral("text"), QString::fromUtf8(label.text)},
		{QStringLiteral("number"), label.number},
	};
}

QString CommandSchemaView::pathOf(const Field& field) {
	if (field.kind == Kind::Lines) {
		return QStringLiteral("/lines");
	}
	if (field.usesString() && (field.kind != Kind::Face && field.kind != Kind::Charset)) {
		return QStringLiteral("/string");
	}
	if (field.index < 0) {
		return QStringLiteral("/string");
	}
	if (field.isTail()) {
		return QStringLiteral("/parameters");
	}
	if (field.bits >= 32) {
		return QStringLiteral("/parameters/%1").arg(field.index);
	}
	return QStringLiteral("/parameters/%1@%2:%3").arg(field.index).arg(field.shift).arg(field.bits);
}

QString CommandSchemaView::listOf(Kind kind) {
	switch (kind) {
		case Kind::Switch: return QStringLiteral("switches");
		case Kind::Variable: return QStringLiteral("variables");
		case Kind::Item: return QStringLiteral("items");
		case Kind::Actor: return QStringLiteral("actors");
		case Kind::Skill: return QStringLiteral("skills");
		case Kind::State: return QStringLiteral("states");
		case Kind::Troop: return QStringLiteral("troops");
		case Kind::Terrain: return QStringLiteral("terrains");
		case Kind::Animation: return QStringLiteral("animations");
		case Kind::Chipset: return QStringLiteral("chipsets");
		case Kind::CommonEvent: return QStringLiteral("commonevents");
		case Kind::Class: return QStringLiteral("classes");
		case Kind::BattleCommand: return QStringLiteral("battlecommands/commands");
		default: return QString();
	}
}

QVariantMap CommandSchemaView::fieldMap(const Field& field) const {
	QVariantList items;
	for (const auto& item : field.items) {
		items.append(QVariantMap{
			{QStringLiteral("value"), item.value},
			{QStringLiteral("text"), label(item.text)},
			{QStringLiteral("rpg2003"), bool(item.flags & Flag::Rpg2003)},
			{QStringLiteral("maniac"), bool(item.flags & Flag::Maniac)},
			{QStringLiteral("kept"), bool(item.flags & Flag::Kept)},
		});
	}
	const QString list = listOf(field.kind);
	QVariantMap none;
	if (!list.isEmpty() && !field.items.empty()) {
		// the entry before the list's ("(None)", "All")
		none = {{QStringLiteral("value"), field.items.front().value}, {QStringLiteral("text"), label(field.items.front().text)}};
	}
	return {
		{QStringLiteral("id"), QString::fromUtf8(field.id)},
		{QStringLiteral("kind"), kindName(field.kind)},
		{QStringLiteral("label"), label(field.label)},
		{QStringLiteral("index"), field.index},
		{QStringLiteral("shift"), field.shift},
		{QStringLiteral("bits"), field.bits},
		{QStringLiteral("path"), field.hasValue() ? pathOf(field) : QString()},
		{QStringLiteral("minimum"), field.minimum},
		{QStringLiteral("maximum"), field.maximum},
		{QStringLiteral("step"), field.step},
		{QStringLiteral("defaultValue"), field.defaultValue},
		{QStringLiteral("suffix"), label(field.suffix)},
		{QStringLiteral("items"), items},
		{QStringLiteral("radio"), bool(field.flags & Flag::Radio)},
		{QStringLiteral("slider"), bool(field.flags & Flag::Slider)},
		{QStringLiteral("ticks"), bool(field.flags & Flag::Ticks)},
		{QStringLiteral("inline"), bool(field.flags & Flag::Inline)},
		{QStringLiteral("inverted"), bool(field.flags & Flag::Inverted)},
		{QStringLiteral("hidden"), bool(field.flags & Flag::Hidden)},
		{QStringLiteral("extension"), bool(field.flags & Flag::Extension)},
		{QStringLiteral("rpg2003"), bool(field.flags & Flag::Rpg2003)},
		{QStringLiteral("maniac"), bool(field.flags & Flag::Maniac)},
		{QStringLiteral("equipment"), bool(field.flags & Flag::Equipment)},
		{QStringLiteral("off"), bool(field.flags & Flag::Off)},
		{QStringLiteral("folder"), QString::fromLatin1(field.folder ? field.folder : "")},
		{QStringLiteral("link"), QString::fromLatin1(field.link ? field.link : "")},
		{QStringLiteral("limit"), field.limit},
		{QStringLiteral("list"), list},
		{QStringLiteral("none"), none.isEmpty() ? QVariant() : QVariant(none)},
	};
}

/*
 * The session as JSON
 */

struct CommandEditSession::Slot {
	enum class What { Root, Code, Indent, String, Lines, Parameters, Bits, Default };

	CommandEditSession* session = nullptr;
	What what = What::Root;
	/** Default: the field */
	QString field;
	int index = 0;
	int shift = 0;
	int bits = 32;
};

/** The JsonNodeTypes of the values of a session: they read and write through their slot */
struct CommandEditSessionNodes {
	using Slot = CommandEditSession::Slot;

	static const Slot& slot(const void* data) {
		return *static_cast<const Slot*>(data);
	}
	static QVariant read(const void* data) {
		return slot(data).session->readSlot(slot(data));
	}
	static bool write(void* data, const QVariant& value, bool* changed) {
		return slot(data).session->writeSlot(slot(data), value, changed);
	}
	static qsizetype size(const void* data) {
		const Slot& one = slot(data);
		return one.what == Slot::What::Parameters ? static_cast<qsizetype>(one.session->m_command.parameters.size()) : -1;
	}
	static bool bit(const void*, qsizetype, bool missing) {
		return missing;
	}
	static bool setBit(void*, qsizetype, bool, bool, bool*) {
		return false;
	}
	static std::string json(void* data) {
		const QVariant value = read(data);
		if (value.typeId() == QMetaType::QString) {
			return '"' + value.toString().replace(u'\\', QStringLiteral("\\\\")).replace(u'"', QStringLiteral("\\\"")).toStdString() + '"';
		}
		if (value.typeId() == QMetaType::QVariantList) {
			QStringList numbers;
			for (const auto& number : value.toList()) {
				numbers << number.toString();
			}
			return '[' + numbers.join(u',').toStdString() + ']';
		}
		return value.toString().toStdString();
	}
	static QVariant nothing(const void*) {
		return {};
	}

	static const JsonNodeType& type(const Slot& slot) {
		static const JsonNodeType root = {JsonNodeType::Kind::Object, &jsonTypeTag<Slot>, &nothing, nullptr,
			&size, &bit, &setBit, &json, nullptr};
		static const JsonNodeType number = {JsonNodeType::Kind::Integer, &jsonTypeTag<int32_t>, &read, &write,
			&size, &bit, &setBit, &json, nullptr};
		static const JsonNodeType text = {JsonNodeType::Kind::String, &jsonTypeTag<lcf::DBString>, &read, &write,
			&size, &bit, &setBit, &json, nullptr};
		static const JsonNodeType array = {JsonNodeType::Kind::Array, &jsonTypeTag<lcf::DBArray<int32_t>>, &read, &write,
			&size, &bit, &setBit, &json, nullptr};
		switch (slot.what) {
			case Slot::What::Root:
				return root;
			case Slot::What::String:
			case Slot::What::Lines:
				return text;
			case Slot::What::Default:
				return slot.session->defaultIsText(slot.field) ? text : number;
			case Slot::What::Parameters:
				return array;
			default:
				return number;
		}
	}
};

CommandEditSession::CommandEditSession(const lcf::rpg::EventCommand& command, QObject* parent) :
	Json(parent), m_command(command)
{
	m_schemaView = new CommandSchemaView(CommandSchema::find(static_cast<Code>(command.code)), this);
	m_lines = {ToQString(command.string)};
	connect(this, &Json::valueChanged, this, [this] {
		++m_revision;
		emit revisionChanged();
	});
	if (command.code == static_cast<int>(Code::ControlVars)) {
		// the expressions of the Maniac Patch: those of the command, else
		// what a new one starts from (V[1] for the target, 0 for the value)
		const auto& parameters = m_command.parameters;
		const int size = static_cast<int>(parameters.size());
		auto words = [&](int from, int count) {
			std::vector<int32_t> result;
			for (int i = std::max(from, 0); i < std::min(from + count, size); ++i) {
				result.push_back(parameters[i]);
			}
			return result;
		};
		auto parameter = [&](int i) { return i < size ? parameters[i] : 0; };
		if (parameter(0) == 4 && parameter(1) >= 0 && parameter(1) < size) {
			m_targetAt = parameter(1);
			m_targetWords = words(m_targetAt + 1, parameter(m_targetAt));
		}
		if (parameter(4) == variableExpressionOperand) {
			m_operandWords = words(6, parameter(5));
		}
		size_t error = 0;
		if (m_targetWords.empty()) {
			ManiacExpression::encode("1", m_targetWords, error);
		}
		if (m_operandWords.empty()) {
			ManiacExpression::encode("0", m_operandWords, error);
		}
	}
	if (command.code == static_cast<int>(Code::MoveEvent)) {
		m_moveRoute = new MoveRouteModel(this);
		m_moveRoute->setCommand(m_command, std::string());
		// the route as the command: written at every change
		auto write = [this] {
			const std::string encoding = m_project ? m_project->project().encoding().toStdString() : std::string();
			m_moveRoute->writeCommand(m_command, encoding);
			notifyChanged(QString());
		};
		connect(m_moveRoute, &MoveRouteModel::movesChanged, this, write);
		connect(m_moveRoute, &MoveRouteModel::repeatChanged, this, write);
		connect(m_moveRoute, &MoveRouteModel::skippableChanged, this, write);
		connect(m_moveRoute, &MoveRouteModel::targetChanged, this, write);
		connect(m_moveRoute, &MoveRouteModel::frequencyChanged, this, write);
	}
}

CommandEditSession::~CommandEditSession() = default;

JsonNode CommandEditSession::node(const QString& jsonPtr) const {
	if (auto found = m_slots.value(jsonPtr)) {
		return {&CommandEditSessionNodes::type(*found), found.get(), -1};
	}
	auto slot = std::make_shared<Slot>();
	slot->session = const_cast<CommandEditSession*>(this);
	static const QRegularExpression parameter(QStringLiteral("^/parameters/(\\d+)(?:@(\\d+):(\\d+))?$"));
	if (jsonPtr.isEmpty()) {
		slot->what = Slot::What::Root;
	} else if (jsonPtr == QLatin1String("/code")) {
		slot->what = Slot::What::Code;
	} else if (jsonPtr == QLatin1String("/indent")) {
		slot->what = Slot::What::Indent;
	} else if (jsonPtr == QLatin1String("/string")) {
		slot->what = Slot::What::String;
	} else if (jsonPtr == QLatin1String("/lines")) {
		slot->what = Slot::What::Lines;
	} else if (jsonPtr == QLatin1String("/parameters")) {
		slot->what = Slot::What::Parameters;
	} else if (jsonPtr.startsWith(QLatin1String("/defaults/")) && field(jsonPtr.mid(10))) {
		slot->what = Slot::What::Default;
		slot->field = jsonPtr.mid(10);
	} else if (const auto match = parameter.match(jsonPtr); match.hasMatch()) {
		slot->what = Slot::What::Bits;
		slot->index = match.captured(1).toInt();
		if (match.hasCaptured(2)) {
			slot->shift = match.captured(2).toInt();
			slot->bits = match.captured(3).toInt();
		}
		// a parameter of RPG Maker: a few hundred at most (a shop's goods)
		if (slot->index > 4096 || slot->bits < 1 || slot->shift < 0 || slot->shift + slot->bits > 32) {
			return {};
		}
	} else {
		return {};
	}
	m_slots.insert(jsonPtr, slot);
	return {&CommandEditSessionNodes::type(*slot), slot.get(), -1};
}

bool CommandEditSession::isRpg2k3() const {
	return m_project && m_project->database().system.ldb_id == 2003;
}

bool CommandEditSession::maniac() const {
	return m_project && GamePatches::usesManiac(m_project->project());
}

JsonView* CommandEditSession::database() {
	return m_project ? m_database.view(QString()) : nullptr;
}

void CommandEditSession::setProject(ProjectData* project) {
	m_project = project;
	m_database.setData(project ? &project->database() : nullptr);
	if (m_moveRoute && project) {
		// the texts of the moves in the encoding of the game
		const QSignalBlocker blocker(m_moveRoute);
		m_moveRoute->setCommand(m_command, project->project().encoding().toStdString());
	}
	emit projectChanged();
}

void CommandEditSession::setContext(bool battle, bool isNew, int mapId, const QVariantList& events) {
	m_battle = battle;
	m_isNew = isNew;
	m_mapId = mapId;
	m_events = events;
	emit contextChanged();
}

QVariantList CommandEditSession::maps() const {
	QVariantList maps;
	if (!m_project) {
		return maps;
	}
	for (const auto& map : m_project->treeMap().maps) {
		if (map.ID > 0) {
			maps.append(QVariantMap{{QStringLiteral("value"), map.ID},
				{QStringLiteral("text"), QStringLiteral("%1: %2").arg(map.ID, 4, 10, QChar(u'0')).arg(ToQString(map.name))}});
		}
	}
	return maps;
}

QVariantList CommandEditSession::characterSets() const {
	// The keyboard pages depend on the game's encoding: EasyRPG Player shows the
	// language's own pages first, then letters and symbols. RPG Maker offers the first two.
	const QString encoding = m_project ? m_project->project().encoding().toLower() : QString();
	auto is = [&](std::initializer_list<const char*> codes) {
		return std::any_of(codes.begin(), codes.end(), [&](const char* code) { return encoding.contains(QLatin1String(code)); });
	};
	std::pair<const char*, const char*> pages = {QT_TRANSLATE_NOOP("EnterHeroNameWidget", "Letters"),
		QT_TRANSLATE_NOOP("EnterHeroNameWidget", "Symbols")};
	if (is({"932", "943"})) {
		pages = {QT_TRANSLATE_NOOP("EnterHeroNameWidget", "Hiragana"), QT_TRANSLATE_NOOP("EnterHeroNameWidget", "Katakana")};
	} else if (is({"949"})) {
		pages = {QT_TRANSLATE_NOOP("EnterHeroNameWidget", "Hangul 1"), QT_TRANSLATE_NOOP("EnterHeroNameWidget", "Hangul 2")};
	} else if (is({"936"})) {
		pages = {QT_TRANSLATE_NOOP("EnterHeroNameWidget", "Simplified Chinese 1"),
			QT_TRANSLATE_NOOP("EnterHeroNameWidget", "Simplified Chinese 2")};
	} else if (is({"950", "big5"})) {
		pages = {QT_TRANSLATE_NOOP("EnterHeroNameWidget", "Traditional Chinese 1"),
			QT_TRANSLATE_NOOP("EnterHeroNameWidget", "Traditional Chinese 2")};
	} else if (is({"1251", "5347"})) {
		pages = {QT_TRANSLATE_NOOP("EnterHeroNameWidget", "Cyrillic"), QT_TRANSLATE_NOOP("EnterHeroNameWidget", "Letters")};
	}
	return {
		QVariantMap{{QStringLiteral("value"), 0}, {QStringLiteral("text"), CommandSchemaView::label(Label("EnterHeroNameWidget", pages.first))}},
		QVariantMap{{QStringLiteral("value"), 1}, {QStringLiteral("text"), CommandSchemaView::label(Label("EnterHeroNameWidget", pages.second))}},
	};
}

void CommandEditSession::setLines(const QStringList& lines) {
	m_lines = lines.isEmpty() ? QStringList{QString()} : lines;
	m_command.string = ToDBString(m_lines.front());
	notifyChanged(QString());
}

const Schema* CommandEditSession::schemaOf() const {
	return m_schemaView->schema();
}

const Field* CommandEditSession::field(const QString& id) const {
	const Schema* schema = schemaOf();
	return schema ? schema->field(id.toStdString()) : nullptr;
}

QString CommandEditSession::path(const QString& fieldId) const {
	const Field* found = field(fieldId);
	return found ? CommandSchemaView::pathOf(*found) : QString();
}

int CommandEditSession::read(const QString& fieldId) const {
	const Field* found = field(fieldId);
	return found && found->isNumber() ? CommandSchema::read(*found, m_command) : 0;
}

bool CommandEditSession::write(const QString& fieldId, int value) {
	const Field* found = field(fieldId);
	if (!found || !found->isNumber()) {
		return false;
	}
	return set(CommandSchemaView::pathOf(*found), value);
}

bool CommandEditSession::isVisible(const QString& fieldId) const {
	const Field* found = field(fieldId);
	return found && CommandSchema::isVisible(*schemaOf(), *found, m_command);
}

bool CommandEditSession::isShown(const QString& fieldId) const {
	const Field* found = field(fieldId);
	if (!found || (found->flags & Flag::Hidden)) {
		return false;
	}
	const Schema& schema = *schemaOf();
	if (CommandSchema::isVisible(schema, *found, m_command)) {
		return true;
	}
	if (m_schemaView->hides()) {
		return false;
	}
	auto sameBits = [](const Field& a, const Field& b) {
		if (a.index < 0 || b.index < 0) {
			return a.usesString() && b.usesString();
		}
		return a.index == b.index && (a.mask() & b.mask()) != 0;
	};
	const std::string_view label = found->label.text ? found->label.text : "";
	for (const auto& other : schema.fields) {
		if (&other != found && other.hasValue() && sameBits(*found, other)
				&& label == std::string_view(other.label.text ? other.label.text : "")
				&& CommandSchema::isVisible(schema, other, m_command)) {
			return false;
		}
	}
	return true;
}

bool CommandEditSession::isEnabled(const QString& fieldId) const {
	const Field* found = field(fieldId);
	return found && CommandSchema::isEnabled(*schemaOf(), *found, m_command) && engineHas(fieldId) && engineHasCommand();
}

bool CommandEditSession::holdsExcept(const QString& fieldId, const QString& otherId) const {
	const Field* found = field(fieldId);
	if (!found) {
		return false;
	}
	const QByteArray other = otherId.toUtf8();
	std::vector<Condition> conditions;
	for (const auto& condition : found->visible) {
		if (other != condition.field) {
			conditions.push_back(condition);
		}
	}
	return CommandSchema::holds(*schemaOf(), conditions, m_command);
}

bool CommandEditSession::engineHas(const QString& fieldId) const {
	const Field* found = field(fieldId);
	if (!found) {
		return true;
	}
	return !((found->flags & Flag::Rpg2003) && !isRpg2k3()) && !((found->flags & Flag::Maniac) && !maniac());
}

bool CommandEditSession::engineHasCommand() const {
	// the commands of the Maniac Patch stay editable (the palette offers
	// them only in its games)
	const Schema* schema = schemaOf();
	return !schema || !((schema->flags & Flag::Rpg2003) && !isRpg2k3());
}

QVariant CommandEditSession::extra(const QString& key) const {
	return m_extras.value(key);
}

void CommandEditSession::setExtra(const QString& key, const QVariant& value) {
	m_extras.insert(key, value);
}

QVariantMap CommandEditSession::expression(const QString& fieldId) const {
	const Field* found = field(fieldId);
	if (!found || found->index < 0) {
		return {};
	}
	const auto& parameters = m_command.parameters;
	const size_t first = std::min<size_t>(found->index, parameters.size());
	const std::vector<int32_t> words(parameters.begin() + first, parameters.end());
	std::string text;
	const bool readable = words.empty() || ManiacExpression::decode(words, text);
	return {{QStringLiteral("text"), QString::fromStdString(text)}, {QStringLiteral("readable"), readable}};
}

int CommandEditSession::setExpression(const QString& fieldId, const QString& text) {
	const Field* found = field(fieldId);
	if (!found || found->index < 0) {
		return 0;
	}
	std::vector<int32_t> words;
	size_t error = 0;
	if (!ManiacExpression::encode(text.toStdString(), words, error)) {
		return static_cast<int>(error);
	}
	std::vector<int32_t> parameters(m_command.parameters.begin(), m_command.parameters.end());
	parameters.resize(found->index);
	parameters.insert(parameters.end(), words.begin(), words.end());
	if (!std::equal(parameters.begin(), parameters.end(), m_command.parameters.begin(), m_command.parameters.end())) {
		m_command.parameters = lcf::DBArray<int32_t>(parameters.begin(), parameters.end());
		notifyChanged(QString());
	}
	return -1;
}

QVariantMap CommandEditSession::variableExpression(bool target) const {
	const auto& words = target ? m_targetWords : m_operandWords;
	std::string text;
	const bool readable = words.empty() || ManiacExpression::decode(words, text);
	return {{QStringLiteral("text"), QString::fromStdString(text)}, {QStringLiteral("readable"), readable}};
}

int CommandEditSession::setVariableExpression(bool target, const QString& text) {
	std::vector<int32_t> words;
	size_t error = 0;
	if (!ManiacExpression::encode(text.toStdString(), words, error)) {
		return static_cast<int>(error);
	}
	(target ? m_targetWords : m_operandWords) = words;
	const lcf::rpg::EventCommand before = m_command;
	layoutExpressions();
	if (!std::equal(before.parameters.begin(), before.parameters.end(), m_command.parameters.begin(), m_command.parameters.end())) {
		notifyChanged(QString());
	}
	return -1;
}

void CommandEditSession::layoutExpressions() {
	if (m_command.code != static_cast<int>(Code::ControlVars)) {
		return;
	}
	auto parameter = [this](int i) { return i < static_cast<int>(m_command.parameters.size()) ? m_command.parameters[i] : 0; };
	const bool targetExpression = parameter(0) == 4;
	const bool operandExpression = parameter(4) == variableExpressionOperand;
	if (!targetExpression && !operandExpression) {
		return;
	}
	std::vector<int32_t> parameters(m_command.parameters.begin(), m_command.parameters.end());
	size_t end = parameters.size();
	if (operandExpression) {
		parameters.resize(6);
		parameters[5] = static_cast<int32_t>(m_operandWords.size());
		parameters.insert(parameters.end(), m_operandWords.begin(), m_operandWords.end());
		end = parameters.size();
	} else if (targetExpression) {
		// after the parameters of any operand, where it already is when it is there
		end = m_targetAt >= variableOperandEnd ? m_targetAt : variableOperandEnd;
		if (m_targetAt >= 0 && m_targetAt < variableOperandEnd) {
			// the old place of the expression: the next operand writes there
			std::fill(parameters.begin() + std::min<size_t>(m_targetAt, parameters.size()),
				parameters.begin() + std::min<size_t>(variableOperandEnd, parameters.size()), 0);
		}
	}
	if (targetExpression) {
		parameters.resize(end);
		parameters[1] = static_cast<int32_t>(end);
		parameters.push_back(static_cast<int32_t>(m_targetWords.size()));
		parameters.insert(parameters.end(), m_targetWords.begin(), m_targetWords.end());
		m_targetAt = static_cast<int>(end);
	}
	m_command.parameters = lcf::DBArray<int32_t>(parameters.begin(), parameters.end());
}

QString CommandEditSession::symbolsSource() {
	if (!m_symbols && m_project) {
		const ExFont::Sheet sheet = ExFont::load(m_project->project());
		m_symbols = std::pair{sheet.image, sheet.source};
	}
	return m_symbols ? m_symbols->second : QString();
}

QImage CommandEditSession::symbolImage(int index, const QColor& color, int scale) {
	symbolsSource();
	if (!m_symbols) {
		return {};
	}
	const QImage glyph = ExFont::glyph(m_symbols->first, index, color.isValid() ? color : QColor(Qt::black));
	return glyph.isNull() ? glyph : glyph.scaled(glyph.size() * std::max(1, scale), Qt::IgnoreAspectRatio, Qt::FastTransformation);
}

QString CommandEditSession::symbolCode(int index) {
	return ExFont::code(index);
}

QVariantList CommandEditSession::equipmentIds() const {
	QVariantList ids;
	if (m_project) {
		for (const auto& item : m_project->database().items) {
			if (item.type >= lcf::rpg::Item::Type_weapon && item.type <= lcf::rpg::Item::Type_accessory) {
				ids.append(item.ID);
			}
		}
	}
	return ids;
}

QString CommandEditSession::entryName(const QString& list, int id) const {
	if (!m_project) {
		return {};
	}
	const auto& db = m_project->database();
	if (list == QLatin1String("actors")) return entryText(db.actors, id);
	if (list == QLatin1String("switches")) return entryText(db.switches, id);
	if (list == QLatin1String("variables")) return entryText(db.variables, id);
	if (list == QLatin1String("items")) return entryText(db.items, id);
	if (list == QLatin1String("skills")) return entryText(db.skills, id);
	if (list == QLatin1String("states")) return entryText(db.states, id);
	if (list == QLatin1String("troops")) return entryText(db.troops, id);
	if (list == QLatin1String("terrains")) return entryText(db.terrains, id);
	if (list == QLatin1String("animations")) return entryText(db.animations, id);
	if (list == QLatin1String("chipsets")) return entryText(db.chipsets, id);
	if (list == QLatin1String("commonevents")) return entryText(db.commonevents, id);
	if (list == QLatin1String("classes")) return entryText(db.classes, id);
	if (list == QLatin1String("enemies")) return entryText(db.enemies, id);
	if (list == QLatin1String("battlecommands/commands")) return entryText(db.battlecommands.commands, id);
	return {};
}

QVariant CommandEditSession::pickEntry(const QString& list, int id, bool none, QQuickItem* from) {
	if (!m_project) {
		return {};
	}
	Pickers::EntryRequest request;
	request.list = list;
	request.id = id;
	request.none = none;
	const auto chosen = Pickers::pickEntry(from, *m_project, request);
	// the names and the size of the list may have changed in the picker
	m_database.notifyChanged(QString());
	return chosen ? QVariant(*chosen) : QVariant();
}

QVariant CommandEditSession::pickLocation(int map, int x, int y, int direction, QQuickItem* from) {
	if (!locationPicker() || !m_project) {
		return {};
	}
	// map 0: a cell of the map of the event
	const bool fixed = map <= 0;
	if (fixed && eventMap() <= 0) {
		return {};
	}
	const auto chosen = locationPicker()(Pickers::windowOf(from), *m_project, {fixed ? eventMap() : map, x, y, direction, fixed});
	if (!chosen) {
		return {};
	}
	return QVariantMap{{QStringLiteral("map"), chosen->map}, {QStringLiteral("x"), chosen->x}, {QStringLiteral("y"), chosen->y},
		{QStringLiteral("direction"), chosen->direction}};
}

QVariant CommandEditSession::pickTile(int layer, int tile, QQuickItem* from) {
	if (!tilePicker() || !m_project) {
		return {};
	}
	if (eventMap() <= 0) {
		return {};
	}
	const auto chosen = tilePicker()(Pickers::windowOf(from), *m_project, eventMap(), layer, tile);
	return chosen ? QVariant(*chosen) : QVariant();
}

void CommandEditSession::help(QQuickItem* from) {
	QmlHost::help(Pickers::windowOf(from), Help::commandTopic(m_command.code));
}

bool CommandEditSession::accept() {
	return !m_acceptor || m_acceptor(*this);
}

int CommandEditSession::eventMap() const {
	return m_mapId > 0 ? m_mapId : Pickers::openMapId();
}

bool CommandEditSession::canPickLocation() const {
	return bool(locationPicker());
}

QObject* CommandEditSession::moveRoute() const {
	return m_moveRoute;
}

bool CommandEditSession::canPickTile() const {
	return bool(tilePicker());
}

void CommandEditSession::setLocationPicker(LocationPicker picker) {
	locationPicker() = std::move(picker);
}

void CommandEditSession::setTilePicker(TilePicker picker) {
	tilePicker() = std::move(picker);
}

/*
 * Reading and writing
 */

bool CommandEditSession::defaultIsText(const QString& fieldId) const {
	const Field* found = field(fieldId);
	return found && !found->isNumber();
}

QVariant CommandEditSession::readSlot(const Slot& slot) const {
	switch (slot.what) {
		case Slot::What::Root:
			return {};
		case Slot::What::Default: {
			const Field* found = field(slot.field);
			if (!found || !found->isNumber()) {
				return QString();
			}
			return CommandSchema::defaultOf(*found, m_command);
		}
		case Slot::What::Code:
			return m_command.code;
		case Slot::What::Indent:
			return m_command.indent;
		case Slot::What::String:
			return ToQString(m_command.string);
		case Slot::What::Lines:
			return m_lines.join(u'\n');
		case Slot::What::Parameters: {
			QVariantList list;
			for (int32_t value : m_command.parameters) {
				list.append(value);
			}
			return list;
		}
		case Slot::What::Bits: {
			uint32_t value = 0;
			if (slot.index < static_cast<int>(m_command.parameters.size())) {
				value = static_cast<uint32_t>(m_command.parameters[slot.index]);
			} else if (const Schema* schema = schemaOf()) {
				// what it reads missing: the defaults of the schema's fields on
				// it (the first of the alternatives on the same bits), as
				// CommandSchema::write grows the list
				uint32_t set = 0;
				for (const auto& one : schema->fields) {
					if (one.isNumber() && one.index == slot.index && !(one.mask() & set)) {
						value |= (static_cast<uint32_t>(CommandSchema::defaultOf(one, m_command)) << one.shift) & one.mask();
						set |= one.mask();
					}
				}
			}
			if (slot.bits >= 32) {
				return static_cast<int>(value);
			}
			return static_cast<int>((value & maskOf(slot.shift, slot.bits)) >> slot.shift);
		}
	}
	return {};
}

bool CommandEditSession::writeBits(int index, int shift, int bits, int value) {
	const Schema* schema = schemaOf();
	Field target;
	target.kind = Kind::Number;
	target.index = index;
	target.shift = shift;
	target.bits = bits;
	if (schema) {
		return CommandSchema::write(*schema, target, m_command, value);
	}
	// no schema: zeros before it
	if (index >= static_cast<int>(m_command.parameters.size())) {
		if (CommandSchema::read(target, m_command) == value) {
			return false;
		}
		std::vector<int32_t> parameters(m_command.parameters.begin(), m_command.parameters.end());
		parameters.resize(static_cast<size_t>(index) + 1, 0);
		m_command.parameters = lcf::DBArray<int32_t>(parameters.begin(), parameters.end());
	}
	const uint32_t mask = maskOf(shift, bits);
	const uint32_t old = static_cast<uint32_t>(m_command.parameters[index]);
	const uint32_t updated = (old & ~mask) | ((static_cast<uint32_t>(value) << shift) & mask);
	if (updated == old) {
		return false;
	}
	m_command.parameters[index] = static_cast<int32_t>(updated);
	return true;
}

bool CommandEditSession::writeSlot(const Slot& slot, const QVariant& value, bool* changed) {
	*changed = false;
	const lcf::rpg::EventCommand before = m_command;
	const QStringList linesBefore = m_lines;
	int index = -1;
	switch (slot.what) {
		case Slot::What::Root:
		case Slot::What::Default:
			// a field greyed shows its default: nothing to write
			return false;
		case Slot::What::Code:
		case Slot::What::Indent: {
			const auto number = toNumber(value);
			if (!number) {
				return false;
			}
			int32_t& target = slot.what == Slot::What::Code ? m_command.code : m_command.indent;
			const auto clamped = static_cast<int32_t>(std::clamp<qint64>(*number, 0, std::numeric_limits<int32_t>::max()));
			*changed = target != clamped;
			target = clamped;
			break;
		}
		case Slot::What::String: {
			if (!value.isValid() || value.typeId() == QMetaType::QVariantList || !value.canConvert<QString>()) {
				return false;
			}
			const QString text = value.toString();
			*changed = ToQString(m_command.string) != text;
			m_command.string = ToDBString(text);
			if (!m_lines.isEmpty()) {
				m_lines.front() = text;
			}
			break;
		}
		case Slot::What::Lines: {
			if (!value.isValid() || value.typeId() == QMetaType::QVariantList || !value.canConvert<QString>()) {
				return false;
			}
			const QStringList lines = value.toString().split(u'\n');
			*changed = lines != m_lines;
			m_lines = lines;
			m_command.string = ToDBString(m_lines.front());
			break;
		}
		case Slot::What::Parameters: {
			if (!value.canConvert<QVariantList>() || value.typeId() == QMetaType::QString) {
				return false;
			}
			std::vector<int32_t> parameters;
			for (const auto& one : value.toList()) {
				const auto number = toNumber(one);
				if (!number) {
					return false;
				}
				parameters.push_back(static_cast<int32_t>(std::clamp<qint64>(*number, std::numeric_limits<int32_t>::min(),
					std::numeric_limits<int32_t>::max())));
			}
			*changed = !std::equal(parameters.begin(), parameters.end(), m_command.parameters.begin(), m_command.parameters.end());
			m_command.parameters = lcf::DBArray<int32_t>(parameters.begin(), parameters.end());
			break;
		}
		case Slot::What::Bits: {
			const auto number = toNumber(value);
			if (!number) {
				return false;
			}
			qint64 low = std::numeric_limits<int32_t>::min();
			qint64 high = std::numeric_limits<int32_t>::max();
			if (slot.bits < 32) {
				low = 0;
				high = (qint64(1) << slot.bits) - 1;
			}
			*changed = writeBits(slot.index, slot.shift, slot.bits, static_cast<int>(std::clamp(*number, low, high)));
			index = slot.index;
			break;
		}
	}
	if (*changed && !m_adjusting && slot.what != Slot::What::Parameters && slot.what != Slot::What::Code) {
		adjust(before, index);
	}
	if (*changed) {
		// the other values of the parameter, the rules: everything is read again
		notifyChanged(QString());
	}
	return true;
}

std::vector<bool> CommandEditSession::shown() const {
	std::vector<bool> result;
	if (const Schema* schema = schemaOf()) {
		for (const auto& one : schema->fields) {
			result.push_back(one.isNumber() && CommandSchema::isVisible(*schema, one, m_command));
		}
	}
	return result;
}

void CommandEditSession::adjust(const lcf::rpg::EventCommand& before, int index) {
	m_adjusting = true;
	const Schema* schema = schemaOf();
	if (m_command.code == static_cast<int>(Code::ControlVars) && m_command.parameters.size() > 4
		&& (before.parameters.size() <= 4 || before.parameters[4] != m_command.parameters[4])) {
		// another operand: its values start from 0, not from the last one's
		// (an actor ID is not an item ID), then from the defaults of its fields
		for (size_t i = 5; i < std::min<size_t>(variableOperandEnd, m_command.parameters.size()); ++i) {
			m_command.parameters[i] = 0;
		}
		// the binary operation: "+"
		if (m_command.parameters[4] == 19) {
			writeBits(5, 0, 32, 1);
		}
	}
	if (schema) {
		// a field the change shows starts on its default (the mode of a
		// value chosen: the variable, not the number it had)
		const lcf::rpg::EventCommand now = m_command;
		m_command = before;
		const std::vector<bool> wasShown = shown();
		m_command = now;
		// a mode written to its default shows other fields: again until none
		// appears (the values of another type of condition read as its modes)
		std::vector<bool> written(schema->fields.size(), false);
		for (int pass = 0; pass < 4; ++pass) {
			const std::vector<bool> isShown = shown();
			bool wrote = false;
			for (size_t i = 0; i < schema->fields.size(); ++i) {
				const Field& one = schema->fields[i];
				if (isShown[i] && !wasShown[i] && !written[i] && one.index != index) {
					CommandSchema::write(*schema, one, m_command, CommandSchema::defaultOf(one, m_command));
					written[i] = true;
					wrote = true;
				}
			}
			if (!wrote) {
				break;
			}
		}
	}

	auto parameter = [this](const lcf::rpg::EventCommand& command, int i, int fallback = 0) {
		return i < static_cast<int>(command.parameters.size()) ? command.parameters[i] : fallback;
	};
	static const lcf::rpg::Database empty;
	const lcf::rpg::Database& db = m_project ? m_project->database() : empty;
	switch (static_cast<Code>(m_command.code)) {
		case Code::ChangeHeroName:
		case Code::ChangeHeroTitle: {
			// A new hero suggests its name, unless a name was typed: the text is
			// kept when the command is opened, and is never another hero's name
			const int previousId = parameter(before, 0, 1);
			const int heroId = parameter(m_command, 0, 1);
			if (!m_project || previousId == heroId) {
				break;
			}
			const auto* hero = lcf::ReaderUtil::GetElement(db.actors, heroId);
			const auto* previous = lcf::ReaderUtil::GetElement(db.actors, previousId);
			const bool title = m_command.code == static_cast<int>(Code::ChangeHeroTitle);
			auto text = [title](const lcf::rpg::Actor& actor) { return ToQString(title ? actor.title : actor.name); };
			const QString typed = ToQString(m_command.string);
			if (hero && (typed.isEmpty() || (previous && typed == text(*previous)))) {
				m_command.string = ToDBString(text(*hero));
				m_lines = {text(*hero)};
			}
			break;
		}
		case Code::ControlVars:
			// a target or an operand of an expression: the expressions placed again
			if (parameter(before, 0) != parameter(m_command, 0) || parameter(before, 4) != parameter(m_command, 4)) {
				layoutExpressions();
			}
			break;
		case Code::EnemyEncounter:
			// RPG Maker 2003 reads the formation of a specific background: written with it
			if (isRpg2k3() && parameter(m_command, 2) == 1 && m_command.parameters.size() <= 7) {
				writeBits(7, 0, 32, 1);
			}
			break;
		case Code::ChangeEquipment:
			// unequipping: the item is 0, as RPG Maker writes it
			if (parameter(m_command, 2) == 1) {
				writeBits(4, 0, 32, 0);
			}
			break;
		case Code::ChangeBattleCommands:
			// "All" (0) only removes: adding needs a command of the database
			if (parameter(m_command, 3) == 1 && parameter(m_command, 2) == 0 && !db.battlecommands.commands.empty()) {
				writeBits(2, 0, 32, db.battlecommands.commands.front().ID);
			}
			break;
		default:
			break;
	}
	m_adjusting = false;
}
