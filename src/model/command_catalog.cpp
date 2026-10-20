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

#include "command_catalog.h"
#include <QCoreApplication>

namespace CommandCatalog {

// The pages and order of RPG Maker 2003
const std::vector<Page>& mapPages()
{
	static const std::vector<Page> pages = {
		{ QT_TRANSLATE_NOOP("EventCommandPalette", "Messages && Party"), {
			{Code::ShowMessage, QT_TRANSLATE_NOOP("EventCommandPalette", "Show Message"), false},
			{Code::MessageOptions, QT_TRANSLATE_NOOP("EventCommandPalette", "Message Options"), false},
			{Code::ChangeFaceGraphic, QT_TRANSLATE_NOOP("EventCommandPalette", "Select Face Graphic"), false},
			{Code::ShowChoice, QT_TRANSLATE_NOOP("EventCommandPalette", "Show Choices"), false},
			{Code::InputNumber, QT_TRANSLATE_NOOP("EventCommandPalette", "Input Number"), false},
			{Code::END, nullptr, false},
			{Code::ControlSwitches, QT_TRANSLATE_NOOP("EventCommandPalette", "Control Switches"), false},
			{Code::ControlVars, QT_TRANSLATE_NOOP("EventCommandPalette", "Control Variables"), false},
			{Code::TimerOperation, QT_TRANSLATE_NOOP("EventCommandPalette", "Timer Operations"), false},
			{Code::ChangeGold, QT_TRANSLATE_NOOP("EventCommandPalette", "Change Money"), false},
			{Code::ChangeItems, QT_TRANSLATE_NOOP("EventCommandPalette", "Change Items"), false},
			{Code::ChangePartyMembers, QT_TRANSLATE_NOOP("EventCommandPalette", "Change Party"), false},
			{Code::END, nullptr, false},
			{Code::EnemyEncounter, QT_TRANSLATE_NOOP("EventCommandPalette", "Start Combat"), false},
			{Code::OpenShop, QT_TRANSLATE_NOOP("EventCommandPalette", "Call Shop"), false},
			{Code::ShowInn, QT_TRANSLATE_NOOP("EventCommandPalette", "Call Inn"), false},
			{Code::EnterHeroName, QT_TRANSLATE_NOOP("EventCommandPalette", "Enter Hero Name"), false},
			{Code::END, nullptr, false},
			{Code::ChangeVehicleGraphic, QT_TRANSLATE_NOOP("EventCommandPalette", "Change Vehicle Graphic"), false},
			{Code::ChangeSystemBGM, QT_TRANSLATE_NOOP("EventCommandPalette", "Change System BGM"), false},
			{Code::ChangeSystemSFX, QT_TRANSLATE_NOOP("EventCommandPalette", "Change System SE"), false},
			{Code::ChangeSystemGraphics, QT_TRANSLATE_NOOP("EventCommandPalette", "Change System Graphics"), false},
			{Code::ChangeScreenTransitions, QT_TRANSLATE_NOOP("EventCommandPalette", "Change Screen Transitions"), false},
			{Code::END, nullptr, false},
			{Code::ChangeExp, QT_TRANSLATE_NOOP("EventCommandPalette", "Change Experience"), false},
			{Code::ChangeLevel, QT_TRANSLATE_NOOP("EventCommandPalette", "Change Level"), false},
			{Code::ChangeParameters, QT_TRANSLATE_NOOP("EventCommandPalette", "Change Parameters"), false},
			{Code::ChangeSkills, QT_TRANSLATE_NOOP("EventCommandPalette", "Change Skills"), false},
			{Code::ChangeEquipment, QT_TRANSLATE_NOOP("EventCommandPalette", "Change Equipment"), false},
			{Code::ChangeHP, QT_TRANSLATE_NOOP("EventCommandPalette", "Change HP"), false},
			{Code::ChangeSP, QT_TRANSLATE_NOOP("EventCommandPalette", "Change MP"), false},
			{Code::ChangeCondition, QT_TRANSLATE_NOOP("EventCommandPalette", "Change Condition"), false},
			{Code::FullHeal, QT_TRANSLATE_NOOP("EventCommandPalette", "Full Recovery"), false},
			{Code::SimulatedAttack, QT_TRANSLATE_NOOP("EventCommandPalette", "Damage Hero"), false},
			{Code::ChangeClass, QT_TRANSLATE_NOOP("EventCommandPalette", "Change Class"), true},
			{Code::ChangeBattleCommands, QT_TRANSLATE_NOOP("EventCommandPalette", "Change Battle Commands"), true},
			{Code::END, nullptr, false},
			{Code::ChangeHeroName, QT_TRANSLATE_NOOP("EventCommandPalette", "Change Hero Name"), false},
			{Code::ChangeHeroTitle, QT_TRANSLATE_NOOP("EventCommandPalette", "Change Hero Title"), false},
			{Code::ChangeSpriteAssociation, QT_TRANSLATE_NOOP("EventCommandPalette", "Change Hero Graphic"), false},
			{Code::ChangeActorFace, QT_TRANSLATE_NOOP("EventCommandPalette", "Change Hero Face"), false},
		}},
		{ QT_TRANSLATE_NOOP("EventCommandPalette", "Map && Screen"), {
			{Code::Teleport, QT_TRANSLATE_NOOP("EventCommandPalette", "Teleport"), false},
			{Code::MemorizeLocation, QT_TRANSLATE_NOOP("EventCommandPalette", "Memorize Location"), false},
			{Code::RecallToLocation, QT_TRANSLATE_NOOP("EventCommandPalette", "Recall to Location"), false},
			{Code::EnterExitVehicle, QT_TRANSLATE_NOOP("EventCommandPalette", "Enter/Exit Vehicle"), false},
			{Code::SetVehicleLocation, QT_TRANSLATE_NOOP("EventCommandPalette", "Set Vehicle Location"), false},
			{Code::ChangeEventLocation, QT_TRANSLATE_NOOP("EventCommandPalette", "Set Event Location"), false},
			{Code::TradeEventLocations, QT_TRANSLATE_NOOP("EventCommandPalette", "Swap Event Locations"), false},
			{Code::StoreTerrainID, QT_TRANSLATE_NOOP("EventCommandPalette", "Get Terrain ID"), false},
			{Code::StoreEventID, QT_TRANSLATE_NOOP("EventCommandPalette", "Get Event ID"), false},
			{Code::END, nullptr, false},
			{Code::EraseScreen, QT_TRANSLATE_NOOP("EventCommandPalette", "Erase Screen"), false},
			{Code::ShowScreen, QT_TRANSLATE_NOOP("EventCommandPalette", "Show Screen"), false},
			{Code::TintScreen, QT_TRANSLATE_NOOP("EventCommandPalette", "Tint Screen"), false},
			{Code::FlashScreen, QT_TRANSLATE_NOOP("EventCommandPalette", "Flash Screen"), false},
			{Code::ShakeScreen, QT_TRANSLATE_NOOP("EventCommandPalette", "Shake Screen"), false},
			{Code::PanScreen, QT_TRANSLATE_NOOP("EventCommandPalette", "Pan Screen"), false},
			{Code::WeatherEffects, QT_TRANSLATE_NOOP("EventCommandPalette", "Weather Effects"), false},
			{Code::END, nullptr, false},
			{Code::ShowPicture, QT_TRANSLATE_NOOP("EventCommandPalette", "Show Picture"), false},
			{Code::MovePicture, QT_TRANSLATE_NOOP("EventCommandPalette", "Move Picture"), false},
			{Code::ErasePicture, QT_TRANSLATE_NOOP("EventCommandPalette", "Erase Picture"), false},
			{Code::ShowBattleAnimation, QT_TRANSLATE_NOOP("EventCommandPalette", "Show Battle Animation"), false},
			{Code::PlayerVisibility, QT_TRANSLATE_NOOP("EventCommandPalette", "Hero Transparency"), false},
			{Code::FlashSprite, QT_TRANSLATE_NOOP("EventCommandPalette", "Flash Event"), false},
			{Code::END, nullptr, false},
			{Code::MoveEvent, QT_TRANSLATE_NOOP("EventCommandPalette", "Move Event"), false},
			{Code::ProceedWithMovement, QT_TRANSLATE_NOOP("EventCommandPalette", "Wait Until Moved"), false},
			{Code::HaltAllMovement, QT_TRANSLATE_NOOP("EventCommandPalette", "Stop All Movement"), false},
			{Code::Wait, QT_TRANSLATE_NOOP("EventCommandPalette", "Wait"), false},
			{Code::END, nullptr, false},
			{Code::PlayBGM, QT_TRANSLATE_NOOP("EventCommandPalette", "Play BGM"), false},
			{Code::FadeOutBGM, QT_TRANSLATE_NOOP("EventCommandPalette", "Fade Out BGM"), false},
			{Code::MemorizeBGM, QT_TRANSLATE_NOOP("EventCommandPalette", "Memorize BGM"), false},
			{Code::PlayMemorizedBGM, QT_TRANSLATE_NOOP("EventCommandPalette", "Play Memorized BGM"), false},
			{Code::PlaySound, QT_TRANSLATE_NOOP("EventCommandPalette", "Play Sound Effect"), false},
			{Code::PlayMovie, QT_TRANSLATE_NOOP("EventCommandPalette", "Play Movie"), false},
			{Code::END, nullptr, false},
			{Code::KeyInputProc, QT_TRANSLATE_NOOP("EventCommandPalette", "Key Input Processing"), false},
			{Code::ChangeMapTileset, QT_TRANSLATE_NOOP("EventCommandPalette", "Change Chipset"), false},
			{Code::ChangePBG, QT_TRANSLATE_NOOP("EventCommandPalette", "Change Panorama"), false},
			{Code::ChangeEncounterSteps, QT_TRANSLATE_NOOP("EventCommandPalette", "Change Encounter Rate"), false},
			{Code::TileSubstitution, QT_TRANSLATE_NOOP("EventCommandPalette", "Change Single Tile"), false},
			{Code::END, nullptr, false},
			{Code::TeleportTargets, QT_TRANSLATE_NOOP("EventCommandPalette", "Set Teleport Location"), false},
			{Code::ChangeTeleportAccess, QT_TRANSLATE_NOOP("EventCommandPalette", "Enable Teleport"), false},
			{Code::EscapeTarget, QT_TRANSLATE_NOOP("EventCommandPalette", "Set Escape Location"), false},
			{Code::ChangeEscapeAccess, QT_TRANSLATE_NOOP("EventCommandPalette", "Enable Escape"), false},
		}},
		{ QT_TRANSLATE_NOOP("EventCommandPalette", "Flow && System"), {
			{Code::OpenSaveMenu, QT_TRANSLATE_NOOP("EventCommandPalette", "Call Save Menu"), false},
			{Code::ChangeSaveAccess, QT_TRANSLATE_NOOP("EventCommandPalette", "Enable Saving"), false},
			{Code::OpenMainMenu, QT_TRANSLATE_NOOP("EventCommandPalette", "Call System Menu"), false},
			{Code::ChangeMainMenuAccess, QT_TRANSLATE_NOOP("EventCommandPalette", "Enable System Menu"), false},
			{Code::END, nullptr, false},
			{Code::ConditionalBranch, QT_TRANSLATE_NOOP("EventCommandPalette", "Conditional Branch"), false},
			{Code::Label, QT_TRANSLATE_NOOP("EventCommandPalette", "Label"), false},
			{Code::JumpToLabel, QT_TRANSLATE_NOOP("EventCommandPalette", "Go to Label"), false},
			{Code::Loop, QT_TRANSLATE_NOOP("EventCommandPalette", "Loop"), false},
			{Code::BreakLoop, QT_TRANSLATE_NOOP("EventCommandPalette", "Break Loop"), false},
			{Code::EndEventProcessing, QT_TRANSLATE_NOOP("EventCommandPalette", "End Event Processing"), false},
			{Code::EraseEvent, QT_TRANSLATE_NOOP("EventCommandPalette", "Erase Event"), false},
			{Code::CallEvent, QT_TRANSLATE_NOOP("EventCommandPalette", "Call Event"), false},
			{Code::END, nullptr, false},
			{Code::Comment, QT_TRANSLATE_NOOP("EventCommandPalette", "Comment"), false},
			{Code::GameOver, QT_TRANSLATE_NOOP("EventCommandPalette", "Game Over"), false},
			{Code::ReturntoTitleScreen, QT_TRANSLATE_NOOP("EventCommandPalette", "Return to Title Screen"), false},
			{Code::END, nullptr, false},
			{Code::OpenLoadMenu, QT_TRANSLATE_NOOP("EventCommandPalette", "Call Load Menu"), true},
			{Code::ExitGame, QT_TRANSLATE_NOOP("EventCommandPalette", "Exit Game"), true},
			{Code::ToggleAtbMode, QT_TRANSLATE_NOOP("EventCommandPalette", "Toggle ATB Mode"), true},
			{Code::ToggleFullscreen, QT_TRANSLATE_NOOP("EventCommandPalette", "Toggle Fullscreen"), true},
			{Code::OpenVideoOptions, QT_TRANSLATE_NOOP("EventCommandPalette", "Open Video Options"), true},
			{Code::END, nullptr, false},
			// RPG Maker 2003 offers them in battle pages only: last, after its own additions
			{battleCommands, QT_TRANSLATE_NOOP("EventCommandPalette", "Battle Event Commands"), false},
		}},
	};
	return pages;
}

// The Maniac Patch commands, by theme
const Page& maniacPage()
{
	static const Page page = { QT_TRANSLATE_NOOP("EventCommandPalette", "Maniac Patch"), {
		{Code::Maniac_GetSaveInfo, QT_TRANSLATE_NOOP("EventCommandPalette", "Get Save Info"), false, true},
		{Code::Maniac_Save, QT_TRANSLATE_NOOP("EventCommandPalette", "Save Game"), false, true},
		{Code::Maniac_Load, QT_TRANSLATE_NOOP("EventCommandPalette", "Load Game"), false, true},
		{Code::Maniac_EndLoadProcess, QT_TRANSLATE_NOOP("EventCommandPalette", "End Load Process"), false, true},
		{Code::Maniac_ControlGlobalSave, QT_TRANSLATE_NOOP("EventCommandPalette", "Control Global Save"), false, true},
		{Code::END, nullptr, false},
		{Code::Maniac_GetMousePosition, QT_TRANSLATE_NOOP("EventCommandPalette", "Get Mouse Position"), false, true},
		{Code::Maniac_SetMousePosition, QT_TRANSLATE_NOOP("EventCommandPalette", "Set Mouse Position"), false, true},
		{Code::Maniac_KeyInputProcEx, QT_TRANSLATE_NOOP("EventCommandPalette", "Key Input Processing Ex"), false, true},
		{Code::END, nullptr, false},
		{Code::Maniac_ShowStringPicture, QT_TRANSLATE_NOOP("EventCommandPalette", "Show String Picture"), false, true},
		{Code::Maniac_GetPictureInfo, QT_TRANSLATE_NOOP("EventCommandPalette", "Get Picture Info"), false, true},
		{Code::Maniac_ChangePictureId, QT_TRANSLATE_NOOP("EventCommandPalette", "Change Picture ID"), false, true},
		{Code::Maniac_EditPicture, QT_TRANSLATE_NOOP("EventCommandPalette", "Edit Picture"), false, true},
		{Code::Maniac_WritePicture, QT_TRANSLATE_NOOP("EventCommandPalette", "Write Picture"), false, true},
		{Code::END, nullptr, false},
		{Code::Maniac_RewriteMap, QT_TRANSLATE_NOOP("EventCommandPalette", "Rewrite Map"), false, true},
		{Code::Maniac_EditTile, QT_TRANSLATE_NOOP("EventCommandPalette", "Edit Tile"), false, true},
		{Code::Maniac_Zoom, QT_TRANSLATE_NOOP("EventCommandPalette", "Zoom"), false, true},
		{Code::Maniac_AddMoveRoute, QT_TRANSLATE_NOOP("EventCommandPalette", "Add Move Route"), false, true},
		{Code::END, nullptr, false},
		{Code::Maniac_ControlVarArray, QT_TRANSLATE_NOOP("EventCommandPalette", "Control Variable Array"), false, true},
		{Code::Maniac_ControlStrings, QT_TRANSLATE_NOOP("EventCommandPalette", "Control Strings"), false, true},
		{Code::Maniac_GetGameInfo, QT_TRANSLATE_NOOP("EventCommandPalette", "Get Game Info"), false, true},
		{Code::Maniac_SetGameOption, QT_TRANSLATE_NOOP("EventCommandPalette", "Set Game Option"), false, true},
		{Code::Maniac_CallCommand, QT_TRANSLATE_NOOP("EventCommandPalette", "Call Command"), false, true},
		{Code::Maniac_ControlTextProcessing, QT_TRANSLATE_NOOP("EventCommandPalette", "Control Text Processing"), false, true},
		{Code::END, nullptr, false},
		{Code::Maniac_ControlBattle, QT_TRANSLATE_NOOP("EventCommandPalette", "Control Battle"), false, true},
		{Code::Maniac_ControlAtbGauge, QT_TRANSLATE_NOOP("EventCommandPalette", "Control ATB Gauge"), false, true},
		{Code::Maniac_ChangeBattleCommandEx, QT_TRANSLATE_NOOP("EventCommandPalette", "Change Battle Commands Ex"), false, true},
		{Code::Maniac_GetBattleInfo, QT_TRANSLATE_NOOP("EventCommandPalette", "Get Battle Info"), false, true},
	}};
	return page;
}

// The battle event commands of RPG Maker 2003 (its 37 buttons, in its order)
const Page& battlePage()
{
	static const Page page = { QT_TRANSLATE_NOOP("EventCommandPalette", "Battle Event Commands"), {
		{Code::ShowMessage, QT_TRANSLATE_NOOP("EventCommandPalette", "Show Message"), false},
		{Code::ChangeFaceGraphic, QT_TRANSLATE_NOOP("EventCommandPalette", "Select Face Graphic"), false},
		{Code::ShowChoice, QT_TRANSLATE_NOOP("EventCommandPalette", "Show Choices"), false},
		{Code::InputNumber, QT_TRANSLATE_NOOP("EventCommandPalette", "Input Number"), false},
		{Code::END, nullptr, false},
		{Code::ControlSwitches, QT_TRANSLATE_NOOP("EventCommandPalette", "Control Switches"), false},
		{Code::ControlVars, QT_TRANSLATE_NOOP("EventCommandPalette", "Control Variables"), false},
		{Code::ChangeGold, QT_TRANSLATE_NOOP("EventCommandPalette", "Change Money"), false},
		{Code::ChangeItems, QT_TRANSLATE_NOOP("EventCommandPalette", "Change Items"), false},
		{Code::ChangePartyMembers, QT_TRANSLATE_NOOP("EventCommandPalette", "Change Party"), false},
		{Code::END, nullptr, false},
		{Code::ChangeLevel, QT_TRANSLATE_NOOP("EventCommandPalette", "Change Level"), false},
		{Code::ChangeSkills, QT_TRANSLATE_NOOP("EventCommandPalette", "Change Skills"), false},
		{Code::ChangeEquipment, QT_TRANSLATE_NOOP("EventCommandPalette", "Change Equipment"), false},
		{Code::ChangeHP, QT_TRANSLATE_NOOP("EventCommandPalette", "Change HP"), false},
		{Code::ChangeSP, QT_TRANSLATE_NOOP("EventCommandPalette", "Change MP"), false},
		{Code::ChangeCondition, QT_TRANSLATE_NOOP("EventCommandPalette", "Change Condition"), false},
		{Code::FullHeal, QT_TRANSLATE_NOOP("EventCommandPalette", "Full Recovery"), false},
		{Code::END, nullptr, false},
		{Code::ChangeMonsterHP, QT_TRANSLATE_NOOP("EventCommandPalette", "Change Enemy HP"), false},
		{Code::ChangeMonsterMP, QT_TRANSLATE_NOOP("EventCommandPalette", "Change Enemy MP"), false},
		{Code::ChangeMonsterCondition, QT_TRANSLATE_NOOP("EventCommandPalette", "Change Enemy Condition"), false},
		{Code::ShowHiddenMonster, QT_TRANSLATE_NOOP("EventCommandPalette", "Show Hidden Enemy"), false},
		{Code::ChangeBattleBG, QT_TRANSLATE_NOOP("EventCommandPalette", "Change Battle Background"), false},
		{Code::END, nullptr, false},
		{Code::TintScreen, QT_TRANSLATE_NOOP("EventCommandPalette", "Tint Screen"), false},
		{Code::FlashScreen, QT_TRANSLATE_NOOP("EventCommandPalette", "Flash Screen"), false},
		{Code::ShakeScreen, QT_TRANSLATE_NOOP("EventCommandPalette", "Shake Screen"), false},
		{Code::ShowBattleAnimation_B, QT_TRANSLATE_NOOP("EventCommandPalette", "Show Battle Animation"), false},
		{Code::END, nullptr, false},
		{Code::EnableCombo, QT_TRANSLATE_NOOP("EventCommandPalette", "Enable Combo"), true},
		{Code::ForceFlee, QT_TRANSLATE_NOOP("EventCommandPalette", "Force Flee"), true},
		{Code::END, nullptr, false},
		{Code::Wait, QT_TRANSLATE_NOOP("EventCommandPalette", "Wait"), false},
		{Code::PlayBGM, QT_TRANSLATE_NOOP("EventCommandPalette", "Play BGM"), false},
		{Code::FadeOutBGM, QT_TRANSLATE_NOOP("EventCommandPalette", "Fade Out BGM"), false},
		{Code::PlaySound, QT_TRANSLATE_NOOP("EventCommandPalette", "Play Sound Effect"), false},
		{Code::END, nullptr, false},
		{Code::ConditionalBranch_B, QT_TRANSLATE_NOOP("EventCommandPalette", "Conditional Branch"), false},
		{Code::EndEventProcessing, QT_TRANSLATE_NOOP("EventCommandPalette", "End Event Processing"), false},
		{Code::CallCommonEvent, QT_TRANSLATE_NOOP("EventCommandPalette", "Call Common Event"), true},
		{Code::Comment, QT_TRANSLATE_NOOP("EventCommandPalette", "Comment"), false},
		{Code::GameOver, QT_TRANSLATE_NOOP("EventCommandPalette", "Game Over"), false},
		{Code::TerminateBattle, QT_TRANSLATE_NOOP("EventCommandPalette", "End Battle"), false},
	}};
	return page;
}

std::vector<const Page*> pages(bool battle, bool maniac)
{
	std::vector<const Page*> shown;
	if (battle) {
		shown.push_back(&battlePage());
		if (maniac) {
			shown.push_back(&maniacPage());
		}
		return shown;
	}
	for (const auto& page : mapPages()) {
		shown.push_back(&page);
	}
	shown.push_back(&maniacPage());
	return shown;
}

QString translate(const char* text)
{
	return QCoreApplication::translate("EventCommandPalette", text);
}

QString commandName(Code code)
{
	for (const Page* page : pages(false, true)) {
		for (const auto& entry : page->entries) {
			if (entry.code == code && entry.name) {
				return translate(entry.name);
			}
		}
	}
	for (const auto& entry : battlePage().entries) {
		if (entry.code == code && entry.name) {
			return translate(entry.name);
		}
	}
	return QString();
}

}
