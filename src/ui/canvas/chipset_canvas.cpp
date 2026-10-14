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

#include "chipset_canvas.h"
#include "common/dbstring.h"
#include "common/image_loader.h"
#include "core.h"
#include "defines.h"
#include "model/project.h"
#include "model/project_data.h"
#include "qmlbinding/database_entry.h"
#include "render/chipset_editor_painter.h"
#include <QPainter>

ChipsetCanvas::ChipsetCanvas(QQuickItem* parent) : QQuickPaintedItem(parent)
{
	setAntialiasing(false);
	setSmooth(false);
	setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
	// the side under the pointer is shown in the Directions mode
	setAcceptHoverEvents(true);
}

JsonView* ChipsetCanvas::entry() const
{
	return m_entry;
}

void ChipsetCanvas::setEntry(JsonView* entry)
{
	if (m_entry == entry) {
		return;
	}
	if (m_entry) {
		disconnect(m_entry, nullptr, this, nullptr);
	}
	m_entry = entry;
	if (entry) {
		connect(entry, &JsonView::valueChanged, this, &ChipsetCanvas::onDataChanged);
	}
	emit entryChanged();
	findChipset();
}

bool ChipsetCanvas::lowerLayer() const
{
	return m_editor.lowerLayer();
}

void ChipsetCanvas::setLowerLayer(bool lower)
{
	if (lower != m_editor.lowerLayer()) {
		m_editor.setLowerLayer(lower);
		emit lowerLayerChanged();
		render();
	}
}

int ChipsetCanvas::mode() const
{
	return static_cast<int>(m_editor.mode());
}

void ChipsetCanvas::setMode(int mode)
{
	if (mode != this->mode()) {
		m_editor.setMode(static_cast<ChipsetEditor::Mode>(mode));
		emit modeChanged();
		render();
	}
}

int ChipsetCanvas::terrainId() const
{
	return m_editor.terrainId();
}

void ChipsetCanvas::setTerrainId(int terrain_id)
{
	if (terrain_id != m_editor.terrainId()) {
		m_editor.setTerrainId(terrain_id);
		emit terrainIdChanged();
	}
}

QColor ChipsetCanvas::background() const
{
	return m_background;
}

void ChipsetCanvas::setBackground(const QColor& color)
{
	if (color != m_background) {
		m_background = color;
		emit backgroundChanged();
		render();
	}
}

void ChipsetCanvas::findChipset()
{
	const DatabaseEntry found = DatabaseEntry::of(m_entry);
	lcf::rpg::Chipset* chipset = nullptr;
	if (found.valid() && found.list == QLatin1String("chipsets")
			&& found.index < static_cast<int>(found.project->database().chipsets.size())) {
		chipset = &found.project->database().chipsets[static_cast<size_t>(found.index)];
	}
	m_editor.setChipset(chipset);
	loadGraphic();
	render();
}

void ChipsetCanvas::loadGraphic()
{
	const auto* chipset = m_editor.chipset();
	const QString name = chipset ? ToQString(chipset->chipset_name) : QString();
	const DatabaseEntry found = DatabaseEntry::of(m_entry);
	m_graphicName = name;
	m_graphic = QPixmap();
	if (name.isEmpty() || !found.project || !found.project->hasProject()) {
		return;
	}
	m_graphic = ImageLoader::Load(found.project->project().findFile(CHIPSET, name, FileFinder::FileType::Image));
	if (m_graphic.isNull()) {
		m_graphic = ImageLoader::Load(core().rtpPath(CHIPSET, name));
	}
}

void ChipsetCanvas::render()
{
	if (!m_editor.chipset()) {
		m_rendered = QImage();
		setImplicitSize(ChipsetEditor::Columns * ChipsetEditorPainter::Cell, 0);
		update();
		return;
	}
	const QSize size = ChipsetEditorPainter::size(m_editor);
	m_rendered = QImage(size, QImage::Format_ARGB32_Premultiplied);
	m_rendered.fill(m_background);
	QPainter painter(&m_rendered);
	ChipsetEditorPainter::paint(painter, m_editor, m_graphic);
	painter.end();
	setImplicitSize(size.width(), size.height());
	update();
}

void ChipsetCanvas::paint(QPainter* painter)
{
	if (!m_rendered.isNull()) {
		painter->drawImage(0, 0, m_rendered);
	}
}

void ChipsetCanvas::onDataChanged(const QString& jsonPtr)
{
	if (m_editing) {
		return;
	}
	// the list moved (a paste, Array Size), the file chosen, a merge
	if (jsonPtr.isEmpty()) {
		findChipset();
	} else if (jsonPtr == QLatin1String("/chipset_name") && m_editor.chipset()
			&& ToQString(m_editor.chipset()->chipset_name) != m_graphicName) {
		loadGraphic();
		render();
	} else {
		render();
	}
}

void ChipsetCanvas::changed()
{
	// the views of the database hear it (the window's copy is modified)
	const DatabaseEntry found = DatabaseEntry::of(m_entry);
	m_editing = true;
	found.changed(m_editor.lowerLayer() ? QStringLiteral("passable_data_lower") : QStringLiteral("passable_data_upper"));
	if (m_editor.mode() == ChipsetEditor::Mode::Terrain) {
		found.changed(QStringLiteral("terrain_data"));
	}
	m_editing = false;
	render();
	emit edited();
}

void ChipsetCanvas::setUniformTerrain(int terrain_id)
{
	if (!m_editor.chipset()) {
		return;
	}
	m_editor.setUniformTerrain(terrain_id);
	const DatabaseEntry found = DatabaseEntry::of(m_entry);
	m_editing = true;
	found.changed(QStringLiteral("terrain_data"));
	m_editing = false;
	render();
	emit edited();
}

int ChipsetCanvas::flags(int index) const
{
	return m_editor.chipset() && index >= 0 && index < m_editor.tileCount() ? m_editor.flags(index) : 0;
}

int ChipsetCanvas::terrain(int index) const
{
	return m_editor.chipset() ? m_editor.terrain(index) : 0;
}

QPointF ChipsetCanvas::tilePoint(const QPointF& pos) const
{
	return pos / ChipsetEditorPainter::Cell;
}

void ChipsetCanvas::mousePressEvent(QMouseEvent* event)
{
	forceActiveFocus(Qt::MouseFocusReason);
	if (m_editor.press(tilePoint(event->position()), event->button() == Qt::RightButton)) {
		changed();
	}
	event->accept();
}

void ChipsetCanvas::mouseDoubleClickEvent(QMouseEvent* event)
{
	// a quick second click is a click too
	mousePressEvent(event);
}

void ChipsetCanvas::mouseMoveEvent(QMouseEvent* event)
{
	const QPointF pos = tilePoint(event->position());
	const bool hover_changed = m_editor.hoverAt(pos);
	// dragging paints what the click did on the other tiles
	if (m_editor.dragging() && m_editor.drag(pos)) {
		changed();
	} else if (hover_changed) {
		render();
	}
}

void ChipsetCanvas::mouseReleaseEvent(QMouseEvent* event)
{
	m_editor.release();
	event->accept();
}

void ChipsetCanvas::hoverMoveEvent(QHoverEvent* event)
{
	if (m_editor.hoverAt(tilePoint(event->position()))) {
		render();
	}
}

void ChipsetCanvas::hoverLeaveEvent(QHoverEvent*)
{
	if (m_editor.clearHover()) {
		render();
	}
}
