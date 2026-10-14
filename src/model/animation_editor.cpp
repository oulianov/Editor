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

#include "animation_editor.h"
#include "animation_tools.h"
#include <QCoreApplication>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <optional>

namespace {
// what Cut and Copy keep, from one animation to another
std::optional<lcf::rpg::AnimationCellData> cell_clipboard;
std::optional<std::vector<lcf::rpg::AnimationCellData>> frame_clipboard;
std::optional<lcf::rpg::AnimationTiming> timing_clipboard;

/** IDs are written to the file: they follow the positions */
template <class T>
void renumber(std::vector<T>& items) {
	for (size_t i = 0; i < items.size(); ++i) {
		items[i].ID = static_cast<int>(i) + 1;
	}
}

int32_t lcf::rpg::AnimationCellData::* member(AnimationEditor::CellField field) {
	using Cell = lcf::rpg::AnimationCellData;
	switch (field) {
		case AnimationEditor::Pattern: return &Cell::cell_id;
		case AnimationEditor::X: return &Cell::x;
		case AnimationEditor::Y: return &Cell::y;
		case AnimationEditor::Zoom: return &Cell::zoom;
		case AnimationEditor::Transparency: return &Cell::transparency;
		case AnimationEditor::Red: return &Cell::tone_red;
		case AnimationEditor::Green: return &Cell::tone_green;
		case AnimationEditor::Blue: return &Cell::tone_blue;
		case AnimationEditor::Saturation: return &Cell::tone_gray;
	}
	return &Cell::cell_id;
}
}

AnimationEditor::AnimationEditor(QObject* parent) :
	QObject(parent)
{
	m_playTimer = new QTimer(this);
	m_playTimer->setInterval(kPlayInterval);
	connect(m_playTimer, &QTimer::timeout, this, &AnimationEditor::step);
}

lcf::rpg::Animation* AnimationEditor::animation() const {
	return m_anim;
}

void AnimationEditor::setAnimation(lcf::rpg::Animation* anim) {
	stop();
	m_anim = anim;
	m_dragging = false;
	// the cel selected stays, as from frame to frame
	refreshFrames(0);
	refreshTimings(0);
}

int AnimationEditor::cellSize() const {
	return m_anim && m_anim->large ? 128 : 96;
}

void AnimationEditor::setGraphicHeight(int height) {
	m_graphicHeight = std::max(0, height);
}

int AnimationEditor::patternCount() const {
	return kPatternColumns * (m_graphicHeight / cellSize());
}

int AnimationEditor::patternLimit() const {
	return patternCount() > 0 ? patternCount() : kMaxPatterns;
}

void AnimationEditor::setTargetHeight(int height) {
	m_targetHeight = std::max(0, height);
}

QPointF AnimationEditor::origin() const {
	// as the Player places an animation in battle
	if (m_anim && m_anim->scope == lcf::rpg::Animation::Scope_screen) {
		return QPointF(kScreenWidth / 2, kScreenHeight / 3);
	}
	const int height = m_targetHeight > 0 ? m_targetHeight : cellSize() / 2;
	int offset = 0;
	if (m_anim && m_anim->position == lcf::rpg::Animation::Position_down) {
		offset = height / 2;
	} else if (m_anim && m_anim->position == lcf::rpg::Animation::Position_up) {
		offset = -(height / 2);
	}
	return QPointF(kScreenWidth / 2, kScreenHeight / 2 + offset);
}

QRectF AnimationEditor::cellRect(const lcf::rpg::AnimationCellData& cell) const {
	const double size = cellSize() * cell.zoom / 100.0;
	const QPointF center = origin() + QPointF(cell.x, cell.y);
	return QRectF(center.x() - size / 2, center.y() - size / 2, size, size);
}

int AnimationEditor::cellAt(const QPointF& pos, bool behind) const {
	const auto* shown = frame();
	if (!shown) {
		return -1;
	}
	// the cels under the point, the last drawn (on top) first
	const auto& cells = shown->cells;
	std::vector<int> hits;
	for (int i = static_cast<int>(cells.size()) - 1; i >= 0; --i) {
		if (cells[i].valid && cellRect(cells[i]).contains(pos)) hits.push_back(i);
	}
	if (hits.empty()) {
		return -1;
	}
	auto current = std::find(hits.begin(), hits.end(), m_cell);
	if (behind && current != hits.end()) {
		++current;
		return current == hits.end() ? hits.front() : *current;
	}
	return hits.front();
}

bool AnimationEditor::useGrid() const {
	return m_grid;
}

void AnimationEditor::setUseGrid(bool on) {
	if (m_grid == on) return;
	m_grid = on;
	emit useGridChanged(on);
}

int AnimationEditor::snap(double value) const {
	if (m_grid) {
		return static_cast<int>(std::lround(value / kGridStep)) * kGridStep;
	}
	return static_cast<int>(std::lround(value));
}

int AnimationEditor::moveStep() const {
	return m_grid ? kGridStep : 1;
}

int AnimationEditor::frameCount() const {
	return m_anim ? static_cast<int>(m_anim->frames.size()) : 0;
}

int AnimationEditor::currentFrame() const {
	return m_frame;
}

void AnimationEditor::setCurrentFrame(int index) {
	if (index < 0 || index >= frameCount() || index == m_frame) return;
	showFrame(index);
	emitFrameShown();
}

const lcf::rpg::AnimationFrame* AnimationEditor::frame() const {
	if (m_frame < 0 || m_frame >= frameCount()) {
		return nullptr;
	}
	return &m_anim->frames[m_frame];
}

lcf::rpg::AnimationFrame* AnimationEditor::mutableFrame() {
	return const_cast<lcf::rpg::AnimationFrame*>(frame());
}

void AnimationEditor::previousFrame() {
	setCurrentFrame(std::max(0, m_frame - 1));
}

void AnimationEditor::nextFrame() {
	setCurrentFrame(std::min(frameCount() - 1, m_frame + 1));
}

void AnimationEditor::resizeFrames(int count) {
	if (!m_anim) return;
	count = std::clamp(count, 1, AnimationTools::kMaxFrames);
	const int row = m_frame;
	m_anim->frames.resize(count);
	renumber(m_anim->frames);
	refreshFrames(std::min(row, count - 1));
}

void AnimationEditor::clonePrevious() {
	if (!m_anim || m_frame < 1 || m_frame >= frameCount()) return;
	m_anim->frames[m_frame].cells = m_anim->frames[m_frame - 1].cells;
	refreshCells(m_cell);
}

void AnimationEditor::insertFrame() {
	if (!m_anim || frameCount() >= AnimationTools::kMaxFrames) return;
	const int row = std::max(0, m_frame);
	m_anim->frames.insert(m_anim->frames.begin() + row, lcf::rpg::AnimationFrame());
	renumber(m_anim->frames);
	// the effects stay on their frames, now one further
	for (auto& timing : m_anim->timings) {
		if (timing.frame > row) ++timing.frame;
	}
	refreshFrames(row);
	refreshTimings(m_timing);
}

void AnimationEditor::deleteFrame() {
	const int row = m_frame;
	if (!m_anim || frameCount() <= 1 || row < 0) return;
	m_anim->frames.erase(m_anim->frames.begin() + row);
	renumber(m_anim->frames);
	// the effects of the next frames stay on them
	for (auto& timing : m_anim->timings) {
		if (timing.frame > row + 1) --timing.frame;
	}
	refreshFrames(row);
	refreshTimings(m_timing);
}

void AnimationEditor::copyFrame() {
	if (const auto* shown = frame()) frame_clipboard = shown->cells;
}

void AnimationEditor::cutFrame() {
	copyFrame();
	clearFrame();
}

void AnimationEditor::pasteFrame() {
	auto* shown = mutableFrame();
	if (!shown || !frame_clipboard) return;
	shown->cells = *frame_clipboard;
	framesEdited();
}

void AnimationEditor::clearFrame() {
	auto* shown = mutableFrame();
	if (!shown) return;
	shown->cells.clear();
	framesEdited();
}

bool AnimationEditor::canPasteFrame() {
	return frame_clipboard.has_value();
}

void AnimationEditor::framesEdited() {
	refreshFrames(m_frame);
}

int AnimationEditor::currentCell() const {
	return m_cell;
}

void AnimationEditor::setCurrentCell(int index) {
	if (index < -1 || index >= cellCount() || index == m_cell) return;
	m_cell = index;
	followCell();
	emit currentCellChanged();
	emit previewChanged();
}

const lcf::rpg::AnimationCellData* AnimationEditor::cell() const {
	const auto* shown = frame();
	if (!shown || m_cell < 0 || m_cell >= static_cast<int>(shown->cells.size())) {
		return nullptr;
	}
	return &shown->cells[m_cell];
}

lcf::rpg::AnimationCellData* AnimationEditor::mutableCell() {
	return const_cast<lcf::rpg::AnimationCellData*>(cell());
}

bool AnimationEditor::isCellEditable() const {
	const auto* current = cell();
	return current && current->valid;
}

int AnimationEditor::cellCount() const {
	const auto* shown = frame();
	return shown ? static_cast<int>(shown->cells.size()) : 0;
}

QString AnimationEditor::cellText(int index) const {
	const auto* shown = frame();
	if (!shown || index < 0 || index >= static_cast<int>(shown->cells.size())) {
		return QString();
	}
	// the texts of the page, before it used this class
	const auto& data = shown->cells[index];
	if (!data.valid) {
		return QCoreApplication::translate("BattleAnimationWidget", "Cel %1: unused").arg(index + 1);
	}
	return QCoreApplication::translate("BattleAnimationWidget", "Cel %1: pattern %2").arg(index + 1).arg(data.cell_id + 1);
}

int AnimationEditor::cellValue(CellField field) const {
	// the fields of no cel show those of a new one
	static const lcf::rpg::AnimationCellData blank;
	return (isCellEditable() ? *cell() : blank).*member(field);
}

void AnimationEditor::setCellValue(CellField field, int value) {
	auto* current = mutableCell();
	if (!current || !current->valid || current->*member(field) == value) return;
	current->*member(field) = value;
	if (field == Pattern) {
		m_pattern = value;
		emit patternChanged();
	}
	cellsEdited();
}

void AnimationEditor::addCell(const lcf::rpg::AnimationCellData& data) {
	auto* shown = mutableFrame();
	if (!shown) return;
	auto& cells = shown->cells;
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
}

void AnimationEditor::deleteCell() {
	auto* shown = mutableFrame();
	auto* current = mutableCell();
	if (!shown || !current) return;
	current->valid = 0;
	auto& cells = shown->cells;
	while (!cells.empty() && !cells.back().valid) {
		cells.pop_back();
	}
	refreshCells(-1);
}

void AnimationEditor::copyCell() {
	if (isCellEditable()) cell_clipboard = *cell();
}

void AnimationEditor::cutCell() {
	copyCell();
	deleteCell();
}

void AnimationEditor::pasteCell() {
	if (cell_clipboard && !isPlaying()) addCell(*cell_clipboard);
}

bool AnimationEditor::canPasteCell() {
	return cell_clipboard.has_value();
}

void AnimationEditor::changePattern(int step) {
	if (!isCellEditable()) return;
	setCellValue(Pattern, std::clamp(cell()->cell_id + step, 0, patternLimit() - 1));
}

void AnimationEditor::moveCells(int dx, int dy, bool all) {
	auto* shown = mutableFrame();
	auto* current = mutableCell();
	if (!shown || isPlaying() || (!all && (!current || !current->valid))) return;
	// with the grid, to its next line in the direction of the move
	auto step = [this](int value, int delta) {
		if (!m_grid || delta == 0) return value + delta;
		const double target = double(value + delta) / kGridStep;
		return static_cast<int>(delta > 0 ? std::floor(target) : std::ceil(target)) * kGridStep;
	};
	for (auto& data : shown->cells) {
		if (data.valid && (all || &data == current)) {
			data.x = step(data.x, dx);
			data.y = step(data.y, dy);
		}
	}
	cellsEdited();
}

int AnimationEditor::pattern() const {
	return m_pattern;
}

void AnimationEditor::setPattern(int index) {
	if (index < 0 || index >= patternCount()) return;
	m_pattern = index;
	auto* current = mutableCell();
	if (current && current->valid) {
		current->cell_id = index;
		cellsEdited();
	}
	emit patternChanged();
}

void AnimationEditor::beginDrag(const QPointF& pos, bool behind, bool all) {
	auto* shown = mutableFrame();
	if (!shown || isPlaying()) return;
	const QPointF o = origin();

	const int index = cellAt(pos, behind);
	if (index >= 0) {
		setCurrentCell(index);
		const auto& data = shown->cells[index];
		m_dragOffset = pos - (o + QPointF(data.x, data.y));
		m_dragging = true;
		m_dragAll = all;
		m_dragStart = pos;
		m_dragCells = shown->cells;
		return;
	}

	// elsewhere a new cel of the selected pattern
	lcf::rpg::AnimationCellData data;
	data.cell_id = m_pattern;
	data.x = snap(pos.x() - o.x());
	data.y = snap(pos.y() - o.y());
	addCell(data);
	m_dragOffset = QPointF();
	m_dragging = true;
	m_dragAll = false;
}

void AnimationEditor::dragTo(const QPointF& pos) {
	auto* shown = mutableFrame();
	auto* current = mutableCell();
	if (!m_dragging || !shown || !current || !current->valid) return;
	if (m_dragAll && m_dragCells.size() == shown->cells.size()) {
		const int dx = snap(pos.x() - m_dragStart.x());
		const int dy = snap(pos.y() - m_dragStart.y());
		for (size_t i = 0; i < m_dragCells.size(); ++i) {
			shown->cells[i].x = m_dragCells[i].x + dx;
			shown->cells[i].y = m_dragCells[i].y + dy;
		}
	} else {
		const QPointF moved = pos - origin() - m_dragOffset;
		current->x = snap(moved.x());
		current->y = snap(moved.y());
	}
	cellsEdited();
}

void AnimationEditor::endDrag() {
	m_dragging = false;
}

bool AnimationEditor::isDragging() const {
	return m_dragging;
}

int AnimationEditor::currentTiming() const {
	return m_timing;
}

void AnimationEditor::setCurrentTiming(int index) {
	const int count = m_anim ? static_cast<int>(m_anim->timings.size()) : 0;
	if (index < -1 || index >= count || index == m_timing) return;
	m_timing = index;
	emit currentTimingChanged();
}

lcf::rpg::AnimationTiming AnimationEditor::newTiming() const {
	lcf::rpg::AnimationTiming timing;
	timing.frame = std::max(1, m_frame + 1);
	return timing;
}

void AnimationEditor::putTiming(const lcf::rpg::AnimationTiming& timing, int index) {
	if (!m_anim) return;
	auto& timings = m_anim->timings;
	if (index < 0 || index >= static_cast<int>(timings.size())) {
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

void AnimationEditor::deleteTiming() {
	const int index = m_timing;
	if (!m_anim || index < 0) return;
	m_anim->timings.erase(m_anim->timings.begin() + index);
	renumber(m_anim->timings);
	refreshTimings(index);
}

void AnimationEditor::copyTiming() {
	if (m_anim && m_timing >= 0) timing_clipboard = m_anim->timings[m_timing];
}

void AnimationEditor::cutTiming() {
	copyTiming();
	deleteTiming();
}

void AnimationEditor::pasteTiming() {
	if (!m_anim || !timing_clipboard) return;
	auto timing = *timing_clipboard;
	timing.frame = newTiming().frame;
	putTiming(timing, -1);
}

bool AnimationEditor::canPasteTiming() {
	return timing_clipboard.has_value();
}

QStringList AnimationEditor::timingTexts(const lcf::rpg::AnimationTiming& timing) {
	// the texts of the page, before it used this class
	const QString scopes[] = {
		QCoreApplication::translate("BattleAnimationWidget", "None"),
		QCoreApplication::translate("BattleAnimationWidget", "Target"),
		QCoreApplication::translate("BattleAnimationWidget", "Screen"),
	};
	auto scope = [&](int value) {
		return value >= 0 && value < 3 ? scopes[value] : QString::number(value);
	};
	QString sound = QString::fromStdString(timing.se.name);
	if (sound.isEmpty()) {
		sound = QStringLiteral("(OFF)");
	}
	QString flash = scope(timing.flash_scope);
	if (timing.flash_scope != lcf::rpg::AnimationTiming::FlashScope_nothing) {
		flash = QCoreApplication::translate("BattleAnimationWidget", "%1 (%2, %3, %4, %5)").arg(flash).arg(timing.flash_red).arg(timing.flash_green)
			.arg(timing.flash_blue).arg(timing.flash_power);
	}
	return {QString::number(timing.frame), sound, flash, scope(timing.screen_shake)};
}

bool AnimationEditor::isPlaying() const {
	return m_playTimer->isActive();
}

void AnimationEditor::play() {
	if (isPlaying() || frameCount() == 0) return;
	m_playTimer->start();
	emit playingChanged(true);
	if (m_frame != 0) {
		setCurrentFrame(0);
	} else {
		// without the selected cel
		emit previewChanged();
	}
}

void AnimationEditor::stop() {
	if (!isPlaying()) return;
	m_playTimer->stop();
	emit playingChanged(false);
	emit previewChanged();
}

void AnimationEditor::togglePlay() {
	if (isPlaying()) {
		stop();
	} else {
		play();
	}
}

void AnimationEditor::step() {
	const int next = m_frame + 1;
	if (next >= frameCount()) {
		stop();
		return;
	}
	setCurrentFrame(next);
}

void AnimationEditor::refreshFrames(int select) {
	const int count = frameCount();
	showFrame(count > 0 ? std::clamp(select, 0, count - 1) : -1);
	emit framesChanged();
	emitFrameShown();
}

void AnimationEditor::showFrame(int index) {
	m_frame = index;
	// the same cel stays selected from frame to frame
	selectCell(m_cell);
}

void AnimationEditor::emitFrameShown() {
	emit currentFrameChanged();
	emit cellsChanged();
	emit currentCellChanged();
	emit previewChanged();
}

void AnimationEditor::refreshCells(int select) {
	selectCell(select);
	emit cellsChanged();
	emit currentCellChanged();
	emit previewChanged();
}

void AnimationEditor::selectCell(int select) {
	const auto* shown = frame();
	const int count = shown ? static_cast<int>(shown->cells.size()) : 0;
	if (select < 0 || select >= count || !shown->cells[select].valid) {
		select = -1;
		for (int i = 0; i < count && select < 0; ++i) {
			if (shown->cells[i].valid) select = i;
		}
	}
	m_cell = select;
	followCell();
}

void AnimationEditor::followCell() {
	const auto* current = cell();
	if (current && current->valid && m_pattern != current->cell_id) {
		m_pattern = current->cell_id;
		emit patternChanged();
	}
}

void AnimationEditor::refreshTimings(int select) {
	const int count = m_anim ? static_cast<int>(m_anim->timings.size()) : 0;
	const int index = count > 0 ? std::clamp(select, 0, count - 1) : -1;
	m_timing = index;
	emit timingsChanged();
	emit currentTimingChanged();
}

void AnimationEditor::cellsEdited() {
	emit cellEdited();
	emit previewChanged();
}
