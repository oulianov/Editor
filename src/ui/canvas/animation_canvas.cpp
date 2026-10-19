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

#include "animation_canvas.h"
#include "common/dbstring.h"
#include "core.h"
#include "defines.h"
#include "model/animation_tools.h"
#include "model/project_data.h"
#include "qmlbinding/database_entry.h"
#include "render/game_graphic.h"
#include <QKeyEvent>
#include <QPainter>
#include <algorithm>

namespace {
constexpr int screen_width = AnimationEditor::kScreenWidth;
constexpr int screen_height = AnimationEditor::kScreenHeight;
// the size of the patterns in the strip under the preview
constexpr int thumb = 48;

/** A cel tinted like the Player does: each channel scaled, then the saturation */
QImage toned(const QImage& source, const lcf::rpg::AnimationCellData& cell)
{
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

QVariantMap toMap(const lcf::rpg::AnimationTiming& timing)
{
	return {
		{QStringLiteral("frame"), timing.frame},
		{QStringLiteral("se"), QVariantMap{
			{QStringLiteral("name"), ToQString(timing.se.name)},
			{QStringLiteral("volume"), timing.se.volume},
			{QStringLiteral("tempo"), timing.se.tempo},
			{QStringLiteral("balance"), timing.se.balance},
		}},
		{QStringLiteral("flash_scope"), timing.flash_scope},
		{QStringLiteral("flash_red"), timing.flash_red},
		{QStringLiteral("flash_green"), timing.flash_green},
		{QStringLiteral("flash_blue"), timing.flash_blue},
		{QStringLiteral("flash_power"), timing.flash_power},
		{QStringLiteral("screen_shake"), timing.screen_shake},
	};
}

lcf::rpg::AnimationTiming fromMap(const QVariantMap& map, lcf::rpg::AnimationTiming timing)
{
	auto value = [&map](const char* key, int32_t fallback) {
		const QVariant v = map.value(QLatin1String(key));
		return v.isValid() ? v.toInt() : fallback;
	};
	timing.frame = value("frame", timing.frame);
	const QVariantMap se = map.value(QStringLiteral("se")).toMap();
	if (!se.isEmpty()) {
		timing.se.name = ToDBString(se.value(QStringLiteral("name")).toString());
		timing.se.volume = se.value(QStringLiteral("volume"), timing.se.volume).toInt();
		timing.se.tempo = se.value(QStringLiteral("tempo"), timing.se.tempo).toInt();
		timing.se.balance = se.value(QStringLiteral("balance"), timing.se.balance).toInt();
	}
	timing.flash_scope = value("flash_scope", timing.flash_scope);
	timing.flash_red = value("flash_red", timing.flash_red);
	timing.flash_green = value("flash_green", timing.flash_green);
	timing.flash_blue = value("flash_blue", timing.flash_blue);
	timing.flash_power = value("flash_power", timing.flash_power);
	timing.screen_shake = value("screen_shake", timing.screen_shake);
	return timing;
}
}

AnimationCanvas::AnimationCanvas(QQuickItem* parent) : QQuickPaintedItem(parent)
{
	// the pixels of the game stay square when the item is scaled
	setAntialiasing(false);
	setSmooth(false);
	setImplicitSize(screen_width, screen_height);
	setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
	// the keys of the cel menu and the arrows act on the preview
	setActiveFocusOnTab(true);

	// the page shows what the editor changed
	connect(&m_editor, &AnimationEditor::framesChanged, this, [this] {
		emit framesChanged();
		bump();
	});
	connect(&m_editor, &AnimationEditor::currentFrameChanged, this, [this] {
		emit currentFrameChanged();
		bump();
	});
	connect(&m_editor, &AnimationEditor::cellsChanged, this, [this] {
		emit cellsChanged();
		bump();
	});
	connect(&m_editor, &AnimationEditor::currentCellChanged, this, [this] {
		emit currentCellChanged();
		bump();
	});
	connect(&m_editor, &AnimationEditor::cellEdited, this, [this] {
		emit cellsChanged();
		bump();
		if (!m_editor.isDragging()) {
			changed(QStringLiteral("frames"));
		}
	});
	connect(&m_editor, &AnimationEditor::patternChanged, this, [this] {
		emit patternChanged();
		renderPatterns();
	});
	connect(&m_editor, &AnimationEditor::timingsChanged, this, &AnimationCanvas::timingsChanged);
	connect(&m_editor, &AnimationEditor::currentTimingChanged, this, &AnimationCanvas::currentTimingChanged);
	connect(&m_editor, &AnimationEditor::playingChanged, this, &AnimationCanvas::playingChanged);
	connect(&m_editor, &AnimationEditor::useGridChanged, this, &AnimationCanvas::useGridChanged);
	connect(&m_editor, &AnimationEditor::previewChanged, this, &AnimationCanvas::render);
}

JsonView* AnimationCanvas::entry() const
{
	return m_entry;
}

void AnimationCanvas::setEntry(JsonView* entry)
{
	if (m_entry == entry) {
		return;
	}
	if (m_entry) {
		disconnect(m_entry, nullptr, this, nullptr);
	}
	m_entry = entry;
	if (entry) {
		connect(entry, &JsonView::valueChanged, this, &AnimationCanvas::onDataChanged);
	}
	emit entryChanged();
	findAnimation();
}

int AnimationCanvas::target() const
{
	return m_target;
}

void AnimationCanvas::setTarget(int target)
{
	if (m_target != target) {
		m_target = target;
		emit targetChanged();
		loadTarget();
		render();
	}
}

int AnimationCanvas::firstTarget() const
{
	ProjectData* project = projectData();
	if (!project) {
		return -1;
	}
	const auto& enemies = project->database().enemies;
	for (size_t i = 0; i < enemies.size(); ++i) {
		if (!enemies[i].battler_name.empty()) {
			return static_cast<int>(i);
		}
	}
	return -1;
}

void AnimationCanvas::findAnimation()
{
	const DatabaseEntry found = DatabaseEntry::of(m_entry);
	lcf::rpg::Animation* anim = nullptr;
	if (found.valid() && found.list == QLatin1String("animations")
			&& found.index < static_cast<int>(found.project->database().animations.size())) {
		anim = &found.project->database().animations[static_cast<size_t>(found.index)];
	} else if (ProjectData* project = m_entry ? nullptr : projectData();
			project && m_animationId <= static_cast<int>(project->database().animations.size())) {
		anim = &project->database().animations[static_cast<size_t>(m_animationId - 1)];
	}
	// it stops playing and shows the first frame
	m_editor.setAnimation(anim);
	loadGraphic();
	loadTarget();
	renderPatterns();
	render();
	emit timingsChanged();
}

void AnimationCanvas::loadGraphic()
{
	m_graphic = QImage();
	ProjectData* project = projectData();
	if (auto* anim = m_editor.animation(); anim && project) {
		m_graphic = GameGraphic::load(*project, anim->large ? BATTLE2 : BATTLE, ToQString(anim->animation_name));
	}
	m_editor.setGraphicHeight(m_graphic.height());
	emit graphicChanged();
}

void AnimationCanvas::loadTarget()
{
	m_battler = QImage();
	ProjectData* project = projectData();
	if (project && m_target >= 0 && m_target < static_cast<int>(project->database().enemies.size())) {
		m_battler = GameGraphic::load(*project, MONSTER,
			ToQString(project->database().enemies[static_cast<size_t>(m_target)].battler_name));
	}
	m_editor.setTargetHeight(m_battler.height());
}

void AnimationCanvas::onDataChanged(const QString& jsonPtr)
{
	if (m_editing) {
		return;
	}
	if (jsonPtr.isEmpty()) {
		// the list moved (a paste, Array Size), a merge
		findAnimation();
	} else if (jsonPtr == QLatin1String("/animation_name") || jsonPtr == QLatin1String("/large")) {
		// the same file name, looked for in the other folder
		loadGraphic();
		renderPatterns();
		render();
	} else {
		render();
	}
}

void AnimationCanvas::changed(const QString& path)
{
	m_editing = true;
	DatabaseEntry::of(m_entry).changed(path);
	m_editing = false;
}

void AnimationCanvas::bump()
{
	++m_revision;
	emit revisionChanged();
}

int AnimationCanvas::frameCount() const
{
	return m_editor.frameCount();
}

int AnimationCanvas::currentFrame() const
{
	return m_editor.currentFrame();
}

void AnimationCanvas::setCurrentFrame(int index)
{
	if (index != m_editor.currentFrame() && index >= 0 && index < m_editor.frameCount()) {
		m_editor.setCurrentFrame(index);
	}
}

int AnimationCanvas::currentCell() const
{
	return m_editor.currentCell();
}

void AnimationCanvas::setCurrentCell(int index)
{
	if (index != m_editor.currentCell() && index >= 0) {
		m_editor.setCurrentCell(index);
	}
}

QStringList AnimationCanvas::cellTexts() const
{
	QStringList texts;
	for (int i = 0; i < m_editor.cellCount(); ++i) {
		texts << m_editor.cellText(i);
	}
	return texts;
}

bool AnimationCanvas::cellEditable() const
{
	return m_editor.isCellEditable();
}

int AnimationCanvas::pattern() const
{
	return m_editor.pattern();
}

void AnimationCanvas::setPattern(int index)
{
	if (index != m_editor.pattern() && index >= 0) {
		// given to the selected cel too
		m_editor.setPattern(index);
	}
}

int AnimationCanvas::patternCount() const
{
	return m_editor.patternCount();
}

int AnimationCanvas::patternLimit() const
{
	return m_editor.patternLimit();
}

QImage AnimationCanvas::patterns() const
{
	return m_patterns;
}

int AnimationCanvas::thumbSize() const
{
	return thumb;
}

QVariantList AnimationCanvas::timingRows() const
{
	QVariantList rows;
	if (auto* anim = m_editor.animation()) {
		for (const auto& timing : anim->timings) {
			rows.append(AnimationEditor::timingTexts(timing));
		}
	}
	return rows;
}

int AnimationCanvas::currentTiming() const
{
	return m_editor.currentTiming();
}

void AnimationCanvas::setCurrentTiming(int index)
{
	if (index != m_editor.currentTiming()) {
		m_editor.setCurrentTiming(index);
	}
}

bool AnimationCanvas::playing() const
{
	return m_editor.isPlaying();
}

bool AnimationCanvas::useGrid() const
{
	return m_editor.useGrid();
}

void AnimationCanvas::setUseGrid(bool on)
{
	m_editor.setUseGrid(on);
}

bool AnimationCanvas::showOrigin() const
{
	return m_showOrigin;
}

void AnimationCanvas::setShowOrigin(bool show)
{
	if (m_showOrigin != show) {
		m_showOrigin = show;
		emit showOriginChanged();
		render();
	}
}

int AnimationCanvas::animationId() const
{
	return m_animationId;
}

void AnimationCanvas::setAnimationId(int id)
{
	if (m_animationId != id) {
		m_animationId = id;
		emit entryChanged();
		findAnimation();
	}
}

ProjectData* AnimationCanvas::projectData() const
{
	if (m_entry) {
		return DatabaseEntry::of(m_entry).project;
	}
	return m_animationId > 0 && core().project() ? &core().project()->projectData() : nullptr;
}

int AnimationCanvas::moveStep() const
{
	return m_editor.moveStep();
}

int AnimationCanvas::revision() const
{
	return m_revision;
}

QColor AnimationCanvas::highlight() const
{
	return m_highlight;
}

void AnimationCanvas::setHighlight(const QColor& color)
{
	if (color != m_highlight) {
		m_highlight = color;
		renderPatterns();
	}
}

QColor AnimationCanvas::patternBackground() const
{
	return m_patternBackground;
}

void AnimationCanvas::setPatternBackground(const QColor& color)
{
	if (color != m_patternBackground) {
		m_patternBackground = color;
		renderPatterns();
	}
}

void AnimationCanvas::previousFrame()
{
	m_editor.previousFrame();
}

void AnimationCanvas::nextFrame()
{
	m_editor.nextFrame();
}

void AnimationCanvas::resizeFrames(int count)
{
	m_editor.resizeFrames(count);
	changed(QStringLiteral("frames"));
}

void AnimationCanvas::clonePrevious()
{
	m_editor.clonePrevious();
	changed(QStringLiteral("frames"));
}

void AnimationCanvas::insertFrame()
{
	m_editor.insertFrame();
	changed(QStringLiteral("frames"));
}

void AnimationCanvas::deleteFrame()
{
	m_editor.deleteFrame();
	changed(QStringLiteral("frames"));
}

void AnimationCanvas::copyFrame()
{
	m_editor.copyFrame();
}

void AnimationCanvas::cutFrame()
{
	m_editor.cutFrame();
	changed(QStringLiteral("frames"));
}

void AnimationCanvas::pasteFrame()
{
	m_editor.pasteFrame();
	changed(QStringLiteral("frames"));
}

void AnimationCanvas::clearFrame()
{
	m_editor.clearFrame();
	changed(QStringLiteral("frames"));
}

bool AnimationCanvas::canPasteFrame() const
{
	return AnimationEditor::canPasteFrame();
}

int AnimationCanvas::cellValue(int field) const
{
	return m_editor.cellValue(static_cast<AnimationEditor::CellField>(field));
}

void AnimationCanvas::setCellValue(int field, int value)
{
	m_editor.setCellValue(static_cast<AnimationEditor::CellField>(field), value);
}

void AnimationCanvas::deleteCell()
{
	m_editor.deleteCell();
	changed(QStringLiteral("frames"));
}

void AnimationCanvas::copyCell()
{
	m_editor.copyCell();
}

void AnimationCanvas::cutCell()
{
	m_editor.cutCell();
	changed(QStringLiteral("frames"));
}

void AnimationCanvas::pasteCell()
{
	m_editor.pasteCell();
	changed(QStringLiteral("frames"));
}

bool AnimationCanvas::canPasteCell() const
{
	return AnimationEditor::canPasteCell() && m_editor.frame();
}

void AnimationCanvas::changePattern(int step)
{
	m_editor.changePattern(step);
}

void AnimationCanvas::moveCells(int dx, int dy, bool all)
{
	m_editor.moveCells(dx, dy, all);
}

int AnimationCanvas::cellAt(qreal x, qreal y) const
{
	return m_editor.cellAt(QPointF(x, y), false);
}

QVariantMap AnimationCanvas::timing(int index) const
{
	auto* anim = m_editor.animation();
	if (!anim || index < 0 || index >= static_cast<int>(anim->timings.size())) {
		return {};
	}
	return toMap(anim->timings[static_cast<size_t>(index)]);
}

QVariantMap AnimationCanvas::newTiming() const
{
	return toMap(m_editor.newTiming());
}

void AnimationCanvas::putTiming(const QVariantMap& timing, int index)
{
	auto* anim = m_editor.animation();
	if (!anim) {
		return;
	}
	const bool exists = index >= 0 && index < static_cast<int>(anim->timings.size());
	const lcf::rpg::AnimationTiming base = exists ? anim->timings[static_cast<size_t>(index)] : m_editor.newTiming();
	m_editor.putTiming(fromMap(timing, base), exists ? index : -1);
	changed(QStringLiteral("timings"));
}

void AnimationCanvas::deleteTiming()
{
	m_editor.deleteTiming();
	changed(QStringLiteral("timings"));
}

void AnimationCanvas::copyTiming()
{
	m_editor.copyTiming();
}

void AnimationCanvas::cutTiming()
{
	m_editor.cutTiming();
	changed(QStringLiteral("timings"));
}

void AnimationCanvas::pasteTiming()
{
	m_editor.pasteTiming();
	changed(QStringLiteral("timings"));
}

bool AnimationCanvas::canPasteTiming() const
{
	return AnimationEditor::canPasteTiming();
}

int AnimationCanvas::maxCells() const
{
	auto* anim = m_editor.animation();
	return anim ? AnimationTools::maxCells(*anim) : 1;
}

void AnimationCanvas::massClear(int first, int last)
{
	if (auto* anim = m_editor.animation()) {
		AnimationTools::massClear(*anim, first, last);
		m_editor.framesEdited();
		changed(QStringLiteral("frames"));
	}
}

void AnimationCanvas::massCopy(int first, int last, int start)
{
	if (auto* anim = m_editor.animation()) {
		AnimationTools::massCopy(*anim, first, last, start);
		m_editor.framesEdited();
		changed(QStringLiteral("frames"));
	}
}

void AnimationCanvas::autocomplete(int first, int last, int first_cel, int last_cel, bool pattern, bool zoom, bool transparency)
{
	if (auto* anim = m_editor.animation()) {
		AnimationTools::autocomplete(*anim, first, last, first_cel, last_cel, {pattern, zoom, transparency});
		m_editor.framesEdited();
		changed(QStringLiteral("frames"));
	}
}

void AnimationCanvas::massModify(int first, int last, const QVariantList& cels, const QVariantMap& values)
{
	auto* anim = m_editor.animation();
	if (!anim) {
		return;
	}
	std::vector<bool> chosen;
	for (const QVariant& cel : cels) {
		chosen.push_back(cel.toBool());
	}
	AnimationTools::CelValues given;
	if (values.contains(QStringLiteral("pattern"))) {
		given.pattern = values.value(QStringLiteral("pattern")).toInt();
	}
	if (values.contains(QStringLiteral("zoom"))) {
		given.zoom = values.value(QStringLiteral("zoom")).toInt();
	}
	if (values.contains(QStringLiteral("transparency"))) {
		given.transparency = values.value(QStringLiteral("transparency")).toInt();
	}
	if (values.contains(QStringLiteral("tone"))) {
		const QVariantList tone = values.value(QStringLiteral("tone")).toList();
		if (tone.size() == 4) {
			given.tone = {tone[0].toInt(), tone[1].toInt(), tone[2].toInt(), tone[3].toInt()};
		}
	}
	AnimationTools::massModify(*anim, first, last, chosen, given);
	m_editor.framesEdited();
	changed(QStringLiteral("frames"));
}

void AnimationCanvas::togglePlay()
{
	m_editor.togglePlay();
}

void AnimationCanvas::stop()
{
	m_editor.stop();
}

void AnimationCanvas::render()
{
	m_screen = QImage(screen_width, screen_height, QImage::Format_ARGB32_Premultiplied);
	m_screen.fill(Qt::black);
	if (!m_editor.animation()) {
		update();
		return;
	}
	QPainter painter(&m_screen);
	if (!m_battler.isNull()) {
		painter.drawImage((screen_width - m_battler.width()) / 2, (screen_height - m_battler.height()) / 2, m_battler);
	}
	const int size = m_editor.cellSize();
	constexpr int columns = AnimationEditor::kPatternColumns;
	if (const auto* frame = m_editor.frame()) {
		for (const auto& cell : frame->cells) {
			if (!cell.valid) {
				continue;
			}
			const QRectF target = m_editor.cellRect(cell);
			painter.setOpacity((100 - cell.transparency) / 100.0);
			if (cell.cell_id < m_editor.patternCount()) {
				const QRect source((cell.cell_id % columns) * size, (cell.cell_id / columns) * size, size, size);
				painter.drawImage(target, toned(m_graphic.copy(source), cell));
			} else {
				// no graphic for this pattern: its place is still shown
				painter.fillRect(target, QColor(255, 255, 255, 60));
			}
		}
	}
	painter.setOpacity(1.0);

	// where the cel coordinates start from
	if (m_showOrigin) {
		const QPointF o = m_editor.origin();
		painter.setPen(QColor(255, 255, 255, 100));
		painter.drawLine(o - QPointF(6, 0), o + QPointF(6, 0));
		painter.drawLine(o - QPointF(0, 6), o + QPointF(0, 6));
	}

	if (m_editor.isCellEditable() && !m_editor.isPlaying()) {
		painter.setPen(QPen(Qt::white, 1, Qt::DashLine));
		painter.drawRect(m_editor.cellRect(*m_editor.cell()));
	}
	painter.end();
	update();
}

void AnimationCanvas::renderPatterns()
{
	const int count = m_editor.patternCount();
	if (!m_editor.animation() || count == 0) {
		m_patterns = QImage();
		emit patternsChanged();
		return;
	}
	const int size = m_editor.cellSize();
	constexpr int columns = AnimationEditor::kPatternColumns;
	m_patterns = QImage(count * thumb, thumb, QImage::Format_ARGB32_Premultiplied);
	m_patterns.fill(m_patternBackground);
	QPainter painter(&m_patterns);
	painter.setRenderHint(QPainter::SmoothPixmapTransform);
	for (int i = 0; i < count; ++i) {
		painter.drawImage(QRect(i * thumb, 0, thumb, thumb), m_graphic,
			QRect((i % columns) * size, (i / columns) * size, size, size));
	}
	painter.setPen(QPen(m_highlight, 2));
	painter.drawRect(QRect(m_editor.pattern() * thumb, 0, thumb, thumb).adjusted(1, 1, -1, -1));
	painter.end();
	emit patternsChanged();
}

void AnimationCanvas::paint(QPainter* painter)
{
	if (!m_screen.isNull()) {
		painter->drawImage(QRectF(0, 0, width(), height()), m_screen);
	}
}

QPointF AnimationCanvas::screenPoint(const QPointF& pos) const
{
	return QPointF(pos.x() * screen_width / width(), pos.y() * screen_height / height());
}

void AnimationCanvas::mousePressEvent(QMouseEvent* event)
{
	forceActiveFocus(Qt::MouseFocusReason);
	const QPointF pos = screenPoint(event->position());
	if (event->button() == Qt::RightButton) {
		// the menu of the cel under the mouse
		const int index = m_editor.cellAt(pos, false);
		if (index >= 0 && !m_editor.isPlaying()) {
			m_editor.setCurrentCell(index);
		}
		emit contextMenuRequested(event->position().x(), event->position().y());
		event->accept();
		return;
	}
	// with Ctrl the cel behind the selected one, with Shift all the cels move
	const auto modifiers = event->modifiers();
	m_editor.beginDrag(pos, modifiers & Qt::ControlModifier, modifiers & Qt::ShiftModifier);
	event->accept();
}

void AnimationCanvas::mouseDoubleClickEvent(QMouseEvent* event)
{
	// RPG Maker opens the cel properties: here the fields beside
	if (event->button() == Qt::LeftButton && m_editor.cellAt(screenPoint(event->position()), false) >= 0) {
		emit cellDoubleClicked();
	}
	event->accept();
}

void AnimationCanvas::mouseMoveEvent(QMouseEvent* event)
{
	if (m_editor.isDragging()) {
		m_editor.dragTo(screenPoint(event->position()));
	}
}

void AnimationCanvas::mouseReleaseEvent(QMouseEvent* event)
{
	if (m_editor.isDragging()) {
		m_editor.endDrag();
		changed(QStringLiteral("frames"));
	}
	event->accept();
}

void AnimationCanvas::keyPressEvent(QKeyEvent* event)
{
	// the arrows move the cel by a pixel (a step of the grid), with Shift all the cels
	const bool all = event->modifiers() & Qt::ShiftModifier;
	const int step = m_editor.moveStep();
	switch (event->key()) {
		case Qt::Key_Left: m_editor.moveCells(-step, 0, all); break;
		case Qt::Key_Right: m_editor.moveCells(step, 0, all); break;
		case Qt::Key_Up: m_editor.moveCells(0, -step, all); break;
		case Qt::Key_Down: m_editor.moveCells(0, step, all); break;
		case Qt::Key_Plus:
		case Qt::Key_PageUp:
			m_editor.changePattern(1);
			break;
		case Qt::Key_Minus:
		case Qt::Key_PageDown:
			m_editor.changePattern(-1);
			break;
		default:
			if (event->matches(QKeySequence::Cut)) {
				cutCell();
			} else if (event->matches(QKeySequence::Copy)) {
				copyCell();
			} else if (event->matches(QKeySequence::Paste)) {
				pasteCell();
			} else if (event->matches(QKeySequence::Delete) || event->key() == Qt::Key_Backspace) {
				deleteCell();
			} else {
				QQuickPaintedItem::keyPressEvent(event);
				return;
			}
	}
	event->accept();
}
