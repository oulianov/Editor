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

#include "event_command_defaults.h"
#include <map>

using lcf::rpg::EventCommand;

namespace {
// Parameter count and most common values of each command in about 190 maps
// and databases of RPG Maker 2000/2003 games (EasyRPG test games, community
// games), with neutral values where the most common one is game specific.
const std::map<EventCommand::Code, std::vector<int32_t>>& table() {
	using Code = EventCommand::Code;
	static const std::map<Code, std::vector<int32_t>> defaults = {
		{Code::END, {}},
		{Code::ChangeClass, {1, 1, 1, 0, 0, 0, 0}},
		{Code::ChangeBattleCommands, {1, 1, 2, 1}},
		{Code::OpenLoadMenu, {}},
		{Code::ExitGame, {}},
		{Code::ToggleAtbMode, {}},
		{Code::ToggleFullscreen, {}},
		{Code::OpenVideoOptions, {}},
		{Code::ShowMessage, {}},
		{Code::MessageOptions, {0, 2, 1, 0}},
		{Code::ChangeFaceGraphic, {0, 0, 0}},
		{Code::ShowChoice, {0}},
		{Code::InputNumber, {9, 1}},
		{Code::ControlSwitches, {0, 10, 10, 0}},
		{Code::ControlVars, {0, 1, 1, 0, 0, 1, 0}},
		{Code::TimerOperation, {0, 0, 60, 1, 0, 0}},
		{Code::ChangeGold, {0, 0, 162}},
		{Code::ChangeItems, {0, 0, 7, 0, 1}},
		{Code::ChangePartyMembers, {0, 0, 4}},
		{Code::ChangeExp, {1, 0, 0, 0, 80, 1}},
		{Code::ChangeLevel, {1, 1, 0, 0, 1, 1}},
		{Code::ChangeParameters, {1, 1, 0, 0, 1, 1}},
		{Code::ChangeSkills, {1, 1, 0, 0, 2}},
		{Code::ChangeEquipment, {1, 0, 0, 0, 0}},
		{Code::ChangeHP, {1, 1, 1, 0, 1, 1}},
		{Code::ChangeSP, {1, 1, 1, 0, 90}},
		{Code::ChangeCondition, {1, 1, 0, 2}},
		{Code::FullHeal, {0, 0}},
		{Code::SimulatedAttack, {1, 1, 100, 0, 0, 0, 1, 1}},
		{Code::ChangeHeroName, {13}},
		{Code::ChangeHeroTitle, {1}},
		{Code::ChangeSpriteAssociation, {1, 0, 1}},
		{Code::ChangeActorFace, {1, 15}},
		{Code::ChangeVehicleGraphic, {0, 0}},
		{Code::ChangeSystemBGM, {0, 0, 100, 100, 50}},
		{Code::ChangeSystemSFX, {0, 100, 100, 50}},
		{Code::ChangeSystemGraphics, {0, 0}},
		{Code::ChangeScreenTransitions, {0, -1}},
		{Code::EnemyEncounter, {0, 1, 0, 0, 0, 0, 0, 1, 0}},
		{Code::OpenShop, {0, 0, 0, 0}},
		{Code::ShowInn, {0, 100, 0}},
		{Code::EnterHeroName, {1, 0, 0}},
		{Code::Teleport, {1, 0, 0, 0}},
		{Code::MemorizeLocation, {1, 2, 3}},
		{Code::RecallToLocation, {1, 2, 3}},
		{Code::EnterExitVehicle, {}},
		{Code::SetVehicleLocation, {0, 0, 1, 0, 0, 0}},
		{Code::ChangeEventLocation, {10005, 0, 1, 0, 0, 0}},
		{Code::TradeEventLocations, {1, 10001, 0, 0, 0}},
		{Code::StoreTerrainID, {1, 3, 4, 1}},
		{Code::StoreEventID, {1, 3, 4, 2}},
		{Code::EraseScreen, {-1}},
		{Code::ShowScreen, {-1}},
		{Code::TintScreen, {100, 100, 100, 100, 5, 0}},
		{Code::FlashScreen, {26, 31, 20, 18, 5, 0, 0}},
		{Code::ShakeScreen, {5, 5, 5, 1}},
		{Code::PanScreen, {2, 0, 1, 4, 1}},
		{Code::WeatherEffects, {0, 1}},
		{Code::ShowPicture, {1, 0, 160, 120, 0, 100, 0, 1, 100, 100, 100, 100, 0, 60}},
		{Code::MovePicture, {32, 0, 160, 120, 0, 100, 100, 0, 100, 100, 100, 100, 0, 0, 1, 0, 100}},
		{Code::ErasePicture, {1}},
		{Code::ShowBattleAnimation, {1, 10001, 1, 0}},
		{Code::PlayerVisibility, {1}},
		{Code::FlashSprite, {10001, 31, 31, 31, 31, 30, 0}},
		{Code::MoveEvent, {10001, 8, 0, 0, 12}},
		{Code::ProceedWithMovement, {}},
		{Code::HaltAllMovement, {}},
		{Code::Wait, {0, 0}},
		{Code::PlayBGM, {0, 100, 100, 50}},
		{Code::FadeOutBGM, {1000}},
		{Code::MemorizeBGM, {}},
		{Code::PlayMemorizedBGM, {}},
		{Code::PlaySound, {100, 100, 50}},
		{Code::PlayMovie, {0, 0, 0, 320, 240}},
		{Code::KeyInputProc, {7, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0}},
		{Code::ChangeMapTileset, {4}},
		{Code::ChangePBG, {1, 1, 0, 5, 0, 0}},
		{Code::ChangeEncounterSteps, {1}},
		{Code::TileSubstitution, {0, 115, 1}},
		{Code::TeleportTargets, {0, 55, 13, 6, 0, 1}},
		{Code::ChangeTeleportAccess, {1}},
		{Code::EscapeTarget, {5, 3, 8, 0, 6}},
		{Code::ChangeEscapeAccess, {1}},
		{Code::OpenSaveMenu, {5, 0}},
		{Code::ChangeSaveAccess, {1}},
		{Code::OpenMainMenu, {}},
		{Code::ChangeMainMenuAccess, {1}},
		{Code::ConditionalBranch, {0, 1, 0, 0, 0, 1}},
		{Code::Label, {1}},
		{Code::JumpToLabel, {1}},
		{Code::Loop, {}},
		{Code::BreakLoop, {}},
		{Code::EndEventProcessing, {}},
		{Code::EraseEvent, {0, 0, 0}},
		{Code::CallEvent, {0, 1, 0}},
		{Code::Comment, {}},
		{Code::GameOver, {}},
		{Code::ReturntoTitleScreen, {}},
		{Code::ChangeMonsterHP, {0, 1, 0, 1, 0}},
		{Code::ChangeMonsterMP, {0, 0, 0, 1}},
		{Code::ChangeMonsterCondition, {0, 0, 1}},
		{Code::ShowHiddenMonster, {0}},
		{Code::ChangeBattleBG, {}},
		{Code::ShowBattleAnimation_B, {1, -1, 1}},
		{Code::ConditionalBranch_B, {1, 1, 0, 0, 0, 0}},
		{Code::TerminateBattle, {}},
		{Code::ShowMessage_2, {}},
		{Code::ShowChoiceOption, {0}},
		{Code::ShowChoiceEnd, {}},
		{Code::VictoryHandler, {}},
		{Code::EscapeHandler, {}},
		{Code::DefeatHandler, {}},
		{Code::EndBattle, {}},
		{Code::Transaction, {}},
		{Code::NoTransaction, {}},
		{Code::EndShop, {}},
		{Code::Stay, {}},
		{Code::NoStay, {}},
		{Code::EndInn, {}},
		{Code::ElseBranch, {}},
		{Code::EndBranch, {}},
		{Code::EndLoop, {}},
		{Code::Comment_2, {}},
		{Code::ElseBranch_B, {}},
		{Code::EndBranch_B, {}},
	};
	return defaults;
}

EventCommand line(EventCommand::Code code, int indent, std::vector<int32_t> parameters = {}, const char* text = "") {
	EventCommand cmd;
	cmd.code = static_cast<int32_t>(code);
	cmd.indent = indent;
	cmd.string = lcf::DBString(text);
	cmd.parameters = lcf::DBArray<int32_t>(parameters.begin(), parameters.end());
	return cmd;
}
}

std::vector<int32_t> EventCommandDefaults::parameters(Code code) {
	auto it = table().find(code);
	return it == table().end() ? std::vector<int32_t>() : it->second;
}

std::vector<EventCommand> EventCommandDefaults::make(Code code, int indent) {
	auto params = parameters(code);
	switch (code) {
		case Code::ConditionalBranch:
			// with the "else" branch, like RPG Maker
			return { line(code, indent, params), line(Code::END, indent + 1),
				line(Code::ElseBranch, indent), line(Code::END, indent + 1),
				line(Code::EndBranch, indent) };
		case Code::ConditionalBranch_B:
			return { line(code, indent, params), line(Code::END, indent + 1),
				line(Code::ElseBranch_B, indent), line(Code::END, indent + 1),
				line(Code::EndBranch_B, indent) };
		case Code::Loop:
			return { line(code, indent), line(Code::END, indent + 1), line(Code::EndLoop, indent) };
		case Code::ShowChoice:
			return { line(code, indent, {0}, "Yes/No"),
				line(Code::ShowChoiceOption, indent, {0}, "Yes"), line(Code::END, indent + 1),
				line(Code::ShowChoiceOption, indent, {1}, "No"), line(Code::END, indent + 1),
				line(Code::ShowChoiceEnd, indent) };
		case Code::Comment:
		case Code::ShowMessage:
		case Code::Label:
		case Code::JumpToLabel:
			return { line(code, indent, code == Code::Label || code == Code::JumpToLabel ? std::vector<int32_t>{1} : params) };
		default:
			return { line(code, indent, params) };
	}
}

bool EventCommandDefaults::isStructural(Code code) {
	switch (code) {
		case Code::END:
		case Code::ShowMessage_2:
		case Code::Comment_2:
		case Code::ShowChoiceOption:
		case Code::ShowChoiceEnd:
		case Code::ElseBranch:
		case Code::EndBranch:
		case Code::ElseBranch_B:
		case Code::EndBranch_B:
		case Code::EndLoop:
		case Code::VictoryHandler:
		case Code::EscapeHandler:
		case Code::DefeatHandler:
		case Code::EndBattle:
		case Code::Transaction:
		case Code::NoTransaction:
		case Code::EndShop:
		case Code::Stay:
		case Code::NoStay:
		case Code::EndInn:
			return true;
		default:
			return false;
	}
}

std::pair<int, int> EventCommandDefaults::range(const std::vector<EventCommand>& commands, int index) {
	const int size = static_cast<int>(commands.size());
	if (index < 0 || index >= size) {
		return {index, index};
	}
	const auto& cmd = commands[index];
	const auto code = static_cast<Code>(cmd.code);
	if (isStructural(code)) {
		return {index, index};
	}

	int last = index + 1;
	auto closes = [](Code c) {
		return c == Code::EndBranch || c == Code::EndBranch_B || c == Code::EndLoop || c == Code::ShowChoiceEnd
			|| c == Code::EndBattle || c == Code::EndShop || c == Code::EndInn;
	};
	auto block_until_end = [&] {
		while (last < size && !(commands[last].indent == cmd.indent && closes(static_cast<Code>(commands[last].code)))) {
			++last;
		}
		return std::make_pair(index, std::min(last + 1, size));
	};

	switch (code) {
		case Code::ShowMessage:
			while (last < size && static_cast<Code>(commands[last].code) == Code::ShowMessage_2) {
				++last;
			}
			return {index, last};
		case Code::Comment:
			while (last < size && static_cast<Code>(commands[last].code) == Code::Comment_2) {
				++last;
			}
			return {index, last};
		case Code::ConditionalBranch:
		case Code::ConditionalBranch_B:
		case Code::Loop:
		case Code::ShowChoice:
			return block_until_end();
		case Code::EnemyEncounter:
		case Code::OpenShop:
		case Code::ShowInn:
			// a block only with its handlers
			if (last < size && commands[last].indent == cmd.indent && isStructural(static_cast<Code>(commands[last].code))) {
				return block_until_end();
			}
			return {index, last};
		default:
			return {index, last};
	}
}
