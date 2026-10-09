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

#include <QObject>
#include <algorithm>
#include "stringizer.h"
#include "model/event_command_defaults.h"
#include "common/dbstring.h"
#include <lcf/rpg/eventcommand.h>
#include "core.h"

/*
 * FIXME: This uses some very verbose way to satisfy lupdate not swallowing
 * strings from anonymous namespace by invoking the translation function directly.
 */

namespace
{
	// Helper functions
	QString variable(int id, bool show_name = false)
	{
		if (show_name)
			return QString("V[%1:%2]").arg(id, 4, 10, QChar('0')).arg(Stringizer::varName(id));
		else
			return QString("V[%1]").arg(id, 4, 10, QChar('0'));
	}

	QString valueOrVariable(bool is_variable, int value)
	{
		return is_variable ? variable(value)
						   : QString::number(value);
	}

	QString heroes(int mode, int id)
	{
		switch (mode)
		{
		case 0:
			return Stringizer::tr("Entire Party");
		case 1:
			return Stringizer::heroName(id);
		case 2:
			return Stringizer::tr("Hero") + " " + variable(id);
		default:
			return Stringizer::tr("Unknown");
		}
	}

	QString map(int id)
	{
		return QString("%1").arg(id, 4, 10, QChar('0')) + ":" + Stringizer::mapName(id);
	}

	QString tenthSeconds(int t)
	{
		return QString::number(t / 10.0, 'f', 1) + "s";
	}

	QString characterName(int id)
	{
		switch (id)
		{
		case 10001:
			return Stringizer::tr("Hero");
		case 10002:
			return Stringizer::tr("Boat");
		case 10003:
			return Stringizer::tr("Ship");
		case 10004:
			return Stringizer::tr("Airship");
		case 10005:
			return Stringizer::tr("This Event");
		default:
			return Stringizer::eventName(id);
		}
	}

	QString formatTime(int seconds)
	{
		return QString("%1M %2S").arg(seconds / 60).arg(seconds % 60, 2, 10, QChar('0'));
	}

	// Stringizers
	QString stringizeCallCommonEvent(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Call Common Event") + ": " + Stringizer::commonEventName(com.parameters[0]);
	}

	QString monsterAmount(const lcf::rpg::EventCommand& com)
	{
		const QString sign = com.parameters[1] ? "-" : "+";
		switch (com.parameters[2]) {
			case 0: return sign + QString::number(com.parameters[3]);
			case 1: return sign + variable(com.parameters[3]);
			case 2: return sign + QString::number(com.parameters[3]) + "%";
			default: return Stringizer::tr("Unknown");
		}
	}

	QString stringizeForceFlee(const lcf::rpg::EventCommand& com)
	{
		QString target;
		switch (com.parameters[0]) {
			case 0: target = Stringizer::tr("Party"); break;
			case 1: target = Stringizer::tr("All Monsters"); break;
			case 2: target = Stringizer::tr("Monster %1").arg(com.parameters[1] + 1); break;
			default: target = Stringizer::tr("Unknown"); break;
		}
		return Stringizer::tr("Force Flee") + ": " + target
			+ (com.parameters[2] == 0 ? " " + Stringizer::tr("(Unless Surrounded)") : QString());
	}

	QString stringizeEnableCombo(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Enable Combo") + ": "
			+ Stringizer::heroName(com.parameters[0]) + ", "
			+ QString::number(com.parameters[2]) + "x"
			+ Stringizer::battleCommandName(com.parameters[1]);
	}

	QString stringizeChangeClass(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Change Class") + ": "
			+ heroes(com.parameters[0], com.parameters[1]) + ", "
			+ Stringizer::className(com.parameters[2]);
	}

	QString stringizeChangeBattleCommands(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Change Battle Commands") + ": "
			+ heroes(com.parameters[0], com.parameters[1]) + ", "
			+ (com.parameters[3] ? Stringizer::tr("Add") : Stringizer::tr("Remove")) + " "
			+ Stringizer::battleCommandName(com.parameters[2]);
	}

	QString stringizeShowMessage(const lcf::rpg::EventCommand& com)
	{
		// the next lines are aligned under the text by the command list
		return Stringizer::tr("Message") + ": " + ToQString(com.string);
	}

	QString stringizeMessageOptions(const lcf::rpg::EventCommand& com)
	{
		QString position;
		switch (com.parameters[1])
		{
		case 0:
			position = Stringizer::tr("Top"); break;
		case 1:
			position = Stringizer::tr("Center"); break;
		case 2:
			position = Stringizer::tr("Bottom"); break;
		default:
			position = Stringizer::tr("Unknown");
		}
		return Stringizer::tr("Message Options") + ": "
			+ (com.parameters[0] ? Stringizer::tr("Transp") : Stringizer::tr("Normal")) + ", "
			+ position + ", "
			+ (com.parameters[2] ? Stringizer::tr("Auto") : Stringizer::tr("Fixed")) + ", "
			+ (com.parameters[3] ? Stringizer::tr("Continue Processes") : Stringizer::tr("Halt Processes"));
	}

	QString stringizeChangeFaceGraphic(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Change Face Graphic") + ": "
			+ ToQString(com.string) + ", "
			+ QString::number(com.parameters[0]) + ", "
			+ (com.parameters[1] ? Stringizer::tr("Right") : Stringizer::tr("Left"))
			+ (com.parameters[2] ? ", " + Stringizer::tr("Flipped") : "");
	}

	QString stringizeShowChoice(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Show Choice") + ": " + ToQString(com.string);
	}

	QString stringizeInputNumber(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Input Number") + ": "
			+ Stringizer::tr("%1 Digits").arg(QString::number(com.parameters[0])) + ", "
			+ variable(com.parameters[1]);
	}

	QString stringizeControlSwitches(const lcf::rpg::EventCommand& com)
	{
		QString switches, operation;
		switch (com.parameters[0])
		{
		case 0:
			switches = QString("S[%1:%2]").arg(com.parameters[1], 4, 10, QChar('0'))
										  .arg(Stringizer::switchName(com.parameters[1]));
			break;
		case 1:
			switches = QString("S[%1-%2]").arg(com.parameters[1], 4, 10, QChar('0'))
										  .arg(com.parameters[2], 4, 10, QChar('0'));
			break;
		case 2:
			switches = QString("S[V%1]").arg(com.parameters[1], 4, 10, QChar('0'));
			break;
		default:
			switches = "S[????]";
		}

		switch (com.parameters[3])
		{
		case 0:
			operation = Stringizer::tr("ON"); break;
		case 1:
			operation = Stringizer::tr("OFF"); break;
		case 2:
			operation = Stringizer::tr("Toggle"); break;
		default:
			operation = Stringizer::tr("Unknown"); break;
		}
		return Stringizer::tr("Control Switches") + ": " + switches + " " + operation;
	}

	QString stringizeControlVars(const lcf::rpg::EventCommand& com)
	{
		QString variables, operation, operand;
		switch (com.parameters[0])
		{
		case 0:
			variables = variable(com.parameters[1], true);
			break;
		case 1:
			variables = QString("V[%1-%2]").arg(com.parameters[1], 4, 10, QChar('0'))
										   .arg(com.parameters[2], 4, 10, QChar('0'));
			break;
		case 2:
			variables = QString("V[%1]").arg(variable(com.parameters[1]));
			break;
		default:
			variables = "[????]";
		}

		switch (com.parameters[3])
		{
		case 0:
			operation = "="; break;
		case 1:
			operation = "+"; break;
		case 2:
			operation = "-"; break;
		case 3:
			operation = "*"; break;
		case 4:
			operation = "/"; break;
		case 5:
			operation = "%"; break;
		default:
			operation = "?";
		}

		switch (com.parameters[4])
		{
		case 0: // Constant
			operand = QString::number(com.parameters[5]);
			break;
		case 1: // Value of Variable
			operand = variable(com.parameters[5]);
			break;
		case 2: // Value of Variable referenced by another Variable
			operand = QString("V[%1]").arg(variable(com.parameters[5]));
			break;
		case 3: // Random Value
			operand = Stringizer::tr("Random %1-%2").arg(com.parameters[5], com.parameters[6]);
			break;
		case 4: // Items
			operand = com.parameters[6] ? Stringizer::tr("Number Equipped") : Stringizer::tr("Number Possessed") + " "
				+ Stringizer::itemName(com.parameters[5]);
			break;
		case 5: // Hero
			operand = Stringizer::heroName(com.parameters[5]) + " ";
			switch (com.parameters[6])
			{
			case 0:
				operand += Stringizer::tr("Level"); break;
			case 1:
				operand += Stringizer::tr("Experience"); break;
			case 2:
				operand += Stringizer::tr("HP"); break;
			case 3:
				operand += Stringizer::tr("MP"); break;
			case 4:
				operand += Stringizer::tr("Max HP"); break;
			case 5:
				operand += Stringizer::tr("Max MP"); break;
			case 6:
				operand += Stringizer::tr("Attack"); break;
			case 7:
				operand += Stringizer::tr("Defense"); break;
			case 8:
				operand += Stringizer::tr("Intelligence"); break;
			case 9:
				operand += Stringizer::tr("Agility"); break;
			case 10:
				operand += Stringizer::tr("Weapon ID"); break;
			case 11:
				operand += Stringizer::tr("Shield ID"); break;
			case 12:
				operand += Stringizer::tr("Armor ID"); break;
			case 13:
				operand += Stringizer::tr("Helmet ID"); break;
			case 14:
				operand += Stringizer::tr("Accesory ID"); break;
			default:
				operand += Stringizer::tr("Unknown");
			}
			break;
		case 6: // Characters
			operand = characterName(com.parameters[5]) + " ";
			switch (com.parameters[6])
			{
			case 0:
				operand += Stringizer::tr("Map ID"); break;
			case 1:
				operand += Stringizer::tr("X Coordinate"); break;
			case 2:
				operand += Stringizer::tr("Y Coordinate"); break;
			case 3:
				operand += Stringizer::tr("Orientation"); break;
			case 4:
				operand += Stringizer::tr("Screen X"); break;
			case 5:
				operand += Stringizer::tr("Screen Y"); break;
			default:
				operand += Stringizer::tr("Unknown");
			}
			break;
		case 7: // More
			switch (com.parameters[5]) {
			case 0:
				operand = Stringizer::tr("Gold"); break;
			case 1:
				operand = Stringizer::tr("Timer 1"); break;
			case 2:
				operand = Stringizer::tr("Size of Party"); break;
			case 3:
				operand = Stringizer::tr("Number of Saves"); break;
			case 4:
				operand = Stringizer::tr("Number of Battles"); break;
			case 5:
				operand = Stringizer::tr("Number of Won Battles"); break;
			case 6:
				operand = Stringizer::tr("Number of Lost Battles"); break;
			case 7:
				operand = Stringizer::tr("Number of Escapes"); break;
			case 8:
				operand = Stringizer::tr("MIDI Play Position"); break;
			case 9:
				operand = Stringizer::tr("Timer 2"); break;
			default:
				operand = Stringizer::tr("Unknown");
			}
			break;
		default:
			operand = Stringizer::tr("Unknown");
		}

		return Stringizer::tr("Control Vars") + ": "
			+ variables + " " + operation + " " + operand;

	}

	QString stringizeTimerOperation(const lcf::rpg::EventCommand& com)
	{
		QString command = Stringizer::tr("Timer %1 Operation").arg(com.parameters.size() <= 5 ? 1 : com.parameters[5]+1);

		switch (com.parameters[0])
		{
		case 0:
			return command + ": " + Stringizer::tr("Set") + " "
				+ (com.parameters[1] ? variable(com.parameters[2])
									 : formatTime(com.parameters[2]));
		case 1:
			return command + ": " + Stringizer::tr("Start");
		case 2:
			return command + ": " + Stringizer::tr("Stop");
		default:
			return command + ": " + Stringizer::tr("Unknown");
		}
	}

	QString stringizeChangeGold(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Change Gold") + ": "
			+ (com.parameters[0] ? "-" : "+")
			+ valueOrVariable(com.parameters[1], com.parameters[2]);
	}

	QString stringizeChangeItems(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Change Items") + ": "
			+ (com.parameters[0] ? "-" : "+") + " "
			+ valueOrVariable(com.parameters[3], com.parameters[4])
			+ (com.parameters[1] ? Stringizer::tr("Item") + " " + variable(com.parameters[2])
								 : Stringizer::itemName(com.parameters[2]));
	}

	QString stringizeChangePartyMembers(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Change Party Members") + ": "
			+ (com.parameters[0] ? Stringizer::tr("Remove") : Stringizer::tr("Add")) + " "
			+ (com.parameters[1] ? Stringizer::tr("Hero") + " " + variable(com.parameters[2])
								 : Stringizer::heroName(com.parameters[2]));
	}

	QString stringizeChangeExp(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Change Exp") + ": "
			+ heroes(com.parameters[0], com.parameters[1]) + ", "
			+ (com.parameters[2] ? "-" : "+")
			+ valueOrVariable(com.parameters[3], com.parameters[4]);
	}

	QString stringizeChangeLevel(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Change Level") + ": "
			+ heroes(com.parameters[0], com.parameters[1])+ ", "
			+ (com.parameters[2] ? "-" : "+")
			+ valueOrVariable(com.parameters[3], com.parameters[4]);
	}

	QString stringizeChangeParameters(const lcf::rpg::EventCommand& com)
	{
		QString parameter;
		switch (com.parameters[3])
		{
		case 0:
			parameter = Stringizer::tr("Max HP"); break;
		case 1:
			parameter = Stringizer::tr("Max MP"); break;
		case 2:
			parameter = Stringizer::tr("Attack"); break;
		case 3:
			parameter = Stringizer::tr("Defense"); break;
		case 4:
			parameter = Stringizer::tr("Spirit"); break;
		case 5:
			parameter = Stringizer::tr("Agility"); break;
		default:
			parameter = Stringizer::tr("Unknown");
		}

		return Stringizer::tr("Change Parameters") + ": "
			+ heroes(com.parameters[0], com.parameters[1])+ ", "
			+ parameter + " "
			+ (com.parameters[2] ? "-" : "+")
			+ valueOrVariable(com.parameters[4], com.parameters[5]);
	}

	QString stringizeChangeSkills(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Change Skills") + ": "
			+ heroes(com.parameters[0], com.parameters[1])+ ", "
			+ (com.parameters[2] ? Stringizer::tr("Remove") : Stringizer::tr("Add")) + " "
			+ (com.parameters[3] ? variable(com.parameters[4])
								 : Stringizer::skillName(com.parameters[4]));
	}

	QString stringizeChangeEquipment(const lcf::rpg::EventCommand& com)
	{
		// p2: 0 equip (p3: item or variable, p4: id), 1 unequip (p3: slot)
		QString what;
		if (com.parameters[2] == 0) {
			what = Stringizer::tr("Equip") + " " + (com.parameters[3] ? variable(com.parameters[4]) : Stringizer::itemName(com.parameters[4]));
		} else {
			switch (com.parameters[3]) {
			case 0: what = Stringizer::tr("Weapon"); break;
			case 1: what = Stringizer::tr("Shield"); break;
			case 2: what = Stringizer::tr("Armor"); break;
			case 3: what = Stringizer::tr("Helmet"); break;
			case 4: what = Stringizer::tr("Accessory"); break;
			case 5: what = Stringizer::tr("All"); break;
			default: what = Stringizer::tr("Unknown");
			}
			what = Stringizer::tr("Unequip") + " " + what;
		}
		return Stringizer::tr("Change Equipment") + ": " + heroes(com.parameters[0], com.parameters[1]) + ", " + what;
	}

	QString stringizeChangeHP(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Change HP") + ": "
			+ heroes(com.parameters[0], com.parameters[1])+ ", "
			+ (com.parameters[2] ? "-" : "+")
			+ valueOrVariable(com.parameters[3], com.parameters[4]);
	}

	QString stringizeChangeSP(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Change SP") + ": "
			+ heroes(com.parameters[0], com.parameters[1])+ ", "
			+ (com.parameters[2] ? "-" : "+")
			+ valueOrVariable(com.parameters[3], com.parameters[4]);
	}

	QString stringizeChangeCondition(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Change Condition") + ": "
			+ heroes(com.parameters[0], com.parameters[1])+ ", "
			+ (com.parameters[2] ? Stringizer::tr("Remove") : Stringizer::tr("Add")) + " "
			+ Stringizer::stateName(com.parameters[3]);
	}

	QString stringizeFullHeal(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Full Heal") + ": "
			+ heroes(com.parameters[0], com.parameters[1]);
	}

	QString stringizeSimulatedAttack(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Simulated Attack") + ": "
			+ heroes(com.parameters[0], com.parameters[1]);
	}

	QString stringizeChangeHeroName(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Change Hero Name") + ": "
			+ Stringizer::heroName(com.parameters[0]) + " -> "
			+ ToQString(com.string);
	}

	QString stringizeChangeHeroTitle(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Change Hero Title") + ": "
			+ Stringizer::heroName(com.parameters[0]) + ", "
			+ ToQString(com.string);
	}

	QString stringizeChangeSpriteAssociation(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Change Sprite Association") + ": "
			+ Stringizer::heroName(com.parameters[0]) + ", "
			+ ToQString(com.string) + "-"
			+ QString::number(com.parameters[1]);
	}

	QString stringizeChangeActorFace(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Change Actor Face") + ": "
			+ Stringizer::heroName(com.parameters[0]) + ", "
			+ ToQString(com.string) + "-"
			+ QString::number(com.parameters[1]);
	}

	QString stringizeChangeVehicleGraphic(const lcf::rpg::EventCommand& com)
	{
		QString vehicle =
			com.parameters[0] == 0 ? Stringizer::tr("Boat") :
			com.parameters[0] == 1 ? Stringizer::tr("Ship") :
			com.parameters[0] == 2 ? Stringizer::tr("Airship") : Stringizer::tr("Unknown");
		return Stringizer::tr("Change Vehicle Graphic") + ": "
			+ vehicle + ", "
			+ ToQString(com.string) + "-"
			+ QString::number(com.parameters[1]);
	}

	QString stringizeChangeSystemBGM(const lcf::rpg::EventCommand& com)
	{
		static const std::vector<const char*> bgm_contexts =
			{
				QT_TRANSLATE_NOOP("Stringizer", "Battle"),
				QT_TRANSLATE_NOOP("Stringizer", "Victory"),
				QT_TRANSLATE_NOOP("Stringizer", "Inn"),
				QT_TRANSLATE_NOOP("Stringizer", "Boat"),
				QT_TRANSLATE_NOOP("Stringizer", "Ship"),
				QT_TRANSLATE_NOOP("Stringizer", "Airship"),
				QT_TRANSLATE_NOOP("Stringizer", "Game Over")
			};

		QString context;
		if (com.parameters[0] < 0 || com.parameters[0] >= static_cast<int>(bgm_contexts.size()))
			context = Stringizer::tr("Unknown");
		else
			context = Stringizer::tr(bgm_contexts[static_cast<size_t>(com.parameters[0])]);
		return Stringizer::tr("Change System BGM") + ": " + context + ", "
			+ ToQString(com.string);
	}

	QString stringizeChangeSystemSFX(const lcf::rpg::EventCommand& com)
	{
		static const std::vector<const char*> system_sfx =
			{
				QT_TRANSLATE_NOOP("Stringizer", "Cursor"),
				QT_TRANSLATE_NOOP("Stringizer", "Decision"),
				QT_TRANSLATE_NOOP("Stringizer", "Cancel"),
				QT_TRANSLATE_NOOP("Stringizer", "Buzzer"),
				QT_TRANSLATE_NOOP("Stringizer", "Begin Battle"),
				QT_TRANSLATE_NOOP("Stringizer", "Escape"),
				QT_TRANSLATE_NOOP("Stringizer", "Enemy Attacks"),
				QT_TRANSLATE_NOOP("Stringizer", "Enemy Damage"),
				QT_TRANSLATE_NOOP("Stringizer", "Ally Damage"),
				QT_TRANSLATE_NOOP("Stringizer", "Evasion"),
				QT_TRANSLATE_NOOP("Stringizer", "Enemy Defeated"),
				QT_TRANSLATE_NOOP("Stringizer", "Use Item")
			};
		QString sfx;
		if (com.parameters[0] < 0 || com.parameters[0] >= static_cast<int>(system_sfx.size()))
			sfx = Stringizer::tr("Unknown");
		else
			sfx = Stringizer::tr(system_sfx[static_cast<size_t>(com.parameters[0])]);
		return Stringizer::tr("Change System SFX") + ": " + sfx + ", "
			+ ToQString(com.string);
	}

	QString stringizeChangeSystemGraphics(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Change System Graphics") + ": " + ToQString(com.string);
	}

	QString stringizeChangeScreenTransitions(const lcf::rpg::EventCommand& com)
	{
		const QStringList events = {Stringizer::tr("Teleport/Erase"), Stringizer::tr("Teleport/Show"),
			Stringizer::tr("Begin Battle/Erase"), Stringizer::tr("Begin Battle/Show"),
			Stringizer::tr("End Battle/Erase"), Stringizer::tr("End Battle/Show")};
		const int context = com.parameters[0];
		return Stringizer::tr("Change Screen Transitions") + ": "
			+ (context >= 0 && context < events.size() ? events[context] : Stringizer::tr("Unknown")) + ", "
			+ ((context & 1) ? Stringizer::showTransitionName(com.parameters[1])
				: Stringizer::eraseTransitionName(com.parameters[1]));
	}

	QString stringizeEnemyEncounter(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Enemy Encounter") + ": "
			+ (com.parameters[0] ? Stringizer::tr("Troop") + " " + variable(com.parameters[1])
								 : Stringizer::troopName(com.parameters[1]));
	}

	QString stringizeOpenShop(const lcf::rpg::EventCommand& com)
	{
		QString buysell;
		switch (com.parameters[0])
		{
			case 0:
				buysell = Stringizer::tr("Buy") + "/" + Stringizer::tr("Sell");
				break;
			case 1:
				buysell = Stringizer::tr("Buy");
				break;
			case 2:
				buysell = Stringizer::tr("Sell");
		}
		return Stringizer::tr("Open Shop") + ": " + buysell;
	}

	QString stringizeShowInn(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Show Inn") + ": " + Stringizer::tr("Price") + " " + QString::number(com.parameters[1]);
	}

	QString stringizeEnterHeroName(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Enter Hero Name") + ": " + Stringizer::heroName(com.parameters[0]);
	}

	QString stringizeTeleport(const lcf::rpg::EventCommand& com)
	{
		QString result = Stringizer::tr("Teleport") + ": " + map(com.parameters[0])
			+ " (" + QString::number(com.parameters[1]) + "," + QString::number(com.parameters[2]) + ")";
		if (com.parameters.size() > 3) {
			const int facing = com.parameters[3];
			result += ", " + (facing == 0 ? Stringizer::tr("Retain Facing") : Stringizer::direction(facing - 1));
		}
		return result;
	}

	QString stringizeMemorizeLocation(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Memorize Location") + ": "
			+ variable(com.parameters[0]) + ", "
			+ variable(com.parameters[1]) + ", "
			+ variable(com.parameters[2]);
	}

	QString stringizeRecallToLocation(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Recall To Location") + ": "
			+ variable(com.parameters[0]) + ", ("
			+ variable(com.parameters[1]) + ","
			+ variable(com.parameters[2]) + ")";
	}

	QString stringizeSetVehicleLocation(const lcf::rpg::EventCommand& com)
	{
		const QString vehicle = com.parameters[0] == -1 ? Stringizer::tr("Entire Party") :
			com.parameters[0] == 0 ? Stringizer::tr("Boat") :
			com.parameters[0] == 1 ? Stringizer::tr("Ship") :
			com.parameters[0] == 2 ? Stringizer::tr("Airship") : Stringizer::tr("Unknown");
		const bool variables = com.parameters[1] != 0;
		return Stringizer::tr("Set Vehicle Location") + ": " + vehicle + ", "
			+ (variables ? variable(com.parameters[2]) : map(com.parameters[2])) + " ("
			+ valueOrVariable(variables, com.parameters[3]) + ","
			+ valueOrVariable(variables, com.parameters[4]) + ")";
	}

	QString stringizeChangeEventLocation(const lcf::rpg::EventCommand& com)
	{
		QString result = Stringizer::tr("Change Event Location") + ": "
			+ characterName(com.parameters[0]) + ", ("
			+ valueOrVariable(com.parameters[1], com.parameters[2]) + ","
			+ valueOrVariable(com.parameters[1], com.parameters[3]) + ")";
		if (com.parameters[1] == 0 && com.parameters.size() > 4) {
			const int facing = com.parameters[4];
			result += ", " + (facing == 0 ? Stringizer::tr("Retain Facing") : Stringizer::direction(facing - 1));
		}
		return result;
	}

	QString stringizeTradeEventLocations(const lcf::rpg::EventCommand& com)
	{
		const int flags = com.parameters.size() > 2 ? com.parameters[2] : 0;
		return Stringizer::tr("Trade Event Locations") + ": "
			+ ((flags & 1) ? variable(com.parameters[0]) : characterName(com.parameters[0])) + ", "
			+ ((flags & 2) ? variable(com.parameters[1]) : characterName(com.parameters[1]));
	}

	QString stringizeStoreTerrainID(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Store Terrain ID") + ": "
			+ "(" + valueOrVariable(com.parameters[0], com.parameters[1]) + ","
			+ valueOrVariable(com.parameters[0], com.parameters[2]) + "), "
			+ variable(com.parameters[3]);
	}

	QString stringizeStoreEventID(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Store Event ID") + ": "
			+ "(" + valueOrVariable(com.parameters[0], com.parameters[1]) + ","
			+ valueOrVariable(com.parameters[0], com.parameters[2]) + "), "
			+ variable(com.parameters[3]);
	}

	QString stringizeEraseScreen(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Erase Screen") + ": " + Stringizer::eraseTransitionName(com.parameters[0]);
	}

	QString stringizeShowScreen(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Show Screen") + ": " + Stringizer::showTransitionName(com.parameters[0]);
	}

	QString stringizeTintScreen(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Tint Screen") + ": ("
			+ "R" + QString::number(com.parameters[0]) + ","
			+ "G" + QString::number(com.parameters[1]) + ","
			+ "B" + QString::number(com.parameters[2]) + ","
			+ "S" + QString::number(com.parameters[3]) + "), "
			+ tenthSeconds(com.parameters[4]) + " "
			+ (com.parameters[5] ? Stringizer::tr("(Wait)") : "");
	}

	QString stringizeFlashScreen(const lcf::rpg::EventCommand& com)
	{
		if (com.parameters.size() > 6)
		{
			if (com.parameters[6] == 1)
				return Stringizer::tr("Flash Screen") + ": " + Stringizer::tr("Start");
			else if (com.parameters[6] == 2)
				return Stringizer::tr("Flash Screen") + ": " + Stringizer::tr("Stop");
		}

		return Stringizer::tr("Flash Screen") + ": ("
			+ "R" + QString::number(com.parameters[0]) + ","
			+ "G" + QString::number(com.parameters[1]) + ","
			+ "B" + QString::number(com.parameters[2]) + ","
			+ "S" + QString::number(com.parameters[3]) + "), "
			+ tenthSeconds(com.parameters[4]) + " "
			+ (com.parameters[5] ? Stringizer::tr("(Wait)") : "");
	}

	QString stringizeShakeScreen(const lcf::rpg::EventCommand& com)
	{
		if (com.parameters.size() > 4)
		{
			if (com.parameters[4] == 1)
				return Stringizer::tr("Shake Screen") + ": " + Stringizer::tr("Start");
			else if (com.parameters[4] == 2)
				return Stringizer::tr("Shake Screen") + ": " + Stringizer::tr("Stop");
		}

		return Stringizer::tr("Shake Screen") + ": "
			+ Stringizer::tr("Strength") + QString::number(com.parameters[0]) + ", "
			+ Stringizer::tr("Speed") + QString::number(com.parameters[1]) + ", "
			+ tenthSeconds(com.parameters[2]) + " "
			+ (com.parameters[3] ? Stringizer::tr("(Wait)") : "");
	}

	QString stringizePanScreen(const lcf::rpg::EventCommand& com)
	{
		switch (com.parameters[0])
		{
		case 0:
			return Stringizer::tr("Pan Screen") + ": " + Stringizer::tr("Lock Position");
		case 1:
			return Stringizer::tr("Pan Screen") + ": " + Stringizer::tr("Unlock Position");
		case 2:
			return Stringizer::tr("Pan Screen") + ": "
				+ Stringizer::direction(com.parameters[1]) + " "
				+ QString::number(com.parameters[2]) + " " + Stringizer::tr("Tiles")
				+ ", " + Stringizer::tr("Speed") + " " + QString::number(com.parameters[3]) + " "
				+ (com.parameters[4] ? Stringizer::tr("(Wait)") : "");
		default:
			return Stringizer::tr("Pan Screen") + ": " + Stringizer::tr("Reset Position")
				+ ", " + Stringizer::tr("Speed") + " " + QString::number(com.parameters[3]) + " "
				+ (com.parameters[4] ? Stringizer::tr("(Wait)") : "");
		}
	}

	QString stringizeWeatherEffects(const lcf::rpg::EventCommand& com)
	{
		QString type, strength;
		switch (com.parameters[0])
		{
		case 0:
			type = Stringizer::tr("None"); break;
		case 1:
			type = Stringizer::tr("Rain"); break;
		case 2:
			type = Stringizer::tr("Snow"); break;
		case 3:
			type = Stringizer::tr("Fog"); break;
		case 4:
			type = Stringizer::tr("Sandstorm"); break;
		default:
			type = Stringizer::tr("Unknown"); break;
		}
		switch (com.parameters[1])
		{
		case 0:
			strength = Stringizer::tr("Weak"); break;
		case 1:
			strength = Stringizer::tr("Medium"); break;
		case 2:
			strength = Stringizer::tr("Strong"); break;
		default:
			strength = Stringizer::tr("Unknown"); break;
		}

		return Stringizer::tr("Weather Effects") + ": " + type + ", " + strength;
	}

	QString stringizeShowPicture(const lcf::rpg::EventCommand& com)
	{
		auto p = [&com](size_t index, int fallback = 0) {
			return index < com.parameters.size() ? com.parameters[index] : fallback;
		};
		QString result = Stringizer::tr("Show Picture") + ": " + QString::number(p(0));
		result += ", " + ToQString(com.string);
		result += ", (" + valueOrVariable((p(1) & 0xff) != 0, p(2)) + ","
			+ valueOrVariable((p(1) & 0xff) != 0, p(3)) + "), "
			+ Stringizer::tr("Magnification") + " " + QString::number(p(5, 100)) + "%, "
			+ Stringizer::tr("Transparency") + " " + QString::number(p(6)) + "/"
			+ QString::number(p(14, p(6)) & 0xff) + "%, "
			+ Stringizer::tr("Color") + QStringLiteral(" (R%1,G%2,B%3,S%4)").arg(p(8, 100)).arg(p(9, 100)).arg(p(10, 100)).arg(p(11, 100));
		const int effect = p(12);
		result += ", " + (effect == 0 ? Stringizer::tr("None") : effect == 1 ? Stringizer::tr("Rotation")
			: effect == 2 ? Stringizer::tr("Waver") : Stringizer::tr("Unknown"));
		if (effect) {
			result += " " + QString::number(p(13));
		}
		if (p(4) > 0) {
			result += ", " + Stringizer::tr("Picture Scrolls with Map");
		}
		result += ", " + (p(7) > 0 ? Stringizer::tr("Transparent Color") : Stringizer::tr("No Transparent Color"));
		return result;
	}

	QString stringizeMovePicture(const lcf::rpg::EventCommand& com)
	{
		auto p = [&com](size_t index, int fallback = 0) {
			return index < com.parameters.size() ? com.parameters[index] : fallback;
		};
		QString result = Stringizer::tr("Move Picture") + ": " + QString::number(p(0));
		result += ", (" + valueOrVariable((p(1) & 0xff) != 0, p(2)) + ","
			+ valueOrVariable((p(1) & 0xff) != 0, p(3)) + "), "
			+ Stringizer::tr("Magnification") + " " + QString::number(p(5, 100)) + "%, "
			+ Stringizer::tr("Transparency") + " " + QString::number(p(6)) + "/"
			+ QString::number(p(16, p(6)) & 0xff) + "%, "
			+ Stringizer::tr("Color") + QStringLiteral(" (R%1,G%2,B%3,S%4)").arg(p(8, 100)).arg(p(9, 100)).arg(p(10, 100)).arg(p(11, 100));
		const int effect = p(12);
		result += ", " + (effect == 0 ? Stringizer::tr("None") : effect == 1 ? Stringizer::tr("Rotation")
			: effect == 2 ? Stringizer::tr("Waver") : Stringizer::tr("Unknown"));
		if (effect) {
			result += " " + QString::number(p(13));
		}
		result += ", " + tenthSeconds(p(14));
		if (p(15) & 1) {
			result += " " + Stringizer::tr("(Wait)");
		}
		return result;
	}

	QString stringizeErasePicture(const lcf::rpg::EventCommand& com)
	{
		const int kind = com.parameters.size() > 1 ? com.parameters[1] : 0;
		QString result = Stringizer::tr("Erase Picture") + ": ";
		if (kind == 5) {
			return result + Stringizer::tr("All Pictures");
		}
		const bool variables = kind == 1 || kind == 3 || kind == 4;
		result += valueOrVariable(variables, com.parameters[0]);
		if ((kind == 2 || kind == 3) && com.parameters.size() > 2) {
			result += " ~ " + valueOrVariable(variables, com.parameters[2]);
		}
		if (kind == 4) {
			result += " " + Stringizer::tr("(Indirect)");
		}
		return result;
	}

	QString stringizeShowBattleAnimation(const lcf::rpg::EventCommand& com)
	{
		QString result = Stringizer::tr("Show Battle Animation") + ": "
			+ Stringizer::animationName(com.parameters[0]) + ", " + characterName(com.parameters[1]);
		if (com.parameters.size() > 2 && com.parameters[2] > 0) {
			result += " " + Stringizer::tr("(Wait)");
		}
		if (com.parameters.size() > 3 && com.parameters[3] > 0) {
			result += ", " + Stringizer::tr("Global Scope");
		}
		return result;
	}

	QString stringizePlayerVisibility(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Player Visibility") + ": "
			+ (com.parameters[0] ? Stringizer::tr("Opaque") : Stringizer::tr("Transparent"));
	}

	QString stringizeFlashSprite(const lcf::rpg::EventCommand& com)
	{
		QString result = Stringizer::tr("Flash Sprite") + ": " + characterName(com.parameters[0])
			+ QStringLiteral(", (R%1,G%2,B%3)").arg(com.parameters[1]).arg(com.parameters[2]).arg(com.parameters[3])
			+ ", " + Stringizer::tr("Strength") + " " + QString::number(com.parameters[4])
			+ ", " + tenthSeconds(com.parameters[5]);
		if (com.parameters.size() > 6 && com.parameters[6] > 0) {
			result += " " + Stringizer::tr("(Wait)");
		}
		return result;
	}

	QString stringizeMoveEvent(const lcf::rpg::EventCommand& com)
	{
		QString movements;
		for (size_t i = 4; i < 8 && i < com.parameters.size(); ++i)
			movements.append(", ").append(Stringizer::moveCommand(com.parameters[i]));
		if (com.parameters.size() >= 8)
			movements.append("...");
		return Stringizer::tr("Move Event") + ": " + characterName(com.parameters[0]) + movements;
	}

	QString stringizeWait(const lcf::rpg::EventCommand& com)
	{
		if (com.parameters.size() <= 1 ||
			(com.parameters.size() > 1 && com.parameters[1] == 0)) {
			return Stringizer::tr("Wait") + ": " + tenthSeconds(com.parameters[0]) + " ";
		} else {
			return Stringizer::tr("Wait") + ": " + Stringizer::tr("Until Key Pressed");
		}
	}

	QString stringizePlayBGM(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Play BGM") + ": " + ToQString(com.string);
	}

	QString stringizeFadeOutBGM(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Fade Out BGM") + ": " + QString::number(com.parameters[0] / 1000) + "s";
	}

	QString stringizePlaySound(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Play Sound") + ": " + ToQString(com.string);
	}

	QString stringizePlayMovie(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Play Movie") + ": " + ToQString(com.string);
	}

	QString stringizeKeyInputProc(const lcf::rpg::EventCommand& com)
	{
		const auto parameter = [&](size_t index) { return index < com.parameters.size() ? com.parameters[index] : 0; };
		const bool rpg2003 = core().project()->database().system.ldb_id == 2003;
		const bool modern2000 = com.parameters.size() == 10 || (!rpg2003 && com.parameters.size() >= 6);
		const bool modern2003 = rpg2003 && com.parameters.size() > 10;
		QStringList keys;
		const QStringList arrows{Stringizer::tr("Down"), Stringizer::tr("Left"), Stringizer::tr("Right"), Stringizer::tr("Up")};
		for (int i = 0; i < 4; ++i) {
			const int index = modern2003 ? i + 10 : modern2000 ? i + 6 : 2;
			if (parameter(index) != 0) {
				keys << arrows[i];
			}
		}
		if (parameter(3) != 0) { keys << Stringizer::tr("Decision"); }
		if (parameter(4) != 0) { keys << Stringizer::tr("Cancel"); }
		if ((modern2003 && parameter(9) != 0) || (modern2000 && parameter(5) != 0)) { keys << Stringizer::tr("Shift"); }
		if (rpg2003 && !modern2000 && parameter(5) != 0) { keys << Stringizer::tr("Number keys"); }
		if (rpg2003 && !modern2000 && parameter(6) != 0) { keys << Stringizer::tr("Operator keys"); }
		QString result = Stringizer::tr("Key Input Proc") + ": " + variable(parameter(0))
			+ ", " + (parameter(1) ? Stringizer::tr("Wait for key") : Stringizer::tr("Read current key"))
			+ ", " + Stringizer::tr("Keys: %1").arg(keys.join(", "));
		if (rpg2003 && !modern2000 && parameter(8) != 0) {
			result += ", " + Stringizer::tr("Elapsed time in %1").arg(variable(parameter(7)));
		}
		return result;
	}

	QString stringizeChangeMapTileset(const lcf::rpg::EventCommand& com)
	{
		if (com.parameters.size() > 1 && (com.parameters[1] & 1)) {
			return Stringizer::tr("Change Map Tileset") + ": " + variable(com.parameters[0]);
		}
		const int id = com.parameters[0];
		const auto& chipsets = core().project()->database().chipsets;
		const auto found = std::find_if(chipsets.begin(), chipsets.end(), [id](const auto& chipset) { return chipset.ID == id; });
		return Stringizer::tr("Change Map Tileset") + ": " + QString::number(id)
			+ (found == chipsets.end() ? QString() : ": " + ToQString(found->name));
	}

	QString stringizeChangePBG(const lcf::rpg::EventCommand& com)
	{
		const int flags = com.parameters.size() > 6 ? com.parameters[6] : 0;
		QString result = Stringizer::tr("Change PBG") + ": "
			+ ((flags & 1) && com.parameters.size() > 7 ? variable(com.parameters[7]) : ToQString(com.string));
		if (com.parameters[0]) {
			result += ", " + Stringizer::tr("Horz Scroll");
			if (com.parameters[2]) {
				result += ", " + Stringizer::tr("Horizontal Autoscroll") + " " + valueOrVariable(flags & (1 << 4), com.parameters[3]);
			}
		}
		if (com.parameters[1]) {
			result += ", " + Stringizer::tr("Vert Scroll");
			if (com.parameters[4]) {
				result += ", " + Stringizer::tr("Vertical Autoscroll") + " " + valueOrVariable(flags & (1 << 6), com.parameters[5]);
			}
		}
		return result;
	}

	QString stringizeChangeEncounterSteps(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Change Encounter Steps") + ": " + QString::number(com.parameters[0]);
	}

	QString stringizeTileSubstitution(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Tile Substitution") + ": "
			+ (com.parameters[0] ? Stringizer::tr("Upper Layer") : Stringizer::tr("Lower Layer")) + ", "
			+ QString::number(com.parameters[1]) + " -> "
			+ QString::number(com.parameters[2]);
	}

	QString stringizeTeleportTargets(const lcf::rpg::EventCommand& com)
	{
		if (com.parameters[0]) {
			return Stringizer::tr("Teleport Targets") + ": " + Stringizer::tr("Remove") + " " + map(com.parameters[1]);
		}
		QString result = Stringizer::tr("Teleport Targets") + ": " + Stringizer::tr("Add") + " " + map(com.parameters[1])
			+ " (" + QString::number(com.parameters[2]) + "," + QString::number(com.parameters[3]) + ")";
		if (com.parameters[4]) {
			result += ", " + Stringizer::tr("Switch %1 ON").arg(com.parameters[5]);
		}
		return result;
	}

	QString stringizeChangeTeleportAccess(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Change Teleport Access") + ": "
			+ (com.parameters[0] ? Stringizer::tr("Allow") : Stringizer::tr("Forbid"));
	}

	QString stringizeEscapeTarget(const lcf::rpg::EventCommand& com)
	{
		QString result = Stringizer::tr("Escape Target") + ": " + map(com.parameters[0])
			+ " (" + QString::number(com.parameters[1]) + "," + QString::number(com.parameters[2]) + ")";
		if (com.parameters[3]) {
			result += ", " + Stringizer::tr("Switch %1 ON").arg(com.parameters[4]);
		}
		return result;
	}

	QString stringizeChangeEscapeAccess(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Change Escape Access") + ": "
			+ (com.parameters[0] ? Stringizer::tr("Allow") : Stringizer::tr("Forbid"));
	}

	QString stringizeChangeSaveAccess(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Change Save Access") + ": "
			+ (com.parameters[0] ? Stringizer::tr("Allow") : Stringizer::tr("Forbid"));
	}

	QString stringizeChangeMainMenuAccess(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Change Main Menu Access") + ": "
			+ (com.parameters[0] ? Stringizer::tr("Allow") : Stringizer::tr("Forbid"));
	}

	QString stringizeConditionalBranch(const lcf::rpg::EventCommand& com)
	{
		QString condition;
		switch(com.parameters[0]) {
		case 0: // Switch
			condition = QString("S[%1:%2]").arg(com.parameters[1], 4, 10, QChar('0'))
										   .arg(Stringizer::switchName(com.parameters[1])) + " "
				+ (com.parameters[2] == 0 ? Stringizer::tr("ON") : Stringizer::tr("OFF"));
			break;
		case 1: // Variable
			condition = variable(com.parameters[1], true) + " ";
			switch (com.parameters[4]) {
			case 0:
				condition += "=="; break;
			case 1:
				condition += ">="; break;
			case 2:
				condition += "<="; break;
			case 3:
				condition += ">"; break;
			case 4:
				condition += "<"; break;
			case 5:
				condition += "!="; break;
			default:
				condition += "?";
			}
			condition += " ";
			condition += valueOrVariable(com.parameters[2], com.parameters[3]);
			break;
		case 2: // Timer 1
			condition = Stringizer::tr("Timer") + "1 "
				+ (com.parameters[2] ? "<=" : ">=") + " "
				+ formatTime(com.parameters[1]);
			break;
		case 3: // Gold
			condition = Stringizer::tr("Money") + " "
				+ (com.parameters[2] ? "<=" : ">=") + " "
				+ QString::number(com.parameters[1]);
			break;
		case 4: // Item
			condition = Stringizer::itemName(com.parameters[1]) + " "
				+ (com.parameters[2] ? Stringizer::tr("Not Possessed") : Stringizer::tr("Possessed"));
			break;
		case 5: // Hero
			condition = Stringizer::heroName(com.parameters[1]) + ", ";
			switch (com.parameters[2])
			{
				case 0: // Is actor in party
					condition += Stringizer::tr("In the Party");
					break;
				case 1: // Name
					condition += Stringizer::tr("Name is %1").arg(ToQString(com.string));
					break;
				case 2: // Higher or equal level
					condition += Stringizer::tr("Level at least %1").arg(QString::number(com.parameters[3]));
					break;
				case 3: // Higher or equal HP
					condition += Stringizer::tr("HP over %1").arg(QString::number(com.parameters[3]));
					break;
				case 4: // Is skill learned
					condition += Stringizer::tr("Can use %1").arg(Stringizer::skillName(com.parameters[3]));
					break;
				case 5: // Equipped object
					condition += Stringizer::tr("%1 is Equipped").arg(Stringizer::itemName(com.parameters[3]));
					break;
				case 6: // Has state
					condition += Stringizer::tr("with Condition %1").arg(Stringizer::conditionName(com.parameters[3]));
					break;
				default:
					condition += Stringizer::tr("Unknown");
			}
			break;
		case 6: // Orientation of char
			condition = characterName(com.parameters[1]) + " "
				+ Stringizer::tr("Facing %1").arg(Stringizer::direction(com.parameters[2]));
			break;
		case 7: // Vehicle in use
			condition = Stringizer::tr("Vehicle") + " "
				+ (com.parameters[1] == 0 ? Stringizer::tr("Boat") :
					 com.parameters[1] == 1 ? Stringizer::tr("Ship") :
					 com.parameters[1] == 2 ? Stringizer::tr("Airship") : Stringizer::tr("Unknown"));
			break;
		case 8: // Key decision initiated this event
			condition = Stringizer::tr("Event Started with Decision Key");
			break;
		case 9: // BGM looped at least once
			condition = Stringizer::tr("BGM has Played Through Once");
			break;
		case 10: // Timer 2
			condition = Stringizer::tr("Timer") + "2 "
				+ (com.parameters[2] ? "<=" : ">=") + " "
				+ formatTime(com.parameters[1]);
			break;
		case 11:
			// RPG Maker 2003 v1.11 features
			// TODO
		default:
			return Stringizer::tr("Unknown");
		}
		return Stringizer::tr("Branch if") + " " + condition;
	}

	QString stringizeLabel(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Label") + ": " + QString::number(com.parameters[0]);
	}

	QString stringizeJumpToLabel(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Jump To Label") + ": " + QString::number(com.parameters[0]);
	}

	QString stringizeCallEvent(const lcf::rpg::EventCommand& com)
	{
		QString event;
		switch (com.parameters[0])
		{
		case 0:
			event = Stringizer::commonEventName(com.parameters[1]);
			break;
		case 1:
			event = characterName(com.parameters[1]) + "["
				+ QString::number(com.parameters[2]) + "]";
			break;
		case 2:
			event = "V[" + QString::number(com.parameters[1]) + "]"
				+ "[V[" + QString::number(com.parameters[2]) + "]]";
		}

		return Stringizer::tr("Call Event") + ": " + event;
	}

	QString stringizeComment(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Comment") + ": " + ToQString(com.string);
	}

	QString stringizeChangeMonsterHP(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Change Monster HP") + ": " + Stringizer::tr("Monster %1").arg(com.parameters[0] + 1)
			+ ", " + monsterAmount(com)
			+ (com.parameters.size() > 4 && com.parameters[4] ? " " + Stringizer::tr("(Can Kill)") : QString());
	}

	QString stringizeChangeMonsterMP(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Change Monster MP") + ": " + Stringizer::tr("Monster %1").arg(com.parameters[0] + 1)
			+ ", " + monsterAmount(com);
	}

	QString stringizeChangeMonsterCondition(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Change Monster Condition") + ": " + Stringizer::tr("Monster %1").arg(com.parameters[0] + 1)
			+ ", " + (com.parameters[1] ? Stringizer::tr("Remove") : Stringizer::tr("Add")) + " " + Stringizer::stateName(com.parameters[2]);
	}

	QString stringizeShowHiddenMonster(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Show Hidden Monster") + ": " + Stringizer::tr("Monster %1").arg(com.parameters[0] + 1);
	}

	QString stringizeChangeBattleBG(const lcf::rpg::EventCommand& com)
	{
		return Stringizer::tr("Change Battle BG") + ": " + ToQString(com.string);
	}

	QString stringizeShowBattleAnimation_B(const lcf::rpg::EventCommand& com)
	{
		// p1: -1 all, else a monster (0-7) or a party member (1-4) when p3 is set (2003)
		const bool party = com.parameters.size() > 3 && com.parameters[3] != 0;
		const int target = com.parameters[1];
		const QString name = target < 0 ? (party ? Stringizer::tr("Entire Party") : Stringizer::tr("All Enemies"))
			: party ? Stringizer::tr("Party Member %1").arg(target) : Stringizer::tr("Monster %1").arg(target + 1);
		return Stringizer::tr("Show Battle Animation") + ": " + Stringizer::animationName(com.parameters[0]) + ", " + name
			+ (com.parameters[2] ? " " + Stringizer::tr("(Wait)") : QString());
	}

	QString stringizeConditionalBranch_B(const lcf::rpg::EventCommand& com)
	{
		QString condition;
		switch (com.parameters[0]) {
		case 0:
		case 1:
			return stringizeConditionalBranch(com);
		case 2:
			condition = Stringizer::tr("%1 can act").arg(Stringizer::heroName(com.parameters[1]));
			break;
		case 3:
			condition = Stringizer::tr("Monster %1 can act").arg(com.parameters[1] + 1);
			break;
		case 4:
			condition = Stringizer::tr("Monster %1 is the current target").arg(com.parameters[1] + 1);
			break;
		case 5:
			condition = Stringizer::tr("%1 uses %2").arg(Stringizer::heroName(com.parameters[1]), Stringizer::battleCommandName(com.parameters[2]));
			break;
		default:
			condition = Stringizer::tr("Unknown");
			break;
		}
		return Stringizer::tr("Branch if") + " " + condition;
	}

	QString stringizeShowMessage_2(const lcf::rpg::EventCommand& com)
	{
		return "	" + ToQString(com.string);
	}

	QString stringizeShowChoiceOption(const lcf::rpg::EventCommand& com)
	{
		// branch 4 is the cancel branch
		return com.parameters.size() > 0 && com.parameters[0] == 4 ? Stringizer::tr("Cancel") : "[" + ToQString(com.string) + "]";
	}

	QString stringizeComment_2(const lcf::rpg::EventCommand& com)
	{
		return "	" + ToQString(com.string);
	}
}

namespace Stringizer
{
	QString varName(int id)
	{
		if (id < 1 || id > static_cast<int>(core().project()->database().variables.size()))
			return QString("<%1?>").arg(id);
		return ToQString(core().project()->database().variables[static_cast<size_t>(id)-1].name);
	}

	QString switchName(int id)
	{
		if (id < 1 || id > static_cast<int>(core().project()->database().switches.size()))
			return QString("<%1?>").arg(id);
		return ToQString(core().project()->database().switches[static_cast<size_t>(id) - 1].name);
	}

	QString itemName(int id)
	{
		if (id < 1 || id > static_cast<int>(core().project()->database().items.size()))
			return QString("<%1?>").arg(id);
		return ToQString(core().project()->database().items[static_cast<size_t>(id)-1].name);
	}

	QString heroName(int id)
	{
		if (id < 1 || id > static_cast<int>(core().project()->database().actors.size()))
			return QString("<%1?>").arg(id);
		return ToQString(core().project()->database().actors[static_cast<size_t>(id)-1].name);
	}

	QString className(int id)
	{
		if (id < 1 || id > static_cast<int>(core().project()->database().classes.size()))
			return QString("<%1?>").arg(id);
		return ToQString(core().project()->database().classes[static_cast<size_t>(id)-1].name);
	}

	QString stateName(int id)
	{
		if (id < 1 || id > static_cast<int>(core().project()->database().states.size()))
			return QString("<%1?>").arg(id);
		return ToQString(core().project()->database().states[static_cast<size_t>(id)-1].name);
	}

	QString skillName(int id)
	{
		if (id < 1 || id > static_cast<int>(core().project()->database().skills.size()))
			return QString("<%1?>").arg(id);
		return ToQString(core().project()->database().skills[static_cast<size_t>(id)-1].name);
	}

	QString battleCommandName(int id)
	{
		if (id < 1 || id > static_cast<int>(core().project()->database().battlecommands.commands.size()))
			return QString("<%1?>").arg(id);
		return ToQString(core().project()->database().battlecommands.commands[static_cast<size_t>(id)-1].name);
	}


	QString animationName(int id)
	{
		if (id < 1 || id > static_cast<int>(core().project()->database().animations.size()))
			return QString("<%1?>").arg(id);
		return ToQString(core().project()->database().animations[static_cast<size_t>(id)-1].name);
	}
	QString conditionName(int id)
	{
		if (id < 1 || id > static_cast<int>(core().project()->database().states.size()))
			return QString("<%1?>").arg(id);
		return ToQString(core().project()->database().states[static_cast<size_t>(id)-1].name);
	}

	QString eventName(int id)
	{
		if (!core().currentMapEvent(id))
			return QString("<%1?>").arg(id);
		return ToQString(core().currentMapEvent(id)->name);
	}

	QString commonEventName(int id)
	{
		if (id < 1 || id > static_cast<int>(core().project()->database().commonevents.size()))
			return QString("<%1?>").arg(id);
		return ToQString(core().project()->database().commonevents[static_cast<size_t>(id)-1].name);
	}

	QString troopName(int id)
	{
		if (id < 1 || id > static_cast<int>(core().project()->database().troops.size()))
			return QString("<%1?>").arg(id);
		return ToQString(core().project()->database().troops[static_cast<size_t>(id)-1].name);
	}

	QString terrainName(int id)
	{
		if (id < 1 || id > static_cast<int>(core().project()->database().terrains.size()))
			return QString("<%1?>").arg(id);
		return ToQString(core().project()->database().terrains[static_cast<size_t>(id)-1].name);
	}

	QString mapName(int id)
	{
		if (id < 1 || id > static_cast<int>(core().project()->treeMap().maps.size()))
			return QString("<%1?>").arg(id);
		for (unsigned i = 0; i < core().project()->treeMap().maps.size(); i++)
			if (core().project()->treeMap().maps[i].ID == id)
				return ToQString(core().project()->treeMap().maps[i].name);
		return QString("<NotFound:%1?>").arg(id);
	}

	QString direction(int id)
	{
		switch (id)
		{
		case 0: return tr("Up");
		case 1: return tr("Right");
		case 2: return tr("Down");
		case 3: return tr("Left");
		case 4: return tr("Up/Right");
		case 5: return tr("Down/Right");
		case 6: return tr("Down/Left");
		case 7: return tr("Up/Left");
		default: return tr("Unknown");
		}
	}

	QString moveCommand(int id)
	{
		using C = lcf::rpg::MoveCommand::Code;
		switch (static_cast<C>(id))
		{
		case C::move_up:					 return tr("Move Up");
		case C::move_right:					 return tr("Move Right");
		case C::move_down:					 return tr("Move Down");
		case C::move_left:					 return tr("Move Left");
		case C::move_upright:				 return tr("Move Up/Right");
		case C::move_downright:				 return tr("Move Down/Right");
		case C::move_downleft:				 return tr("Move Down/Left");
		case C::move_upleft:				 return tr("Move Up/Left");
		case C::move_random:				 return tr("Move Randomly");
		case C::move_towards_hero:			 return tr("Move Towards Hero");
		case C::move_away_from_hero:		 return tr("Move Away from Hero");
		case C::move_forward:				 return tr("Move Forward");
		case C::face_up:					 return tr("Face Up");
		case C::face_right:					 return tr("Face Right");
		case C::face_down:					 return tr("Face Down");
		case C::face_left:					 return tr("Face Left");
		case C::turn_90_degree_right:		 return tr("Turn 90° Right");
		case C::turn_90_degree_left:		 return tr("Turn 90° Left");
		case C::turn_180_degree:			 return tr("Turn 180°");
		case C::turn_90_degree_random:		 return tr("Turn 90° Randomly");
		case C::face_random_direction:		 return tr("Face Random Direction");
		case C::face_hero:					 return tr("Face Hero");
		case C::face_away_from_hero:		 return tr("Turn Away from Hero");
		case C::wait:						 return tr("Wait");
		case C::begin_jump:					 return tr("Begin Jump");
		case C::end_jump:					 return tr("End Jump");
		case C::lock_facing:				 return tr("Lock Facing");
		case C::unlock_facing:				 return tr("Unlock Facing");
		case C::increase_movement_speed:	 return tr("Increase Movement Speed");
		case C::decrease_movement_speed:	 return tr("Decrease Movement Speed");
		case C::increase_movement_frequence: return tr("Increase Movement Frequency");
		case C::decrease_movement_frequence: return tr("Decrease Movement Frequency");
		case C::switch_on:					 return tr("Switch ON");
		case C::switch_off:					 return tr("Switch OFF");
		case C::change_graphic:				 return tr("Change Graphic");
		case C::play_sound_effect:			 return tr("Play Sound Effect");
		case C::walk_everywhere_on:			 return tr("Walk Everywhere ON");
		case C::walk_everywhere_off:		 return tr("Walk Everywhere OFF");
		case C::stop_animation:				 return tr("Stop Animation");
		case C::start_animation:			 return tr("Start Animation");
		case C::increase_transp:			 return tr("Increase Transparency");
		case C::decrease_transp:			 return tr("Decrease Transparency");
		default:							 return tr("Unknown");
		}
	}

	QString eraseTransitionName(int id)
	{
		static const std::vector<const char*> names = {
			QT_TR_NOOP("Fade Out"),
			QT_TR_NOOP("Random Blocks"),
			QT_TR_NOOP("Random Blocks Down"),
			QT_TR_NOOP("Random Blocks Up"),
			QT_TR_NOOP("Blind Close"),
			QT_TR_NOOP("Vertical Stripes Out"),
			QT_TR_NOOP("Horizontal Stripes Out"),
			QT_TR_NOOP("Border to Center Out"),
			QT_TR_NOOP("Center to Border Out"),
			QT_TR_NOOP("Scroll Up Out"),
			QT_TR_NOOP("Scroll Down Out"),
			QT_TR_NOOP("Scroll Left Out"),
			QT_TR_NOOP("Scroll Right Out"),
			QT_TR_NOOP("Vertical Division"),
			QT_TR_NOOP("Horizontal Division"),
			QT_TR_NOOP("Cross Division"),
			QT_TR_NOOP("Zoom In"),
			QT_TR_NOOP("Mosaic Out"),
			QT_TR_NOOP("Wave Out"),
			QT_TR_NOOP("Instant"),
			QT_TR_NOOP("No Transition"),
		};
		if (id < 0) {
			return tr("Use Default");
		}
		return id < static_cast<int>(names.size()) ? tr(names[id]) : tr("Unknown");
	}

	QString showTransitionName(int id)
	{
		static const std::vector<const char*> names = {
			QT_TR_NOOP("Fade In"),
			QT_TR_NOOP("Random Blocks"),
			QT_TR_NOOP("Random Blocks Down"),
			QT_TR_NOOP("Random Blocks Up"),
			QT_TR_NOOP("Blind Open"),
			QT_TR_NOOP("Vertical Stripes In"),
			QT_TR_NOOP("Horizontal Stripes In"),
			QT_TR_NOOP("Border to Center In"),
			QT_TR_NOOP("Center to Border In"),
			QT_TR_NOOP("Scroll Up In"),
			QT_TR_NOOP("Scroll Down In"),
			QT_TR_NOOP("Scroll Left In"),
			QT_TR_NOOP("Scroll Right In"),
			QT_TR_NOOP("Vertical Combine"),
			QT_TR_NOOP("Horizontal Combine"),
			QT_TR_NOOP("Cross Combine"),
			QT_TR_NOOP("Zoom Out"),
			QT_TR_NOOP("Mosaic In"),
			QT_TR_NOOP("Wave In"),
			QT_TR_NOOP("Instant"),
			QT_TR_NOOP("No Transition"),
		};
		if (id < 0) {
			return tr("Use Default");
		}
		return id < static_cast<int>(names.size()) ? tr(names[id]) : tr("Unknown");
	}

	QString stringize(const lcf::rpg::EventCommand& com)
	{
		using C = lcf::rpg::EventCommand::Code;
		// Commands can be shorter than usual (older editors, other tools):
		// the missing parameters read as 0, as in EasyRPG Player
		const size_t usual = EventCommandDefaults::parameters(static_cast<C>(com.code)).size();
		if (com.parameters.size() < usual) {
			lcf::rpg::EventCommand padded = com;
			std::vector<int32_t> parameters(com.parameters.begin(), com.parameters.end());
			parameters.resize(usual);
			padded.parameters = lcf::DBArray<int32_t>(parameters.begin(), parameters.end());
			return stringize(padded);
		}
		switch (static_cast<C>(com.code))
		{
		case C::END:					 return "";
		case C::CallCommonEvent:		 return stringizeCallCommonEvent(com);
		case C::ForceFlee:				 return stringizeForceFlee(com);
		case C::EnableCombo:			 return stringizeEnableCombo(com);
		case C::ChangeClass:			 return stringizeChangeClass(com);
		case C::ChangeBattleCommands:	 return stringizeChangeBattleCommands(com);
		case C::ShowMessage:			 return stringizeShowMessage(com);
		case C::MessageOptions:			 return stringizeMessageOptions(com);
		case C::ChangeFaceGraphic:		 return stringizeChangeFaceGraphic(com);
		case C::ShowChoice:				 return stringizeShowChoice(com);
		case C::InputNumber:			 return stringizeInputNumber(com);
		case C::ControlSwitches:		 return stringizeControlSwitches(com);
		case C::ControlVars:			 return stringizeControlVars(com);
		case C::TimerOperation:			 return stringizeTimerOperation(com);
		case C::ChangeGold:				 return stringizeChangeGold(com);
		case C::ChangeItems:		return stringizeChangeItems(com);
		case C::ChangePartyMembers:		 return stringizeChangePartyMembers(com);
		case C::ChangeExp:				 return stringizeChangeExp(com);
		case C::ChangeLevel:			 return stringizeChangeLevel(com);
		case C::ChangeParameters:		 return stringizeChangeParameters(com);
		case C::ChangeSkills:			 return stringizeChangeSkills(com);
		case C::ChangeEquipment:		 return stringizeChangeEquipment(com);
		case C::ChangeHP:				 return stringizeChangeHP(com);
		case C::ChangeSP:				 return stringizeChangeSP(com);
		case C::ChangeCondition:		 return stringizeChangeCondition(com);
		case C::FullHeal:				 return stringizeFullHeal(com);
		case C::SimulatedAttack:		 return stringizeSimulatedAttack(com);
		case C::ChangeHeroName:			 return stringizeChangeHeroName(com);
		case C::ChangeHeroTitle:		 return stringizeChangeHeroTitle(com);
		case C::ChangeSpriteAssociation: return stringizeChangeSpriteAssociation(com);
		case C::ChangeActorFace:		 return stringizeChangeActorFace(com);
		case C::ChangeVehicleGraphic:	 return stringizeChangeVehicleGraphic(com);
		case C::ChangeSystemBGM:		 return stringizeChangeSystemBGM(com);
		case C::ChangeSystemSFX:		 return stringizeChangeSystemSFX(com);
		case C::ChangeSystemGraphics:	 return stringizeChangeSystemGraphics(com);
		case C::ChangeScreenTransitions: return stringizeChangeScreenTransitions(com);
		case C::EnemyEncounter:			 return stringizeEnemyEncounter(com);
		case C::OpenShop:				 return stringizeOpenShop(com);
		case C::ShowInn:				 return stringizeShowInn(com);
		case C::EnterHeroName:			 return stringizeEnterHeroName(com);
		case C::Teleport:				 return stringizeTeleport(com);
		case C::MemorizeLocation:		 return stringizeMemorizeLocation(com);
		case C::RecallToLocation:		 return stringizeRecallToLocation(com);
		case C::EnterExitVehicle:		 return tr("Enter/Exit Vehicle");
		case C::SetVehicleLocation:		 return stringizeSetVehicleLocation(com);
		case C::ChangeEventLocation:	 return stringizeChangeEventLocation(com);
		case C::TradeEventLocations:	 return stringizeTradeEventLocations(com);
		case C::StoreTerrainID:			 return stringizeStoreTerrainID(com);
		case C::StoreEventID:			 return stringizeStoreEventID(com);
		case C::EraseScreen:			 return stringizeEraseScreen(com);
		case C::ShowScreen:				 return stringizeShowScreen(com);
		case C::TintScreen:				 return stringizeTintScreen(com);
		case C::FlashScreen:			 return stringizeFlashScreen(com);
		case C::ShakeScreen:			 return stringizeShakeScreen(com);
		case C::PanScreen:				 return stringizePanScreen(com);
		case C::WeatherEffects:			 return stringizeWeatherEffects(com);
		case C::ShowPicture:			 return stringizeShowPicture(com);
		case C::MovePicture:			 return stringizeMovePicture(com);
		case C::ErasePicture:			 return stringizeErasePicture(com);
		case C::ShowBattleAnimation:	 return stringizeShowBattleAnimation(com);
		case C::PlayerVisibility:		 return stringizePlayerVisibility(com);
		case C::FlashSprite:			 return stringizeFlashSprite(com);
		case C::MoveEvent:				 return stringizeMoveEvent(com);
		case C::ProceedWithMovement:	 return tr("Proceed With Movement");
		case C::HaltAllMovement:		 return tr("Halt All Movement");
		case C::Wait:					 return stringizeWait(com);
		case C::PlayBGM:				 return stringizePlayBGM(com);
		case C::FadeOutBGM:				 return stringizeFadeOutBGM(com);
		case C::MemorizeBGM:			 return tr("Memorize BGM");
		case C::PlayMemorizedBGM:		 return tr("Play Memorized BGM");
		case C::PlaySound:				 return stringizePlaySound(com);
		case C::PlayMovie:				 return stringizePlayMovie(com);
		case C::KeyInputProc:			 return stringizeKeyInputProc(com);
		case C::ChangeMapTileset:		 return stringizeChangeMapTileset(com);
		case C::ChangePBG:				 return stringizeChangePBG(com);
		case C::ChangeEncounterSteps:	 return stringizeChangeEncounterSteps(com);
		case C::TileSubstitution:		 return stringizeTileSubstitution(com);
		case C::TeleportTargets:		 return stringizeTeleportTargets(com);
		case C::ChangeTeleportAccess:	 return stringizeChangeTeleportAccess(com);
		case C::EscapeTarget:			 return stringizeEscapeTarget(com);
		case C::ChangeEscapeAccess:		 return stringizeChangeEscapeAccess(com);
		case C::OpenSaveMenu:			 return tr("Open Save Menu");
		case C::ChangeSaveAccess:		 return stringizeChangeSaveAccess(com);
		case C::OpenMainMenu:			 return tr("Open Main Menu");
		case C::ChangeMainMenuAccess:	 return stringizeChangeMainMenuAccess(com);
		case C::ConditionalBranch:		 return stringizeConditionalBranch(com);
		case C::Label:					 return stringizeLabel(com);
		case C::JumpToLabel:			 return stringizeJumpToLabel(com);
		case C::Loop:					 return tr("Loop");
		case C::BreakLoop:				 return tr("Break Loop");
		case C::EndEventProcessing:		 return tr("End Event Processing");
		case C::EraseEvent:				 return tr("Erase Event");
		case C::CallEvent:				 return stringizeCallEvent(com);
		case C::Comment:				 return stringizeComment(com);
		case C::GameOver:				 return tr("Game Over");
		case C::ReturntoTitleScreen:	 return tr("Return to Title Screen");
		case C::ChangeMonsterHP:		 return stringizeChangeMonsterHP(com);
		case C::ChangeMonsterMP:		 return stringizeChangeMonsterMP(com);
		case C::ChangeMonsterCondition:	 return stringizeChangeMonsterCondition(com);
		case C::ShowHiddenMonster:		 return stringizeShowHiddenMonster(com);
		case C::ChangeBattleBG:			 return stringizeChangeBattleBG(com);
		case C::ShowBattleAnimation_B:	 return stringizeShowBattleAnimation_B(com);
		case C::ConditionalBranch_B:	 return stringizeConditionalBranch_B(com);
		case C::TerminateBattle:		 return tr("Terminate Battle");
		case C::ShowMessage_2:			 return stringizeShowMessage_2(com);
		case C::ShowChoiceOption:		 return stringizeShowChoiceOption(com);
		case C::ShowChoiceEnd:			 return tr("End");
		case C::VictoryHandler:			 return tr("Victory");
		case C::EscapeHandler:			 return tr("Escape");
		case C::DefeatHandler:			 return tr("Defeat");
		case C::EndBattle:				 return tr("End");
		case C::Transaction:			 return tr("Transaction");
		case C::NoTransaction:			 return tr("No Transaction");
		case C::EndShop:				 return tr("End");
		case C::Stay:					 return tr("Stay");
		case C::NoStay:					 return tr("No Stay");
		case C::EndInn:					 return tr("End");
		case C::ElseBranch:				 return tr("Else");
		case C::EndBranch:				 return tr("End");
		case C::EndLoop:				 return tr("End Loop");
		case C::Comment_2:				 return stringizeComment_2(com);
		case C::ElseBranch_B:			 return tr("Else");
		case C::EndBranch_B:			 return tr("End");
		default:						 return tr(QString("Unknown Command %1").arg(com.code).toUtf8());
		}
	}
}
