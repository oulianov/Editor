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

#include "battle_screen_widget.h"
#include "ui_battle_screen_widget.h"

#include <QButtonGroup>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <algorithm>

#include "battle_animation_widget.h"
#include "common/dbstring.h"
#include "common/image_loader.h"
#include "common/lcf_widget_binding.h"
#include "defines.h"
#include "model/project.h"
#include "model/project_data.h"
#include "ui/picker/picker_backdrop_widget.h"
#include "ui/picker/picker_dialog.h"
#include "ui/common/text_limit.h"

namespace {
constexpr int kScreenWidth = 320;
constexpr int kScreenHeight = 240;
constexpr int kBattlerSize = 48;
// commands a hero chooses; the 7th is always Row
constexpr int kHeroCommandSlots = 6;

/**
 * A hero on the battle screen at their RPG Maker 2003 battle position. The
 * Player draws a front row hero half a battler to the left
 * (Game_Battle::Calculate2k3BattlePosition): row_offset is that shift.
 */
class HeroItem : public QGraphicsPixmapItem {
public:
	enum { Type = UserType + 2 };

	HeroItem(lcf::rpg::Actor& actor, const QPixmap& pixmap, int row_offset) :
		QGraphicsPixmapItem(pixmap), m_actor(actor), m_rowOffset(row_offset)
	{
		setOffset(-pixmap.width() / 2, -pixmap.height() / 2);
		setZValue(1);
		setPos(actor.battle_x - row_offset, actor.battle_y);
		setFlag(ItemSendsGeometryChanges);
	}

	int type() const override {
		return Type;
	}

protected:
	QVariant itemChange(GraphicsItemChange change, const QVariant& value) override {
		if (change == ItemPositionChange) {
			// the Player keeps heroes inside the screen horizontally
			const QPointF p = value.toPointF();
			const int half = static_cast<int>(boundingRect().width()) / 2;
			return QPointF(std::clamp(qRound(p.x()), half, kScreenWidth - half), std::clamp(qRound(p.y()), 0, kScreenHeight));
		}
		if (change == ItemPositionHasChanged && (flags() & ItemIsMovable)) {
			m_actor.battle_x = qRound(pos().x()) + m_rowOffset;
			m_actor.battle_y = qRound(pos().y());
		}
		return QGraphicsPixmapItem::itemChange(change, value);
	}

private:
	lcf::rpg::Actor& m_actor;
	int m_rowOffset;
};

QButtonGroup* makeGroup(QWidget* parent, std::initializer_list<QAbstractButton*> buttons) {
	auto* group = new QButtonGroup(parent);
	int id = 0;
	for (auto* button : buttons) {
		group->addButton(button, id++);
	}
	return group;
}

QString commandText(const lcf::rpg::BattleCommand& command) {
	return QString("%1: %2").arg(command.ID, 4, 10, QChar('0')).arg(ToQString(command.name));
}
}

BattleScreenWidget::BattleScreenWidget(ProjectData& project, QWidget *parent) :
	QWidget(parent),
	ui(new Ui::BattleScreenWidget),
	m_project(project)
{
	ui->setupUi(this);
	TextLimit::mark(ui->lineCommandName, 10);

	auto& battle = project.database().battlecommands;
	const bool is2k3 = project.database().system.ldb_id == 2003;
	ui->labelRpg2003Only->setVisible(!is2k3);
	ui->widgetSettings->setEnabled(is2k3);

	auto bindGroup = [this](std::initializer_list<QAbstractButton*> buttons, int32_t& data) {
		auto* group = makeGroup(this, buttons);
		LcfWidgetBinding::connect<int32_t>(this, group);
		LcfWidgetBinding::bind(group, data);
	};
	bindGroup({ui->radioTraditional, ui->radioAlternative, ui->radioGauge}, battle.battle_type);
	bindGroup({ui->radioWindowLarge, ui->radioWindowSmall}, battle.window_size);
	bindGroup({ui->radioPlacementManual, ui->radioPlacementAutomatic}, battle.placement);
	bindGroup({ui->radioRowFront, ui->radioRowBack}, battle.row);
	LcfWidgetBinding::connect<int32_t>(this, ui->checkTransparent);
	LcfWidgetBinding::bind(ui->checkTransparent, battle.transparency);

	// The hero shown on the battle screen, with a monster group and a
	// background shown behind (both only for the preview, like in RPG Maker)
	for (const auto& troop : project.database().troops) {
		ui->comboTroopShown->addItem(QString("%1: %2").arg(troop.ID, 4, 10, QChar('0')).arg(ToQString(troop.name)), troop.ID);
	}
	ui->graphicsLayout->setReadOnly(true);
	connect(ui->comboTroopShown, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
		auto& troops = m_project.database().troops;
		ui->graphicsLayout->setData(&m_project, index >= 0 && index < static_cast<int>(troops.size())
			? &troops[static_cast<size_t>(index)] : nullptr);
	});
	ui->graphicsLayout->setData(&m_project, project.database().troops.empty() ? nullptr : &project.database().troops.front());
	m_backdrop = ToQString(project.database().system.battletest_background);
	if (!m_backdrop.isEmpty()) {
		ui->graphicsLayout->setBackdrop(ImageLoader::Load(project.project().findFile(BACKDROP, m_backdrop, FileFinder::FileType::Image)));
	}
	connect(ui->pushChangeBackdrop, &QPushButton::clicked, this, &BattleScreenWidget::changeBackdrop);
	connect(ui->listHeroes, &QListWidget::currentRowChanged, this, &BattleScreenWidget::showHero);
	// manual placement moves the heroes; the row moves them on the screen
	connect(ui->radioPlacementManual, &QRadioButton::toggled, this, &BattleScreenWidget::updateHeroItem);
	connect(ui->radioRowFront, &QRadioButton::toggled, this, &BattleScreenWidget::updateHeroItem);
	for (auto* combo : {ui->comboHeroCommand1, ui->comboHeroCommand2, ui->comboHeroCommand3,
			ui->comboHeroCommand4, ui->comboHeroCommand5, ui->comboHeroCommand6}) {
		combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
		combo->setMinimumContentsLength(8);
		connect(combo, qOverload<int>(&QComboBox::currentIndexChanged), this, &BattleScreenWidget::writeHeroCommands);
	}

	m_deathHandler = makeGroup(this, {ui->radioDeathGameOver, ui->radioDeathEvent});
	m_deathHandler->button(battle.death_handler ? 1 : 0)->setChecked(true);
	connect(m_deathHandler, &QButtonGroup::idToggled, this, [this](int id, bool checked) {
		if (checked) {
			// RPG Maker writes both chunks; RPG_RT only reads death_handler
			auto& battle = m_project.database().battlecommands;
			battle.death_handler = battle.death_handler_unused = id == 1;
			updateEnabled();
		}
	});
	ui->comboDeathEvent->makeModel(project);
	LcfWidgetBinding::connect<int32_t>(this, ui->comboDeathEvent);
	LcfWidgetBinding::bind(ui->comboDeathEvent, battle.death_event);

	LcfWidgetBinding::connect<bool>(this, ui->checkDeathTeleport);
	LcfWidgetBinding::bind(ui->checkDeathTeleport, battle.death_teleport);
	connect(ui->checkDeathTeleport, &QCheckBox::toggled, this, &BattleScreenWidget::updateEnabled);
	for (const auto& map : project.treeMap().maps) {
		if (map.ID > 0) {
			ui->comboDeathMap->addItem(QString("%1: %2").arg(map.ID, 4, 10, QChar('0')).arg(ToQString(map.name)), map.ID);
		}
	}
	if (ui->comboDeathMap->findData(battle.death_teleport_id) < 0) {
		ui->comboDeathMap->addItem(tr("Unknown Map (%1)").arg(battle.death_teleport_id), battle.death_teleport_id);
	}
	LcfWidgetBinding::connect<int32_t>(this, ui->comboDeathMap);
	LcfWidgetBinding::bind(ui->comboDeathMap, battle.death_teleport_id);
	for (auto* spin : {ui->spinDeathX, ui->spinDeathY}) {
		LcfWidgetBinding::connect<int32_t>(this, spin);
	}
	LcfWidgetBinding::bind(ui->spinDeathX, battle.death_teleport_x);
	LcfWidgetBinding::bind(ui->spinDeathY, battle.death_teleport_y);
	ui->comboDeathFacing->addItem(tr("Retain Facing"), 0);
	ui->comboDeathFacing->addItem(tr("Up"), 1);
	ui->comboDeathFacing->addItem(tr("Right"), 2);
	ui->comboDeathFacing->addItem(tr("Down"), 3);
	ui->comboDeathFacing->addItem(tr("Left"), 4);
	LcfWidgetBinding::connect<int32_t>(this, ui->comboDeathFacing);
	LcfWidgetBinding::bind(ui->comboDeathFacing, battle.death_teleport_face);

	ui->comboCommandType->addItem(tr("Attack"), lcf::rpg::BattleCommand::Type_attack);
	ui->comboCommandType->addItem(tr("Skill"), lcf::rpg::BattleCommand::Type_skill);
	ui->comboCommandType->addItem(tr("Skill Subset"), lcf::rpg::BattleCommand::Type_subskill);
	ui->comboCommandType->addItem(tr("Defend"), lcf::rpg::BattleCommand::Type_defense);
	ui->comboCommandType->addItem(tr("Item"), lcf::rpg::BattleCommand::Type_item);
	ui->comboCommandType->addItem(tr("Escape"), lcf::rpg::BattleCommand::Type_escape);
	ui->comboCommandType->addItem(tr("Link to Event"), lcf::rpg::BattleCommand::Type_special);
	LcfWidgetBinding::connect(this, ui->lineCommandName);
	LcfWidgetBinding::connect<int32_t>(this, ui->comboCommandType);
	connect(ui->listCommands, &QListWidget::currentRowChanged, this, &BattleScreenWidget::showCommand);
	connect(ui->lineCommandName, &QLineEdit::textChanged, this, [this] {
		const int row = ui->listCommands->currentRow();
		const auto& commands = m_project.database().battlecommands.commands;
		if (row >= 0 && row < static_cast<int>(commands.size())) {
			ui->listCommands->item(row)->setText(commandText(commands[static_cast<size_t>(row)]));
			fillHeroCommands();
		}
	});
	// Commands are referenced by their ID: only the last one can go, like
	// shrinking the list in RPG Maker
	connect(ui->pushAddCommand, &QPushButton::clicked, this, [this] {
		auto& commands = m_project.database().battlecommands.commands;
		lcf::rpg::BattleCommand command;
		command.ID = static_cast<int>(commands.size()) + 1;
		commands.push_back(command);
		fillCommands(command.ID - 1);
	});
	connect(ui->pushRemoveCommand, &QPushButton::clicked, this, [this] {
		auto& commands = m_project.database().battlecommands.commands;
		if (commands.size() > 1) {
			commands.pop_back();
		}
		fillCommands(static_cast<int>(commands.size()) - 1);
	});
	fillCommands(0);

	updateEnabled();
}

BattleScreenWidget::~BattleScreenWidget()
{
	delete ui;
}

void BattleScreenWidget::showEvent(QShowEvent* event)
{
	QWidget::showEvent(event);
	// the heroes and their battle graphics can have changed on other pages
	const int current = std::max(0, ui->listHeroes->currentRow());
	{
		SignalBlocker blocker(ui->listHeroes);
		ui->listHeroes->clear();
		for (const auto& actor : m_project.database().actors) {
			ui->listHeroes->addItem(QString("%1: %2").arg(actor.ID, 4, 10, QChar('0')).arg(ToQString(actor.name)));
		}
		ui->listHeroes->setCurrentRow(std::min(current, ui->listHeroes->count() - 1));
	}
	showHero(ui->listHeroes->currentRow());
}

void BattleScreenWidget::showHero(int)
{
	updateHeroItem();
	fillHeroCommands();
}

void BattleScreenWidget::updateHeroItem()
{
	delete m_heroItem;
	m_heroItem = nullptr;
	auto& database = m_project.database();
	const int row = ui->listHeroes->currentRow();
	if (row < 0 || row >= static_cast<int>(database.actors.size())) {
		return;
	}
	auto& actor = database.actors[static_cast<size_t>(row)];

	// the first frame of the idle pose of the hero's battle graphics
	QPixmap pixmap;
	const int id = actor.battler_animation;
	if (id >= 1 && id <= static_cast<int>(database.battleranimations.size())) {
		const auto& poses = database.battleranimations[static_cast<size_t>(id) - 1].poses;
		if (!poses.empty() && poses.front().animation_type == lcf::rpg::BattlerAnimationPose::AnimType_character) {
			const QPixmap sheet = BattleAnimationWidget::loadGraphic(m_project, BATTLECHARSET, ToQString(poses.front().battler_name));
			if (!sheet.isNull()) {
				pixmap = sheet.copy(0, poses.front().battler_index * kBattlerSize, kBattlerSize, kBattlerSize);
			}
		}
	}
	if (pixmap.isNull()) {
		// still something to place
		pixmap = QPixmap(kBattlerSize, kBattlerSize);
		pixmap.fill(QColor(160, 160, 160, 160));
	}

	const int row_offset = ui->radioRowFront->isChecked() ? pixmap.width() / 2 : 0;
	m_heroItem = new HeroItem(actor, pixmap, row_offset);
	const bool manual = ui->radioPlacementManual->isChecked();
	m_heroItem->setFlag(QGraphicsItem::ItemIsMovable, manual);
	m_heroItem->setCursor(manual ? Qt::OpenHandCursor : Qt::ArrowCursor);
	m_heroItem->setToolTip(manual ? tr("Drag %1 to their battle position").arg(ToQString(actor.name))
		: tr("The automatic placement puts the heroes on the grid of the terrain"));
	ui->graphicsLayout->scene()->addItem(m_heroItem);
}

void BattleScreenWidget::fillHeroCommands()
{
	const auto& database = m_project.database();
	const int row = ui->listHeroes->currentRow();
	const bool valid = row >= 0 && row < static_cast<int>(database.actors.size());
	ui->groupHeroCommands->setEnabled(valid);
	// The commands before the Row command (0), as in the Classes page
	std::vector<int32_t> ids;
	if (valid) {
		for (int32_t id : database.actors[static_cast<size_t>(row)].battle_commands) {
			if (id > 0) {
				ids.push_back(id);
			}
		}
	}
	int slot = 0;
	for (auto* combo : {ui->comboHeroCommand1, ui->comboHeroCommand2, ui->comboHeroCommand3,
			ui->comboHeroCommand4, ui->comboHeroCommand5, ui->comboHeroCommand6}) {
		SignalBlocker blocker(combo);
		combo->clear();
		combo->addItem(tr("<none>"), 0);
		for (const auto& command : database.battlecommands.commands) {
			combo->addItem(commandText(command), command.ID);
		}
		const int id = slot < static_cast<int>(ids.size()) ? ids[static_cast<size_t>(slot)] : 0;
		if (combo->findData(id) < 0) {
			combo->addItem(tr("<%1?>").arg(id), id);
		}
		combo->setCurrentIndex(combo->findData(id));
		++slot;
	}
}

void BattleScreenWidget::writeHeroCommands()
{
	auto& actors = m_project.database().actors;
	const int row = ui->listHeroes->currentRow();
	if (row < 0 || row >= static_cast<int>(actors.size())) {
		return;
	}
	std::vector<int32_t> ids;
	for (auto* combo : {ui->comboHeroCommand1, ui->comboHeroCommand2, ui->comboHeroCommand3,
			ui->comboHeroCommand4, ui->comboHeroCommand5, ui->comboHeroCommand6}) {
		const int id = combo->currentData().toInt();
		if (id > 0) {
			ids.push_back(id);
		}
	}
	// Like RPG Maker: the Row command (0) ends the list, padded to 7 entries with -1
	ids.push_back(0);
	ids.resize(std::max<size_t>(ids.size(), kHeroCommandSlots + 1), -1);
	actors[static_cast<size_t>(row)].battle_commands = ids;
}

void BattleScreenWidget::changeBackdrop()
{
	auto* widget = new PickerBackdropWidget(this);
	PickerDialog dialog(m_project, FileFinder::FileType::Image, widget, this);
	connect(&dialog, &PickerDialog::fileSelected, this, [this](const QString& baseName) {
		m_backdrop = baseName;
		ui->graphicsLayout->setBackdrop(ImageLoader::Load(m_project.project().findFile(BACKDROP, baseName, FileFinder::FileType::Image)));
	});
	dialog.setDirectoryAndFile(BACKDROP, m_backdrop);
	dialog.exec();
}

void BattleScreenWidget::fillCommands(int current_row)
{
	const auto& commands = m_project.database().battlecommands.commands;
	{
		SignalBlocker blocker(ui->listCommands);
		ui->listCommands->clear();
		for (const auto& command : commands) {
			ui->listCommands->addItem(commandText(command));
		}
		ui->listCommands->setCurrentRow(current_row);
	}
	// the list can have moved in memory: bind the editors again
	showCommand(ui->listCommands->currentRow());
	ui->pushRemoveCommand->setEnabled(commands.size() > 1);
	fillHeroCommands();
}

void BattleScreenWidget::showCommand(int row)
{
	auto& commands = m_project.database().battlecommands.commands;
	const bool valid = row >= 0 && row < static_cast<int>(commands.size());
	if (valid) {
		auto& command = commands[static_cast<size_t>(row)];
		LcfWidgetBinding::bind(ui->lineCommandName, command.name);
		LcfWidgetBinding::bind(ui->comboCommandType, command.type);
	} else {
		for (QWidget* editor : std::initializer_list<QWidget*>{ui->lineCommandName, ui->comboCommandType}) {
			editor->setProperty("ee_data", QVariant());
		}
		SignalBlocker blocker({ui->lineCommandName, ui->comboCommandType});
		ui->lineCommandName->clear();
		ui->comboCommandType->setCurrentIndex(-1);
	}
	ui->lineCommandName->setEnabled(valid);
	ui->comboCommandType->setEnabled(valid);
}

void BattleScreenWidget::updateEnabled()
{
	const bool event = ui->radioDeathEvent->isChecked();
	ui->comboDeathEvent->setEnabled(event);
	ui->groupDeathTeleport->setEnabled(event);
	const bool teleport = ui->checkDeathTeleport->isChecked();
	for (QWidget* widget : std::initializer_list<QWidget*>{ui->labelDeathMap, ui->comboDeathMap,
			ui->labelDeathX, ui->spinDeathX, ui->labelDeathY, ui->spinDeathY, ui->labelDeathFacing, ui->comboDeathFacing}) {
		widget->setEnabled(teleport);
	}
}
