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

#include <QColor>
#include <QHash>
#include <QImage>
#include <QObject>
#include <QQuickItem>
#include <QStringList>
#include <QVariant>
#include <QtQmlIntegration>
#include <functional>
#include <memory>
#include <optional>
#include <lcf/rpg/database.h>
#include <lcf/rpg/eventcommand.h>

#include "json.h"
#include "json_t.h"
#include "json_view.h"
#include "model/command_schema.h"

class MoveRouteModel;
class ProjectData;
class QWindow;

/**
 * The schema of a command for QML (CommandSchema::Schema): its fields as
 * maps, and the layout of its form (SchemaForm.qml). The texts are
 * {context, text, number}: QML translates them (qsTranslate), so they follow
 * the language of the interface.
 *
 * A field: {id, kind ("Number", "Switch"...), label, index, shift, bits,
 * path (its pointer in the session), minimum, maximum, step, defaultValue,
 * suffix, items [{value, text, rpg2003, maniac, kept}], radio, slider,
 * inline, inverted, hidden, extension, rpg2003, maniac, equipment, off,
 * folder, link, limit, list (the database list of an ID: "switches"...),
 * none (the items before the list's entries: {text, value}, or null)}.
 *
 * sections: [{title (a text or null), rows}], a row: {field, inline: [the
 * fields after it on its row], options: [{item, inline}] for radio buttons,
 * each with the fields on its line}.
 */
class CommandSchemaView : public QObject {
	Q_OBJECT
	QML_ELEMENT
	QML_UNCREATABLE("A command editor's session makes it")
	/** There is a schema (else: the raw editor) */
	Q_PROPERTY(bool valid READ valid CONSTANT)
	/** The editor is hand-written (the fields describe it) */
	Q_PROPERTY(bool custom READ custom CONSTANT)
	/** Its fields hide when their rules fail (as the Maniac Patch editors); else they are greyed */
	Q_PROPERTY(bool hides READ hides CONSTANT)
	Q_PROPERTY(QVariantMap title READ title CONSTANT)
	Q_PROPERTY(QVariantList fields READ fields CONSTANT)
	Q_PROPERTY(QVariantList sections READ sections CONSTANT)

public:
	explicit CommandSchemaView(const CommandSchema::Schema* schema, QObject* parent = nullptr);

	bool valid() const {
		return m_schema != nullptr;
	}
	bool custom() const;
	bool hides() const;
	QVariantMap title() const;
	QVariantList fields() const {
		return m_fields;
	}
	QVariantList sections() const {
		return m_sections;
	}
	const CommandSchema::Schema* schema() const {
		return m_schema;
	}

	/** The field with this id as a map (empty for none) */
	Q_INVOKABLE QVariantMap field(const QString& id) const;

	/** A text as QML translates it: {context, text, number} */
	static QVariantMap label(const CommandSchema::Label& label);
	/** The pointer of a field in a session: "/parameters/3", "/parameters/0@4:4", "/string" */
	static QString pathOf(const CommandSchema::Field& field);
	/** The list of the database of an ID kind ("switches"...), "" for the others */
	static QString listOf(CommandSchema::Kind kind);

private:
	QVariantMap fieldMap(const CommandSchema::Field& field) const;

	const CommandSchema::Schema* m_schema;
	QVariantList m_fields;
	QVariantList m_sections;
};

/**
 * What an editor of an event command edits: a copy of the command as JSON,
 * so the Ez controls bind to it (Ez.SpinBox { jsonData: session; key:
 * "/parameters/2" }):
 *
 * - "/code", "/indent", "/string": the command;
 * - "/parameters": all of them (a list); "/parameters/N": one, also beyond
 *   the end (it reads the default of its field, and grows the list when
 *   written: the new parameters get the defaults of the schema's fields);
 * - "/parameters/N@shift:bits": some bits of one, the others kept;
 * - "/lines": the text of a message or comment, its lines joined by "\n";
 * - "/defaults/ID": the default value of the field ID, read-only (what a
 *   field greyed by its rules shows).
 *
 * A value the editor does not know stays as it is until a control writes it.
 * When a write makes a field of the schema shown (a mode chosen), it gets
 * its default value. The editors of some commands
 * suggest values too (a hero's name when the hero changes...).
 *
 * The context of the editor comes with it: the database of the project
 * (database, its lists for the Ez.RefComboBox), the engine (rpg2k3,
 * maniac), the maps and the events of the map of the event.
 */
class CommandEditSession : public Json {
	Q_OBJECT
	QML_ELEMENT
	QML_UNCREATABLE("Made by the command editors (CommandEditorRegistry)")
	Q_PROPERTY(int code READ code NOTIFY revisionChanged)
	/** Grows on every change: the bindings reading the rules depend on it */
	Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)
	Q_PROPERTY(CommandSchemaView* schema READ schema CONSTANT)
	/** The database of the project (null without a project) */
	Q_PROPERTY(JsonView* database READ database NOTIFY projectChanged)
	Q_PROPERTY(bool rpg2k3 READ isRpg2k3 NOTIFY projectChanged)
	/** The game uses the Maniac Patch */
	Q_PROPERTY(bool maniac READ maniac NOTIFY projectChanged)
	/** A command of a troop's battle page */
	Q_PROPERTY(bool battle READ battle NOTIFY contextChanged)
	/** The command was just added (cancel removes it) */
	Q_PROPERTY(bool isNew READ isNew NOTIFY contextChanged)
	/** The map of the event (0: a common event or a battle page) */
	Q_PROPERTY(int mapId READ mapId NOTIFY contextChanged)
	/** The map the cells of the event are on: its own, else the map open (0: none) */
	Q_PROPERTY(int eventMap READ eventMap NOTIFY contextChanged)
	/** The maps of the project: {value, text} ("0001: name") */
	Q_PROPERTY(QVariantList maps READ maps NOTIFY projectChanged)
	/** The events of the map of the event: {value, text} */
	Q_PROPERTY(QVariantList events READ events NOTIFY contextChanged)
	/** The pages of the keyboard of Enter Hero Name in the encoding of the game: {value, text} */
	Q_PROPERTY(QVariantList characterSets READ characterSets NOTIFY projectChanged)
	/** A cell can be chosen on a map (pickLocation) */
	Q_PROPERTY(bool canPickLocation READ canPickLocation CONSTANT)
	/** A tile can be chosen in the chipset (pickTile) */
	Q_PROPERTY(bool canPickTile READ canPickTile CONSTANT)
	/**
	 * Move Event: its target, frequency and moves as a MoveRouteModel
	 * (null for the other commands); its changes are the command's
	 */
	Q_PROPERTY(QObject* moveRoute READ moveRoute CONSTANT)

public:
	using Code = lcf::rpg::EventCommand::Code;

	struct Location {
		int map = 0;
		int x = 0;
		int y = 0;
		/** The facing chosen with the cell (0 retained, 1 up...), -1: not asked */
		int direction = -1;
		/** A cell of map only (the map of the event: no Map field) */
		bool fixed = false;
	};
	/** Chooses a cell on a map of the project, over a window (nothing: cancelled) */
	using LocationPicker = std::function<std::optional<Location>(QWindow* over, ProjectData& project, const Location& current)>;
	/** Chooses a tile (0-143) of the chipset of a map: layer 0 lower, 1 upper */
	using TilePicker = std::function<std::optional<int>(QWindow* over, ProjectData& project, int map, int layer, int tile)>;

	explicit CommandEditSession(const lcf::rpg::EventCommand& command, QObject* parent = nullptr);
	~CommandEditSession() override;

	JsonNode node(const QString& jsonPtr) const override;
	bool isRpg2k3() const override;

	/** The command edited */
	const lcf::rpg::EventCommand& command() const {
		return m_command;
	}
	int code() const {
		return m_command.code;
	}
	int revision() const {
		return m_revision;
	}
	CommandSchemaView* schema() const {
		return m_schemaView;
	}
	JsonView* database();

	/** The project whose database the editor shows (and its pickers edit) */
	void setProject(ProjectData* project);
	ProjectData* project() const {
		return m_project;
	}
	/** Where the command is (the defaults: an event of no map, new) */
	void setContext(bool battle, bool isNew, int mapId, const QVariantList& events);
	bool battle() const {
		return m_battle;
	}
	bool isNew() const {
		return m_isNew;
	}
	int mapId() const {
		return m_mapId;
	}
	int eventMap() const;
	bool maniac() const;
	QVariantList maps() const;
	QVariantList events() const {
		return m_events;
	}
	QVariantList characterSets() const;

	/** The lines of a message or comment ("/lines"), the first one being the string */
	QStringList lines() const {
		return m_lines;
	}
	void setLines(const QStringList& lines);

	/** The pointer of a field of the schema ("" for none) */
	Q_INVOKABLE QString path(const QString& fieldId) const;
	/** The value of a number field */
	Q_INVOKABLE int read(const QString& fieldId) const;
	/** Writes a number field, as its control would */
	Q_INVOKABLE bool write(const QString& fieldId, int value);
	/** Its rules show the field */
	Q_INVOKABLE bool isVisible(const QString& fieldId) const;
	/**
	 * The form shows the field: its rules show it, else it is greyed (with
	 * its default value), unless the form hides what does not apply (the
	 * Maniac Patch) or another field takes its place (one on the same bits
	 * with the same label: the transitions of erasing or showing)
	 */
	Q_INVOKABLE bool isShown(const QString& fieldId) const;
	/** Its rules let it be changed, in this engine (RPG Maker 2003, the Maniac Patch) */
	Q_INVOKABLE bool isEnabled(const QString& fieldId) const;
	/** The rules of a field showing it, except the ones on another field */
	Q_INVOKABLE bool holdsExcept(const QString& fieldId, const QString& otherId) const;
	/** The engine has what the field needs (RPG Maker 2003, the Maniac Patch) */
	Q_INVOKABLE bool engineHas(const QString& fieldId) const;
	/** The engine has what the command needs */
	Q_INVOKABLE bool engineHasCommand() const;

	/**
	 * Values of the editor beside the command (the texts of the choices of
	 * Show Choices...): what CommandEditorRegistry gives and takes back
	 */
	Q_INVOKABLE QVariant extra(const QString& key) const;
	Q_INVOKABLE void setExtra(const QString& key, const QVariant& value);
	QVariantMap extras() const {
		return m_extras;
	}

	/**
	 * The Maniac Patch expression of a field (Expression: the words from its
	 * parameter to the last one) as text: {text, readable (false: the Player
	 * could not read it, it is kept)}
	 */
	Q_INVOKABLE QVariantMap expression(const QString& fieldId) const;
	/** Writes the expression of a field from its text: -1, else the offset of the error (nothing written) */
	Q_INVOKABLE int setExpression(const QString& fieldId, const QString& text);

	/**
	 * Control Variables with the Maniac Patch: the expression of the target
	 * (target) or of the operand, {text, readable}. Each is kept while the
	 * other target or operand is chosen, and placed again in the parameters
	 * when one is (the operand's after p5, the target's after the operand)
	 */
	Q_INVOKABLE QVariantMap variableExpression(bool target) const;
	/** Writes it from its text: -1, else the offset of the error (nothing written) */
	Q_INVOKABLE int setVariableExpression(bool target, const QString& text);

	/** Where the symbols of messages come from (ExFont::load): the game's font, its RPG_RT.exe, EasyRPG's */
	Q_INVOKABLE QString symbolsSource();
	/** A symbol of messages ($A: 0, $a: 26) in color, its pixels scale times larger */
	Q_INVOKABLE QImage symbolImage(int index, const QColor& color, int scale);
	/** The code of a symbol: "$A" */
	Q_INVOKABLE static QString symbolCode(int index);

	/** The IDs of the items one equips (weapons, shields, armors, helmets, accessories) */
	Q_INVOKABLE QVariantList equipmentIds() const;

	/** The name of an entry of the database list ("0012: Potion"), "" for none */
	Q_INVOKABLE QString entryName(const QString& list, int id) const;

	/**
	 * Picks an entry of a database list in its picker (Pickers::pickEntry,
	 * over the window of from): the ID, null when cancelled
	 */
	Q_INVOKABLE QVariant pickEntry(const QString& list, int id, bool none, QQuickItem* from);
	/**
	 * Picks a cell of a map (map 0: the map of the event), with a facing
	 * when direction is not -1: {map, x, y, direction}, null when cancelled
	 * or without a picker
	 */
	Q_INVOKABLE QVariant pickLocation(int map, int x, int y, int direction, QQuickItem* from);
	/** Picks a tile of the chipset: its number, null when cancelled or without a picker */
	Q_INVOKABLE QVariant pickTile(int layer, int tile, QQuickItem* from);
	/** Opens the guide at the topic of the command, next to from */
	Q_INVOKABLE void help(QQuickItem* from);

	/**
	 * OK of the editor: the check of CommandEditorRegistry (branches to
	 * delete asked...). False: the editor stays open
	 */
	Q_INVOKABLE bool accept();
	using Acceptor = std::function<bool(CommandEditSession&)>;
	void setAcceptor(Acceptor acceptor) {
		m_acceptor = std::move(acceptor);
	}

	bool canPickLocation() const;
	bool canPickTile() const;
	QObject* moveRoute() const;
	/** How the editors pick a cell or a tile (by default: Pickers::pickLocation
	 *  and the Select Tile window of the map tree); tests replace them */
	static void setLocationPicker(LocationPicker picker);
	static void setTilePicker(TilePicker picker);

signals:
	void revisionChanged();
	void projectChanged();
	void contextChanged();

private:
	struct Slot;
	friend struct CommandEditSessionNodes;

	/** The default of the field is a text (a file...), not a number */
	bool defaultIsText(const QString& fieldId) const;
	/** The value of a slot */
	QVariant readSlot(const Slot& slot) const;
	/** Writes a slot (changed: the command differs now); false: not a value of it */
	bool writeSlot(const Slot& slot, const QVariant& value, bool* changed);
	/** Writes bits of a parameter, growing the list with the schema's defaults */
	bool writeBits(int index, int shift, int bits, int value);
	/** After a change: defaults of the fields shown, the suggestions of the command */
	void adjust(const lcf::rpg::EventCommand& before, int index);
	/** The fields of the schema the rules show (by position) */
	std::vector<bool> shown() const;
	const CommandSchema::Field* field(const QString& id) const;
	const CommandSchema::Schema* schemaOf() const;

	lcf::rpg::EventCommand m_command;
	QStringList m_lines;
	CommandSchemaView* m_schemaView = nullptr;
	ProjectData* m_project = nullptr;
	JsonT<lcf::rpg::Database> m_database;
	bool m_battle = false;
	bool m_isNew = true;
	int m_mapId = 0;
	QVariantList m_events;
	QVariantMap m_extras;
	Acceptor m_acceptor;
	int m_revision = 0;
	/** Writing: the changes the session makes itself are not adjusted again */
	bool m_adjusting = false;
	mutable QHash<QString, std::shared_ptr<Slot>> m_slots;
	/** Control Variables: the words of the expressions, where the target's is (-1: nowhere) */
	std::vector<int32_t> m_targetWords;
	std::vector<int32_t> m_operandWords;
	int m_targetAt = -1;
	/** Places the expressions of Control Variables in the parameters */
	void layoutExpressions();
	/** Move Event: the route edited */
	MoveRouteModel* m_moveRoute = nullptr;
	/** The ExFont of the game, read once asked */
	std::optional<std::pair<QImage, QString>> m_symbols;
};
