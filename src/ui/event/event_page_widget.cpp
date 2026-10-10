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

#include "event_page_widget.h"
#include <QMouseEvent>
#include "ui_event_page_widget.h"
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QDialogButtonBox>
#include <QGraphicsScene>
#include <QMenu>
#include <QVBoxLayout>
#include "ui/viewer/rpg_graphics_view.h"
#include <QScrollBar>
#include <QSettings>
#include <QSplitter>
#include "core.h"
#include "common/editor_settings.h"
#include "defines.h"
#include "ui/picker/picker_charset_widget.h"
#include "ui/picker/picker_dialog.h"
#include "ui/event/move_route_dialog.h"
#include "stringizer.h"
#include "ui/commands/all_commands.h"
#include "common/dbstring.h"
#include "common/lcf_widget_binding.h"

static const char SPLITTER_KEY[] = "event_page_settings_width";

EventPageWidget::EventPageWidget(ProjectData& project, QWidget *parent) :
	QWidget(parent),
	ui(new Ui::QEventWidget),
	m_project(project)
{
	ui->setupUi(this);

	// The settings keep their width and scroll when the window is short,
	// the command list takes the rest of the window
	ui->splitter->setStretchFactor(0, 0);
	ui->splitter->setStretchFactor(1, 1);
	connect(ui->splitter, &QSplitter::splitterMoved, this, [this] {
		QSettings().setValue(SPLITTER_KEY, ui->splitter->sizes().value(0));
	});
	connect(&EditorSettings::instance(), &EditorSettings::localStateReset, this, [this] {
		m_sized = false;
		if (isVisible()) sizeSettings();
	});


	ui->comboVarOperation->addItem(tr("Equal to") + " (==)", lcf::rpg::EventPageCondition::Comparison_equal);
	ui->comboVarOperation->addItem(tr("Greater or equal to") + " (>=)", lcf::rpg::EventPageCondition::Comparison_greater_equal);
	ui->comboVarOperation->addItem(tr("Lesser or equal to") + " (<=)", lcf::rpg::EventPageCondition::Comparison_less_equal);
	ui->comboVarOperation->addItem(tr("Greater than") + " (>)", lcf::rpg::EventPageCondition::Comparison_greater);
	ui->comboVarOperation->addItem(tr("Lesser than") + " (<)", lcf::rpg::EventPageCondition::Comparison_less);
	ui->comboVarOperation->addItem(tr("Not equal to") + " (!=)", lcf::rpg::EventPageCondition::Comparison_not_equal);
	ui->comboVarOperation->setCurrentIndex(1);

	ui->comboMoveType->addItem(tr("Stationary"), lcf::rpg::EventPage::MoveType_stationary);
	ui->comboMoveType->addItem(tr("Random"), lcf::rpg::EventPage::MoveType_random);
	ui->comboMoveType->addItem(tr("Vertical"), lcf::rpg::EventPage::MoveType_vertical);
	ui->comboMoveType->addItem(tr("Horizontal"), lcf::rpg::EventPage::MoveType_horizontal);
	ui->comboMoveType->addItem(tr("Toward Hero"), lcf::rpg::EventPage::MoveType_toward);
	ui->comboMoveType->addItem(tr("Away from Hero"), lcf::rpg::EventPage::MoveType_away);
	ui->comboMoveType->addItem(tr("Custom Pattern"), lcf::rpg::EventPage::MoveType_custom);

	for (int i = 1; i <= 8; i++) {
		ui->comboMoveFrequency->addItem(QString("%1").arg(i), i);
	}
	ui->comboMoveFrequency->setCurrentIndex(2);

	ui->comboCondition->addItem(tr("Action Key"), lcf::rpg::EventPage::Trigger_action);
	ui->comboCondition->addItem(tr("Touched by Hero"), lcf::rpg::EventPage::Trigger_touched);
	ui->comboCondition->addItem(tr("Collision with Hero"), lcf::rpg::EventPage::Trigger_collision);
	ui->comboCondition->addItem(tr("AutoStart"), lcf::rpg::EventPage::Trigger_auto_start);
	ui->comboCondition->addItem(tr("Parallel Process"), lcf::rpg::EventPage::Trigger_parallel);

	ui->comboLayer->addItem(tr("Below Hero"), lcf::rpg::EventPage::Layers_below);
	ui->comboLayer->addItem(tr("Same as Hero"), lcf::rpg::EventPage::Layers_same);
	ui->comboLayer->addItem(tr("Above Hero"), lcf::rpg::EventPage::Layers_above);

	ui->comboAnimationType->addItem(tr("Non-Continuous"), lcf::rpg::EventPage::AnimType_non_continuous);
	ui->comboAnimationType->addItem(tr("Continuous"), lcf::rpg::EventPage::AnimType_continuous);
	ui->comboAnimationType->addItem(tr("Fixed Dir/Non-Continuous"), lcf::rpg::EventPage::AnimType_fixed_non_continuous);
	ui->comboAnimationType->addItem(tr("Fixed Dir/Continuous"), lcf::rpg::EventPage::AnimType_fixed_continuous);
	ui->comboAnimationType->addItem(tr("Fixed Graphic"), lcf::rpg::EventPage::AnimType_fixed_graphic);
	ui->comboAnimationType->addItem(tr("Spin Around"), lcf::rpg::EventPage::AnimType_spin);

	ui->comboMoveSpeed->addItem(tr("1: (1/8) Normal"), lcf::rpg::EventPage::MoveSpeed_eighth);
	ui->comboMoveSpeed->addItem(tr("2: (1/4) Normal"), lcf::rpg::EventPage::MoveSpeed_quarter);
	ui->comboMoveSpeed->addItem(tr("3: (1/2) Normal"), lcf::rpg::EventPage::MoveSpeed_half);
	ui->comboMoveSpeed->addItem(tr("4: Normal"), lcf::rpg::EventPage::MoveSpeed_normal);
	ui->comboMoveSpeed->addItem(tr("5: Normal x 2"), lcf::rpg::EventPage::MoveSpeed_double);
	ui->comboMoveSpeed->addItem(tr("6: Normal x 4"), lcf::rpg::EventPage::MoveSpeed_fourfold);
	ui->comboMoveSpeed->setCurrentIndex(2);

	m_charaItem = new CharSetGraphicsItem(project);
	m_tileItem = new QGraphicsPixmapItem();
	m_scene = new QGraphicsScene(this);
	m_effect = new QGraphicsOpacityEffect(this);
	m_charaItem->setGraphicsEffect(m_effect);
	m_tileItem->setGraphicsEffect(new QGraphicsOpacityEffect(m_effect));
	connect(m_effect,
			SIGNAL(enabledChanged(bool)),
			m_tileItem->graphicsEffect(),
			SLOT(setEnabled(bool)));
	m_charaItem->setScale(2.0);
	m_tileItem->setScale(2.0);
	m_scene->addItem(m_charaItem);
	m_scene->addItem(m_tileItem);
	m_scene->setBackgroundBrush(Core::transparencyBrush());
	ui->graphicsSprite->setScene(m_scene);
	// a click on the sprite chooses its character
	ui->graphicsSprite->viewport()->installEventFilter(this);
	ui->graphicsSprite->viewport()->setCursor(Qt::PointingHandCursor);
	ui->graphicsSprite->setToolTip(tr("Click to choose the character"));

	ui->comboSwitchA->makeModel(project);
	ui->comboSwitchB->makeModel(project);
	ui->comboVariable->makeModel(project);
	ui->comboItem->makeModel(project);
	ui->comboHero->makeModel(project);

	LcfWidgetBinding::connect<bool>(this, ui->checkSwitchA);
	LcfWidgetBinding::connect<int32_t>(this, ui->comboSwitchA);
	LcfWidgetBinding::connect<bool>(this, ui->checkSwitchB);
	LcfWidgetBinding::connect<int32_t>(this, ui->comboSwitchB);
	LcfWidgetBinding::connect<bool>(this, ui->checkVar);
	LcfWidgetBinding::connect<int32_t>(this, ui->comboVariable);
	LcfWidgetBinding::connect<int32_t>(this, ui->comboVarOperation);
	LcfWidgetBinding::connect<int32_t>(this, ui->spinVarValue);
	LcfWidgetBinding::connect<bool>(this, ui->checkItem);
	LcfWidgetBinding::connect<int32_t>(this, ui->comboItem);
	LcfWidgetBinding::connect<bool>(this, ui->checkHero);
	LcfWidgetBinding::connect<int32_t>(this, ui->comboHero);
	LcfWidgetBinding::connect<bool>(this, ui->checkTimerA);
	LcfWidgetBinding::connect<bool>(this, ui->checkTimerB);
	LcfWidgetBinding::connect<bool>(this, ui->checkTransparent);
	LcfWidgetBinding::connect<int32_t>(this, ui->comboMoveType);
	LcfWidgetBinding::connect<int32_t>(this, ui->comboMoveFrequency);
	LcfWidgetBinding::connect<int32_t>(this, ui->comboCondition);
	LcfWidgetBinding::connect<int32_t>(this, ui->comboLayer);
	LcfWidgetBinding::connect<bool>(this, ui->checkOverlapForbidden);
	LcfWidgetBinding::connect<int32_t>(this, ui->comboAnimationType);
	LcfWidgetBinding::connect<int32_t>(this, ui->comboMoveSpeed);
}

EventPageWidget::~EventPageWidget()
{
	delete m_tileItem;
	delete m_charaItem;
	delete ui;
}
lcf::rpg::EventPage *EventPageWidget::eventPage() const
{
	return m_eventPage;
}

void EventPageWidget::setEventPage(lcf::rpg::EventPage *eventPage)
{
	auto& database = m_project.database();
	const bool isRPG2k3 = database.system.ldb_id == 2003;

	ui->spinVarValue->setMinimum(isRPG2k3 ? -9999999 : -999999);
	ui->spinVarValue->setMaximum(isRPG2k3 ? 9999999 : 999999);

	m_eventPage = eventPage;
	LcfWidgetBinding::bind(ui->checkSwitchA, eventPage->condition.flags.switch_a);
	LcfWidgetBinding::bind(ui->comboSwitchA, eventPage->condition.switch_a_id);
	LcfWidgetBinding::bind(ui->checkSwitchB, eventPage->condition.flags.switch_b);
	LcfWidgetBinding::bind(ui->comboSwitchB, eventPage->condition.switch_b_id);
	LcfWidgetBinding::bind(ui->checkVar, eventPage->condition.flags.variable);
	LcfWidgetBinding::bind(ui->comboVariable, eventPage->condition.variable_id);
	LcfWidgetBinding::bind(ui->comboVarOperation, eventPage->condition.compare_operator);
	LcfWidgetBinding::bind(ui->spinVarValue, eventPage->condition.variable_value);
	LcfWidgetBinding::bind(ui->checkItem, eventPage->condition.flags.item);
	LcfWidgetBinding::bind(ui->comboItem, eventPage->condition.item_id);
	LcfWidgetBinding::bind(ui->checkHero, eventPage->condition.flags.actor);
	LcfWidgetBinding::bind(ui->comboHero, eventPage->condition.actor_id);
	LcfWidgetBinding::bind(ui->checkTimerA, eventPage->condition.flags.timer);
	LcfWidgetBinding::bind(ui->checkTimerB, eventPage->condition.flags.timer2);
	LcfWidgetBinding::bind(ui->checkTransparent, eventPage->translucent);
	LcfWidgetBinding::bind(ui->comboMoveType, eventPage->move_type);
	LcfWidgetBinding::bind(ui->comboMoveFrequency, eventPage->move_frequency);
	LcfWidgetBinding::bind(ui->comboCondition, eventPage->trigger);
	LcfWidgetBinding::bind(ui->comboLayer, eventPage->layer);
	LcfWidgetBinding::bind(ui->checkOverlapForbidden, eventPage->overlap_forbidden);
	LcfWidgetBinding::bind(ui->comboAnimationType, eventPage->animation_type);
	LcfWidgetBinding::bind(ui->comboMoveSpeed, eventPage->move_speed);

	ui->spinTimerAMin->setValue(eventPage->condition.timer_sec/60);
	ui->spinTimerASec->setValue(eventPage->condition.timer_sec%60);
	ui->spinTimerBMin->setValue(eventPage->condition.timer2_sec/60);
	ui->spinTimerBSec->setValue(eventPage->condition.timer2_sec%60);

	m_effect->setEnabled(m_eventPage->translucent);
	updateGraphic();

	ui->commands->setData(m_project, eventPage);

	ui->comboSwitchA->setEnabled(ui->checkSwitchA->isChecked());
	ui->comboSwitchB->setEnabled(ui->checkSwitchB->isChecked());
	ui->comboVariable->setEnabled(ui->checkVar->isChecked());
	ui->comboVarOperation->setEnabled(isRPG2k3 && ui->checkVar->isChecked());
	ui->spinVarValue->setEnabled(ui->checkVar->isChecked());
	ui->spinTimerAMin->setEnabled(ui->checkTimerA->isChecked());
	ui->spinTimerASec->setEnabled(ui->checkTimerA->isChecked());
	ui->checkTimerB->setEnabled(isRPG2k3);
	ui->spinTimerBMin->setEnabled(ui->checkTimerB->isChecked());
	ui->spinTimerBSec->setEnabled(ui->checkTimerB->isChecked());
	ui->comboItem->setEnabled(ui->checkItem->isChecked());
	ui->comboHero->setEnabled(ui->checkHero->isChecked());
	ui->label->setEnabled(ui->comboMoveType->currentIndex() != lcf::rpg::EventPage::MoveType_stationary);
	ui->comboMoveFrequency->setEnabled(ui->comboMoveType->currentIndex() != lcf::rpg::EventPage::MoveType_stationary);
	ui->pushMovePattern->setEnabled(ui->comboMoveType->currentIndex() == lcf::rpg::EventPage::MoveType_custom);
}

void EventPageWidget::on_comboMoveType_currentIndexChanged(int index)
{
	ui->label->setEnabled(index != lcf::rpg::EventPage::MoveType_stationary);
	ui->comboMoveFrequency->setEnabled(index != lcf::rpg::EventPage::MoveType_stationary);
	ui->pushMovePattern->setEnabled(index == lcf::rpg::EventPage::MoveType_custom);
}

void EventPageWidget::on_pushMovePattern_clicked()
{
	MoveRouteDialog dialog(m_project, m_eventPage->move_route, this);
	if (dialog.exec() == QDialog::Accepted) {
		m_eventPage->move_route = dialog.route();
	}
}

void EventPageWidget::on_checkVar_toggled(bool checked)
{
	auto& database = m_project.database();
	const bool isRPG2k3 = database.system.ldb_id == 2003;
	ui->comboVarOperation->setEnabled(isRPG2k3 && checked);
}

void EventPageWidget::on_checkTransparent_toggled(bool checked)
{
	m_effect->setEnabled(checked);
}

void EventPageWidget::on_spinTimerAMin_valueChanged(int arg1)
{
	if (!m_eventPage)
		return;
	m_eventPage->condition.timer_sec = arg1*60 + ui->spinTimerASec->value();
}

void EventPageWidget::on_spinTimerASec_valueChanged(int arg1)
{
	if (!m_eventPage)
		return;
	m_eventPage->condition.timer_sec = ui->spinTimerAMin->value()*60 + arg1;
}

void EventPageWidget::on_spinTimerBMin_valueChanged(int arg1)
{
	if (!m_eventPage)
		return;
	m_eventPage->condition.timer2_sec = arg1*60 + ui->spinTimerBSec->value();
}

void EventPageWidget::on_spinTimerBSec_valueChanged(int arg1)
{
	if (!m_eventPage)
		return;
	m_eventPage->condition.timer2_sec = ui->spinTimerBMin->value()*60 + arg1;
}

void EventPageWidget::chooseCharacter()
{
	// the character, its facing and its frame, like RPG Maker
	auto* widget = new PickerCharsetWidget(m_eventPage->character_name.empty() ? 0 : m_eventPage->character_index,
		m_eventPage->character_direction, m_eventPage->character_pattern, this);
	PickerDialog dialog(m_project, FileFinder::FileType::Image, widget, this);
	dialog.setWindowTitle(tr("Select Event Graphic"));
	QObject::connect(&dialog, &PickerDialog::fileSelected, this, [&](const QString& baseName) {
		m_eventPage->character_name = ToDBString(baseName);
		m_eventPage->character_index = widget->index();
		m_eventPage->character_direction = widget->direction();
		m_eventPage->character_pattern = widget->pattern();
		updateGraphic();
	});
	dialog.setDirectoryAndFile(CHARSET, ToQString(m_eventPage->character_name));
	dialog.exec();
}

bool EventPageWidget::eventFilter(QObject* watched, QEvent* event)
{
	if (watched == ui->graphicsSprite->viewport() && event->type() == QEvent::MouseButtonRelease
			&& static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton) {
		chooseCharacter();
		return true;
	}
	return QWidget::eventFilter(watched, event);
}

void EventPageWidget::on_pushSetSprite_clicked()
{
	// A character of a charset, a tile of the upper layer, or nothing
	QMenu menu(this);
	menu.addAction(tr("Character..."), this, &EventPageWidget::chooseCharacter);
	menu.addAction(tr("Tile..."), this, [this] {
		const int tile = pickUpperTile(m_eventPage->character_name.empty() ? m_eventPage->character_index : -1);
		if (tile >= 0) {
			m_eventPage->character_name = lcf::DBString();
			m_eventPage->character_index = tile;
			updateGraphic();
		}
	})->setEnabled(!core().chipsetIsNull());
	menu.addAction(tr("No Graphic"), this, [this] {
		m_eventPage->character_name = lcf::DBString();
		m_eventPage->character_index = 0;
		updateGraphic();
	});
	menu.exec(ui->pushSetSprite->mapToGlobal(QPoint(0, ui->pushSetSprite->height())));
}

int EventPageWidget::pickUpperTile(int current)
{
	// The upper layer tiles of the chipset of the map, as in the palette
	constexpr int columns = 6, count = 144, size = 32;
	const QPixmap tiles = core().paletteTiles(Core::UPPER, size);

	QDialog dialog(this);
	dialog.setWindowTitle(tr("Tile"));
	auto* layout = new QVBoxLayout(&dialog);
	auto* scene = new QGraphicsScene(&dialog);
	scene->setBackgroundBrush(Core::transparencyBrush());
	scene->addPixmap(tiles);
	auto* frame = scene->addRect(0, 0, size, size, QPen(Qt::yellow, 2));
	frame->setVisible(current >= 0);
	if (current >= 0) {
		frame->setPos((current % columns) * size, (current / columns) * size);
	}
	auto* view = new RpgGraphicsViewBase(&dialog);
	view->setScene(scene);
	view->setMinimumSize(columns * size + view->verticalScrollBar()->sizeHint().width() + 4, 6 * size);
	layout->addWidget(view, 1);
	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
	layout->addWidget(buttons);
	connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

	int chosen = current;
	connect(view, &RpgGraphicsViewBase::clicked, &dialog, [&](const QPointF& pos) {
		const int col = static_cast<int>(pos.x()) / size, row = static_cast<int>(pos.y()) / size;
		if (col < 0 || col >= columns || row < 0 || row * columns + col >= count) {
			return;
		}
		chosen = row * columns + col;
		frame->setPos(col * size, row * size);
		frame->setVisible(true);
	});
	if (dialog.exec() != QDialog::Accepted || chosen < 0) {
		return -1;
	}
	return chosen;
}

void EventPageWidget::updateGraphic()
{
	if (m_eventPage->character_name.empty())
	{
		QPixmap pix(16,16);
		pix.fill(QColor(0,0,0,0));
		core().beginPainting(pix);
		core().renderTile(10000 + static_cast<short>(m_eventPage->character_index), QRect(0,0,16,16));
		core().endPainting();
		m_tileItem->setPixmap(pix);
		m_tileItem->setVisible(true);
		m_charaItem->setVisible(false);
		m_scene->setSceneRect(0,0,32,32);
	}
	else
	{
		m_charaItem->refresh(ToQString(m_eventPage->character_name), m_eventPage->character_index);
		m_charaItem->setFrame(m_eventPage->character_pattern);
		m_charaItem->setFacing(m_eventPage->character_direction);
		m_tileItem->setVisible(false);
		m_charaItem->setVisible(true);
		m_scene->setSceneRect(0,0,48,64);
	}
}

void EventPageWidget::showEvent(QShowEvent* event)
{
	QWidget::showEvent(event);
	sizeSettings();
}

void EventPageWidget::sizeSettings()
{
	if (m_sized) {
		return;
	}
	m_sized = true;

	// Now that the combo boxes are filled, size the settings from their content:
	// never squeezed, at most a bit wider than needed, at most half the page
	// to start with. The splitter handle resizes them, and is remembered.
	const int scrollbar = ui->scrollSettings->verticalScrollBar()->sizeHint().width();
	const int minimum = ui->settingsContents->minimumSizeHint().width() + scrollbar;
	const int natural = std::max(minimum, ui->settingsContents->sizeHint().width() + scrollbar);
	ui->scrollSettings->setMinimumWidth(minimum);
	ui->scrollSettings->setMaximumWidth(natural + natural / 2);

	int width = QSettings().value(SPLITTER_KEY, natural).toInt();
	width = std::clamp(width, minimum, std::max(minimum, std::min(natural + natural / 2, ui->splitter->width() / 2)));
	ui->splitter->setSizes({width, std::max(0, ui->splitter->width() - width)});
}
