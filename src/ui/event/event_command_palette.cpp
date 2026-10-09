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

#include "event_command_palette.h"
#include <algorithm>
#include "core.h"
#include "ui/common/help.h"
#include <QDateTime>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSettings>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {
using Code = lcf::rpg::EventCommand::Code;

struct Entry {
	Code code;
	const char* name;
	bool rpg2k3_only;
	bool maniac_only = false;
};

struct Page {
	const char* title;
	std::vector<Entry> entries;
};

// The pages and order of RPG Maker 2003
const std::vector<Page>& mapPages() {
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
		}},
	};
	return pages;
}

// The Maniac Patch commands, by theme
const Page& maniacPage() {
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
const Page& battlePage() {
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
}

EventCommandPalette::EventCommandPalette(bool rpg2k3, bool battle, QWidget* parent) :
	QDialog(parent)
{
	setWindowTitle(tr("Event Commands"));
	const auto& project = core().project();
	const bool maniac = project && usesManiacPatch(*project);
	auto* layout = new QVBoxLayout(this);

	m_search = new QLineEdit(this);
	m_search->setPlaceholderText(tr("Search a command, Enter picks the best match"));
	m_search->setClearButtonEnabled(true);
	layout->addWidget(m_search);

	m_tabs = new QTabWidget(this);
	// all the pages in view
	m_tabs->setUsesScrollButtons(false);
	layout->addWidget(m_tabs);

	auto add_page = [&](const Page& page, int number) {
		// groups of related commands (separated by END entries), in two columns
		std::vector<std::vector<const Entry*>> groups(1);
		for (const auto& entry : page.entries) {
			if (entry.code == Code::END) {
				groups.emplace_back();
			} else {
				groups.back().push_back(&entry);
			}
		}
		// the second column starts at the group that best balances the two
		int total = 0;
		for (const auto& group : groups) {
			total += static_cast<int>(group.size());
		}
		size_t split = groups.size();
		int best = total, before = 0;
		for (size_t i = 0; i < groups.size(); ++i) {
			int difference = std::abs(total - 2 * before);
			if (i > 0 && difference < best) {
				best = difference;
				split = i;
			}
			before += static_cast<int>(groups[i].size());
		}

		auto* widget = new QWidget(m_tabs);
		auto* columns = new QHBoxLayout(widget);
		columns->setSpacing(10);
		QVBoxLayout* column = nullptr;
		for (size_t g = 0; g < groups.size(); ++g) {
			const auto& group = groups[g];
			if (g == 0 || g == split) {
				column = new QVBoxLayout();
				column->setSpacing(10);
				columns->addLayout(column, 1);
			}
			auto* box = new QVBoxLayout();
			box->setSpacing(2);
			for (const Entry* entry : group) {
				auto* button = new QPushButton(tr(entry->name) + "...", widget);
				button->setProperty("code", static_cast<int>(entry->code));
				button->setAutoDefault(false);
				if (entry->rpg2k3_only && !rpg2k3) {
					button->setEnabled(false);
					button->setToolTip(tr("Only in RPG Maker 2003"));
				}
				if (entry->maniac_only && !maniac) {
					button->setEnabled(false);
					button->setToolTip(tr("Only with the Maniac Patch"));
				}
				connect(button, &QPushButton::clicked, this, [this, button] {
					m_code = static_cast<Code>(button->property("code").toInt());
					accept();
				});
				box->addWidget(button);
				m_buttons.append(button);
			}
			column->addLayout(box);
		}
		for (int i = 0; i < columns->count(); ++i) {
			static_cast<QVBoxLayout*>(columns->itemAt(i)->layout())->addStretch(1);
		}
		m_tabs->addTab(widget, QString("%1 %2").arg(number).arg(tr(page.title)));
	};

	int number = 1;
	if (battle) {
		// only the commands of battles, like RPG Maker's window for them;
		// the Maniac Patch also runs its commands in battles
		setWindowTitle(tr("Battle Event Commands"));
		add_page(battlePage(), number++);
		if (maniac) {
			add_page(maniacPage(), number++);
		}
	} else {
		for (const auto& page : mapPages()) {
			add_page(page, number++);
		}
		add_page(maniacPage(), number++);
	}

	// the results of a search replace the pages, which would hide the
	// matches of the other pages
	m_results = new QWidget(this);
	m_results->setObjectName(QStringLiteral("searchResults"));
	auto* results_layout = new QVBoxLayout(m_results);
	results_layout->setContentsMargins(0, 0, 0, 0);
	results_layout->setSpacing(2);
	results_layout->addStretch(1);
	m_results->hide();
	layout->insertWidget(layout->indexOf(m_tabs) + 1, m_results, 1);

	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Help, this);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(buttons, &QDialogButtonBox::helpRequested, this, [this] { Help::open(this, QStringLiteral("command-palette")); });
	layout->addWidget(buttons);

	connect(m_search, &QLineEdit::textChanged, this, &EventCommandPalette::filter);
	connect(m_search, &QLineEdit::returnPressed, this, [this] {
		// the command of that name, else the first result that can be added
		const QString search = m_search->text().trimmed();
		QPushButton* first = nullptr;
		for (auto* button : m_resultButtons) {
			if (!button->isEnabled()) {
				continue;
			}
			if (button->text().chopped(3).compare(search, Qt::CaseInsensitive) == 0) {
				button->click();
				return;
			}
			first = first ? first : button;
		}
		if (first) {
			first->click();
		}
	});
	m_search->setFocus();
}

void EventCommandPalette::filter(const QString& text) {
	qDeleteAll(m_resultButtons);
	m_resultButtons.clear();
	const QString search = text.trimmed();
	m_tabs->setVisible(search.isEmpty());
	m_results->setVisible(!search.isEmpty());
	if (search.isEmpty()) {
		return;
	}

	// The commands containing the text, in the order of the pages and of
	// their place in them (left column, then right): always the same list
	std::vector<QPushButton*> matches;
	for (QPushButton* button : m_buttons) {
		if (button->text().chopped(3).contains(search, Qt::CaseInsensitive)) {  // without "..."
			matches.push_back(button);
		}
	}

	auto* layout = static_cast<QVBoxLayout*>(m_results->layout());
	for (QPushButton* button : matches) {
		// the page of the command, as its tooltip if it has none
		const QString page = m_tabs->tabText(m_tabs->indexOf(button->parentWidget()));
		auto* result = new QPushButton(button->text(), m_results);
		result->setAutoDefault(false);
		result->setEnabled(button->isEnabled());
		result->setToolTip(button->toolTip().isEmpty() ? page : button->toolTip());
		connect(result, &QPushButton::clicked, button, &QPushButton::click);
		layout->insertWidget(layout->count() - 1, result);
		m_resultButtons.append(result);
	}
	if (matches.empty()) {
		auto* none = new QPushButton(tr("No command matches \"%1\"").arg(search), m_results);
		none->setEnabled(false);
		layout->insertWidget(layout->count() - 1, none);
		m_resultButtons.append(none);
	}
}

EventCommandPalette::Code EventCommandPalette::selectedCode() const {
	return m_code;
}

QString EventCommandPalette::commandName(Code code) {
	for (const auto& page : mapPages()) {
		for (const auto& entry : page.entries) {
			if (entry.code == code && entry.name) {
				return tr(entry.name);
			}
		}
	}
	for (const auto& page : {battlePage(), maniacPage()}) {
		for (const auto& entry : page.entries) {
			if (entry.code == code && entry.name) {
				return tr(entry.name);
			}
		}
	}
	return QString();
}

bool EventCommandPalette::usesManiacPatch(const Project& project) {
	// As EasyRPG Player decides (Game_ConfigGame, Player::CreateGameObjects):
	// a [Patch] section of EasyRPG.ini overrides the detection, then the
	// version information of RPG_RT.exe, then accord.dll in the game folder
	const QString ini = project.findFile(QStringLiteral("EasyRPG.ini"));
	if (!ini.isEmpty()) {
		QSettings settings(ini, QSettings::IniFormat);
		settings.beginGroup(QStringLiteral("Patch"));
		const QStringList keys = settings.childKeys();
		if (!keys.isEmpty()) {
			for (const auto& key : keys) {
				if (key.compare(QStringLiteral("Maniac"), Qt::CaseInsensitive) == 0) {
					return settings.value(key).toInt() > 0;
				}
			}
			return false;
		}
	}

	const QString exe = project.findFile(QStringLiteral("RPG_RT.exe"));
	if (!exe.isEmpty()) {
		// "Maniacs, v" in UTF-16, in the version resource; read once per file version
		static QHash<QString, bool> known;
		const QString key = exe + QFileInfo(exe).lastModified().toString(Qt::ISODateWithMs);
		auto it = known.find(key);
		if (it == known.end()) {
			QFile file(exe);
			bool found = false;
			if (file.open(QIODevice::ReadOnly)) {
				const QByteArray marker = QByteArray::fromRawData(reinterpret_cast<const char*>(u"Maniacs, v"), 20);
				found = file.readAll().contains(marker);
			}
			it = known.insert(key, found);
		}
		if (*it) {
			return true;
		}
	}

	return !project.findFile(QStringLiteral("accord.dll")).isEmpty();
}
