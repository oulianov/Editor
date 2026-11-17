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

#include "command_origin.h"
#include "common/editor_settings.h"
#include "model/game_setup.h"
#include <string_view>

namespace {

using Code = lcf::rpg::EventCommand::Code;
using CommandSchema::Flag;

/** The prefix of the calls of EasyRPG Player's own plugin (as it reads them: case matters) */
constexpr std::string_view easyRpgCall = "@easyrpg_";

void add(QStringList& origins, const QString& origin) {
	if (!origin.isEmpty() && !origins.contains(origin))
		origins.append(origin);
}

/** A comment calling EasyRPG Player's plugin */
bool callsEasyRpg(const lcf::rpg::EventCommand& command) {
	const Code code = static_cast<Code>(command.code);
	return (code == Code::Comment || code == Code::Comment_2)
		&& std::string_view(command.string).starts_with(easyRpgCall);
}

}

QString CommandOrigin::ofFlags(CommandSchema::Flags flags) {
	if (flags & (Flag::Maniac | Flag::ManiacOnly))
		return QStringLiteral("maniac");
	if (flags & Flag::EasyRpg)
		return QStringLiteral("easyrpg");
	if (flags & Flag::Engine2003)
		return QStringLiteral("engine2003");
	if (flags & Flag::Rpg2003E)
		return QStringLiteral("rpg2003e");
	if (flags & Flag::Rpg2003)
		return QStringLiteral("rpg2003");
	return QString();
}

QString CommandOrigin::ofCode(Code code) {
	switch (code) {
		// RPG Maker 2003's commands (its battles, its classes): EasyRPG
		// Player runs them in a 2000 game with RPG2k3Commands
		case Code::CallCommonEvent:
		case Code::ForceFlee:
		case Code::EnableCombo:
		case Code::ChangeClass:
		case Code::ChangeBattleCommands:
			return QStringLiteral("rpg2003");
		// those of its versions 1.10 and later, without settings
		// (Player::IsRPG2k3ECommands)
		case Code::OpenLoadMenu:
		case Code::ExitGame:
		case Code::ToggleAtbMode:
		case Code::ToggleFullscreen:
		case Code::OpenVideoOptions:
			return QStringLiteral("rpg2003e");
		default:
			break;
	}
	// the commands of the extensions, by their codes. (A schema of a command
	// of RPG Maker marked Maniac has settings only with the Maniac Patch:
	// Loop, Erase Event... RPG Maker's commands, the settings the patch's)
	const int number = static_cast<int>(code);
	if (number >= 3000 && number < 4000)
		return QStringLiteral("maniac");
	if (number >= 2000 && number < 3000)
		return QStringLiteral("easyrpg");
	return QString();
}

QStringList CommandOrigin::uses(const lcf::rpg::EventCommand& command) {
	const Code code = static_cast<Code>(command.code);
	QStringList origins;
	add(origins, ofCode(code));

	// a call of a plugin: EasyRPG Player's own (@easyrpg_..., on any line of
	// a comment), else DynRPG's (a first line starting with @; DynRPG adds
	// the next lines to it)
	if (callsEasyRpg(command))
		add(origins, QStringLiteral("easyrpg"));
	else if (code == Code::Comment && std::string_view(command.string).starts_with('@'))
		add(origins, QStringLiteral("dynrpg"));
	// a script of the Ineluki Key Patch, read by Play Sound Effect
	if (code == Code::PlaySound
			&& QString::fromStdString(std::string(command.string)).endsWith(QLatin1String(".script"), Qt::CaseInsensitive))
		add(origins, QStringLiteral("keypatch"));

	// the options set: a field of an extension with a value other than its
	// default, an item of an extension chosen. The fields its rules apply:
	// the hidden ones too (values the rules read: a target in a variable)
	const CommandSchema::Schema* schema = CommandSchema::find(code);
	if (!schema)
		return origins;
	// a command of RPG Maker with settings only with the Maniac Patch (Loop,
	// Break Loop, Erase Event...): its fields are the patch's
	const CommandSchema::Flags inherited = (schema->flags & Flag::Maniac) && ofCode(code).isEmpty()
		? CommandSchema::Flags(Flag::Maniac) : CommandSchema::Flags();
	for (const CommandSchema::Field& field : schema->fields) {
		if (!field.hasValue() || (field.flags & Flag::NotManiac) || !CommandSchema::holds(*schema, field.visible, command)
				|| !CommandSchema::isEnabled(*schema, field, command))
			continue;
		const QString origin = ofFlags(field.flags | inherited);
		if (field.isNumber()) {
			const int value = CommandSchema::read(field, command);
			if (!origin.isEmpty() && value != CommandSchema::defaultOf(field, command))
				add(origins, origin);
			for (const CommandSchema::Item& item : field.items)
				if (item.value == value)
					add(origins, ofFlags(item.flags));
		} else if (field.usesString() && !origin.isEmpty() && !command.string.empty()) {
			add(origins, origin);
		}
	}
	return origins;
}

bool CommandOrigin::available(const QString& origin) {
	const GameSetup& game = GameSetup::instance();
	if (origin == QLatin1String("rpg2003"))
		return game.commands2k3();
	if (origin == QLatin1String("rpg2003e"))
		return game.commands2k3e();
	if (origin == QLatin1String("engine2003"))
		return game.rpg2k3();
	if (origin == QLatin1String("maniac"))
		return game.maniac();
	if (origin == QLatin1String("easyrpg"))
		return game.easyrpg();
	// the patches: their key in GameSetup::patches
	const QString patch = origin == QLatin1String("dynrpg") ? QStringLiteral("DynRPG")
		: origin == QLatin1String("keypatch") ? QStringLiteral("KeyPatch")
		: origin == QLatin1String("powermode") ? QStringLiteral("PowerMode2003") : QString();
	return patch.isEmpty() || game.patches().value(patch).toInt() > 0;
}

bool CommandOrigin::tagged(const QString& origin) {
	// the badges hidden in the Editor Settings
	if (!EditorSettings::instance().originBadges())
		return false;
	if (origin == QLatin1String("rpg2003"))
		return !GameSetup::instance().rpg2k3();
	// the version of RPG Maker 2003 a command needs: tagged as the add-ons
	static const QStringList all = {QStringLiteral("rpg2003e"), QStringLiteral("maniac"), QStringLiteral("easyrpg"), QStringLiteral("player"),
		QStringLiteral("dynrpg"), QStringLiteral("keypatch"), QStringLiteral("powermode")};
	return all.contains(origin);
}

bool CommandOrigin::shown(const QString& origin) {
	return EditorSettings::instance().unavailableShown() || available(origin);
}

bool CommandOrigin::runs(const QString& origin, const lcf::rpg::EventCommand& command) {
	if (available(origin))
		return true;
	// DynRPG runs EasyRPG Player's plugin too, from the first line of a comment
	return origin == QLatin1String("easyrpg") && static_cast<Code>(command.code) == Code::Comment
		&& callsEasyRpg(command) && available(QStringLiteral("dynrpg"));
}

QStringList CommandOrigin::notRun(const lcf::rpg::EventCommand& command) {
	QStringList origins;
	for (const QString& origin : uses(command))
		if (!runs(origin, command))
			origins.append(origin);
	return origins;
}
