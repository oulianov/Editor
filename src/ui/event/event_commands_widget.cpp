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

#include "event_commands_widget.h"
#include "stringizer.h"
#include "model/event_command_defaults.h"
#include "model/event_command_list.h"
#include "ui/event/event_command_palette.h"
#include "ui/event/move_route_dialog.h"
#include "ui/common/help.h"
#include "model/move_route_codec.h"
#include "ui/commands/all_commands.h"
#include "ui/event/event_raw_widget.h"
#include "ui/common/widget_as_dialog_wrapper.h"
#include <iostream>
#include <QMenu>
#include <QUndoStack>
#include <functional>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QTreeWidgetItemIterator>
#include "common/dbstring.h"
#include "common/editor_settings.h"
#include "common/script_link.h"
#include <QPainter>
#include <QHeaderView>
#include <QAction>
#include "ui/common/edit_actions.h"
#include <QStyledItemDelegate>
#include <memory>

namespace {
class CommandLineDelegate : public QStyledItemDelegate {
public:
	explicit CommandLineDelegate(QTreeWidget* view) : QStyledItemDelegate(view), m_view(view) {}

	QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
		QSize size = QStyledItemDelegate::sizeHint(option, index);
		if (index.data(Qt::UserRole + 1).toBool()) {
			// All preceding lines have the same height. Only the final insertion
			// row expands; long lists retain a normal last row and scroll normally.
			// (the count of the lines before it is on the row itself: the list
			// shows one column, the line numbers of the second are not in it)
			const int line = index.data(Qt::UserRole + 2).toInt();
			size.setHeight(std::max(size.height(), m_view->viewport()->height() - line * size.height()));
		}
		return size;
	}

	void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
		QStyleOptionViewItem line = option;
		if (index.data(Qt::UserRole + 1).toBool()) {
			// Keep "<>" on the first line rather than in the middle of the space.
			line.rect.setHeight(QStyledItemDelegate::sizeHint(option, index).height());
		}
		QStyledItemDelegate::paint(painter, line, index);
	}

private:
	QTreeWidget* m_view;
};
}

std::vector<lcf::rpg::EventCommand> EventCommandsWidget::s_clipboard;

EventCommandsWidget::EventCommandsWidget(QWidget* parent) :
	QTreeWidget(parent), m_undoStack(new QUndoStack(this))
{
	m_undoStack->setUndoLimit(100);
	setHeaderHidden(true);
	// Show whole lines and scroll sideways, like RPG Maker
	setTextElideMode(Qt::ElideNone);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	header()->setSectionResizeMode(QHeaderView::ResizeToContents);
	header()->setStretchLastSection(false);
	// No expand arrows nor their gutter: nesting shows by indentation only,
	// one step per level, like RPG Maker; every block stays open
	setRootIsDecorated(false);
	setItemsExpandable(false);
	// The last insertion row fills the space left below the command lines.
	setItemDelegate(new CommandLineDelegate(this));
	updateLook();
	connect(&EditorSettings::instance(), &EditorSettings::changed, this, [this] {
		updateLook();
		viewport()->update();
	});

	setContextMenuPolicy(Qt::CustomContextMenu);

	connect(this, &QTreeWidget::itemDoubleClicked, this, &EventCommandsWidget::editEvent);
	// One line is current, as usual; the other lines of its command (the
	// lines of a message, a whole branch...) are drawn selected with it, and
	// with Shift the commands up to the line where the selection started
	connect(this, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* current) {
		if (!m_extending) {
			m_anchor = indexOf(current);
		}
		highlightCommand(current);
	});
	connect(this, &QTreeWidget::customContextMenuRequested, this, &EventCommandsWidget::showContextMenu);
}

void EventCommandsWidget::setData(ProjectData& project, lcf::rpg::CommonEvent* event) {
	setDataInternal(project, event);
}

void EventCommandsWidget::setData(ProjectData& project, lcf::rpg::TroopPage* event) {
	m_battle = true;
	setDataInternal(project, event);
}

void EventCommandsWidget::setData(ProjectData& project, lcf::rpg::EventPage* event) {
	setDataInternal(project, event);
}

template<typename T>
void EventCommandsWidget::setDataInternal(ProjectData& project, T* event) {
	assert(event);
	m_project = &project;
	m_commands = &event->event_commands;
	m_undoStack->clear();
	m_shown.reset();
	refreshList();
}

namespace {
// A change of the command list: the list before and after it
class CommandListChange : public QUndoCommand {
public:
	using Commands = std::vector<lcf::rpg::EventCommand>;
	CommandListChange(std::function<void(const Commands&)> show, Commands before, Commands after) :
		m_show(std::move(show)), m_before(std::move(before)), m_after(std::move(after)) {}

	void undo() override { m_show(m_before); }
	void redo() override {
		// the change is already done when it is pushed
		if (!m_done) {
			m_done = true;
			return;
		}
		m_show(m_after);
	}

private:
	std::function<void(const Commands&)> m_show;
	Commands m_before, m_after;
	bool m_done = false;
};
}

void EventCommandsWidget::showCommands(const std::vector<lcf::rpg::EventCommand>& commands) {
	*m_commands = commands;
	m_shown = commands;
	refreshList();
}

template<typename T>
WidgetAsDialogWrapper<T, lcf::rpg::EventCommand>* make_evt_dialog(ProjectData* prj, lcf::rpg::EventCommand& cmd, EventCommandsWidget* self) {
	return new WidgetAsDialogWrapper<T, lcf::rpg::EventCommand>(*prj, cmd, self);
}

template<typename T>
WidgetAsDialogWrapper<T, EventCommandList>* make_complex_evt_dialog(ProjectData* prj, EventCommandList& commands, EventCommandsWidget* self) {
	return new WidgetAsDialogWrapper<T, EventCommandList>(*prj, commands, self);
}

void EventCommandsWidget::editEvent(QTreeWidgetItem* item, int column) {
	Q_UNUSED(column)
	if (!item || !(item->flags() & Qt::ItemIsSelectable)) {
		return;
	}
	int index = indexOf(item);
	using Cmd = lcf::rpg::EventCommand::Code;
	// The empty line closing a block: insert a command there, like RPG Maker
	if (index >= static_cast<int>(m_commands->size()) || static_cast<Cmd>((*m_commands)[index].code) == Cmd::END) {
		insertCommand(index);
		return;
	}
	if (editCommand(index, false)) {
		refreshList();
		selectIndex(index);
	}
}

bool EventCommandsWidget::hasEditor(lcf::rpg::EventCommand::Code code) {
	using Cmd = lcf::rpg::EventCommand::Code;
	// Commands with no configuration
	switch (code) {
		case Cmd::END:
		case Cmd::EnterExitVehicle:
		case Cmd::ProceedWithMovement:
		case Cmd::HaltAllMovement:
		case Cmd::MemorizeBGM:
		case Cmd::PlayMemorizedBGM:
		case Cmd::OpenSaveMenu:
		case Cmd::OpenMainMenu:
		case Cmd::Loop:
		case Cmd::BreakLoop:
		case Cmd::EndEventProcessing:
		case Cmd::EraseEvent:
		case Cmd::GameOver:
		case Cmd::ReturntoTitleScreen:
		case Cmd::OpenLoadMenu:
		case Cmd::ExitGame:
		case Cmd::ToggleAtbMode:
		case Cmd::ToggleFullscreen:
		case Cmd::OpenVideoOptions:
		case Cmd::TerminateBattle:
		case Cmd::ShowChoiceOption:
		case Cmd::ShowChoiceEnd:
		case Cmd::ElseBranch:
		case Cmd::EndBranch:
		case Cmd::EndLoop:
		case Cmd::EndBattle:
		case Cmd::EndShop:
		case Cmd::VictoryHandler:
		case Cmd::EscapeHandler:
		case Cmd::DefeatHandler:
		case Cmd::Transaction:
		case Cmd::NoTransaction:
		case Cmd::Stay:
		case Cmd::NoStay:
		case Cmd::EndInn:
		case Cmd::ElseBranch_B:
		case Cmd::EndBranch_B:
		case Cmd::Maniac_EndLoadProcess:
			return false;
		default:
			return true;
	}
}

bool EventCommandsWidget::editCommand(int index, bool is_new) {
	using Cmd = lcf::rpg::EventCommand::Code;
	// The next lines of a message or comment edit its first line.
	index = commandStart(index);
	auto& cmd = (*m_commands)[index];
	if (!hasEditor(static_cast<Cmd>(cmd.code))) {
		return true;
	}

	std::unique_ptr<QDialog> evt_dialog;

	EventCommandList evt_lst(*m_commands, index);

	switch (static_cast<Cmd>(cmd.code))	{
		case Cmd::CallCommonEvent: evt_dialog.reset(make_evt_dialog<CallCommonEventWidget>(m_project, cmd, this)); break;
		case Cmd::ForceFlee: evt_dialog.reset(make_evt_dialog<ForceFleeWidget>(m_project, cmd, this)); break;
		case Cmd::EnableCombo: evt_dialog.reset(make_evt_dialog<EnableComboWidget>(m_project, cmd, this)); break;
		case Cmd::ChangeClass: evt_dialog.reset(make_evt_dialog<ChangeClassWidget>(m_project, cmd, this)); break;
		case Cmd::ChangeBattleCommands: evt_dialog.reset(make_evt_dialog<ChangeBattleCommandsWidget>(m_project, cmd, this)); break;
		case Cmd::ShowMessage: evt_dialog.reset(make_complex_evt_dialog<ShowMessageWidget>(m_project, evt_lst, this)); break;
		case Cmd::MessageOptions: evt_dialog.reset(make_evt_dialog<MessageOptionsWidget>(m_project, cmd, this)); break;
		case Cmd::ChangeFaceGraphic: evt_dialog.reset(make_evt_dialog<ChangeFaceGraphicWidget>(m_project, cmd, this)); break;
		case Cmd::ShowChoice: evt_dialog.reset(make_complex_evt_dialog<ShowChoiceWidget>(m_project, evt_lst, this)); break;
		case Cmd::InputNumber: evt_dialog.reset(make_evt_dialog<InputNumberWidget>(m_project, cmd, this)); break;
		case Cmd::ControlSwitches: evt_dialog.reset(make_evt_dialog<SwitchOperationsWidget>(m_project, cmd, this)); break;
		case Cmd::ControlVars: evt_dialog.reset(make_evt_dialog<VariableOperationsWidget>(m_project, cmd, this)); break;
		case Cmd::TimerOperation: evt_dialog.reset(make_evt_dialog<TimerOperationWidget>(m_project, cmd, this)); break;
		case Cmd::ChangeGold: evt_dialog.reset(make_evt_dialog<ChangeMoneyWidget>(m_project, cmd, this)); break;
		case Cmd::ChangeItems: evt_dialog.reset(make_evt_dialog<ChangeItemWidget>(m_project, cmd, this)); break;
		case Cmd::ChangePartyMembers: evt_dialog.reset(make_evt_dialog<ChangePartyWidget>(m_project, cmd, this)); break;
		case Cmd::ChangeExp: evt_dialog.reset(make_evt_dialog<ChangeExperienceWidget>(m_project, cmd, this)); break;
		case Cmd::ChangeLevel: evt_dialog.reset(make_evt_dialog<ChangeLevelWidget>(m_project, cmd, this)); break;
		case Cmd::ChangeParameters: evt_dialog.reset(make_evt_dialog<ChangeParametersWidget>(m_project, cmd, this)); break;
		case Cmd::ChangeSkills: evt_dialog.reset(make_evt_dialog<ChangeSkillsWidget>(m_project, cmd, this)); break;
		case Cmd::ChangeEquipment: evt_dialog.reset(make_evt_dialog<ChangeEquipmentWidget>(m_project, cmd, this)); break;
		case Cmd::ChangeHP: evt_dialog.reset(make_evt_dialog<ChangeHPWidget>(m_project, cmd, this)); break;
		case Cmd::ChangeSP: evt_dialog.reset(make_evt_dialog<ChangeSPWidget>(m_project, cmd, this)); break;
		case Cmd::ChangeCondition: evt_dialog.reset(make_evt_dialog<ChangeConditionWidget>(m_project, cmd, this)); break;
		case Cmd::FullHeal: evt_dialog.reset(make_evt_dialog<FullHealWidget>(m_project, cmd, this)); break;
		case Cmd::SimulatedAttack: evt_dialog.reset(make_evt_dialog<SimulatedAttackWidget>(m_project, cmd, this)); break;
		case Cmd::ChangeHeroName: evt_dialog.reset(make_evt_dialog<ChangeActorNameWidget>(m_project, cmd, this)); break;
		case Cmd::ChangeHeroTitle: evt_dialog.reset(make_evt_dialog<ChangeActorTitleWidget>(m_project, cmd, this)); break;
		case Cmd::ChangeSpriteAssociation: evt_dialog.reset(make_evt_dialog<ChangeActorGraphicWidget>(m_project, cmd, this)); break;
		case Cmd::ChangeActorFace: evt_dialog.reset(make_evt_dialog<ChangeActorFaceWidget>(m_project, cmd, this)); break;
		case Cmd::ChangeVehicleGraphic: evt_dialog.reset(make_evt_dialog<ChangeVehicleGraphicWidget>(m_project, cmd, this)); break;
		case Cmd::ChangeSystemBGM: evt_dialog.reset(make_evt_dialog<ChangeSystemBGMWidget>(m_project, cmd, this)); break;
		case Cmd::ChangeSystemSFX: evt_dialog.reset(make_evt_dialog<ChangeSystemSFXWidget>(m_project, cmd, this)); break;
		case Cmd::ChangeSystemGraphics: evt_dialog.reset(make_evt_dialog<ChangeSystemGraphicsWidget>(m_project, cmd, this)); break;
		case Cmd::ChangeScreenTransitions: evt_dialog.reset(make_evt_dialog<ChangeScreenTransitionsWidget>(m_project, cmd, this)); break;
		case Cmd::EnemyEncounter: evt_dialog.reset(make_complex_evt_dialog<EnemyEncounterWidget>(m_project, evt_lst, this)); break;
		case Cmd::OpenShop: evt_dialog.reset(make_complex_evt_dialog<OpenShopWidget>(m_project, evt_lst, this)); break;
		case Cmd::ShowInn: evt_dialog.reset(make_complex_evt_dialog<ShowInnWidget>(m_project, evt_lst, this)); break;
		case Cmd::EnterHeroName: evt_dialog.reset(make_evt_dialog<EnterHeroNameWidget>(m_project, cmd, this)); break;
		case Cmd::Teleport: evt_dialog.reset(make_evt_dialog<TeleportWidget>(m_project, cmd, this)); break;
		case Cmd::MemorizeLocation: evt_dialog.reset(make_evt_dialog<MemorizeLocationWidget>(m_project, cmd, this)); break;
		case Cmd::RecallToLocation: evt_dialog.reset(make_evt_dialog<RecallToLocationWidget>(m_project, cmd, this)); break;
		case Cmd::SetVehicleLocation: evt_dialog.reset(make_evt_dialog<SetVehicleLocationWidget>(m_project, cmd, this)); break;
		case Cmd::ChangeEventLocation: evt_dialog.reset(make_evt_dialog<ChangeEventLocationWidget>(m_project, cmd, this)); break;
		case Cmd::TradeEventLocations: evt_dialog.reset(make_evt_dialog<TradeEventLocationsWidget>(m_project, cmd, this)); break;
		case Cmd::StoreTerrainID: evt_dialog.reset(make_evt_dialog<StoreTerrainIDWidget>(m_project, cmd, this)); break;
		case Cmd::StoreEventID: evt_dialog.reset(make_evt_dialog<StoreEventIDWidget>(m_project, cmd, this)); break;
		case Cmd::EraseScreen: evt_dialog.reset(make_evt_dialog<EraseScreenWidget>(m_project, cmd, this)); break;
		case Cmd::ShowScreen: evt_dialog.reset(make_evt_dialog<ShowScreenWidget>(m_project, cmd, this)); break;
		case Cmd::TintScreen: evt_dialog.reset(make_evt_dialog<TintScreenWidget>(m_project, cmd, this)); break;
		case Cmd::FlashScreen: evt_dialog.reset(make_evt_dialog<FlashScreenWidget>(m_project, cmd, this)); break;
		case Cmd::ShakeScreen: evt_dialog.reset(make_evt_dialog<ShakeScreenWidget>(m_project, cmd, this)); break;
		case Cmd::PanScreen: evt_dialog.reset(make_evt_dialog<PanScreenWidget>(m_project, cmd, this)); break;
		case Cmd::WeatherEffects: evt_dialog.reset(make_evt_dialog<WeatherEffectsWidget>(m_project, cmd, this)); break;
		case Cmd::ShowPicture: evt_dialog.reset(make_evt_dialog<ShowPictureWidget>(m_project, cmd, this)); break;
		case Cmd::MovePicture: evt_dialog.reset(make_evt_dialog<MovePictureWidget>(m_project, cmd, this)); break;
		case Cmd::ErasePicture: evt_dialog.reset(make_evt_dialog<ErasePictureWidget>(m_project, cmd, this)); break;
		case Cmd::ShowBattleAnimation: evt_dialog.reset(make_evt_dialog<ShowBattleAnimationWidget>(m_project, cmd, this)); break;
		case Cmd::PlayerVisibility: evt_dialog.reset(make_evt_dialog<PlayerVisibilityWidget>(m_project, cmd, this)); break;
		case Cmd::FlashSprite: evt_dialog.reset(make_evt_dialog<FlashSpriteWidget>(m_project, cmd, this)); break;
		case Cmd::MoveEvent: {
			const std::string encoding = m_project->project().encoding().toStdString();
			auto move = MoveRouteCodec::decode(cmd, encoding);
			MoveRouteDialog dialog(*m_project, move.route, this, &move);
			if (dialog.exec() != QDialog::Accepted) {
				return false;
			}
			MoveRouteCodec::encode(dialog.moveEvent(), cmd, encoding);
			return true;
		}
		case Cmd::Wait: evt_dialog.reset(make_evt_dialog<WaitWidget>(m_project, cmd, this)); break;
		case Cmd::PlayBGM: evt_dialog.reset(make_evt_dialog<PlayBGMWidget>(m_project, cmd, this)); break;
		case Cmd::FadeOutBGM: evt_dialog.reset(make_evt_dialog<FadeOutBGMWidget>(m_project, cmd, this)); break;
		case Cmd::PlaySound: evt_dialog.reset(make_evt_dialog<PlaySoundWidget>(m_project, cmd, this)); break;
		case Cmd::PlayMovie: evt_dialog.reset(make_evt_dialog<PlayMovieWidget>(m_project, cmd, this)); break;
		case Cmd::KeyInputProc: evt_dialog.reset(make_evt_dialog<KeyInputProcWidget>(m_project, cmd, this)); break;
		case Cmd::ChangeMapTileset: evt_dialog.reset(make_evt_dialog<ChangeMapTilesetWidget>(m_project, cmd, this)); break;
		case Cmd::ChangePBG: evt_dialog.reset(make_evt_dialog<ChangePBGWidget>(m_project, cmd, this)); break;
		case Cmd::ChangeEncounterSteps: evt_dialog.reset(make_evt_dialog<ChangeEncounterStepsWidget>(m_project, cmd, this)); break;
		case Cmd::TileSubstitution: evt_dialog.reset(make_evt_dialog<TileSubstitutionWidget>(m_project, cmd, this)); break;
		case Cmd::TeleportTargets: evt_dialog.reset(make_evt_dialog<TeleportTargetsWidget>(m_project, cmd, this)); break;
		case Cmd::ChangeTeleportAccess: evt_dialog.reset(make_evt_dialog<EnableDisableWidget>(m_project, cmd, this)); break;
		case Cmd::EscapeTarget: evt_dialog.reset(make_evt_dialog<EscapeTargetWidget>(m_project, cmd, this)); break;
		case Cmd::ChangeEscapeAccess: evt_dialog.reset(make_evt_dialog<EnableDisableWidget>(m_project, cmd, this)); break;
		case Cmd::ChangeSaveAccess: evt_dialog.reset(make_evt_dialog<EnableDisableWidget>(m_project, cmd, this)); break;
		case Cmd::ChangeMainMenuAccess: evt_dialog.reset(make_evt_dialog<EnableDisableWidget>(m_project, cmd, this)); break;
		case Cmd::ConditionalBranch: evt_dialog.reset(make_complex_evt_dialog<ConditionalBranchWidget>(m_project, evt_lst, this)); break;
		case Cmd::Label: evt_dialog.reset(make_evt_dialog<LabelWidget>(m_project, cmd, this)); break;
		case Cmd::JumpToLabel: evt_dialog.reset(make_evt_dialog<LabelWidget>(m_project, cmd, this)); break;
		case Cmd::CallEvent: evt_dialog.reset(make_evt_dialog<CallEventWidget>(m_project, cmd, this)); break;
		case Cmd::Comment: evt_dialog.reset(make_complex_evt_dialog<CommentWidget>(m_project, evt_lst, this)); break;
		case Cmd::ChangeMonsterHP: evt_dialog.reset(make_evt_dialog<ChangeMonsterHPWidget>(m_project, cmd, this)); break;
		case Cmd::ChangeMonsterMP: evt_dialog.reset(make_evt_dialog<ChangeMonsterMPWidget>(m_project, cmd, this)); break;
		case Cmd::ChangeMonsterCondition: evt_dialog.reset(make_evt_dialog<ChangeMonsterConditionWidget>(m_project, cmd, this)); break;
		case Cmd::ShowHiddenMonster: evt_dialog.reset(make_evt_dialog<ShowHiddenMonsterWidget>(m_project, cmd, this)); break;
		case Cmd::ChangeBattleBG: evt_dialog.reset(make_evt_dialog<ChangeBattleBGWidget>(m_project, cmd, this)); break;
		case Cmd::ShowBattleAnimation_B: evt_dialog.reset(make_evt_dialog<ShowBattleAnimation_BWidget>(m_project, cmd, this)); break;
		case Cmd::ConditionalBranch_B: evt_dialog.reset(make_complex_evt_dialog<ConditionalBranchBattleWidget>(m_project, evt_lst, this)); break;
		//case Cmd::Comment_2: evt_dialog.reset(make_evt_dialog<Comment_2Widget>(m_project, cmd, this)); break;
		case Cmd::Maniac_ShowStringPicture: evt_dialog.reset(make_evt_dialog<ManiacShowStringPictureWidget>(m_project, cmd, this)); break;
		case Cmd::Maniac_ControlStrings: evt_dialog.reset(make_evt_dialog<ManiacControlStringsWidget>(m_project, cmd, this)); break;
		case Cmd::Maniac_GetSaveInfo:
		case Cmd::Maniac_Save:
		case Cmd::Maniac_Load:
		case Cmd::Maniac_GetMousePosition:
		case Cmd::Maniac_SetMousePosition:
		case Cmd::Maniac_GetPictureInfo:
		case Cmd::Maniac_ControlBattle:
		case Cmd::Maniac_ControlAtbGauge:
		case Cmd::Maniac_ChangeBattleCommandEx:
		case Cmd::Maniac_GetBattleInfo:
		case Cmd::Maniac_ControlVarArray:
		case Cmd::Maniac_KeyInputProcEx:
		case Cmd::Maniac_RewriteMap:
		case Cmd::Maniac_ControlGlobalSave:
		case Cmd::Maniac_ChangePictureId:
		case Cmd::Maniac_SetGameOption:
		case Cmd::Maniac_CallCommand:
		case Cmd::Maniac_GetGameInfo:
		case Cmd::Maniac_EditPicture:
		case Cmd::Maniac_WritePicture:
		case Cmd::Maniac_EditTile:
		case Cmd::Maniac_ControlTextProcessing:
		case Cmd::Maniac_Zoom: evt_dialog.reset(make_evt_dialog<ManiacCommandWidget>(m_project, cmd, this)); break;
		default: {
			// No dedicated editor yet: edit the parameters
			auto* wrapper = new WidgetAsDialogWrapper<EventRawWidget, lcf::rpg::EventCommand>(*m_project, cmd, this);
			wrapper->widget()->setShowWarning(!is_new);
			evt_dialog.reset(wrapper);
			break;
		}
	}

	if (auto* wrapper = qobject_cast<WidgetAsDialogWrapperBase*>(evt_dialog.get())) {
		wrapper->setHelpTopic(Help::commandTopic(cmd.code));
		// editors without a title of their own: the command name, as on its button
		if (wrapper->windowTitle().isEmpty()) {
			wrapper->setWindowTitle(EventCommandPalette::commandName(static_cast<Cmd>(cmd.code)));
		}
	}
	return evt_dialog->exec() == QDialog::Accepted;
}

void EventCommandsWidget::addRawEvent(QTreeWidgetItem *item) {
	lcf::rpg::EventCommand cmd;

	auto* wrapper = new WidgetAsDialogWrapper<EventRawWidget, lcf::rpg::EventCommand>(*m_project, cmd, this);
	wrapper->widget()->setShowWarning(false);

	std::unique_ptr<QDialog> dialog;
	dialog.reset(wrapper);

	if (dialog->exec() == QDialog::Accepted) {
		if (!item || indexOf(item) >= static_cast<int>(m_commands->size())) {
			m_commands->push_back(cmd);
		} else {
			m_commands->insert(m_commands->begin() + item->text(1).toInt() + 1, cmd);
		}
		refreshList();
	}
}

void EventCommandsWidget::editRawEvent(QTreeWidgetItem *item, int column, bool show_warning) {
	assert(column == 0);

	int index = indexOf(item);
	auto* wrapper = new WidgetAsDialogWrapper<EventRawWidget, lcf::rpg::EventCommand>(*m_project, (*m_commands)[index], this);
	wrapper->widget()->setShowWarning(show_warning);

	std::unique_ptr<QDialog> dialog;
	dialog.reset(wrapper);

	dialog->exec();
	refreshList();
	selectIndex(index);
}

void EventCommandsWidget::deleteLines(std::pair<int, int> range) {
	if (range.first >= range.second) {
		// a line of the structure of a block: remove the block instead
		return;
	}
	// one change of the list: one undo step
	m_commands->erase(m_commands->begin() + range.first, m_commands->begin() + range.second);
	refreshList();
	selectIndex(range.first);
}

void EventCommandsWidget::insertCommand(int index) {
	using Cmd = lcf::rpg::EventCommand::Code;
	if (!m_commands) {
		return;
	}
	// Insert before the selected line, as RPG Maker does
	if (index < 0 || index > static_cast<int>(m_commands->size())) {
		index = static_cast<int>(m_commands->size());
	}

	const bool rpg2k3 = m_project->database().system.ldb_id == 2003;
	EventCommandPalette palette(rpg2k3, m_battle, this);
	if (palette.exec() != QDialog::Accepted) {
		return;
	}
	Cmd code = palette.selectedCode();

	int indent = index < static_cast<int>(m_commands->size()) ? (*m_commands)[index].indent : 0;
	auto lines = EventCommandDefaults::make(code, indent);
	m_commands->insert(m_commands->begin() + index, lines.begin(), lines.end());
	refreshList();

	// Configure it now; cancelling removes it
	if (!editCommand(index, true)) {
		auto range = EventCommandDefaults::range(*m_commands, index);
		m_commands->erase(m_commands->begin() + range.first, m_commands->begin() + std::max(range.second, range.first + 1));
	}
	refreshList();
	selectIndex(index);
}

void EventCommandsWidget::copyLines(std::pair<int, int> range, bool cut) {
	if (range.first >= range.second) {
		return;
	}
	s_clipboard.assign(m_commands->begin() + range.first, m_commands->begin() + range.second);
	if (cut) {
		deleteLines(range);
	}
}

void EventCommandsWidget::pasteCommands(int index) {
	if (s_clipboard.empty()) {
		return;
	}
	if (index < 0 || index > static_cast<int>(m_commands->size())) {
		index = static_cast<int>(m_commands->size());
	}
	m_commands->insert(m_commands->begin() + index, s_clipboard.begin(), s_clipboard.end());
	refreshList();
	selectIndex(index);
}

int EventCommandsWidget::commandStart(int index) const {
	using Cmd = lcf::rpg::EventCommand::Code;
	while (index > 0 && index < static_cast<int>(m_commands->size())) {
		auto code = static_cast<Cmd>((*m_commands)[index].code);
		if (code != Cmd::ShowMessage_2 && code != Cmd::Comment_2) {
			break;
		}
		--index;
	}
	return index;
}

std::pair<int, int> EventCommandsWidget::commandRange(QTreeWidgetItem* item) const {
	return EventCommandDefaults::range(*m_commands, commandStart(indexOf(item)));
}

void EventCommandsWidget::highlightCommand(QTreeWidgetItem* current) {
	m_highlight = m_selection = {-1, -1};
	if (current && m_commands && multiSelection()) {
		const int line = indexOf(current);
		m_selection = wholeCommands(std::min(m_anchor, line), std::max(m_anchor, line) + 1);
		m_highlight = m_selection.first < m_selection.second ? m_selection : std::make_pair(line, line + 1);
	} else if (current && (current->flags() & Qt::ItemIsSelectable)) {
		m_highlight = m_selection = commandRange(current);
		if (m_highlight.first == m_highlight.second) {
			m_highlight = {indexOf(current), indexOf(current) + 1};
		}
	}
	viewport()->update();
}

std::pair<int, int> EventCommandsWidget::wholeCommands(int first, int last) const {
	using Cmd = lcf::rpg::EventCommand::Code;
	const auto& commands = *m_commands;
	first = std::max(first, 0);
	last = std::min(last, static_cast<int>(commands.size()));
	auto is_end = [&](int i) { return static_cast<Cmd>(commands[i].code) == Cmd::END; };

	// Grow until every line goes with its whole command, like RPG Maker:
	// a line of the structure of a block (else, an option, its end) takes
	// the block with it. The empty lines of a block are left for the end.
	for (bool grown = true; grown;) {
		grown = false;
		for (int i = first; i < last && !grown; ++i) {
			if (is_end(i)) {
				continue;
			}
			auto range = EventCommandDefaults::range(commands, commandStart(i));
			if (range.first == range.second) {
				// the nearest command before it whose block goes past it
				for (int k = i - 1; k >= 0; --k) {
					if (commands[k].indent > commands[i].indent) {
						continue;
					}
					auto block = EventCommandDefaults::range(commands, k);
					if (block.first < block.second && block.second > i) {
						range = block;
						break;
					}
				}
			}
			if (range.first < range.second && (range.first < first || range.second > last)) {
				first = std::min(first, range.first);
				last = std::max(last, range.second);
				grown = true;
			}
		}
	}
	// An empty line at an edge is outside of the blocks selected: not taken
	while (first < last && is_end(first)) {
		++first;
	}
	while (first < last && is_end(last - 1)) {
		--last;
	}
	return {first, last};
}

bool EventCommandsWidget::multiSelection() const {
	return m_anchor >= 0 && currentItem() && m_anchor != indexOf(currentItem());
}

std::pair<int, int> EventCommandsWidget::rangeFor(QTreeWidgetItem* item) const {
	const int line = indexOf(item);
	if (item && (item == currentItem() || (multiSelection() && line >= m_highlight.first && line < m_highlight.second))) {
		return m_selection;
	}
	return commandRange(item);
}

int EventCommandsWidget::insertionIndexFor(QTreeWidgetItem* item) const {
	// before the selection, as RPG Maker does (-1: at the end)
	const int line = indexOf(item);
	if (item && multiSelection() && line >= m_highlight.first && line < m_highlight.second) {
		return m_highlight.first;
	}
	return line;
}

void EventCommandsWidget::selectAllCommands() {
	if (!m_commands || m_commands->empty()) {
		return;
	}
	// From the first line to the empty line ending the page, the current
	// line on the first one: the selection is every command
	m_extending = true;
	setCurrentItem(topLevelItem(0));
	m_extending = false;
	m_anchor = static_cast<int>(m_commands->size());
	highlightCommand(currentItem());
}

void EventCommandsWidget::updateLook() {
	using CommandColors::Kind;
	const auto& settings = EditorSettings::instance();
	m_colors = settings.commandColors();
	m_stripes = settings.commandStripes();

	const QPalette base = palette();
	m_stripe = CommandColors::stripeColor(base);
	for (size_t i = 0; i < m_linePalettes.size(); ++i) {
		const auto kind = static_cast<Kind>(i);
		QPalette line = base;
		if (kind == Kind::End && !m_colors) {
			// the empty lines are dimmed when commands are not colored
			line.setColor(QPalette::Text, base.color(QPalette::Disabled, QPalette::Text));
		} else if (m_colors && kind != Kind::Text && kind != Kind::End) {
			line.setColor(QPalette::Text, CommandColors::color(kind, base));
		}
		m_linePalettes[i] = line;
	}

	// A nested line starts under the text of its parent, after its "<>"
	setIndentation(std::max(8, fontMetrics().horizontalAdvance(QStringLiteral("<>"))));
}

void EventCommandsWidget::resizeEvent(QResizeEvent* event)
{
	QTreeWidget::resizeEvent(event);
	// Lines are as wide as the list at least (wider ones scroll): a click or
	// a double click anywhere on a line hits it, not only on its text
	header()->setMinimumSectionSize(viewport()->width());
	scheduleDelayedItemsLayout();
}

void EventCommandsWidget::changeEvent(QEvent* event) {
	QTreeWidget::changeEvent(event);
	switch (event->type()) {
		case QEvent::PaletteChange:
		case QEvent::FontChange:
		case QEvent::StyleChange:
			// light / dark mode: colors made for the new base
			updateLook();
			viewport()->update();
			break;
		default:
			break;
	}
}

void EventCommandsWidget::drawRow(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
	using CommandColors::Kind;
	QStyleOptionViewItem row = option;
	const int line = indexOf(itemFromIndex(index));
	if (line >= m_highlight.first && line < m_highlight.second) {
		row.state |= QStyle::State_Selected;
	}

	// Every block is open and there is one row per line: the line number
	// is the row, so every other line is tinted, as in RPG Maker 2003
	if (m_stripes && (line & 1)) {
		painter->fillRect(QRect(0, option.rect.top(), viewport()->width(), option.rect.height()), m_stripe);
	}
	if (index.data(Qt::UserRole + 1).toBool() && (row.state & QStyle::State_Selected)) {
		painter->fillRect(QRect(0, option.rect.top(), viewport()->width(), option.rect.height()), palette().highlight());
	}

	Kind kind = Kind::End;
	if (m_commands && line >= 0 && line < static_cast<int>(m_commands->size())) {
		kind = CommandColors::kind((*m_commands)[line].code);
	}
	if (kind != Kind::Text) {
		// the delegate draws the text with the palette's Text (HighlightedText when selected)
		row.palette = m_linePalettes[static_cast<size_t>(kind)];
	}
	QTreeWidget::drawRow(painter, row, index);
}

void EventCommandsWidget::drawBranches(QPainter* painter, const QRect& rect, const QModelIndex& index) const {
	// No expand arrows nor branch lines in the indentation. Its background
	// (selection) is drawn by QTreeView::drawRow before this.
	Q_UNUSED(painter)
	Q_UNUSED(rect)
	Q_UNUSED(index)
}

int EventCommandsWidget::indexOf(QTreeWidgetItem* item) const {
	return item ? item->text(1).toInt() : -1;
}

void EventCommandsWidget::selectIndex(int index) {
	for (QTreeWidgetItemIterator it(this); *it; ++it) {
		if (indexOf(*it) >= index && ((*it)->flags() & Qt::ItemIsSelectable)) {
			setCurrentItem(*it);
			scrollToItem(*it);
			return;
		}
	}
}

void EventCommandsWidget::mousePressEvent(QMouseEvent* event) {
	auto* item = itemAt(event->position().toPoint());
	if (item && !(item->flags() & Qt::ItemIsSelectable)) {
		event->accept();
		return;
	}
	// A right click in the selection keeps it, for its menu
	const int line = indexOf(item);
	if (event->button() == Qt::RightButton && item && multiSelection()
			&& line >= m_highlight.first && line < m_highlight.second) {
		event->accept();
		return;
	}
	// Shift+click selects the commands from the current line to this one
	const bool shift = event->modifiers() & Qt::ShiftModifier;
	m_extending = shift && event->button() == Qt::LeftButton;
	QTreeWidget::mousePressEvent(event);
	m_extending = false;
	// A click on the current line itself (no change) ends a selection too
	if (item && !shift && m_anchor != indexOf(currentItem())) {
		m_anchor = indexOf(currentItem());
		highlightCommand(currentItem());
	}
}

void EventCommandsWidget::mouseMoveEvent(QMouseEvent* event) {
	// Dragging selects from the line pressed, like Shift+click
	m_extending = event->buttons() & Qt::LeftButton;
	QTreeWidget::mouseMoveEvent(event);
	m_extending = false;
}

QModelIndex EventCommandsWidget::moveCursor(CursorAction action, Qt::KeyboardModifiers modifiers) {
	QModelIndex next = QTreeWidget::moveCursor(action, modifiers);
	if (!next.isValid()) {
		return next;
	}
	QTreeWidgetItemIterator it(itemFromIndex(next));
	const bool backwards = action == MoveUp || action == MovePrevious || action == MovePageUp
		|| action == MoveLeft || action == MoveEnd;
	const bool by_command = m_commands && (action == MoveUp || action == MoveDown);
	const int current_start = by_command ? commandStart(indexOf(currentItem())) : -1;
	while (*it) {
		bool selectable = (*it)->flags() & Qt::ItemIsSelectable;
		if (by_command) {
			// Messages and comments highlight as a whole, so arrow keys must
			// leave the whole command, even after clicking a continuation line.
			const int index = indexOf(*it);
			if (index == current_start) {
				selectable = false;
			} else if (index > 0 && index < static_cast<int>(m_commands->size())) {
				using Cmd = lcf::rpg::EventCommand::Code;
				const auto code = static_cast<Cmd>((*m_commands)[index].code);
				if (code == Cmd::ShowMessage_2 || code == Cmd::Comment_2) {
					selectable = false;
				}
			}
		}
		if (selectable) break;
		if (backwards) --it;
		else ++it;
	}
	return *it ? indexFromItem(*it) : currentIndex();
}

void EventCommandsWidget::keyPressEvent(QKeyEvent* event) {
	QTreeWidgetItem* item = currentItem();
	if (event->key() == Qt::Key_O && event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier)
			&& !m_scriptLocation.isEmpty() && ScriptLink::unavailableReason().isEmpty()) {
		openScript();
	} else if (event->matches(QKeySequence::Undo)) {
		m_undoStack->undo();
	} else if (event->matches(QKeySequence::Redo)) {
		m_undoStack->redo();
	} else if (event->matches(QKeySequence::SelectAll)) {
		selectAllCommands();
	} else if (event->matches(QKeySequence::Copy) && item) {
		copyLines(rangeFor(item), false);
	} else if (event->matches(QKeySequence::Cut) && item) {
		copyLines(rangeFor(item), true);
	} else if (event->matches(QKeySequence::Paste)) {
		pasteCommands(insertionIndexFor(item));
	} else if ((event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) && item) {
		deleteLines(rangeFor(item));
	} else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter || event->key() == Qt::Key_Insert) {
		// Enter inserts and Space edits, as in RPG Maker
		insertCommand(insertionIndexFor(item));
	} else if (event->key() == Qt::Key_Space && item) {
		editEvent(item, 0);
	} else {
		// Shift with the arrow keys selects several commands
		m_extending = event->modifiers() & Qt::ShiftModifier;
		QTreeWidget::keyPressEvent(event);
		m_extending = false;
	}
}

void EventCommandsWidget::setScriptLocation(const QStringList& location) {
	m_scriptLocation = location;
}

void EventCommandsWidget::openScript() {
	if (m_scriptLocation.isEmpty()) {
		return;
	}
	QTreeWidgetItem* item = currentItem();
	const int index = item ? indexOf(item) : -1;
	// the line of the command (a message opens at its first line)
	ScriptLink::open(this, m_scriptLocation, index >= 0 && m_commands && index < static_cast<int>(m_commands->size())
		? commandStart(index) : -1);
}

void EventCommandsWidget::showContextMenu(const QPoint& pos) {
	auto* item = this->itemAt(pos);
	using Cmd = lcf::rpg::EventCommand::Code;
	// a line of the selection acts on the whole selection
	const auto range = rangeFor(item);
	const int insert_at = insertionIndexFor(item);
	bool removable = item && range.first < range.second;
	bool editable = item && indexOf(item) < static_cast<int>(m_commands->size())
		&& hasEditor(static_cast<Cmd>((*m_commands)[commandStart(indexOf(item))].code));

	// the keys are read by keyPressEvent: the menu only shows them
	using EditActions::Kind;
	QMenu menu(this);
	auto* undo = m_undoStack->createUndoAction(&menu);
	EditActions::decorate(undo, Kind::Undo);
	auto* redo = m_undoStack->createRedoAction(&menu);
	EditActions::decorate(redo, Kind::Redo);
	menu.addActions({undo, redo});
	menu.addSeparator();
	EditActions::add(menu, Kind::Insert, this, [=, this] { insertCommand(insert_at); });
	if (editable) {
		// Space, as in RPG Maker: Enter inserts
		EditActions::add(menu, Kind::Edit, this, [=, this] { editEvent(item, 0); }, {QKeySequence(Qt::Key_Space)});
	}
	menu.addSeparator();
	EditActions::add(menu, Kind::Cut, this, [=, this] { copyLines(range, true); })->setEnabled(removable);
	EditActions::add(menu, Kind::Copy, this, [=, this] { copyLines(range, false); })->setEnabled(removable);
	EditActions::add(menu, Kind::Paste, this, [=, this] { pasteCommands(insert_at); })->setEnabled(!s_clipboard.empty());
	EditActions::add(menu, Kind::Delete, this, [=, this] { deleteLines(range); })->setEnabled(removable);
	menu.addSeparator();
	EditActions::add(menu, Kind::SelectAll, this, [this] { selectAllCommands(); })
		->setEnabled(m_commands && !m_commands->empty());
	if (!m_scriptLocation.isEmpty()) {
		// greyed without rpgsync, the tooltip says how to set it up
		menu.addSeparator();
		auto* open = EditActions::create(&menu, tr("Open in Script Editor"), {QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O)}, QStringLiteral("code"));
		connect(open, &QAction::triggered, this, [this] { openScript(); });
		const QString reason = ScriptLink::unavailableReason();
		open->setEnabled(reason.isEmpty());
		open->setToolTip(reason.isEmpty() ? tr("Open the script at the line of this command") : reason);
		menu.setToolTipsVisible(true);
		menu.addAction(open);
	}
	menu.addSeparator();
	menu.addAction(tr("Add raw") + "...", this, [=, this] { addRawEvent(item); });
	if (item && indexOf(item) < static_cast<int>(m_commands->size())) {
		menu.addAction(tr("Edit raw") + "...", this, [=, this] { editRawEvent(item, 0, false); });
	}

	EditActions::exec(menu, this, pos);
}

void EventCommandsWidget::refreshList() {
	using Cmd = lcf::rpg::EventCommand::Code;

	clear();
	// the lines change: no selection until one is chosen again
	m_anchor = -1;
	highlightCommand(nullptr);

	// Populate event command list
	int32_t prev_indent = -1;
	std::vector<QTreeWidgetItem*> parent_stack;
	parent_stack.reserve(10);
	int32_t indent = 0;

	for (size_t i = 0; i < m_commands->size(); ++i) {
		auto& cmd = (*m_commands)[i];

		if (i > 0) {
			auto& last_cmd = (*m_commands)[i - 1];
			switch (static_cast<Cmd>(last_cmd.code)) {
				case Cmd::ConditionalBranch:
				case Cmd::ElseBranch:
				case Cmd::ConditionalBranch_B:
				case Cmd::ElseBranch_B:
				case Cmd::Loop:
				case Cmd::ShowChoiceOption:
				case Cmd::VictoryHandler:
				case Cmd::EscapeHandler:
				case Cmd::DefeatHandler:
				case Cmd::Transaction:
				case Cmd::NoTransaction:
				case Cmd::Stay:
				case Cmd::NoStay:
					indent++;
					break;
				case Cmd::END:
					// Some imported pages retain an END at the root. Inserting
					// after it must not index the parent stack with a negative indent.
					indent = std::max(0, indent - 1);
					break;
				default:
					break;
			}
		}
		cmd.indent = indent;

		auto* item = new QTreeWidgetItem({Stringizer::stringize(cmd), QString::number(i)});
		if (static_cast<Cmd>(cmd.code) != Cmd::END && !hasEditor(static_cast<Cmd>(cmd.code))) {
			// Keep structural parents enabled so their editable children stay enabled.
			item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
		}
		if (static_cast<Cmd>(cmd.code) == Cmd::ShowMessage_2 || static_cast<Cmd>(cmd.code) == Cmd::Comment_2) {
			// under the text of the first line, after its "Message: "
			QString first = static_cast<Cmd>(cmd.code) == Cmd::ShowMessage_2
				? Stringizer::tr("Message") : Stringizer::tr("Comment");
			const QFontMetrics metrics(font());
			const int spaces = (metrics.horizontalAdvance(first + ": ") - metrics.horizontalAdvance(QStringLiteral(": ")))
				/ std::max(1, metrics.horizontalAdvance(QChar(' ')));
			item->setText(0, QString(spaces, ' ') + ": " + ToQString(cmd.string));
		}
		if (static_cast<Cmd>(cmd.code) == Cmd::END) {
			// where commands are inserted (its color: drawRow)
			item->setText(0, QStringLiteral("<>"));
		}
		item->setToolTip(0, tr("Line") + ": " + QString::number(i + 1));
		item->setData(0, Qt::UserRole, QVariant::fromValue(reinterpret_cast<void*>(&cmd)));

		if (indent == 0 || prev_indent == -1) {
			addTopLevelItem(item);
			parent_stack = {item};
		} else {
			if (indent != prev_indent) {
				parent_stack.resize(indent + 1);
			}
			if (!parent_stack[indent - 1]) {
				std::cerr << QString("Event Command %1 (Line %2) has bad indent").arg(cmd.code).arg(i+1).toStdString() << "\n";
				for (auto j = indent - 1; j >= 0; --j) {
					if (parent_stack[j]) {
						indent = j + 1;
						break;
					}
				}
				parent_stack.resize(indent + 1);
			}

			parent_stack[indent - 1]->addChild(item);
			parent_stack[indent] = item;
		}

		prev_indent = indent;
	}

	// RPG Maker does not store the empty line ending the page, it only shows it
	auto* last = new QTreeWidgetItem({QStringLiteral("<>"), QString::number(m_commands->size())});
	last->setData(0, Qt::UserRole + 1, true);
	last->setData(0, Qt::UserRole + 2, static_cast<int>(m_commands->size()));
	addTopLevelItem(last);

	expandAll();

	// Every change of the list ends here: remember the list as it was
	if (m_shown && *m_shown != *m_commands) {
		m_undoStack->push(new CommandListChange([this](const auto& commands) { showCommands(commands); },
			std::move(*m_shown), *m_commands));
	}
	m_shown = *m_commands;
}
