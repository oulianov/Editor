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

#include "chipset_widget.h"
#include "core.h"
#include "defines.h"
#include "common/dbstring.h"
#include "common/image_loader.h"
#include "model/project.h"
#include "model/project_data.h"
#include "ui/common/rpg_combobox.h"
#include "ui/picker/picker_dialog.h"
#include "ui/common/text_limit.h"
#include <QButtonGroup>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QPainterPath>
#include <QRadioButton>
#include <QScrollBar>
#include <QTabBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace {
constexpr int columns = 6;
constexpr int cell = 32;  // tiles are shown at twice their size

enum Flag {
	Down = 0x01, Left = 0x02, Right = 0x04, Up = 0x08,
	Above = 0x10, Wall = 0x20, Counter = 0x40,
	AllDirections = Down | Left | Right | Up
};

/** A mark of the Directions mode, as RPG Maker 2003 draws it in a square of
 *  16 pixels: an arrow where the hero can walk out of the tile, a dot where
 *  not; white, the one under the mouse yellow */
void drawDirection(QPainter& painter, QPoint at, int side, bool open, bool hovered) {
	// pointing up; on pixel centers, for a crisp outline
	static const QPolygonF arrow({{8, 0.5}, {14.5, 7.5}, {10.5, 7.5}, {10.5, 12.5},
		{5.5, 12.5}, {5.5, 7.5}, {1.5, 7.5}});
	painter.save();
	painter.translate(at);
	painter.setPen(QPen(hovered ? QColor(128, 0, 0) : QColor(Qt::black), 1));
	painter.setBrush(hovered ? QColor(255, 255, 0) : QColor(Qt::white));
	if (open) {
		const int angle = side == Right ? 90 : side == Down ? 180 : side == Left ? 270 : 0;
		painter.translate(8, 8);
		painter.rotate(angle);
		painter.translate(-8, -8);
		painter.drawPolygon(arrow);
	} else {
		painter.drawRect(QRectF(5.5, 5.5, 5, 5));
	}
	painter.restore();
}
}

ChipsetWidget::ChipsetWidget(ProjectData& project, QWidget *parent) :
	QWidget(parent), m_project(project)
{
	auto* layout = new QHBoxLayout(this);

	// tiles
	auto* tiles = new QVBoxLayout();
	m_layer = new QTabBar(this);
	m_layer->addTab(tr("Lower Layer"));
	m_layer->addTab(tr("Upper Layer"));
	m_layer->setDrawBase(false);
	tiles->addWidget(m_layer);
	m_scene = new QGraphicsScene(this);
	m_view = new QGraphicsView(m_scene, this);
	m_view->setObjectName(QStringLiteral("viewTiles"));
	m_view->setFixedWidth(columns * cell + m_view->verticalScrollBar()->sizeHint().width() + 4);
	m_view->setAlignment(Qt::AlignLeft | Qt::AlignTop);
	// it scrolls its tiles: the whole chipset must not make the page as tall
	// (the page scrolls when it is taller than the window)
	m_view->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Ignored);
	m_view->viewport()->installEventFilter(this);
	// the side under the mouse is shown in the Directions mode
	m_view->viewport()->setMouseTracking(true);
	tiles->addWidget(m_view, 1);
	layout->addLayout(tiles);

	// settings
	auto* settings = new QVBoxLayout();
	auto* form = new QFormLayout();
	form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	form->setLabelAlignment(Qt::AlignLeft);
	m_name = new QLineEdit(this);
	TextLimit::mark(m_name, 20);
	form->addRow(tr("Name"), m_name);
	auto* file_row = new QHBoxLayout();
	m_file = new QLineEdit(this);
	m_file->setReadOnly(true);
	auto* pick = new QToolButton(this);
	pick->setObjectName(QStringLiteral("pushChipsetFile"));
	pick->setText(QStringLiteral("..."));
	file_row->addWidget(m_file, 1);
	file_row->addWidget(pick);
	form->addRow(tr("Graphic"), file_row);
	settings->addLayout(form);

	auto* modes = new QGroupBox(tr("Editing Mode"), this);
	auto* modes_layout = new QVBoxLayout(modes);
	m_mode = new QButtonGroup(this);
	m_mode->setObjectName(QStringLiteral("editingMode"));
	const std::vector<std::pair<Mode, QString>> mode_names = {
		{Mode::Terrain, tr("Terrain")},
		{Mode::Passability, tr("Passability")},
		{Mode::Directions, tr("Directions")},
		{Mode::Counter, tr("Counter")},
	};
	for (const auto& [mode, name] : mode_names) {
		auto* radio = new QRadioButton(name, modes);
		m_mode->addButton(radio, static_cast<int>(mode));
		modes_layout->addWidget(radio);
	}
	m_mode->button(static_cast<int>(Mode::Passability))->setChecked(true);
	auto* terrain_row = new QHBoxLayout();
	auto* terrain = new TerrainRpgComboBox(modes);
	terrain->makeModel(project);
	m_terrain = terrain;
	terrain_row->addSpacing(20);
	terrain_row->addWidget(m_terrain, 1);
	modes_layout->insertLayout(1, terrain_row);
	m_uniformTerrain = new QPushButton(tr("Set Uniform Terrain"), modes);
	m_uniformTerrain->setObjectName(QStringLiteral("pushUniformTerrain"));
	m_uniformTerrain->setToolTip(tr("Gives one terrain to all the tiles of the lower layer"));
	modes_layout->insertWidget(2, m_uniformTerrain, 0, Qt::AlignLeft);
	// what a click does in the current mode
	auto* hint = new QLabel(modes);
	hint->setWordWrap(true);
	hint->setForegroundRole(QPalette::PlaceholderText);
	modes_layout->addWidget(hint);
	auto update_hint = [this, hint] {
		switch (static_cast<Mode>(m_mode->checkedId())) {
			case Mode::Terrain: hint->setText(tr("Click or drag over tiles to give them the terrain above.")); break;
			case Mode::Passability: hint->setText(tr("Click a tile: passable (○), blocked (×), above the hero (☆). Right click goes back. Drag to paint the same.")); break;
			case Mode::Directions: hint->setText(tr("An arrow: the hero can walk out of the tile that way; a dot: not. Click a side of a tile to change it. Drag to do the same on other tiles.")); break;
			case Mode::Counter: hint->setText(tr("Click a tile to make it a counter: events can be talked to across it (□). Drag to paint the same.")); break;
		}
	};
	update_hint();
	connect(m_mode, &QButtonGroup::idClicked, this, update_hint);
	settings->addWidget(modes);

	m_water = new QGroupBox(tr("Water Animation"), this);
	auto* water = new QFormLayout(m_water);
	water->setLabelAlignment(Qt::AlignLeft);
	water->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
	m_sequence = new QButtonGroup(this);
	m_speed = new QButtonGroup(this);
	auto* sequence = new QHBoxLayout();
	auto* speed = new QHBoxLayout();
	for (const auto& [id, name] : std::vector<std::pair<int, QString>>{{0, QStringLiteral("1-2-3-2")}, {1, QStringLiteral("1-2-3")}}) {
		auto* radio = new QRadioButton(name, m_water);
		m_sequence->addButton(radio, id);
		sequence->addWidget(radio);
	}
	for (const auto& [id, name] : std::vector<std::pair<int, QString>>{{0, tr("Slow")}, {1, tr("Fast")}}) {
		auto* radio = new QRadioButton(name, m_water);
		m_speed->addButton(radio, id);
		speed->addWidget(radio);
	}
	water->addRow(tr("Sequence"), sequence);
	water->addRow(tr("Speed"), speed);
	settings->addWidget(m_water);
	settings->addStretch(1);
	layout->addLayout(settings, 1);

	connect(m_name, &QLineEdit::textEdited, this, [this](const QString& text) {
		if (m_chipset) m_chipset->name = ToDBString(text);
	});
	connect(pick, &QToolButton::clicked, this, [this] {
		if (!m_chipset) return;
		PickerDialog dialog(m_project, FileFinder::FileType::Image, nullptr, this);
		connect(&dialog, &PickerDialog::fileSelected, this, [this](const QString& name) {
			m_chipset->chipset_name = ToDBString(name);
			m_file->setText(name);
			loadGraphic();
			redraw();
		});
		dialog.setDirectoryAndFile(CHIPSET, ToQString(m_chipset->chipset_name));
		dialog.exec();
	});
	connect(m_layer, &QTabBar::currentChanged, this, [this] {
		m_water->setEnabled(lowerLayer());
		m_uniformTerrain->setEnabled(lowerLayer());
		redraw();
	});
	connect(m_uniformTerrain, &QPushButton::clicked, this, &ChipsetWidget::setUniformTerrain);
	connect(m_mode, &QButtonGroup::idClicked, this, [this] {
		m_terrain->setEnabled(m_mode->checkedId() == static_cast<int>(Mode::Terrain));
		redraw();
	});
	connect(m_sequence, &QButtonGroup::idClicked, this, [this](int id) {
		if (m_chipset) m_chipset->animation_type = id;
	});
	connect(m_speed, &QButtonGroup::idClicked, this, [this](int id) {
		if (m_chipset) m_chipset->animation_speed = id;
	});
	m_terrain->setEnabled(false);
	setEnabled(false);
}

void ChipsetWidget::setData(lcf::rpg::Chipset* chipset) {
	m_chipset = chipset;
	setEnabled(chipset != nullptr);
	if (!chipset) {
		m_scene->clear();
		return;
	}
	// RPG Maker 2003 sizes; older files can have shorter lists
	chipset->terrain_data.resize(162, 1);
	chipset->passable_data_lower.resize(162, AllDirections);
	chipset->passable_data_upper.resize(144, AllDirections);

	m_name->setText(ToQString(chipset->name));
	m_file->setText(ToQString(chipset->chipset_name));
	if (auto* button = m_sequence->button(chipset->animation_type)) button->setChecked(true);
	if (auto* button = m_speed->button(chipset->animation_speed)) button->setChecked(true);
	loadGraphic();
	redraw();
}

bool ChipsetWidget::lowerLayer() const {
	return m_layer->currentIndex() == 0;
}

int ChipsetWidget::tileCount() const {
	return lowerLayer() ? 162 : 144;
}

QPoint ChipsetWidget::tilePosition(bool lower, int index) {
	// Positions in the 30 x 16 tiles of a chipset, as EasyRPG Player reads them
	if (!lower) {
		return index < 48 ? QPoint(18 + index % 6, 8 + index / 6) : QPoint(24 + (index - 48) % 6, (index - 48) / 6);
	}
	switch (index) {
		case 0: return {0, 0};  // water A
		case 1: return {3, 0};  // water B
		case 2: return {0, 4};  // deep water
		case 3: case 4: case 5: return {index, 4};  // animated tiles
		default: break;
	}
	if (index < 18) {
		const int block = index - 6;
		return block < 4 ? QPoint((block % 2) * 3, 8 + (block / 2) * 4)
			: QPoint(6 + (block % 2) * 3, ((block - 4) / 2) * 4);
	}
	const int id = index - 18;
	return id < 96 ? QPoint(12 + id % 6, id / 6) : QPoint(18 + (id - 96) % 6, (id - 96) / 6);
}

void ChipsetWidget::loadGraphic() {
	const QString name = ToQString(m_chipset->chipset_name);
	m_graphic = ImageLoader::Load(m_project.project().findFile(CHIPSET, name, FileFinder::FileType::Image));
	if (m_graphic.isNull()) {
		m_graphic = ImageLoader::Load(core().rtpPath(CHIPSET, name));
	}
}

void ChipsetWidget::redraw() {
	m_scene->clear();
	if (!m_chipset) {
		return;
	}
	const bool lower = lowerLayer();
	const int count = tileCount();
	const int rows = (count + columns - 1) / columns;
	QPixmap canvas(columns * cell, rows * cell);
	canvas.fill(palette().color(QPalette::Base));
	QPainter painter(&canvas);
	painter.setRenderHint(QPainter::Antialiasing);

	const int size = m_graphic.isNull() ? 16 : m_graphic.width() / 30;
	const auto& passable = lower ? m_chipset->passable_data_lower : m_chipset->passable_data_upper;
	const auto mode = static_cast<Mode>(m_mode->checkedId());
	QFont font = painter.font();
	font.setBold(true);
	font.setPixelSize(13);
	painter.setFont(font);

	for (int i = 0; i < count; ++i) {
		const QRect target((i % columns) * cell, (i / columns) * cell, cell, cell);
		if (!m_graphic.isNull()) {
			const QPoint pos = tilePosition(lower, i);
			painter.drawPixmap(target, m_graphic, QRect(pos.x() * size, pos.y() * size, size, size));
		}
		// the outline only: the dots of Directions leave their brush set
		painter.setPen(QColor(0, 0, 0, 60));
		painter.setBrush(Qt::NoBrush);
		painter.drawRect(target.adjusted(0, 0, -1, -1));

		// marks with a dark outline, readable on any tile
		auto mark = [&](const QString& text, QColor color) {
			QPainterPath path;
			const QRect bounds = painter.fontMetrics().boundingRect(text);
			path.addText(target.center().x() - bounds.width() / 2.0, target.center().y() + bounds.height() / 3.0, font, text);
			painter.strokePath(path, QPen(QColor(0, 0, 0, 200), 3));
			painter.fillPath(path, color);
		};
		const int flags = i < static_cast<int>(passable.size()) ? passable[i] : AllDirections;
		switch (mode) {
			case Mode::Terrain:
				if (lower && i < static_cast<int>(m_chipset->terrain_data.size())) {
					mark(QString::number(m_chipset->terrain_data[i]), Qt::white);
				}
				break;
			case Mode::Passability:
				if (flags & Above) {
					mark(QStringLiteral("☆"), QColor(255, 220, 90));
				} else if ((flags & AllDirections) == 0) {
					mark(QStringLiteral("×"), QColor(255, 110, 110));
				} else {
					mark(QStringLiteral("○"), QColor(120, 200, 255));
				}
				break;
			case Mode::Directions: {
				// where RPG Maker 2003 puts them in its tiles of 32 pixels
				static const std::pair<int, QPoint> sides[] = {
					{Down, {8, 16}}, {Left, {0, 8}}, {Right, {16, 8}}, {Up, {8, 0}}};
				for (const auto& [side, at] : sides) {
					drawDirection(painter, target.topLeft() + at, side, flags & side,
						i == m_hover.tile && side == m_hover.side);
				}
				break;
			}
			case Mode::Counter:
				if (flags & Counter) {
					mark(QStringLiteral("□"), QColor(255, 220, 90));
				}
				break;
		}
	}
	painter.end();
	m_scene->addPixmap(canvas);
	m_scene->setSceneRect(canvas.rect());
}

bool ChipsetWidget::eventFilter(QObject* watched, QEvent* event) {
	if (watched != m_view->viewport()) {
		return QWidget::eventFilter(watched, event);
	}
	auto* mouse = static_cast<QMouseEvent*>(event);
	switch (event->type()) {
		case QEvent::MouseButtonPress:
		// a quick second click comes as a double click: it is a click too
		case QEvent::MouseButtonDblClick:
			tileClicked(m_view->mapToScene(mouse->position().toPoint()), mouse->button() == Qt::RightButton);
			return true;
		case QEvent::MouseMove: {
			const QPointF pos = m_view->mapToScene(mouse->position().toPoint());
			const bool hover_changed = hoverAt(pos);
			// dragging paints what the click did on the other tiles
			if (m_stroke.tile >= 0 && (mouse->buttons() & (Qt::LeftButton | Qt::RightButton))) {
				const int tile = m_stroke.tile;
				tileDragged(pos);
				if (hover_changed && m_stroke.tile == tile) {
					redraw();
				}
				return true;
			}
			if (hover_changed) {
				redraw();
			}
			break;
		}
		case QEvent::Leave:
			if (m_hover.tile >= 0) {
				m_hover = {};
				redraw();
			}
			break;
		case QEvent::MouseButtonRelease:
			m_stroke.tile = -1;
			break;
		default:
			break;
	}
	return QWidget::eventFilter(watched, event);
}

int ChipsetWidget::tileAt(const QPointF& pos) const {
	const int col = static_cast<int>(std::floor(pos.x() / cell));
	const int row = static_cast<int>(std::floor(pos.y() / cell));
	const int index = row * columns + col;
	if (col < 0 || col >= columns || row < 0 || index >= tileCount()) {
		return -1;
	}
	return index;
}

int ChipsetWidget::sideAt(const QPointF& pos, int index) {
	// the tile is cut along its diagonals, as in RPG Maker
	const QPointF offset = pos - QPointF((index % columns) * cell + cell / 2.0, (index / columns) * cell + cell / 2.0);
	if (std::abs(offset.x()) > std::abs(offset.y())) {
		return offset.x() < 0 ? Left : Right;
	}
	return offset.y() < 0 ? Up : Down;
}

bool ChipsetWidget::hoverAt(const QPointF& pos) {
	Hover hover;
	if (m_chipset && static_cast<Mode>(m_mode->checkedId()) == Mode::Directions) {
		hover.tile = tileAt(pos);
		if (hover.tile >= 0) {
			hover.side = sideAt(pos, hover.tile);
		}
	}
	const bool changed = hover.tile != m_hover.tile || hover.side != m_hover.side;
	m_hover = hover;
	return changed;
}

void ChipsetWidget::tileClicked(const QPointF& pos, bool right_button) {
	m_stroke.tile = -1;
	if (!m_chipset) {
		return;
	}
	const int index = tileAt(pos);
	if (index < 0) {
		return;
	}
	const bool lower = lowerLayer();
	auto& passable = lower ? m_chipset->passable_data_lower : m_chipset->passable_data_upper;
	uint8_t& flags = passable[index];

	switch (static_cast<Mode>(m_mode->checkedId())) {
		case Mode::Terrain:
			if (lower) {
				m_chipset->terrain_data[index] = m_terrain->comboBox()->currentIndex() + 1;
			}
			break;
		case Mode::Passability: {
			// passable -> blocked -> above the hero -> passable (backwards with the right button)
			const int state = (flags & Above) ? 2 : ((flags & AllDirections) == 0 ? 1 : 0);
			const int next = (state + (right_button ? 2 : 1)) % 3;
			flags &= ~(AllDirections | Above);
			if (next == 0) flags |= AllDirections;
			if (next == 2) flags |= AllDirections | Above;
			break;
		}
		case Mode::Directions:
			m_stroke.side = sideAt(pos, index);
			flags ^= m_stroke.side;
			break;
		case Mode::Counter:
			flags ^= Counter;
			break;
	}
	// what the drag that may follow gives to the other tiles
	m_stroke.tile = index;
	m_stroke.flags = flags;
	redraw();
}

void ChipsetWidget::tileDragged(const QPointF& pos) {
	const int index = tileAt(pos);
	if (!m_chipset || index < 0 || index == m_stroke.tile) {
		return;
	}
	m_stroke.tile = index;
	const bool lower = lowerLayer();
	uint8_t& flags = (lower ? m_chipset->passable_data_lower : m_chipset->passable_data_upper)[index];
	switch (static_cast<Mode>(m_mode->checkedId())) {
		case Mode::Terrain:
			if (lower) {
				m_chipset->terrain_data[index] = m_terrain->comboBox()->currentIndex() + 1;
			}
			break;
		case Mode::Passability: {
			const uint8_t mask = AllDirections | Above;
			flags = (flags & ~mask) | (m_stroke.flags & mask);
			break;
		}
		case Mode::Directions:
			flags = (flags & ~m_stroke.side) | (m_stroke.flags & m_stroke.side);
			break;
		case Mode::Counter:
			flags = (flags & ~Counter) | (m_stroke.flags & Counter);
			break;
	}
	redraw();
}

void ChipsetWidget::setUniformTerrain() {
	if (!m_chipset) {
		return;
	}
	QDialog dialog(this);
	dialog.setObjectName(QStringLiteral("SetUniformTerrainDialog"));
	dialog.setWindowTitle(tr("Set Uniform Terrain"));
	auto* layout = new QVBoxLayout(&dialog);
	auto* group = new QGroupBox(tr("Select Terrain"), &dialog);
	auto* group_layout = new QVBoxLayout(group);
	auto* terrain = new TerrainRpgComboBox(group);
	terrain->makeModel(m_project);
	terrain->comboBox()->setObjectName(QStringLiteral("comboUniformTerrain"));
	terrain->setCurrentIndex(m_terrain->comboBox()->currentIndex());
	group_layout->addWidget(terrain);
	layout->addWidget(group);
	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
	connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	layout->addWidget(buttons);
	if (dialog.exec() != QDialog::Accepted || terrain->currentIndex() < 0) {
		return;
	}
	std::fill(m_chipset->terrain_data.begin(), m_chipset->terrain_data.end(), terrain->currentIndex() + 1);
	redraw();
}
