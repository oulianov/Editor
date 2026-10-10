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

#include "battle_animation_widget.h"
#include "animation_tools.h"
#include "defines.h"
#include "common/dbstring.h"
#include "common/image_loader.h"
#include "model/project.h"
#include "model/project_data.h"
#include "ui/common/rpg_audio_lineedit.h"
#include "ui/picker/picker_dialog.h"
#include <QAction>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include "ui/common/edit_actions.h"
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollBar>
#include <QSpinBox>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QResizeEvent>
#include <algorithm>
#include <cmath>
#include <optional>

namespace {
// the battle screen
constexpr int screen_width = 320;
constexpr int screen_height = 240;
// patterns per row of an animation graphic, as the Player reads them
constexpr int pattern_columns = 5;
// size of the patterns in the list under the preview
constexpr int thumb = 48;
// the step of Use Grid
constexpr int grid_step = 8;

// what Cut and Copy keep, from one animation to another
std::optional<lcf::rpg::AnimationCellData> cell_clipboard;
std::optional<std::vector<lcf::rpg::AnimationCellData>> frame_clipboard;
std::optional<lcf::rpg::AnimationTiming> timing_clipboard;

/** An action of a widget, its shortcut working while the widget has the focus */
QAction* widgetAction(QWidget* widget, const QString& text, const QList<QKeySequence>& keys) {
	auto* action = new QAction(text, widget);
	action->setShortcuts(keys);
	action->setShortcutContext(Qt::WidgetShortcut);
	widget->addAction(action);
	return action;
}

/** IDs are written to the file: they follow the positions */
template <class T>
void renumber(std::vector<T>& items) {
	for (size_t i = 0; i < items.size(); ++i) {
		items[i].ID = static_cast<int>(i) + 1;
	}
}

/** A cel tinted like the Player does: each channel scaled, then the saturation */
QImage toned(const QImage& source, const lcf::rpg::AnimationCellData& cell) {
	if (cell.tone_red == 100 && cell.tone_green == 100 && cell.tone_blue == 100 && cell.tone_gray == 100) {
		return source;
	}
	QImage image = source.convertToFormat(QImage::Format_ARGB32);
	for (int y = 0; y < image.height(); ++y) {
		auto* line = reinterpret_cast<QRgb*>(image.scanLine(y));
		for (int x = 0; x < image.width(); ++x) {
			const QRgb p = line[x];
			int r = qRed(p) * cell.tone_red / 100;
			int g = qGreen(p) * cell.tone_green / 100;
			int b = qBlue(p) * cell.tone_blue / 100;
			const int gray = (r * 299 + g * 587 + b * 114) / 1000;
			r = gray + (r - gray) * cell.tone_gray / 100;
			g = gray + (g - gray) * cell.tone_gray / 100;
			b = gray + (b - gray) * cell.tone_gray / 100;
			line[x] = qRgba(std::clamp(r, 0, 255), std::clamp(g, 0, 255), std::clamp(b, 0, 255), qAlpha(p));
		}
	}
	return image;
}
}

QPixmap BattleAnimationWidget::loadGraphic(const ProjectData& project, const QString& dir, const QString& name) {
	if (name.isEmpty()) {
		return QPixmap();
	}
	const QString path = project.project().findFile(dir, name, FileFinder::FileType::Image);
	if (path.isEmpty()) {
		return QPixmap(); // not in the game nor the RTP
	}
	QImage image(path);
	if (image.isNull()) {
		// XYZ pictures: the loader keeps no palette
		return ImageLoader::Load(path);
	}
	if (image.colorCount() > 0) {
		auto table = image.colorTable();
		table[0] &= 0x00FFFFFF;
		image.setColorTable(table);
	}
	return QPixmap::fromImage(image);
}

BattleAnimationWidget::BattleAnimationWidget(ProjectData& project, QWidget *parent) :
	QWidget(parent), m_project(project)
{
	m_rpg2k3 = project.database().system.ldb_id == 2003;
	auto* layout = new QVBoxLayout(this);

	// general settings
	auto* general = new QGridLayout();
	m_name = new QLineEdit(this);
	m_file = new QLineEdit(this);
	m_file->setReadOnly(true);
	auto* pick = new QToolButton(this);
	pick->setText(QStringLiteral("..."));
	// RPG Maker 2003 picks it in the Large Animations tab of its file chooser
	m_large = new QCheckBox(tr("Large"), this);
	m_large->setObjectName(QStringLiteral("checkLarge"));
	m_large->setToolTip(tr("A large animation: cels of 128x128 pixels, from the Battle2 folder (RPG Maker 2003)"));
	m_large->setEnabled(m_rpg2k3);
	auto* file_row = new QHBoxLayout();
	file_row->addWidget(m_file, 1);
	file_row->addWidget(pick);
	file_row->addWidget(m_large);
	m_scope = new QComboBox(this);
	m_scope->addItem(tr("Local"), static_cast<int>(lcf::rpg::Animation::Scope_target));
	m_scope->addItem(tr("Global"), static_cast<int>(lcf::rpg::Animation::Scope_screen));
	m_scope->setToolTip(tr("Local: on each target. Global: once, on the whole screen."));
	m_position = new QComboBox(this);
	m_position->addItem(tr("High"), static_cast<int>(lcf::rpg::Animation::Position_up));
	m_position->addItem(tr("Center"), static_cast<int>(lcf::rpg::Animation::Position_middle));
	m_position->addItem(tr("Low"), static_cast<int>(lcf::rpg::Animation::Position_down));
	m_target = new QComboBox(this);
	m_target->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
	m_target->setMinimumContentsLength(10);
	m_target->setToolTip(tr("The monster the preview shows the animation on"));
	int first_battler = -1;
	const auto& enemies = project.database().enemies;
	for (size_t i = 0; i < enemies.size(); ++i) {
		m_target->addItem(ToQString(enemies[i].name));
		if (first_battler < 0 && !enemies[i].battler_name.empty()) {
			first_battler = static_cast<int>(i);
		}
	}
	m_target->setCurrentIndex(first_battler);
	general->addWidget(new QLabel(tr("Name"), this), 0, 0);
	general->addWidget(m_name, 0, 1);
	general->addWidget(new QLabel(tr("Animation File"), this), 0, 2);
	general->addLayout(file_row, 0, 3, 1, 3);
	general->addWidget(new QLabel(tr("Scope"), this), 1, 0);
	general->addWidget(m_scope, 1, 1);
	general->addWidget(new QLabel(tr("Y Axis Line"), this), 1, 2);
	general->addWidget(m_position, 1, 3);
	general->addWidget(new QLabel(tr("Target"), this), 1, 4);
	general->addWidget(m_target, 1, 5);
	general->setColumnStretch(1, 1);
	general->setColumnStretch(3, 1);
	general->setColumnStretch(5, 1);
	layout->addLayout(general);

	auto* middle = new QHBoxLayout();
	layout->addLayout(middle, 1);

	// frames
	auto* frames_box = new QGroupBox(tr("Frames"), this);
	auto* frames_layout = new QVBoxLayout(frames_box);
	m_frames = new QListWidget(frames_box);
	m_frames->setObjectName(QStringLiteral("listFrames"));
	// the list takes the width of the buttons
	m_frames->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
	m_frameCount = new QPushButton(tr("Number of Frames..."), frames_box);
	m_clone = new QPushButton(tr("Clone Previous"), frames_box);
	m_clone->setToolTip(tr("Replaces the cels of this frame by the ones of the previous frame"));
	auto* mass_copy = new QPushButton(tr("Mass Copy/Clear..."), frames_box);
	mass_copy->setToolTip(tr("Copies the cels of a range of frames to other frames, or removes them"));
	auto* autocomplete = new QPushButton(tr("Autocomplete..."), frames_box);
	autocomplete->setToolTip(tr("Places the cels between two frames, moving from the first frame to the last"));
	auto* mass_modify = new QPushButton(tr("Mass Modify..."), frames_box);
	mass_modify->setToolTip(tr("Gives a pattern, zoom, transparency or color to cels of a range of frames"));
	frames_layout->addWidget(m_frames, 1);
	frames_layout->addWidget(m_frameCount);
	frames_layout->addWidget(m_clone);
	frames_layout->addWidget(mass_copy);
	frames_layout->addWidget(autocomplete);
	frames_layout->addWidget(mass_modify);
	frames_box->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
	middle->addWidget(frames_box);

	// preview, frame navigation and patterns of the graphic
	auto* center = new QVBoxLayout();
	m_scene = new QGraphicsScene(this);
	m_scene->setSceneRect(0, 0, screen_width, screen_height);
	m_view = new QGraphicsView(m_scene, this);
	m_view->setObjectName(QStringLiteral("viewAnimation"));
	// the keys of the cel menu and the arrows act on the preview
	m_view->setFocusPolicy(Qt::StrongFocus);
	m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_view->viewport()->installEventFilter(this);
	center->addWidget(m_view, 1);
	auto* nav = new QHBoxLayout();
	m_back = new QPushButton(tr("&Back"), this);
	m_next = new QPushButton(tr("&Next"), this);
	m_frameLabel = new QLabel(this);
	m_frameLabel->setObjectName(QStringLiteral("labelFrame"));
	m_frameLabel->setAlignment(Qt::AlignCenter);
	m_play = new QPushButton(tr("Play"), this);
	m_grid = new QCheckBox(tr("Use Grid"), this);
	m_grid->setObjectName(QStringLiteral("checkGrid"));
	m_grid->setToolTip(tr("Cels are placed and moved by steps of %1 pixels").arg(grid_step));
	nav->addWidget(m_back);
	nav->addWidget(m_frameLabel, 1);
	nav->addWidget(m_next);
	nav->addWidget(m_grid);
	nav->addWidget(m_play);
	center->addLayout(nav);
	m_patternScene = new QGraphicsScene(this);
	m_patternView = new QGraphicsView(m_patternScene, this);
	m_patternView->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	m_patternView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_patternView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	// as wide as the preview, whatever the number of patterns
	m_patternView->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
	m_patternView->setFixedHeight(thumb + m_patternView->horizontalScrollBar()->sizeHint().height() + 2 * m_patternView->frameWidth());
	m_patternView->viewport()->installEventFilter(this);
	center->addWidget(m_patternView);
	auto* hint = new QLabel(tr("Click the preview to place a cel of the selected pattern, drag a cel to move it "
		"(with Shift: all the cels), right-click it for its menu. The arrow keys move it."), this);
	hint->setWordWrap(true);
	hint->setForegroundRole(QPalette::PlaceholderText);
	center->addWidget(hint);
	middle->addLayout(center, 1);

	auto* right = new QVBoxLayout();
	middle->addLayout(right);

	// cels of the current frame
	auto* cels_box = new QGroupBox(tr("Cels"), this);
	auto* cels = new QGridLayout(cels_box);
	m_cell = new QComboBox(cels_box);
	m_cell->setObjectName(QStringLiteral("comboCel"));
	m_deleteCell = new QPushButton(tr("Delete"), cels_box);
	cels->addWidget(m_cell, 0, 0, 1, 3);
	cels->addWidget(m_deleteCell, 0, 3);
	auto spin = [cels_box](int min, int max, const QString& suffix) {
		auto* box = new QSpinBox(cels_box);
		box->setRange(min, max);
		box->setSuffix(suffix);
		return box;
	};
	const QString percent = QStringLiteral("%");
	m_cellPattern = spin(1, 999, QString());
	m_cellZoom = spin(0, 999, percent);
	m_cellX = spin(-999, 999, QString());
	m_cellY = spin(-999, 999, QString());
	m_cellTransparency = spin(0, 100, percent);
	m_cellRed = spin(0, 200, percent);
	m_cellGreen = spin(0, 200, percent);
	m_cellBlue = spin(0, 200, percent);
	m_cellSaturation = spin(0, 200, percent);
	// named for scripts
	m_cellPattern->setObjectName(QStringLiteral("spinCelPattern"));
	m_cellX->setObjectName(QStringLiteral("spinCelX"));
	m_cellY->setObjectName(QStringLiteral("spinCelY"));
	m_cellZoom->setObjectName(QStringLiteral("spinCelZoom"));
	m_cellTransparency->setObjectName(QStringLiteral("spinCelTransparency"));
	const std::vector<std::pair<QString, QSpinBox*>> fields = {
		{tr("Pattern"), m_cellPattern}, {tr("Zoom"), m_cellZoom},
		{tr("X"), m_cellX}, {tr("Y"), m_cellY},
		{tr("Red"), m_cellRed}, {tr("Green"), m_cellGreen},
		{tr("Blue"), m_cellBlue}, {tr("Saturation"), m_cellSaturation},
	};
	for (size_t i = 0; i < fields.size(); ++i) {
		const int row = 1 + static_cast<int>(i) / 2;
		const int column = static_cast<int>(i % 2) * 2;
		cels->addWidget(new QLabel(fields[i].first, cels_box), row, column);
		cels->addWidget(fields[i].second, row, column + 1);
	}
	cels->addWidget(new QLabel(tr("Transparency"), cels_box), 5, 0, 1, 3);
	cels->addWidget(m_cellTransparency, 5, 3);
	right->addWidget(cels_box);

	// sound effects, flashes and shakes
	auto* timing_box = new QGroupBox(tr("Effect Timing"), this);
	auto* timing_layout = new QVBoxLayout(timing_box);
	m_timings = new QTreeWidget(timing_box);
	m_timings->setObjectName(QStringLiteral("listTimings"));
	m_timings->setHeaderLabels({tr("Frame"), tr("Sound Effect"), tr("Flash"), tr("Shake")});
	m_timings->setRootIsDecorated(false);
	timing_layout->addWidget(m_timings, 1);
	auto* timing_buttons = new QHBoxLayout();
	auto* add_timing = new QPushButton(tr("Add"), timing_box);
	m_editTiming = new QPushButton(tr("Edit"), timing_box);
	m_deleteTiming = new QPushButton(tr("Delete"), timing_box);
	timing_buttons->addWidget(add_timing);
	timing_buttons->addWidget(m_editTiming);
	timing_buttons->addWidget(m_deleteTiming);
	timing_layout->addLayout(timing_buttons);
	right->addWidget(timing_box, 1);

	m_playTimer = new QTimer(this);
	// the Player shows a frame of an animation for 2 frames at 60 per second
	m_playTimer->setInterval(1000 / 30);

	connect(m_name, &QLineEdit::textEdited, this, [this](const QString& text) {
		if (m_anim) m_anim->name = ToDBString(text);
	});
	connect(pick, &QToolButton::clicked, this, [this] {
		if (!m_anim) return;
		PickerDialog dialog(m_project, FileFinder::FileType::Image, nullptr, this);
		connect(&dialog, &PickerDialog::fileSelected, this, [this](const QString& name) {
			m_anim->animation_name = ToDBString(name);
			m_file->setText(name);
			loadAnimationGraphic();
			redrawPatterns();
			redraw();
		});
		// large animations (128 pixel cels) are in Battle2
		dialog.setDirectoryAndFile(m_anim->large ? BATTLE2 : BATTLE, ToQString(m_anim->animation_name));
		dialog.exec();
	});
	connect(m_scope, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
		if (!m_anim) return;
		m_anim->scope = m_scope->currentData().toInt();
		redraw();
	});
	connect(m_position, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
		if (!m_anim) return;
		m_anim->position = m_position->currentData().toInt();
		redraw();
	});
	connect(m_target, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
		loadTarget();
		redraw();
	});

	connect(m_frames, &QListWidget::currentRowChanged, this, &BattleAnimationWidget::frameChanged);
	connect(m_frameCount, &QPushButton::clicked, this, [this] {
		if (!m_anim) return;
		bool ok = false;
		const int count = QInputDialog::getInt(this, tr("Number of Frames"), tr("Frames"),
			static_cast<int>(m_anim->frames.size()), 1, AnimationTools::kMaxFrames, 1, &ok);
		if (ok) resizeFrames(count);
	});
	connect(m_clone, &QPushButton::clicked, this, [this] {
		const int row = m_frames->currentRow();
		if (!m_anim || row < 1) return;
		m_anim->frames[row].cells = m_anim->frames[row - 1].cells;
		refreshCells(m_cell->currentIndex());
		redraw();
	});
	connect(m_back, &QPushButton::clicked, this, [this] {
		m_frames->setCurrentRow(std::max(0, m_frames->currentRow() - 1));
	});
	connect(m_next, &QPushButton::clicked, this, [this] {
		m_frames->setCurrentRow(std::min(m_frames->count() - 1, m_frames->currentRow() + 1));
	});
	connect(m_play, &QPushButton::clicked, this, &BattleAnimationWidget::togglePlay);
	connect(m_playTimer, &QTimer::timeout, this, [this] {
		const int next = m_frames->currentRow() + 1;
		if (next >= m_frames->count()) {
			togglePlay();
			return;
		}
		m_frames->setCurrentRow(next);
	});

	connect(m_cell, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
		refreshCellFields();
		redraw();
	});
	connect(m_deleteCell, &QPushButton::clicked, this, &BattleAnimationWidget::deleteCell);
	auto bind_cell = [this](QSpinBox* box, int32_t lcf::rpg::AnimationCellData::* field, int shown_offset) {
		connect(box, qOverload<int>(&QSpinBox::valueChanged), this, [this, field, shown_offset](int value) {
			auto* cell = currentCell();
			if (!cell) return;
			cell->*field = value - shown_offset;
			if (field == &lcf::rpg::AnimationCellData::cell_id) {
				m_pattern = cell->cell_id;
				m_cell->setItemText(m_cell->currentIndex(), cellText(m_cell->currentIndex()));
				redrawPatterns();
			}
			redraw();
		});
	};
	// patterns are numbered from 1 like the cels
	bind_cell(m_cellPattern, &lcf::rpg::AnimationCellData::cell_id, 1);
	bind_cell(m_cellX, &lcf::rpg::AnimationCellData::x, 0);
	bind_cell(m_cellY, &lcf::rpg::AnimationCellData::y, 0);
	bind_cell(m_cellZoom, &lcf::rpg::AnimationCellData::zoom, 0);
	bind_cell(m_cellTransparency, &lcf::rpg::AnimationCellData::transparency, 0);
	bind_cell(m_cellRed, &lcf::rpg::AnimationCellData::tone_red, 0);
	bind_cell(m_cellGreen, &lcf::rpg::AnimationCellData::tone_green, 0);
	bind_cell(m_cellBlue, &lcf::rpg::AnimationCellData::tone_blue, 0);
	bind_cell(m_cellSaturation, &lcf::rpg::AnimationCellData::tone_gray, 0);

	connect(add_timing, &QPushButton::clicked, this, [this] { editTiming(-1); });
	connect(m_editTiming, &QPushButton::clicked, this, [this] {
		editTiming(m_timings->indexOfTopLevelItem(m_timings->currentItem()));
	});
	connect(m_timings, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item) {
		editTiming(m_timings->indexOfTopLevelItem(item));
	});
	connect(m_timings, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* item) {
		m_editTiming->setEnabled(item != nullptr);
		m_deleteTiming->setEnabled(item != nullptr);
	});
	connect(m_deleteTiming, &QPushButton::clicked, this, &BattleAnimationWidget::deleteTiming);

	connect(m_large, &QCheckBox::toggled, this, [this](bool large) {
		if (!m_anim || m_anim->large == large) return;
		m_anim->large = large;
		// the same file name, looked for in the other folder
		loadAnimationGraphic();
		redrawPatterns();
		redraw();
	});
	connect(m_grid, &QCheckBox::toggled, this, [this](bool on) {
		m_cellX->setSingleStep(on ? grid_step : 1);
		m_cellY->setSingleStep(on ? grid_step : 1);
	});
	connect(mass_copy, &QPushButton::clicked, this, [this] {
		if (m_anim && AnimationTools::massCopyClear(this, *m_anim, std::max(0, m_frames->currentRow()))) framesEdited();
	});
	connect(autocomplete, &QPushButton::clicked, this, [this] {
		if (m_anim && AnimationTools::autocomplete(this, *m_anim, std::max(0, m_frames->currentRow()))) framesEdited();
	});
	connect(mass_modify, &QPushButton::clicked, this, [this] {
		// without the graphic, any pattern
		const int patterns = patternCount() > 0 ? patternCount() : m_cellPattern->maximum();
		if (m_anim && AnimationTools::massModify(this, *m_anim, std::max(0, m_frames->currentRow()), patterns)) framesEdited();
	});

	// the cel menu of RPG Maker, on the preview
	m_cellMenu = new QMenu(this);
	auto* cel_properties = EditActions::create(m_cellMenu, tr("Cel Properties"), {}, QStringLiteral("properties"));
	m_cellMenu->addAction(cel_properties);
	connect(cel_properties, &QAction::triggered, this, [this] {
		m_cellPattern->setFocus(Qt::OtherFocusReason);
		m_cellPattern->selectAll();
	});
	m_cellMenu->addSeparator();
	using EditActions::Kind;
	auto* cel_cut = EditActions::create(Kind::Cut, m_view);
	auto* cel_copy = EditActions::create(Kind::Copy, m_view);
	auto* cel_paste = EditActions::create(Kind::Paste, m_view);
	auto* cel_delete = EditActions::create(Kind::Delete, m_view);
	auto* cel_up = EditActions::create(m_view, tr("Increment Image ID"), {QKeySequence(Qt::Key_Plus), QKeySequence(Qt::Key_PageUp)});
	auto* cel_down = EditActions::create(m_view, tr("Decrement Image ID"), {QKeySequence(Qt::Key_Minus), QKeySequence(Qt::Key_PageDown)});
	m_cellMenu->addActions({cel_cut, cel_copy, cel_paste, cel_delete});
	m_cellMenu->addSeparator();
	m_cellMenu->addActions({cel_up, cel_down});
	auto copy_cell = [this] {
		auto* cell = currentCell();
		if (cell && cell->valid) cell_clipboard = *cell;
	};
	connect(cel_cut, &QAction::triggered, this, [this, copy_cell] {
		copy_cell();
		deleteCell();
	});
	connect(cel_copy, &QAction::triggered, this, copy_cell);
	connect(cel_paste, &QAction::triggered, this, [this] {
		if (cell_clipboard && !m_playTimer->isActive()) addCell(*cell_clipboard);
	});
	connect(cel_delete, &QAction::triggered, this, &BattleAnimationWidget::deleteCell);
	connect(cel_up, &QAction::triggered, this, [this] { changePattern(1); });
	connect(cel_down, &QAction::triggered, this, [this] { changePattern(-1); });
	connect(m_cellMenu, &QMenu::aboutToShow, this, [this, cel_properties, cel_cut, cel_copy, cel_delete, cel_up, cel_down, cel_paste] {
		auto* cell = currentCell();
		const bool on_cell = cell && cell->valid;
		for (auto* action : {cel_properties, cel_cut, cel_copy, cel_delete, cel_up, cel_down}) {
			action->setEnabled(on_cell);
		}
		cel_paste->setEnabled(cell_clipboard.has_value() && currentFrame());
	});
	// the shortcuts work whatever the menu showed
	connect(m_cellMenu, &QMenu::aboutToHide, this, [this] {
		for (auto* action : m_cellMenu->actions()) action->setEnabled(true);
	});
	// the arrows move the cel by a pixel (a step of the grid), with Shift all the cels
	const std::pair<Qt::Key, QPoint> arrows[] = {
		{Qt::Key_Left, QPoint(-1, 0)}, {Qt::Key_Right, QPoint(1, 0)}, {Qt::Key_Up, QPoint(0, -1)}, {Qt::Key_Down, QPoint(0, 1)},
	};
	for (const auto& [key, direction] : arrows) {
		for (bool all : {false, true}) {
			auto* move = widgetAction(m_view, QString(), {QKeySequence(all ? (Qt::ShiftModifier | key) : QKeyCombination(key))});
			connect(move, &QAction::triggered, this, [this, direction, all] {
				const int step = m_grid->isChecked() ? grid_step : 1;
				moveCells(direction.x() * step, direction.y() * step, all);
			});
		}
	}

	// the frame menu of RPG Maker
	m_frames->setContextMenuPolicy(Qt::CustomContextMenu);
	auto* frame_menu = new QMenu(this);
	// RPG Maker's order: Clear empties the frame, Delete removes it
	auto* frame_copy = EditActions::create(Kind::Copy, m_frames);
	auto* frame_paste = EditActions::create(Kind::Paste, m_frames);
	auto* frame_clear = EditActions::create(Kind::Clear, m_frames);
	auto* frame_cut = EditActions::create(Kind::Cut, m_frames);
	// a blank frame, no window: no "..."
	auto* frame_insert = EditActions::create(Kind::Insert, m_frames, {QKeySequence(Qt::Key_Insert)});
	frame_insert->setText(tr("Insert"));
	auto* frame_delete = EditActions::create(Kind::Delete, m_frames, {QKeySequence(Qt::ShiftModifier | Qt::Key_Delete)});
	frame_menu->addActions({frame_copy, frame_paste, frame_clear, frame_cut});
	frame_menu->addSeparator();
	frame_menu->addActions({frame_insert, frame_delete});
	connect(frame_copy, &QAction::triggered, this, [this] {
		if (auto* frame = currentFrame()) frame_clipboard = frame->cells;
	});
	connect(frame_paste, &QAction::triggered, this, [this] {
		auto* frame = currentFrame();
		if (!frame || !frame_clipboard) return;
		frame->cells = *frame_clipboard;
		framesEdited();
	});
	connect(frame_clear, &QAction::triggered, this, [this] {
		auto* frame = currentFrame();
		if (!frame) return;
		frame->cells.clear();
		framesEdited();
	});
	connect(frame_cut, &QAction::triggered, this, [frame_copy, frame_clear] {
		frame_copy->trigger();
		frame_clear->trigger();
	});
	connect(frame_insert, &QAction::triggered, this, &BattleAnimationWidget::insertFrame);
	connect(frame_delete, &QAction::triggered, this, &BattleAnimationWidget::deleteFrame);
	connect(m_frames, &QWidget::customContextMenuRequested, this, [this, frame_menu, frame_paste, frame_insert, frame_delete](const QPoint& pos) {
		const int count = m_anim ? static_cast<int>(m_anim->frames.size()) : 0;
		frame_paste->setEnabled(frame_clipboard.has_value());
		frame_insert->setEnabled(count < AnimationTools::kMaxFrames);
		frame_delete->setEnabled(count > 1);
		EditActions::exec(*frame_menu, m_frames, pos);
	});

	// the effect timing menu of RPG Maker
	m_timings->setContextMenuPolicy(Qt::CustomContextMenu);
	auto* timing_menu = new QMenu(this);
	auto* timing_edit = EditActions::create(Kind::Edit, m_timings);
	auto* timing_cut = EditActions::create(Kind::Cut, m_timings);
	auto* timing_copy = EditActions::create(Kind::Copy, m_timings);
	auto* timing_paste = EditActions::create(Kind::Paste, m_timings);
	auto* timing_delete = EditActions::create(Kind::Delete, m_timings);
	timing_menu->addAction(timing_edit);
	timing_menu->addSeparator();
	timing_menu->addActions({timing_cut, timing_copy, timing_paste, timing_delete});
	connect(timing_edit, &QAction::triggered, this, [this] { editTiming(currentTiming()); });
	connect(timing_copy, &QAction::triggered, this, [this] {
		const int index = currentTiming();
		if (m_anim && index >= 0) timing_clipboard = m_anim->timings[index];
	});
	connect(timing_cut, &QAction::triggered, this, [this, timing_copy] {
		timing_copy->trigger();
		deleteTiming();
	});
	connect(timing_paste, &QAction::triggered, this, &BattleAnimationWidget::pasteTiming);
	connect(timing_delete, &QAction::triggered, this, &BattleAnimationWidget::deleteTiming);
	connect(m_timings, &QWidget::customContextMenuRequested, this, [this, timing_menu, timing_cut, timing_copy, timing_paste, timing_delete](const QPoint& pos) {
		const bool on_timing = currentTiming() >= 0;
		for (auto* action : {timing_cut, timing_copy, timing_delete}) action->setEnabled(on_timing);
		timing_paste->setEnabled(timing_clipboard.has_value());
		EditActions::exec(*timing_menu, m_timings, pos);
	});

	loadTarget();
	setData(nullptr);
}

void BattleAnimationWidget::setData(lcf::rpg::Animation* anim) {
	if (m_playTimer->isActive()) {
		togglePlay();
	}
	m_anim = anim;
	m_dragging = false;
	setEnabled(anim != nullptr);

	m_name->setText(anim ? ToQString(anim->name) : QString());
	m_file->setText(anim ? ToQString(anim->animation_name) : QString());
	{
		const QSignalBlocker block_scope(m_scope);
		const QSignalBlocker block_position(m_position);
		const QSignalBlocker block_large(m_large);
		m_scope->setCurrentIndex(anim ? m_scope->findData(anim->scope) : -1);
		m_position->setCurrentIndex(anim ? m_position->findData(anim->position) : -1);
		m_large->setChecked(anim && anim->large);
	}
	loadAnimationGraphic();
	refreshFrames(0);
	refreshTimings(0);
	redrawPatterns();
}

lcf::rpg::AnimationFrame* BattleAnimationWidget::currentFrame() {
	const int row = m_frames->currentRow();
	if (!m_anim || row < 0 || row >= static_cast<int>(m_anim->frames.size())) {
		return nullptr;
	}
	return &m_anim->frames[row];
}

lcf::rpg::AnimationCellData* BattleAnimationWidget::currentCell() {
	auto* frame = currentFrame();
	const int index = m_cell->currentIndex();
	if (!frame || index < 0 || index >= static_cast<int>(frame->cells.size())) {
		return nullptr;
	}
	return &frame->cells[index];
}

int BattleAnimationWidget::cellSize() const {
	return m_anim && m_anim->large ? 128 : 96;
}

int BattleAnimationWidget::patternCount() const {
	if (m_graphic.isNull()) {
		return 0;
	}
	return pattern_columns * (m_graphic.height() / cellSize());
}

QPointF BattleAnimationWidget::origin() const {
	// as the Player places an animation in battle
	if (m_anim && m_anim->scope == lcf::rpg::Animation::Scope_screen) {
		return QPointF(screen_width / 2, screen_height / 3);
	}
	const int height = m_battler.isNull() ? cellSize() / 2 : m_battler.height();
	int offset = 0;
	if (m_anim && m_anim->position == lcf::rpg::Animation::Position_down) {
		offset = height / 2;
	} else if (m_anim && m_anim->position == lcf::rpg::Animation::Position_up) {
		offset = -(height / 2);
	}
	return QPointF(screen_width / 2, screen_height / 2 + offset);
}

QRectF BattleAnimationWidget::cellRect(const lcf::rpg::AnimationCellData& cell) const {
	const double size = cellSize() * cell.zoom / 100.0;
	const QPointF center = origin() + QPointF(cell.x, cell.y);
	return QRectF(center.x() - size / 2, center.y() - size / 2, size, size);
}

QString BattleAnimationWidget::cellText(int index) const {
	const int row = m_frames->currentRow();
	if (!m_anim || row < 0 || row >= static_cast<int>(m_anim->frames.size())) {
		return QString();
	}
	const auto& cell = m_anim->frames[row].cells[index];
	if (!cell.valid) {
		return tr("Cel %1: unused").arg(index + 1);
	}
	return tr("Cel %1: pattern %2").arg(index + 1).arg(cell.cell_id + 1);
}

void BattleAnimationWidget::loadAnimationGraphic() {
	m_graphic = QImage();
	if (m_anim) {
		m_graphic = loadGraphic(m_project, m_anim->large ? BATTLE2 : BATTLE, ToQString(m_anim->animation_name)).toImage();
	}
}

void BattleAnimationWidget::loadTarget() {
	const auto& enemies = m_project.database().enemies;
	const int index = m_target->currentIndex();
	m_battler = QPixmap();
	if (index >= 0 && index < static_cast<int>(enemies.size())) {
		m_battler = loadGraphic(m_project, MONSTER, ToQString(enemies[index].battler_name));
	}
}

void BattleAnimationWidget::refreshFrames(int select) {
	const int count = m_anim ? static_cast<int>(m_anim->frames.size()) : 0;
	{
		const QSignalBlocker block_frames(m_frames);
		m_frames->clear();
		for (int i = 0; i < count; ++i) {
			m_frames->addItem(QStringLiteral("%1").arg(i + 1, 3, 10, QLatin1Char('0')));
		}
		m_frames->setCurrentRow(count > 0 ? std::clamp(select, 0, count - 1) : -1);
	}
	frameChanged();
}

void BattleAnimationWidget::frameChanged() {
	const int row = m_frames->currentRow();
	const int count = m_frames->count();
	m_frameLabel->setText(count > 0 ? tr("Frame %1 / %2").arg(row + 1).arg(count) : QString());
	m_back->setEnabled(row > 0);
	m_next->setEnabled(row + 1 < count);
	m_clone->setEnabled(row > 0);
	// the same cel stays selected from frame to frame
	refreshCells(m_cell->currentIndex());
	redraw();
}

void BattleAnimationWidget::resizeFrames(int count) {
	if (!m_anim) return;
	const int row = m_frames->currentRow();
	m_anim->frames.resize(count);
	renumber(m_anim->frames);
	refreshFrames(std::min(row, count - 1));
}

void BattleAnimationWidget::refreshCells(int select) {
	auto* frame = currentFrame();
	const int count = frame ? static_cast<int>(frame->cells.size()) : 0;
	{
		const QSignalBlocker block(m_cell);
		m_cell->clear();
		for (int i = 0; i < count; ++i) {
			m_cell->addItem(cellText(i));
		}
		if (select < 0 || select >= count || !frame->cells[select].valid) {
			select = -1;
			for (int i = 0; i < count && select < 0; ++i) {
				if (frame->cells[i].valid) select = i;
			}
		}
		m_cell->setCurrentIndex(select);
	}
	refreshCellFields();
}

void BattleAnimationWidget::refreshCellFields() {
	auto* cell = currentCell();
	const bool editable = cell && cell->valid;
	const std::vector<std::pair<QSpinBox*, int>> fields = {
		{m_cellPattern, editable ? cell->cell_id + 1 : 1},
		{m_cellX, editable ? cell->x : 0},
		{m_cellY, editable ? cell->y : 0},
		{m_cellZoom, editable ? cell->zoom : 100},
		{m_cellTransparency, editable ? cell->transparency : 0},
		{m_cellRed, editable ? cell->tone_red : 100},
		{m_cellGreen, editable ? cell->tone_green : 100},
		{m_cellBlue, editable ? cell->tone_blue : 100},
		{m_cellSaturation, editable ? cell->tone_gray : 100},
	};
	for (const auto& [box, value] : fields) {
		const QSignalBlocker block(box);
		box->setValue(value);
		box->setEnabled(editable);
	}
	m_deleteCell->setEnabled(editable);
	if (editable && m_pattern != cell->cell_id) {
		m_pattern = cell->cell_id;
		redrawPatterns();
	}
}

void BattleAnimationWidget::deleteCell() {
	auto* frame = currentFrame();
	auto* cell = currentCell();
	if (!frame || !cell) return;
	// A deleted cel leaves an unused slot, so the next cels keep their number
	// (RPG Maker does the same); unused slots at the end are dropped.
	cell->valid = 0;
	auto& cells = frame->cells;
	while (!cells.empty() && !cells.back().valid) {
		cells.pop_back();
	}
	refreshCells(-1);
	redraw();
}

void BattleAnimationWidget::addCell(const lcf::rpg::AnimationCellData& data) {
	auto* frame = currentFrame();
	if (!frame) return;
	auto& cells = frame->cells;
	size_t slot = 0;
	while (slot < cells.size() && cells[slot].valid) {
		++slot;
	}
	if (slot == cells.size()) {
		cells.emplace_back();
	}
	cells[slot] = data;
	cells[slot].ID = static_cast<int>(slot) + 1;
	cells[slot].valid = 1;
	refreshCells(static_cast<int>(slot));
	redraw();
}

void BattleAnimationWidget::changePattern(int step) {
	auto* cell = currentCell();
	if (!cell || !cell->valid) return;
	const int last = patternCount() > 0 ? patternCount() : m_cellPattern->maximum();
	// through the field: it updates the list, the patterns and the preview
	m_cellPattern->setValue(std::clamp(cell->cell_id + 1 + step, 1, last));
}

void BattleAnimationWidget::moveCells(int dx, int dy, bool all) {
	auto* frame = currentFrame();
	auto* current = currentCell();
	if (!frame || m_playTimer->isActive() || (!all && (!current || !current->valid))) return;
	// with the grid, to its next line in the direction of the move
	auto step = [this](int value, int delta) {
		if (!m_grid->isChecked() || delta == 0) return value + delta;
		const double target = double(value + delta) / grid_step;
		return static_cast<int>(delta > 0 ? std::floor(target) : std::ceil(target)) * grid_step;
	};
	for (auto& cell : frame->cells) {
		if (cell.valid && (all || &cell == current)) {
			cell.x = step(cell.x, dx);
			cell.y = step(cell.y, dy);
		}
	}
	refreshCellFields();
	redraw();
}

int BattleAnimationWidget::cellAt(const QPointF& pos, bool behind) const {
	const int row = m_frames->currentRow();
	if (!m_anim || row < 0 || row >= static_cast<int>(m_anim->frames.size())) {
		return -1;
	}
	// the cels under the point, the last drawn (on top) first
	const auto& cells = m_anim->frames[row].cells;
	std::vector<int> hits;
	for (int i = static_cast<int>(cells.size()) - 1; i >= 0; --i) {
		if (cells[i].valid && cellRect(cells[i]).contains(pos)) hits.push_back(i);
	}
	if (hits.empty()) {
		return -1;
	}
	auto current = std::find(hits.begin(), hits.end(), m_cell->currentIndex());
	if (behind && current != hits.end()) {
		++current;
		return current == hits.end() ? hits.front() : *current;
	}
	return hits.front();
}

int BattleAnimationWidget::snap(double value) const {
	if (m_grid->isChecked()) {
		return static_cast<int>(std::lround(value / grid_step)) * grid_step;
	}
	return static_cast<int>(std::lround(value));
}

void BattleAnimationWidget::insertFrame() {
	if (!m_anim || static_cast<int>(m_anim->frames.size()) >= AnimationTools::kMaxFrames) return;
	const int row = std::max(0, m_frames->currentRow());
	m_anim->frames.insert(m_anim->frames.begin() + row, lcf::rpg::AnimationFrame());
	renumber(m_anim->frames);
	// the effects stay on their frames, now one further
	for (auto& timing : m_anim->timings) {
		if (timing.frame > row) ++timing.frame;
	}
	refreshFrames(row);
	refreshTimings(currentTiming());
}

void BattleAnimationWidget::deleteFrame() {
	const int row = m_frames->currentRow();
	if (!m_anim || m_anim->frames.size() <= 1 || row < 0) return;
	m_anim->frames.erase(m_anim->frames.begin() + row);
	renumber(m_anim->frames);
	// the effects of the next frames stay on them
	for (auto& timing : m_anim->timings) {
		if (timing.frame > row + 1) --timing.frame;
	}
	refreshFrames(row);
	refreshTimings(currentTiming());
}

void BattleAnimationWidget::framesEdited() {
	refreshFrames(m_frames->currentRow());
}

int BattleAnimationWidget::currentTiming() const {
	return m_timings->indexOfTopLevelItem(m_timings->currentItem());
}

void BattleAnimationWidget::pasteTiming() {
	if (!m_anim || !timing_clipboard) return;
	auto timing = *timing_clipboard;
	// on the frame shown, like a new effect
	timing.frame = std::max(1, m_frames->currentRow() + 1);
	putTiming(timing, -1);
}

void BattleAnimationWidget::deleteTiming() {
	const int index = currentTiming();
	if (!m_anim || index < 0) return;
	m_anim->timings.erase(m_anim->timings.begin() + index);
	renumber(m_anim->timings);
	refreshTimings(index);
}

void BattleAnimationWidget::refreshTimings(int select) {
	const QSignalBlocker block(m_timings);
	m_timings->clear();
	const QString scopes[] = {tr("None"), tr("Target"), tr("Screen")};
	auto scope = [&](int value) {
		return value >= 0 && value < 3 ? scopes[value] : QString::number(value);
	};
	if (m_anim) {
		for (const auto& timing : m_anim->timings) {
			QString sound = QString::fromStdString(timing.se.name);
			if (sound.isEmpty()) {
				sound = QStringLiteral("(OFF)");
			}
			QString flash = scope(timing.flash_scope);
			if (timing.flash_scope != lcf::rpg::AnimationTiming::FlashScope_nothing) {
				flash = tr("%1 (%2, %3, %4, %5)").arg(flash).arg(timing.flash_red).arg(timing.flash_green)
					.arg(timing.flash_blue).arg(timing.flash_power);
			}
			m_timings->addTopLevelItem(new QTreeWidgetItem({QString::number(timing.frame), sound, flash, scope(timing.screen_shake)}));
		}
	}
	const int count = m_timings->topLevelItemCount();
	auto* current = count > 0 ? m_timings->topLevelItem(std::clamp(select, 0, count - 1)) : nullptr;
	m_timings->setCurrentItem(current);
	m_editTiming->setEnabled(current != nullptr);
	m_deleteTiming->setEnabled(current != nullptr);
}

void BattleAnimationWidget::editTiming(int index) {
	if (!m_anim) return;
	auto& timings = m_anim->timings;
	lcf::rpg::AnimationTiming timing;
	if (index >= 0 && index < static_cast<int>(timings.size())) {
		timing = timings[index];
	} else {
		index = -1;
		timing.frame = std::max(1, m_frames->currentRow() + 1);
	}
	if (timingDialog(timing)) {
		putTiming(timing, index);
	}
}

void BattleAnimationWidget::putTiming(const lcf::rpg::AnimationTiming& timing, int index) {
	auto& timings = m_anim->timings;
	if (index < 0) {
		timings.push_back(timing);
		index = static_cast<int>(timings.size()) - 1;
	} else {
		timings[index] = timing;
	}
	// marked to be selected again once sorted (the IDs are renumbered)
	timings[index].ID = -1;
	// listed by frame, like RPG Maker does
	std::stable_sort(timings.begin(), timings.end(), [](const auto& a, const auto& b) { return a.frame < b.frame; });
	int select = 0;
	for (size_t i = 0; i < timings.size(); ++i) {
		if (timings[i].ID == -1) select = static_cast<int>(i);
	}
	renumber(timings);
	refreshTimings(select);
}

bool BattleAnimationWidget::timingDialog(lcf::rpg::AnimationTiming& timing) {
	QDialog dialog(this);
	dialog.setWindowTitle(tr("Animation Effect Timing"));
	auto* layout = new QVBoxLayout(&dialog);

	auto* form = new QFormLayout();
	auto* frame = new QSpinBox(&dialog);
	frame->setRange(1, std::max(1, static_cast<int>(m_anim->frames.size())));
	frame->setValue(timing.frame);
	form->addRow(tr("Frame"), frame);
	lcf::rpg::Sound se = timing.se;
	auto* sound = new RpgSoundLineEdit(&dialog);
	sound->makeModel(m_project);
	sound->bindSound(se);
	form->addRow(tr("Sound Effect"), sound);
	layout->addLayout(form);

	auto scope_buttons = [&dialog, this](QWidget* parent, QBoxLayout* row, int value) {
		auto* group = new QButtonGroup(&dialog);
		const QString names[] = {tr("None"), tr("Target"), tr("Screen")};
		for (int i = 0; i < 3; ++i) {
			auto* radio = new QRadioButton(names[i], parent);
			group->addButton(radio, i);
			row->addWidget(radio);
		}
		if (auto* button = group->button(value)) button->setChecked(true);
		return group;
	};

	auto* flash_box = new QGroupBox(tr("Flash"), &dialog);
	auto* flash_form = new QFormLayout(flash_box);
	auto* flash_row = new QHBoxLayout();
	auto* flash_scope = scope_buttons(flash_box, flash_row, timing.flash_scope);
	flash_form->addRow(tr("Flash Scope"), flash_row);
	auto* colors = new QWidget(flash_box);
	auto* colors_form = new QFormLayout(colors);
	colors_form->setContentsMargins(0, 0, 0, 0);
	QSpinBox* channels[4];
	const QString channel_names[] = {tr("Red"), tr("Green"), tr("Blue"), tr("Strength")};
	const int values[] = {timing.flash_red, timing.flash_green, timing.flash_blue, timing.flash_power};
	for (int i = 0; i < 4; ++i) {
		channels[i] = new QSpinBox(colors);
		channels[i]->setRange(0, 31);
		channels[i]->setValue(values[i]);
		colors_form->addRow(channel_names[i], channels[i]);
	}
	auto* sample = new QLabel(colors);
	sample->setAutoFillBackground(true);
	sample->setFrameShape(QFrame::Box);
	sample->setMinimumHeight(2 * fontMetrics().height());
	colors_form->addRow(tr("Sample"), sample);
	flash_form->addRow(colors);
	layout->addWidget(flash_box);
	auto update_flash = [&] {
		colors->setEnabled(flash_scope->checkedId() != lcf::rpg::AnimationTiming::FlashScope_nothing);
		// the flash color over black, at its strength
		const double strength = channels[3]->value() / 31.0;
		QPalette palette = sample->palette();
		palette.setColor(QPalette::Window, QColor::fromRgbF(
			channels[0]->value() / 31.0 * strength, channels[1]->value() / 31.0 * strength, channels[2]->value() / 31.0 * strength));
		sample->setPalette(palette);
	};
	for (auto* box : channels) {
		connect(box, qOverload<int>(&QSpinBox::valueChanged), &dialog, update_flash);
	}
	connect(flash_scope, &QButtonGroup::idClicked, &dialog, update_flash);
	update_flash();

	// shaking is new in RPG Maker 2003
	auto* shake_box = new QGroupBox(tr("Shake Scope"), &dialog);
	auto* shake_row = new QHBoxLayout(shake_box);
	auto* shake = scope_buttons(shake_box, shake_row, timing.screen_shake);
	shake_box->setEnabled(m_rpg2k3);
	layout->addWidget(shake_box);

	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
	connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	layout->addWidget(buttons);

	if (dialog.exec() != QDialog::Accepted) {
		return false;
	}
	timing.frame = frame->value();
	timing.se = se;
	timing.flash_scope = std::max(0, flash_scope->checkedId());
	timing.flash_red = channels[0]->value();
	timing.flash_green = channels[1]->value();
	timing.flash_blue = channels[2]->value();
	timing.flash_power = channels[3]->value();
	if (shake->checkedId() >= 0) {
		timing.screen_shake = shake->checkedId();
	}
	return true;
}

void BattleAnimationWidget::togglePlay() {
	if (m_playTimer->isActive()) {
		m_playTimer->stop();
		m_play->setText(tr("Play"));
		redraw();
		return;
	}
	if (m_frames->count() == 0) return;
	m_play->setText(tr("Stop"));
	m_playTimer->start();
	m_frames->setCurrentRow(0);
}

void BattleAnimationWidget::redraw() {
	m_scene->clear();
	if (!m_anim) {
		return;
	}
	QPixmap canvas(screen_width, screen_height);
	canvas.fill(Qt::black);
	QPainter painter(&canvas);
	if (!m_battler.isNull()) {
		painter.drawPixmap((screen_width - m_battler.width()) / 2, (screen_height - m_battler.height()) / 2, m_battler);
	}

	const int size = cellSize();
	if (auto* frame = currentFrame()) {
		for (const auto& cell : frame->cells) {
			if (!cell.valid) continue;
			const QRectF target = cellRect(cell);
			painter.setOpacity((100 - cell.transparency) / 100.0);
			if (cell.cell_id < patternCount()) {
				const QRect source((cell.cell_id % pattern_columns) * size, (cell.cell_id / pattern_columns) * size, size, size);
				painter.drawImage(target, toned(m_graphic.copy(source), cell));
			} else {
				// no graphic for this pattern: its place is still shown
				painter.fillRect(target, QColor(255, 255, 255, 60));
			}
		}
	}
	painter.setOpacity(1.0);

	// where the cel coordinates start from
	const QPointF o = origin();
	painter.setPen(QColor(255, 255, 255, 100));
	painter.drawLine(o - QPointF(6, 0), o + QPointF(6, 0));
	painter.drawLine(o - QPointF(0, 6), o + QPointF(0, 6));

	auto* cell = currentCell();
	if (cell && cell->valid && !m_playTimer->isActive()) {
		painter.setPen(QPen(Qt::white, 1, Qt::DashLine));
		painter.drawRect(cellRect(*cell));
	}
	painter.end();
	m_scene->addPixmap(canvas);
}

void BattleAnimationWidget::redrawPatterns() {
	m_patternScene->clear();
	const int count = patternCount();
	if (!m_anim || count == 0) {
		return;
	}
	const int size = cellSize();
	QPixmap strip(count * thumb, thumb);
	strip.fill(palette().color(QPalette::Base));
	QPainter painter(&strip);
	painter.setRenderHint(QPainter::SmoothPixmapTransform);
	for (int i = 0; i < count; ++i) {
		painter.drawImage(QRect(i * thumb, 0, thumb, thumb), m_graphic,
			QRect((i % pattern_columns) * size, (i / pattern_columns) * size, size, size));
	}
	const QRect selected(m_pattern * thumb, 0, thumb, thumb);
	painter.setPen(QPen(palette().color(QPalette::Highlight), 2));
	painter.drawRect(selected.adjusted(1, 1, -1, -1));
	painter.end();
	m_patternScene->addPixmap(strip);
	m_patternScene->setSceneRect(strip.rect());
	m_patternView->ensureVisible(selected, 0, 0);
}

bool BattleAnimationWidget::eventFilter(QObject* watched, QEvent* event) {
	const auto type = event->type();
	if (type == QEvent::Resize && watched == m_view->viewport()) {
		// the whole screen stays visible: shrunk when the page is small, enlarged
		// by whole steps (pixels stay square) when it is large
		const auto* resize = static_cast<QResizeEvent*>(event);
		double scale = std::min(resize->size().width() / double(screen_width), resize->size().height() / double(screen_height));
		scale = scale >= 1.0 ? std::floor(scale) : std::max(scale, 0.1);
		m_view->setTransform(QTransform::fromScale(scale, scale));
	}
	if (type == QEvent::ContextMenu && watched == m_view->viewport()) {
		// the menu of the cel under the mouse
		const auto* menu = static_cast<QContextMenuEvent*>(event);
		const int index = cellAt(m_view->mapToScene(menu->pos()), false);
		if (index >= 0 && !m_playTimer->isActive()) {
			m_cell->setCurrentIndex(index);
		}
		m_view->setFocus(Qt::MouseFocusReason);
		m_cellMenu->exec(menu->globalPos());
		return true;
	}
	if (type != QEvent::MouseButtonPress && type != QEvent::MouseMove && type != QEvent::MouseButtonRelease
		&& type != QEvent::MouseButtonDblClick) {
		return QWidget::eventFilter(watched, event);
	}
	auto* mouse = static_cast<QMouseEvent*>(event);
	if (watched == m_view->viewport()) {
		const QPointF pos = m_view->mapToScene(mouse->position().toPoint());
		if (type == QEvent::MouseButtonPress && mouse->button() == Qt::LeftButton) {
			canvasPressed(pos, mouse->modifiers());
			return true;
		}
		if (type == QEvent::MouseButtonDblClick && mouse->button() == Qt::LeftButton) {
			// RPG Maker opens the cel properties: here the fields beside
			if (cellAt(pos, false) >= 0) {
				m_cellPattern->setFocus(Qt::MouseFocusReason);
				m_cellPattern->selectAll();
			}
			return true;
		}
		if (type == QEvent::MouseMove && m_dragging) {
			canvasDragged(pos);
			return true;
		}
		if (type == QEvent::MouseButtonRelease) {
			m_dragging = false;
		}
	} else if (m_patternView && watched == m_patternView->viewport() && type == QEvent::MouseButtonPress) {
		patternClicked(m_patternView->mapToScene(mouse->position().toPoint()));
		return true;
	}
	return QWidget::eventFilter(watched, event);
}

void BattleAnimationWidget::canvasPressed(const QPointF& pos, Qt::KeyboardModifiers modifiers) {
	auto* frame = currentFrame();
	m_view->setFocus(Qt::MouseFocusReason);
	if (!frame || m_playTimer->isActive()) return;
	const QPointF o = origin();

	// the cel under the mouse is selected and dragged; with Ctrl the one
	// behind the selected cel, with Shift all the cels move
	const int index = cellAt(pos, modifiers & Qt::ControlModifier);
	if (index >= 0) {
		const auto& cell = frame->cells[index];
		m_cell->setCurrentIndex(index);
		m_dragOffset = pos - (o + QPointF(cell.x, cell.y));
		m_dragging = true;
		m_dragAll = modifiers & Qt::ShiftModifier;
		m_dragStart = pos;
		m_dragCells = frame->cells;
		return;
	}

	// elsewhere a new cel of the selected pattern
	lcf::rpg::AnimationCellData cell;
	cell.cell_id = m_pattern;
	cell.x = snap(pos.x() - o.x());
	cell.y = snap(pos.y() - o.y());
	addCell(cell);
	m_dragOffset = QPointF();
	m_dragging = true;
	m_dragAll = false;
}

void BattleAnimationWidget::canvasDragged(const QPointF& pos) {
	auto* frame = currentFrame();
	auto* cell = currentCell();
	if (!frame || !cell || !cell->valid) return;
	if (m_dragAll && m_dragCells.size() == frame->cells.size()) {
		const int dx = snap(pos.x() - m_dragStart.x());
		const int dy = snap(pos.y() - m_dragStart.y());
		for (size_t i = 0; i < m_dragCells.size(); ++i) {
			frame->cells[i].x = m_dragCells[i].x + dx;
			frame->cells[i].y = m_dragCells[i].y + dy;
		}
	} else {
		const QPointF moved = pos - origin() - m_dragOffset;
		cell->x = snap(moved.x());
		cell->y = snap(moved.y());
	}
	refreshCellFields();
	redraw();
}

void BattleAnimationWidget::patternClicked(const QPointF& pos) {
	const int index = static_cast<int>(pos.x()) / thumb;
	if (pos.x() < 0 || index >= patternCount()) return;
	m_pattern = index;
	auto* cell = currentCell();
	if (cell && cell->valid) {
		cell->cell_id = index;
		m_cell->setItemText(m_cell->currentIndex(), cellText(m_cell->currentIndex()));
		refreshCellFields();
		redraw();
	}
	redrawPatterns();
}
