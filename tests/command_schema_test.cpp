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

// The schemas the QML command editors are generated from (CommandSchema):
// one for each command with an editor of its own, their
// fields within the parameters of a new command (EventCommandDefaults),
// bits that do not overlap, values read and written back, rules on fields
// of the schema, and texts with a French translation.

#include <QFile>
#include <QMetaEnum>
#include <QSet>
#include <QTest>
#include <QXmlStreamReader>
#include <algorithm>
#include <limits>
#include <optional>
#include "common/dbstring.h"
#include "model/command_schema.h"
#include "model/event_command_defaults.h"

using Code = lcf::rpg::EventCommand::Code;
using namespace CommandSchema;

namespace {
// The commands with an editor of their own (with Move Event): each has a
// schema, so a QML editor (hand-written for a custom schema, else the form
// of the schema: the automation's check-command-editors opens them all).
// The other commands with settings (hasEditor) get the editor of the raw
// parameters.
const QList<Code> widgetsEditors = {
	Code::CallCommonEvent, Code::ForceFlee, Code::EnableCombo, Code::ChangeClass, Code::ChangeBattleCommands,
	Code::ShowMessage, Code::MessageOptions, Code::ChangeFaceGraphic, Code::ShowChoice, Code::InputNumber,
	Code::ControlSwitches, Code::ControlVars, Code::TimerOperation, Code::ChangeGold, Code::ChangeItems,
	Code::ChangePartyMembers, Code::ChangeExp, Code::ChangeLevel, Code::ChangeParameters, Code::ChangeSkills,
	Code::ChangeEquipment, Code::ChangeHP, Code::ChangeSP, Code::ChangeCondition, Code::FullHeal,
	Code::SimulatedAttack, Code::ChangeHeroName, Code::ChangeHeroTitle, Code::ChangeSpriteAssociation,
	Code::ChangeActorFace, Code::ChangeVehicleGraphic, Code::ChangeSystemBGM, Code::ChangeSystemSFX,
	Code::ChangeSystemGraphics, Code::ChangeScreenTransitions, Code::EnemyEncounter, Code::OpenShop,
	Code::ShowInn, Code::EnterHeroName, Code::Teleport, Code::MemorizeLocation, Code::RecallToLocation,
	Code::SetVehicleLocation, Code::ChangeEventLocation, Code::TradeEventLocations, Code::StoreTerrainID,
	Code::StoreEventID, Code::EraseScreen, Code::ShowScreen, Code::TintScreen, Code::FlashScreen,
	Code::ShakeScreen, Code::PanScreen, Code::WeatherEffects, Code::ShowPicture, Code::MovePicture,
	Code::ErasePicture, Code::ShowBattleAnimation, Code::PlayerVisibility, Code::FlashSprite, Code::MoveEvent,
	Code::Wait, Code::PlayBGM, Code::FadeOutBGM, Code::PlaySound, Code::PlayMovie, Code::KeyInputProc,
	Code::ChangeMapTileset, Code::ChangePBG, Code::ChangeEncounterSteps, Code::TileSubstitution,
	Code::TeleportTargets, Code::ChangeTeleportAccess, Code::EscapeTarget, Code::ChangeEscapeAccess,
	Code::ChangeSaveAccess, Code::ChangeMainMenuAccess, Code::ConditionalBranch, Code::Label,
	Code::JumpToLabel, Code::CallEvent, Code::Comment, Code::ChangeMonsterHP, Code::ChangeMonsterMP,
	Code::ChangeMonsterCondition, Code::ShowHiddenMonster, Code::ChangeBattleBG, Code::ShowBattleAnimation_B,
	Code::ConditionalBranch_B, Code::Maniac_ShowStringPicture, Code::Maniac_ControlStrings,
	Code::Maniac_GetSaveInfo, Code::Maniac_Save, Code::Maniac_Load, Code::Maniac_GetMousePosition,
	Code::Maniac_SetMousePosition, Code::Maniac_GetPictureInfo, Code::Maniac_ControlBattle,
	Code::Maniac_ControlAtbGauge, Code::Maniac_ChangeBattleCommandEx, Code::Maniac_GetBattleInfo,
	Code::Maniac_ControlVarArray, Code::Maniac_KeyInputProcEx, Code::Maniac_RewriteMap,
	Code::Maniac_ControlGlobalSave, Code::Maniac_ChangePictureId, Code::Maniac_SetGameOption,
	Code::Maniac_CallCommand, Code::Maniac_GetGameInfo, Code::Maniac_EditPicture, Code::Maniac_WritePicture,
	Code::Maniac_EditTile, Code::Maniac_ControlTextProcessing, Code::Maniac_Zoom,
};

// The commands with settings and no editor of their own: the raw parameters
// (EasyRPG Player's commands, Add Move Route), or the first line of their
// message or comment
const QList<Code> rawEditors = {
	Code::EasyRpg_TriggerEventAt, Code::EasyRpg_Pathfinder, Code::EasyRpg_CallMovementAction,
	Code::EasyRpg_WaitForSingleMovement, Code::EasyRpg_AnimateVariable, Code::EasyRpg_SetInterpreterFlag,
	Code::EasyRpg_ProcessJson, Code::EasyRpg_CloneMapEvent, Code::EasyRpg_DestroyMapEvent,
	Code::EasyRpg_StringPictureMenu, Code::Maniac_AddMoveRoute, Code::ShowMessage_2, Code::Comment_2,
};

QByteArray codeName(Code code) {
	const char* tag = lcf::rpg::EventCommand::kCodeTags.tag(code);
	return tag ? QByteArray(tag) : QByteArray::number(static_cast<int>(code));
}

QByteArray where(const Schema& schema, const Field& field) {
	return codeName(schema.code) + '.' + field.id + " (" + QMetaEnum::fromType<Kind>().valueToKey(static_cast<int>(field.kind)) + ')';
}

lcf::rpg::EventCommand newCommand(Code code) {
	return EventCommandDefaults::make(code).front();
}

std::vector<int32_t> parameters(const lcf::rpg::EventCommand& command) {
	return {command.parameters.begin(), command.parameters.end()};
}

/** The first field of a parameter, with the bits it uses (the tails: all the next parameters) */
bool sameBits(const Field& a, const Field& b) {
	if (a.usesString() && b.usesString()) {
		return true;
	}
	if (a.index < 0 || b.index < 0) {
		return false;
	}
	if (a.isTail() || b.isTail()) {
		const Field& tail = a.isTail() ? a : b;
		const Field& other = a.isTail() ? b : a;
		return other.index >= tail.index;
	}
	return a.index == b.index && (a.mask() & b.mask()) != 0;
}

bool disjoint(const Condition& a, const Condition& b) {
	auto contains = [](const Condition& condition, int value) {
		return std::find(condition.values.begin(), condition.values.end(), value) != condition.values.end();
	};
	if (!a.negated && !b.negated) {
		return std::none_of(a.values.begin(), a.values.end(), [&](int value) { return contains(b, value); });
	}
	if (a.negated != b.negated) {
		// one has values all excluded by the other
		const Condition& positive = a.negated ? b : a;
		const Condition& negative = a.negated ? a : b;
		return std::all_of(positive.values.begin(), positive.values.end(), [&](int value) { return contains(negative, value); });
	}
	return false;
}

/** Their rules never show both: a field each has values the other excludes */
bool exclusive(const Field& a, const Field& b) {
	for (const auto& first : a.visible) {
		for (const auto& second : b.visible) {
			if (std::string_view(first.field) == second.field && disjoint(first, second)) {
				return true;
			}
		}
	}
	return false;
}

/** Another value the field can have, std::nullopt for none */
std::optional<int> otherValue(const Field& field, int value) {
	const int largest = field.bits >= 32 ? std::numeric_limits<int>::max() : static_cast<int>(field.mask() >> field.shift);
	auto fits = [&](int other) { return other != value && (field.bits >= 32 || (other >= 0 && other <= largest)); };
	switch (field.kind) {
		case Kind::Choice:
		case Kind::CharacterSet:
			for (const auto& item : field.items) {
				if (fits(item.value)) {
					return item.value;
				}
			}
			return std::nullopt;
		case Kind::Check:
			return value == 0 ? 1 : 0;
		case Kind::Number:
		case Kind::Time:
		case Kind::Tile:
		case Kind::MapX:
		case Kind::MapY:
		case Kind::StringId: {
			const int high = std::min(field.maximum, largest);
			for (int other : {value + 1, value - 1, field.minimum, high}) {
				if (other >= field.minimum && other <= high && fits(other)) {
					return other;
				}
			}
			return std::nullopt;
		}
		default:
			// an ID, a character, a face or character index
			return fits(value + 1) ? std::optional<int>(value + 1) : std::nullopt;
	}
}

/** (context, source) of the messages of a .ts file */
QSet<QPair<QString, QString>> translations(const QString& path) {
	QSet<QPair<QString, QString>> messages;
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly)) {
		return messages;
	}
	QXmlStreamReader xml(&file);
	QString context;
	while (!xml.atEnd()) {
		if (xml.readNext() != QXmlStreamReader::StartElement) {
			continue;
		}
		if (xml.name() == QLatin1String("name")) {
			context = xml.readElementText();
		} else if (xml.name() == QLatin1String("source")) {
			messages.insert({context, xml.readElementText()});
		}
	}
	return messages;
}
}

class CommandSchemaTest : public QObject
{
	Q_OBJECT

private slots:
	void widgetsEditorsHaveSchemas()
	{
		for (Code code : widgetsEditors) {
			QVERIFY2(hasEditor(code), codeName(code));
			const Schema* schema = find(code);
			QVERIFY2(schema, "no schema: " + codeName(code));
			QCOMPARE(static_cast<int>(schema->code), static_cast<int>(code));
		}
		// and no schema for a command without an editor of its own
		QSet<int> codes;
		for (Code code : widgetsEditors) {
			codes.insert(static_cast<int>(code));
		}
		QCOMPARE(static_cast<qsizetype>(all().size()), codes.size());
		for (const auto& schema : all()) {
			QVERIFY2(codes.contains(static_cast<int>(schema.code)), "no Widgets editor: " + codeName(schema.code));
		}
		// by code, one each
		QVERIFY(std::is_sorted(all().begin(), all().end(), [](const Schema& a, const Schema& b) {
			return static_cast<int>(a.code) < static_cast<int>(b.code);
		}));
	}

	void commandsWithSettings_data()
	{
		QTest::addColumn<int>("code");
		for (const auto& tag : lcf::rpg::EventCommand::kCodeTags) {
			QTest::newRow(tag.name) << static_cast<int>(tag.value);
		}
	}

	void commandsWithSettings()
	{
		QFETCH(int, code);
		const auto value = static_cast<Code>(code);
		if (!CommandSchema::hasEditor(value)) {
			// no settings: the structure of a block, Loop, Game Over...
			QVERIFY(!find(value));
			QVERIFY(!widgetsEditors.contains(value));
		} else {
			QCOMPARE(find(value) != nullptr, !rawEditors.contains(value));
		}
	}

	void fieldsAreWellFormed()
	{
		for (const auto& schema : all()) {
			QVERIFY2(!schema.title.isEmpty(), codeName(schema.code));
			QSet<QByteArray> ids;
			for (const auto& field : schema.fields) {
				if (!field.hasValue()) {
					QVERIFY2(!field.label.isEmpty() && *field.id == 0 && field.index == StringIndex, where(schema, field));
					continue;
				}
				QVERIFY2(*field.id != 0, where(schema, field));
				QVERIFY2(!ids.contains(field.id), "the same id twice: " + where(schema, field));
				ids.insert(field.id);
				QVERIFY2(field.shift >= 0 && field.bits >= 1 && field.shift + field.bits <= 32, where(schema, field));
				QVERIFY2(field.minimum <= field.maximum, where(schema, field));
				// the string, else a parameter
				const bool string = field.kind == Kind::String || field.kind == Kind::Lines || field.kind == Kind::File;
				QVERIFY2(string == (field.index == StringIndex), where(schema, field));
				QVERIFY2(!field.usesString() || string || field.index >= 0, where(schema, field));
				if (field.kind == Kind::File || field.kind == Kind::Face || field.kind == Kind::Charset) {
					QVERIFY2(field.folder, where(schema, field));
				}
				if (field.kind == Kind::Choice || field.kind == Kind::CharacterSet) {
					QVERIFY2(!field.items.empty(), where(schema, field));
				}
				QSet<int> values;
				for (const auto& item : field.items) {
					QVERIFY2(!item.text.isEmpty() && !item.text.translated().isEmpty(), where(schema, field));
					QVERIFY2(!values.contains(item.value), "the same value twice: " + where(schema, field));
					values.insert(item.value);
				}
				if (field.link) {
					const Field* linked = schema.field(field.link);
					QVERIFY2(linked && linked != &field, "link: " + where(schema, field));
				}
			}
		}
	}

	void parametersWithinDefaults()
	{
		for (const auto& schema : all()) {
			const auto count = static_cast<int>(EventCommandDefaults::parameters(schema.code).size());
			for (const auto& field : schema.fields) {
				if (!field.hasValue() || field.index == StringIndex) {
					continue;
				}
				if (field.isTail()) {
					// after the parameters, none in a new command
					QVERIFY2(field.index <= count || (field.flags & Flag::Extension), where(schema, field));
				} else if (field.flags & Flag::Extension) {
					// of a newer engine or a patch: missing in a new command
					QVERIFY2(field.index >= count, "an extension in the defaults: " + where(schema, field));
				} else {
					QVERIFY2(field.index < count, "beyond the defaults: " + where(schema, field));
				}
			}
		}
	}

	void bitsDoNotOverlap()
	{
		for (const auto& schema : all()) {
			for (size_t i = 0; i < schema.fields.size(); ++i) {
				const Field& a = schema.fields[i];
				if (!a.hasValue()) {
					continue;
				}
				for (size_t j = i + 1; j < schema.fields.size(); ++j) {
					const Field& b = schema.fields[j];
					if (b.hasValue() && sameBits(a, b)) {
						// alternatives: the value of another mode, of another operation
						QVERIFY2(exclusive(a, b), "overlap: " + where(schema, a) + " and " + where(schema, b));
					}
				}
			}
		}
	}

	void valuesRoundTrip()
	{
		for (const auto& schema : all()) {
			const auto command = newCommand(schema.code);
			for (const auto& field : schema.fields) {
				if (field.usesString() && field.index == StringIndex) {
					auto copy = command;
					writeString(copy, readString(command));
					QCOMPARE(ToQString(copy.string), ToQString(command.string));
					const QString text = QStringLiteral("Été %1").arg(QString::fromLatin1(field.id));
					writeString(copy, text);
					QCOMPARE(readString(copy), text);
					continue;
				}
				if (!field.isNumber()) {
					continue;
				}
				// the value written back changes nothing
				const int value = read(field, command);
				auto copy = command;
				QVERIFY2(!write(schema, field, copy, value), where(schema, field));
				QVERIFY2(parameters(copy) == parameters(command), where(schema, field));

				// another value changes only the bits of the field
				const auto other = otherValue(field, value);
				if (!other) {
					continue;
				}
				QVERIFY2(write(schema, field, copy, *other), where(schema, field));
				QVERIFY2(read(field, copy) == *other, where(schema, field));
				const auto before = parameters(command);
				const auto after = parameters(copy);
				for (size_t i = 0; i < before.size(); ++i) {
					const uint32_t kept = static_cast<int>(i) == field.index ? ~field.mask() : 0xFFFFFFFFu;
					QVERIFY2((static_cast<uint32_t>(before[i]) & kept) == (static_cast<uint32_t>(after[i]) & kept), where(schema, field));
				}
				// (the alternatives not shown on a missing parameter read the default of the shown one;
				// a missing one copying the parameter follows it)
				for (const auto& another : schema.fields) {
					const bool copying = another.copies == field.index && another.index >= static_cast<int>(before.size());
					if (another.isNumber() && &another != &field && !sameBits(field, another) && !copying
						&& (another.index < static_cast<int>(before.size()) || isVisible(schema, another, command))) {
						QVERIFY2(read(another, copy) == read(another, command), where(schema, field) + " changed " + another.id);
					}
				}

				// and back
				write(schema, field, copy, value);
				QVERIFY2(read(field, copy) == value, where(schema, field));
				if (field.index < static_cast<int>(before.size())) {
					QVERIFY2(parameters(copy) == before, where(schema, field));
				}
			}
		}
	}

	void rulesReferenceFields()
	{
		for (const auto& schema : all()) {
			for (const auto& field : schema.fields) {
				for (const auto* rules : {&field.visible, &field.enabled}) {
					for (const auto& condition : *rules) {
						const Field* target = schema.field(condition.field ? condition.field : "");
						QVERIFY2(target, "no field " + QByteArray(condition.field) + ": " + where(schema, field));
						QVERIFY2(target != &field, where(schema, field));
						QVERIFY2(target->isNumber(), where(schema, field));
						QVERIFY2(!condition.values.empty(), where(schema, field));
					}
				}
			}
		}
		// and they read the command
		const Schema* money = find(Code::ChangeGold);
		QVERIFY(money);
		auto command = newCommand(Code::ChangeGold);
		const Field* amount = money->field("amount");
		const Field* variable = money->field("amountVariable");
		QVERIFY(amount && variable);
		QVERIFY(isVisible(*money, *amount, command));
		QVERIFY(!isVisible(*money, *variable, command));
		QVERIFY(write(*money, *money->field("amountOperand"), command, 1));
		QVERIFY(!isVisible(*money, *amount, command));
		QVERIFY(isVisible(*money, *variable, command));
	}

	void newCommandsHaveListedValues()
	{
		for (const auto& schema : all()) {
			const auto command = newCommand(schema.code);
			for (const auto& field : schema.fields) {
				if (field.kind != Kind::Choice || !field.isNumber() || !isVisible(schema, field, command)) {
					continue;
				}
				const int value = read(field, command);
				QVERIFY2(std::any_of(field.items.begin(), field.items.end(), [&](const Item& item) { return item.value == value; }),
					where(schema, field) + " reads " + QByteArray::number(value));
			}
		}
	}

	void labelsAreTranslated()
	{
		auto messages = translations(QStringLiteral(EDITOR_TRANSLATIONS_DIR "/editor_fr.ts"));
		if (messages.isEmpty()) {
			QSKIP("no translations/editor_fr.ts");
		}
		// texts of the command editors the translation has not caught up with
		// yet (lupdate adds them): the old "Saturation:" is in it
		messages.insert({QStringLiteral("FlashScreenWidget"), QStringLiteral("Strength:")});
		QStringList missing;
		auto check = [&](const Label& label) {
			if (label.isEmpty() || !label.context || !*label.context) {
				return;
			}
			const QString context = QString::fromLatin1(label.context);
			const QString source = QString::fromUtf8(label.text);
			if (!messages.contains({context, source})) {
				missing << context + QStringLiteral(": ") + source;
			}
		};
		for (const auto& schema : all()) {
			check(schema.title);
			for (const auto& field : schema.fields) {
				check(field.label);
				check(field.suffix);
				for (const auto& item : field.items) {
					check(item.text);
				}
			}
		}
		missing.removeDuplicates();
		QVERIFY2(missing.isEmpty(), qPrintable(missing.join(QLatin1Char('\n'))));
	}
};

QTEST_GUILESS_MAIN(CommandSchemaTest)
#include "command_schema_test.moc"
