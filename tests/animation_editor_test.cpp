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

// The editing of the battle animations without their pages: the frame
// tools (AnimationTools), the cels, frames, timings and playback of an
// animation (AnimationEditor) and the poses and previews of a battler
// animation (BattlerAnimationEditor), on data in memory. What is cut or
// copied stays from one test to the next, like from one animation to another.

#include <QSignalSpy>
#include <QTest>
#include "model/animation_editor.h"
#include "model/animation_tools.h"
#include "model/battler_animation_editor.h"

namespace {
lcf::rpg::AnimationCellData cel(int id, int pattern, int x, int y) {
	lcf::rpg::AnimationCellData data;
	data.ID = id;
	data.cell_id = pattern;
	data.x = x;
	data.y = y;
	return data;
}

lcf::rpg::AnimationCellData unused(int id) {
	auto data = cel(id, 0, 0, 0);
	data.valid = 0;
	return data;
}

lcf::rpg::AnimationFrame frame(int id, std::vector<lcf::rpg::AnimationCellData> cells) {
	lcf::rpg::AnimationFrame data;
	data.ID = id;
	data.cells = std::move(cells);
	return data;
}

lcf::rpg::AnimationTiming timing(int id, int frame) {
	lcf::rpg::AnimationTiming data;
	data.ID = id;
	data.frame = frame;
	return data;
}

/** Four frames: two cels, the second only, none, the first only; effects on
 *  the frames 1 and 3 */
lcf::rpg::Animation sample() {
	lcf::rpg::Animation anim;
	anim.ID = 1;
	anim.frames = {
		frame(1, {cel(1, 0, 0, 0), cel(2, 3, 40, 0)}),
		frame(2, {unused(1), cel(2, 5, 10, 10)}),
		frame(3, {}),
		frame(4, {cel(1, 2, -40, 20)}),
	};
	anim.timings = {timing(1, 1), timing(2, 3)};
	return anim;
}

std::vector<int> timingFrames(const lcf::rpg::Animation& anim) {
	std::vector<int> frames;
	for (const auto& data : anim.timings) frames.push_back(data.frame);
	return frames;
}

bool numbered(const lcf::rpg::Animation& anim) {
	for (size_t i = 0; i < anim.frames.size(); ++i) {
		if (anim.frames[i].ID != static_cast<int>(i) + 1) return false;
	}
	for (size_t i = 0; i < anim.timings.size(); ++i) {
		if (anim.timings[i].ID != static_cast<int>(i) + 1) return false;
	}
	return true;
}
}

class AnimationEditorTest : public QObject
{
	Q_OBJECT

private slots:
	void toolsCopyClear();
	void toolsAutocomplete();
	void toolsMassModify();
	void showsFrames();
	void addDeletePasteCels();
	void cellFields();
	void moveOnGrid();
	void geometry();
	void drag();
	void frames();
	void timings();
	void playback();
	void battlerSlotsAndRules();
	void battlerPreview();
};

void AnimationEditorTest::toolsCopyClear()
{
	auto anim = sample();
	QCOMPARE(AnimationTools::maxCells(anim), 2);
	QCOMPARE(AnimationTools::maxCells(lcf::rpg::Animation()), 1);

	// the range either way round
	AnimationTools::massClear(anim, 1, 0);
	QVERIFY(anim.frames[0].cells.empty());
	QVERIFY(anim.frames[1].cells.empty());
	QCOMPARE(anim.frames[3].cells.size(), size_t(1));

	// over the next frames, which the copy overlaps: frames are added
	anim = sample();
	const auto original = anim;
	AnimationTools::massCopy(anim, 0, 3, 2);
	QCOMPARE(anim.frames.size(), size_t(6));
	for (int i = 0; i < 4; ++i) {
		QVERIFY(anim.frames[2 + i].cells == original.frames[i].cells);
	}
	QVERIFY(anim.frames[0].cells == original.frames[0].cells);
	QVERIFY(numbered(anim));

	// never past the last frame RPG Maker allows
	anim = sample();
	AnimationTools::massCopy(anim, 0, 3, AnimationTools::kMaxFrames - 2);
	QCOMPARE(anim.frames.size(), size_t(AnimationTools::kMaxFrames));
	QVERIFY(anim.frames.back().cells == original.frames[1].cells);
	QVERIFY(anim.frames[5].cells.empty());
	QVERIFY(numbered(anim));

	// nothing to do without frames
	lcf::rpg::Animation empty;
	AnimationTools::massCopy(empty, 0, 1, 2);
	AnimationTools::massClear(empty, 0, 1);
	QVERIFY(empty.frames.empty());
}

void AnimationEditorTest::toolsAutocomplete()
{
	lcf::rpg::Animation anim;
	auto first = cel(1, 0, 0, 0);
	auto last = cel(1, 4, 40, -20);
	last.zoom = 200;
	last.transparency = 40;
	last.tone_red = 0;
	anim.frames = {frame(1, {first, cel(2, 1, 5, 5)}), frame(2, {}), frame(3, {}), frame(4, {}), frame(5, {last})};

	// the second cel is not in the last frame: it is left out
	AnimationTools::autocomplete(anim, 4, 0, 0, 1, {true, false, true});
	for (int f = 1; f <= 3; ++f) {
		const auto& cells = anim.frames[f].cells;
		QCOMPARE(cells.size(), size_t(1));
		QCOMPARE(cells[0].ID, 1);
		QCOMPARE(cells[0].x, 10 * f);
		QCOMPARE(cells[0].y, -5 * f);
		QCOMPARE(cells[0].cell_id, f);
		QCOMPARE(cells[0].transparency, 10 * f);
		// not carried, and the color: as in the first frame
		QCOMPARE(cells[0].zoom, 100);
		QCOMPARE(cells[0].tone_red, 100);
	}

	// a cel further in the frames comes after unused slots
	anim.frames = {frame(1, {first, cel(2, 0, 0, 0)}), frame(2, {}), frame(3, {unused(1), cel(2, 0, 30, 0)})};
	AnimationTools::autocomplete(anim, 0, 2, 1, 1);
	const auto& cells = anim.frames[1].cells;
	QCOMPARE(cells.size(), size_t(2));
	QCOMPARE(cells[0].valid, 0);
	QCOMPARE(cells[0].ID, 1);
	QCOMPARE(cells[1].valid, 1);
	QCOMPARE(cells[1].ID, 2);
	QCOMPARE(cells[1].x, 15);
}

void AnimationEditorTest::toolsMassModify()
{
	lcf::rpg::Animation anim;
	anim.frames = {
		frame(1, {cel(1, 0, 0, 0), cel(2, 1, 0, 0)}),
		frame(2, {unused(1), cel(2, 2, 0, 0)}),
		frame(3, {cel(1, 3, 0, 0)}),
	};
	const auto before = anim;
	AnimationTools::CelValues values;
	values.pattern = 7;
	values.tone = std::array<int, 4>{50, 60, 70, 80};
	// the second cel only
	AnimationTools::massModify(anim, 2, 0, {false, true}, values);
	QVERIFY(anim.frames[0].cells[0] == before.frames[0].cells[0]);
	QVERIFY(anim.frames[1].cells[0] == before.frames[1].cells[0]);
	QVERIFY(anim.frames[2].cells == before.frames[2].cells);
	for (int f : {0, 1}) {
		const auto& changed = anim.frames[f].cells[1];
		QCOMPARE(changed.cell_id, 7);
		QCOMPARE(changed.tone_red, 50);
		QCOMPARE(changed.tone_green, 60);
		QCOMPARE(changed.tone_blue, 70);
		QCOMPARE(changed.tone_gray, 80);
		// not given
		QCOMPARE(changed.zoom, 100);
		QCOMPARE(changed.transparency, 0);
	}

	// unused slots stay unused
	values = {};
	values.zoom = 300;
	AnimationTools::massModify(anim, 0, 2, {true, true}, values);
	QCOMPARE(anim.frames[1].cells[0].valid, 0);
	QCOMPARE(anim.frames[1].cells[0].zoom, 100);
	QCOMPARE(anim.frames[2].cells[0].zoom, 300);
}

void AnimationEditorTest::showsFrames()
{
	auto anim = sample();
	AnimationEditor editor;
	QSignalSpy frames(&editor, &AnimationEditor::framesChanged);
	QSignalSpy shown(&editor, &AnimationEditor::currentFrameChanged);
	QSignalSpy cells(&editor, &AnimationEditor::cellsChanged);
	QSignalSpy current(&editor, &AnimationEditor::currentCellChanged);
	QSignalSpy preview(&editor, &AnimationEditor::previewChanged);
	QSignalSpy timings(&editor, &AnimationEditor::timingsChanged);
	editor.setAnimation(&anim);
	QCOMPARE(frames.count(), 1);
	QCOMPARE(shown.count(), 1);
	QCOMPARE(cells.count(), 1);
	QCOMPARE(current.count(), 1);
	QCOMPARE(preview.count(), 1);
	QCOMPARE(timings.count(), 1);
	QCOMPARE(editor.animation(), &anim);
	QCOMPARE(editor.frameCount(), 4);
	QCOMPARE(editor.currentFrame(), 0);
	QCOMPARE(editor.frame(), &anim.frames[0]);
	QCOMPARE(editor.currentCell(), 0);
	QCOMPARE(editor.cellCount(), 2);
	QCOMPARE(editor.pattern(), 0);
	QCOMPARE(editor.currentTiming(), 0);
	QCOMPARE(editor.cellText(1), QStringLiteral("Cel 2: pattern 4"));

	// the same cel stays selected from frame to frame, the pattern follows it
	editor.setCurrentCell(1);
	QCOMPARE(editor.pattern(), 3);
	editor.setCurrentFrame(1);
	QCOMPARE(shown.count(), 2);
	QCOMPARE(editor.currentCell(), 1);
	QCOMPARE(editor.pattern(), 5);
	QCOMPARE(editor.cellText(0), QStringLiteral("Cel 1: unused"));

	// a frame without cels: no cel, the fields of a new one
	editor.setCurrentFrame(2);
	QCOMPARE(editor.currentCell(), -1);
	QVERIFY(!editor.cell());
	QVERIFY(!editor.isCellEditable());
	QCOMPARE(editor.cellValue(AnimationEditor::Zoom), 100);
	QCOMPARE(editor.cellValue(AnimationEditor::Pattern), 0);
	QCOMPARE(editor.pattern(), 5);

	// else the first cel used
	editor.setCurrentFrame(3);
	QCOMPARE(editor.currentCell(), 0);
	QCOMPARE(editor.pattern(), 2);

	editor.previousFrame();
	QCOMPARE(editor.currentFrame(), 2);
	editor.nextFrame();
	editor.nextFrame();
	QCOMPARE(editor.currentFrame(), 3);
	const int count = shown.count();
	editor.setCurrentFrame(9);
	editor.setCurrentFrame(3);
	QCOMPARE(shown.count(), count);

	editor.setAnimation(nullptr);
	QCOMPARE(editor.frameCount(), 0);
	QCOMPARE(editor.currentFrame(), -1);
	QCOMPARE(editor.currentCell(), -1);
	QCOMPARE(editor.currentTiming(), -1);
	QVERIFY(!editor.frame());
	editor.nextFrame();
	editor.insertFrame();
	editor.addCell(cel(1, 0, 0, 0));
	QCOMPARE(editor.frameCount(), 0);
}

void AnimationEditorTest::addDeletePasteCels()
{
	auto anim = sample();
	AnimationEditor editor;
	editor.setAnimation(&anim);
	editor.setCurrentFrame(1);
	const auto& cells = anim.frames[1].cells;

	// in the first unused slot, then selected
	QSignalSpy changed(&editor, &AnimationEditor::cellsChanged);
	editor.addCell(cel(0, 9, 5, 6));
	QCOMPARE(changed.count(), 1);
	QCOMPARE(cells.size(), size_t(2));
	QCOMPARE(cells[0].valid, 1);
	QCOMPARE(cells[0].ID, 1);
	QCOMPARE(cells[0].cell_id, 9);
	QCOMPARE(editor.currentCell(), 0);
	QCOMPARE(editor.pattern(), 9);
	editor.addCell(cel(0, 1, 0, 0));
	QCOMPARE(cells.size(), size_t(3));
	QCOMPARE(cells[2].ID, 3);
	QCOMPARE(editor.currentCell(), 2);

	// the next cels keep their number: an unused slot is left
	editor.setCurrentCell(0);
	editor.deleteCell();
	QCOMPARE(cells.size(), size_t(3));
	QCOMPARE(cells[0].valid, 0);
	QCOMPARE(editor.currentCell(), 1);
	// unused slots at the end are dropped
	editor.setCurrentCell(2);
	editor.deleteCell();
	QCOMPARE(cells.size(), size_t(2));
	QCOMPARE(editor.currentCell(), 1);
	editor.deleteCell();
	QVERIFY(cells.empty());
	QCOMPARE(editor.currentCell(), -1);

	// Copy, Paste and Cut
	editor.setCurrentFrame(0);
	editor.setCurrentCell(1);
	editor.copyCell();
	QVERIFY(AnimationEditor::canPasteCell());
	editor.setCurrentFrame(2);
	editor.pasteCell();
	QCOMPARE(anim.frames[2].cells.size(), size_t(1));
	QCOMPARE(anim.frames[2].cells[0].cell_id, 3);
	QCOMPARE(anim.frames[2].cells[0].x, 40);
	QCOMPARE(anim.frames[2].cells[0].ID, 1);
	QCOMPARE(editor.currentCell(), 0);
	editor.cutCell();
	QVERIFY(anim.frames[2].cells.empty());
	editor.pasteCell();
	QCOMPARE(anim.frames[2].cells.size(), size_t(1));
}

void AnimationEditorTest::cellFields()
{
	auto anim = sample();
	AnimationEditor editor;
	editor.setAnimation(&anim);
	auto& data = anim.frames[0].cells[0];
	QSignalSpy edited(&editor, &AnimationEditor::cellEdited);
	QSignalSpy patterns(&editor, &AnimationEditor::patternChanged);

	editor.setCellValue(AnimationEditor::X, 12);
	QCOMPARE(data.x, 12);
	QCOMPARE(edited.count(), 1);
	editor.setCellValue(AnimationEditor::X, 12);
	QCOMPARE(edited.count(), 1);
	editor.setCellValue(AnimationEditor::Red, 150);
	QCOMPARE(data.tone_red, 150);
	QCOMPARE(editor.cellValue(AnimationEditor::Red), 150);
	editor.setCellValue(AnimationEditor::Pattern, 4);
	QCOMPARE(data.cell_id, 4);
	QCOMPARE(editor.pattern(), 4);
	QCOMPARE(patterns.count(), 1);

	// without the graphic, any pattern
	QCOMPARE(editor.patternCount(), 0);
	QCOMPARE(editor.patternLimit(), AnimationEditor::kMaxPatterns);
	editor.changePattern(2000);
	QCOMPARE(data.cell_id, AnimationEditor::kMaxPatterns - 1);
	// two rows of five patterns
	editor.setGraphicHeight(2 * 96);
	QCOMPARE(editor.patternCount(), 10);
	editor.changePattern(1);
	QCOMPARE(data.cell_id, 9);
	editor.changePattern(-20);
	QCOMPARE(data.cell_id, 0);
	editor.setPattern(3);
	QCOMPARE(data.cell_id, 3);
	QCOMPARE(editor.pattern(), 3);
	editor.setPattern(10);
	QCOMPARE(editor.pattern(), 3);
	// larger patterns: one row
	anim.large = true;
	QCOMPARE(editor.cellSize(), 128);
	QCOMPARE(editor.patternCount(), 5);
	anim.large = false;

	// without a cel the pattern is the one of the next cel placed
	editor.setCurrentFrame(2);
	const int count = edited.count();
	editor.setCellValue(AnimationEditor::X, 5);
	editor.setPattern(4);
	QCOMPARE(edited.count(), count);
	QCOMPARE(editor.pattern(), 4);
}

void AnimationEditorTest::moveOnGrid()
{
	auto anim = sample();
	AnimationEditor editor;
	editor.setAnimation(&anim);
	const auto& cells = anim.frames[0].cells;

	editor.moveCells(1, -1, false);
	QCOMPARE(QPoint(cells[0].x, cells[0].y), QPoint(1, -1));
	QCOMPARE(QPoint(cells[1].x, cells[1].y), QPoint(40, 0));
	editor.moveCells(2, 3, true);
	QCOMPARE(QPoint(cells[0].x, cells[0].y), QPoint(3, 2));
	QCOMPARE(QPoint(cells[1].x, cells[1].y), QPoint(42, 3));

	// to the next line of the grid in the direction of the move
	QSignalSpy grid(&editor, &AnimationEditor::useGridChanged);
	editor.setUseGrid(true);
	QCOMPARE(grid.count(), 1);
	QCOMPARE(editor.moveStep(), AnimationEditor::kGridStep);
	editor.moveCells(8, 0, false);
	QCOMPARE(QPoint(cells[0].x, cells[0].y), QPoint(8, 2));
	editor.moveCells(-8, -8, false);
	QCOMPARE(QPoint(cells[0].x, cells[0].y), QPoint(0, 0));
	QCOMPARE(editor.snap(11.0), 8);
	QCOMPARE(editor.snap(12.0), 16);
	QCOMPARE(editor.snap(-12.0), -16);
	editor.setUseGrid(false);
	QCOMPARE(editor.snap(11.4), 11);
	QCOMPARE(editor.moveStep(), 1);

	// not while playing
	editor.play();
	editor.moveCells(5, 5, true);
	QCOMPARE(QPoint(cells[0].x, cells[0].y), QPoint(0, 0));
	editor.stop();
}

void AnimationEditorTest::geometry()
{
	auto anim = sample();
	AnimationEditor editor;
	editor.setAnimation(&anim);
	// low (the default), without a monster: half a cel below the center
	QCOMPARE(editor.cellSize(), 96);
	QCOMPARE(editor.origin(), QPointF(160, 144));
	editor.setTargetHeight(80);
	QCOMPARE(editor.origin(), QPointF(160, 160));
	anim.position = lcf::rpg::Animation::Position_up;
	QCOMPARE(editor.origin(), QPointF(160, 80));
	anim.position = lcf::rpg::Animation::Position_middle;
	QCOMPARE(editor.origin(), QPointF(160, 120));
	// on the whole screen
	anim.scope = lcf::rpg::Animation::Scope_screen;
	QCOMPARE(editor.origin(), QPointF(160, 80));
	anim.scope = lcf::rpg::Animation::Scope_target;

	auto half = cel(1, 0, 10, -10);
	half.zoom = 50;
	QCOMPARE(editor.cellRect(half), QRectF(146, 86, 48, 48));

	// the top cel (drawn last), or with behind the one below the current cel
	QCOMPARE(editor.cellAt(QPointF(120, 120), false), 0);
	QCOMPARE(editor.cellAt(QPointF(230, 120), false), 1);
	QCOMPARE(editor.cellAt(QPointF(180, 120), false), 1);
	editor.setCurrentCell(1);
	QCOMPARE(editor.cellAt(QPointF(180, 120), true), 0);
	editor.setCurrentCell(0);
	QCOMPARE(editor.cellAt(QPointF(180, 120), true), 1);
	QCOMPARE(editor.cellAt(QPointF(5, 5), false), -1);
}

void AnimationEditorTest::drag()
{
	auto anim = sample();
	anim.position = lcf::rpg::Animation::Position_middle;
	AnimationEditor editor;
	editor.setAnimation(&anim);
	auto& cells = anim.frames[0].cells;

	// grabbed off its center, the cel keeps the offset
	editor.beginDrag(QPointF(130, 125), false, false);
	QVERIFY(editor.isDragging());
	QCOMPARE(editor.currentCell(), 0);
	editor.dragTo(QPointF(140, 135));
	QCOMPARE(QPoint(cells[0].x, cells[0].y), QPoint(10, 10));
	editor.endDrag();
	QVERIFY(!editor.isDragging());
	editor.dragTo(QPointF(0, 0));
	QCOMPARE(QPoint(cells[0].x, cells[0].y), QPoint(10, 10));

	// with Shift every cel moves, here by steps of the grid
	editor.setUseGrid(true);
	editor.beginDrag(QPointF(240, 120), false, true);
	QCOMPARE(editor.currentCell(), 1);
	editor.dragTo(QPointF(251, 117));
	QCOMPARE(QPoint(cells[0].x, cells[0].y), QPoint(18, 10));
	QCOMPARE(QPoint(cells[1].x, cells[1].y), QPoint(48, 0));
	editor.endDrag();
	editor.setUseGrid(false);

	// elsewhere a new cel of the selected pattern, dragged at once
	QCOMPARE(editor.pattern(), 3);
	editor.beginDrag(QPointF(20, 30), false, false);
	QCOMPARE(cells.size(), size_t(3));
	QCOMPARE(editor.currentCell(), 2);
	QCOMPARE(cells[2].cell_id, 3);
	QCOMPARE(QPoint(cells[2].x, cells[2].y), QPoint(-140, -90));
	editor.dragTo(QPointF(28, 31));
	QCOMPARE(QPoint(cells[2].x, cells[2].y), QPoint(-132, -89));
	editor.endDrag();

	// not while playing
	editor.play();
	editor.beginDrag(QPointF(20, 30), false, false);
	QVERIFY(!editor.isDragging());
	QCOMPARE(cells.size(), size_t(3));
	editor.stop();
}

void AnimationEditorTest::frames()
{
	auto anim = sample();
	AnimationEditor editor;
	editor.setAnimation(&anim);
	QSignalSpy frames(&editor, &AnimationEditor::framesChanged);

	// Number of Frames
	editor.setCurrentFrame(3);
	editor.resizeFrames(6);
	QCOMPARE(frames.count(), 1);
	QCOMPARE(editor.frameCount(), 6);
	QCOMPARE(editor.currentFrame(), 3);
	QVERIFY(numbered(anim));
	editor.resizeFrames(2);
	QCOMPARE(editor.currentFrame(), 1);

	// the effects stay on their frames
	anim = sample();
	editor.setAnimation(&anim);
	editor.setCurrentFrame(1);
	const auto second = anim.frames[1].cells;
	editor.insertFrame();
	QCOMPARE(editor.frameCount(), 5);
	QCOMPARE(editor.currentFrame(), 1);
	QVERIFY(anim.frames[1].cells.empty());
	QVERIFY(anim.frames[2].cells == second);
	QVERIFY(numbered(anim));
	QCOMPARE(timingFrames(anim), std::vector<int>({1, 4}));
	editor.deleteFrame();
	QCOMPARE(editor.frameCount(), 4);
	QVERIFY(anim.frames[1].cells == second);
	QCOMPARE(timingFrames(anim), std::vector<int>({1, 3}));

	// Clone Previous, not on the first frame
	editor.clonePrevious();
	QVERIFY(anim.frames[1].cells == anim.frames[0].cells);
	QCOMPARE(editor.currentCell(), 1);
	editor.setCurrentFrame(0);
	const auto first = anim.frames[0].cells;
	editor.clonePrevious();
	QVERIFY(anim.frames[0].cells == first);

	// Copy, Paste, Clear and Cut
	editor.setCurrentFrame(3);
	editor.copyFrame();
	QVERIFY(AnimationEditor::canPasteFrame());
	editor.setCurrentFrame(2);
	editor.pasteFrame();
	QVERIFY(anim.frames[2].cells == anim.frames[3].cells);
	QCOMPARE(editor.currentCell(), 0);
	editor.clearFrame();
	QVERIFY(anim.frames[2].cells.empty());
	editor.setCurrentFrame(0);
	editor.cutFrame();
	QVERIFY(anim.frames[0].cells.empty());
	editor.setCurrentFrame(2);
	editor.pasteFrame();
	QVERIFY(anim.frames[2].cells == first);

	// a frame is always left, and never more than RPG Maker allows
	editor.resizeFrames(1);
	editor.deleteFrame();
	QCOMPARE(editor.frameCount(), 1);
	editor.resizeFrames(AnimationTools::kMaxFrames + 5);
	QCOMPARE(editor.frameCount(), AnimationTools::kMaxFrames);
	editor.insertFrame();
	QCOMPARE(editor.frameCount(), AnimationTools::kMaxFrames);

	// after a tool, the frame list again
	const int count = frames.count();
	AnimationTools::massClear(anim, 0, 1);
	editor.framesEdited();
	QCOMPARE(frames.count(), count + 1);
}

void AnimationEditorTest::timings()
{
	auto anim = sample();
	AnimationEditor editor;
	editor.setAnimation(&anim);
	QSignalSpy changed(&editor, &AnimationEditor::timingsChanged);

	// a new effect on the frame shown, kept sorted by frame
	editor.setCurrentFrame(1);
	auto added = editor.newTiming();
	QCOMPARE(added.frame, 2);
	added.flash_scope = lcf::rpg::AnimationTiming::FlashScope_target;
	editor.putTiming(added, -1);
	QCOMPARE(changed.count(), 1);
	QCOMPARE(timingFrames(anim), std::vector<int>({1, 2, 3}));
	QCOMPARE(editor.currentTiming(), 1);
	QVERIFY(numbered(anim));

	// moved past the others, it stays selected
	auto edited = anim.timings[0];
	edited.frame = 4;
	editor.putTiming(edited, 0);
	QCOMPARE(timingFrames(anim), std::vector<int>({2, 3, 4}));
	QCOMPARE(editor.currentTiming(), 2);
	QVERIFY(numbered(anim));

	editor.deleteTiming();
	QCOMPARE(timingFrames(anim), std::vector<int>({2, 3}));
	QCOMPARE(editor.currentTiming(), 1);

	// pasted on the frame shown
	editor.setCurrentTiming(0);
	editor.copyTiming();
	QVERIFY(AnimationEditor::canPasteTiming());
	editor.setCurrentFrame(3);
	editor.pasteTiming();
	QCOMPARE(timingFrames(anim), std::vector<int>({2, 3, 4}));
	QCOMPARE(editor.currentTiming(), 2);
	QCOMPARE(anim.timings[2].flash_scope, int(lcf::rpg::AnimationTiming::FlashScope_target));
	editor.cutTiming();
	QCOMPARE(timingFrames(anim), std::vector<int>({2, 3}));
	QCOMPARE(editor.currentTiming(), 1);
	editor.setCurrentTiming(7);
	QCOMPARE(editor.currentTiming(), 1);

	// the columns of the list
	lcf::rpg::AnimationTiming flash;
	flash.frame = 3;
	flash.se.name = "Explosion";
	flash.flash_scope = lcf::rpg::AnimationTiming::FlashScope_target;
	flash.flash_green = 10;
	flash.flash_blue = 0;
	flash.flash_power = 20;
	flash.screen_shake = lcf::rpg::AnimationTiming::ScreenShake_screen;
	QCOMPARE(AnimationEditor::timingTexts(flash), QStringList({QStringLiteral("3"), QStringLiteral("Explosion"),
		QStringLiteral("Target (31, 10, 0, 20)"), QStringLiteral("Screen")}));
	lcf::rpg::AnimationTiming quiet;
	quiet.frame = 1;
	quiet.se.name.clear();
	QCOMPARE(AnimationEditor::timingTexts(quiet), QStringList({QStringLiteral("1"), QStringLiteral("(OFF)"),
		QStringLiteral("None"), QStringLiteral("None")}));
}

void AnimationEditorTest::playback()
{
	auto anim = sample();
	AnimationEditor editor;
	editor.setAnimation(&anim);
	editor.setCurrentFrame(2);
	QSignalSpy playing(&editor, &AnimationEditor::playingChanged);

	// from the first frame
	editor.play();
	QVERIFY(editor.isPlaying());
	QCOMPARE(editor.currentFrame(), 0);
	QCOMPARE(playing.count(), 1);
	QCOMPARE(playing.last().at(0).toBool(), true);
	// nothing is placed meanwhile
	editor.copyCell();
	editor.pasteCell();
	QCOMPARE(anim.frames[0].cells.size(), size_t(2));

	// it stops after the last frame
	editor.step();
	editor.step();
	editor.step();
	QCOMPARE(editor.currentFrame(), 3);
	QVERIFY(editor.isPlaying());
	editor.step();
	QVERIFY(!editor.isPlaying());
	QCOMPARE(editor.currentFrame(), 3);
	QCOMPARE(playing.count(), 2);
	QCOMPARE(playing.last().at(0).toBool(), false);

	// played by the timer
	editor.togglePlay();
	QVERIFY(editor.isPlaying());
	QTRY_VERIFY_WITH_TIMEOUT(!editor.isPlaying(), 5000);
	QCOMPARE(editor.currentFrame(), 3);

	// another animation stops it
	editor.play();
	auto other = sample();
	editor.setAnimation(&other);
	QVERIFY(!editor.isPlaying());

	// nothing to play
	lcf::rpg::Animation empty;
	editor.setAnimation(&empty);
	editor.play();
	QVERIFY(!editor.isPlaying());
}

void AnimationEditorTest::battlerSlotsAndRules()
{
	lcf::rpg::BattlerAnimation anim;
	anim.poses.resize(3);
	anim.poses[lcf::rpg::BattlerAnimation::Pose_AttackRight].battler_name = "Hero";
	anim.poses[lcf::rpg::BattlerAnimation::Pose_AttackLeft].animation_type = lcf::rpg::BattlerAnimationPose::AnimType_battle;
	BattlerAnimationEditor editor;

	// not RPG Maker 2003: left as it is
	editor.setAnimation(&anim, false);
	QCOMPARE(anim.poses.size(), size_t(3));
	QCOMPARE(editor.currentPose(), 0);
	QCOMPARE(editor.currentWeapon(), -1);
	QVERIFY(!editor.weapon());

	// shorter lists are filled
	editor.setAnimation(&anim, true);
	QCOMPARE(anim.poses.size(), size_t(BattlerAnimationEditor::kSlotCount));
	QCOMPARE(anim.weapons.size(), size_t(BattlerAnimationEditor::kSlotCount));
	QCOMPARE(anim.poses.back().ID, BattlerAnimationEditor::kSlotCount);
	QCOMPARE(anim.weapons.back().ID, BattlerAnimationEditor::kSlotCount);
	QCOMPARE(editor.currentWeapon(), 0);
	QCOMPARE(editor.weapon(), &anim.weapons[0]);

	QSignalSpy poses(&editor, &BattlerAnimationEditor::currentPoseChanged);
	editor.setCurrentPose(5);
	QCOMPARE(poses.count(), 1);
	QCOMPARE(editor.pose(), &anim.poses[5]);
	editor.setCurrentPose(40);
	QCOMPARE(editor.currentPose(), 5);
	QCOMPARE(poses.count(), 1);

	QVERIFY(BattlerAnimationEditor::isPoseNameFixed(11));
	QVERIFY(!BattlerAnimationEditor::isPoseNameFixed(12));
	QVERIFY(BattlerAnimationEditor::usesAnimation(anim.poses[2]));
	QVERIFY(!BattlerAnimationEditor::usesAnimation(anim.poses[1]));

	// under the weapon, the attack pose of a hand when it is a character pose
	QCOMPARE(editor.handPose(BattlerAnimationEditor::RightHand), &anim.poses[1]);
	QVERIFY(!editor.handPose(BattlerAnimationEditor::LeftHand));
	QVERIFY(!editor.handPose(BattlerAnimationEditor::WeaponOnly));

	// the speed: frames at 60 per second
	editor.setSpeed(lcf::rpg::BattlerAnimation::Speed_fast);
	QCOMPARE(anim.speed, int(lcf::rpg::BattlerAnimation::Speed_fast));
	QCOMPARE(BattlerAnimationEditor::interval(lcf::rpg::BattlerAnimation::Speed_fast), 133);

	editor.setAnimation(nullptr, true);
	QCOMPARE(editor.currentPose(), -1);
	QVERIFY(!editor.pose());
	QVERIFY(!editor.handPose(BattlerAnimationEditor::RightHand));
}

void AnimationEditorTest::battlerPreview()
{
	BattlerAnimationEditor editor;
	QSignalSpy stepped(&editor, &BattlerAnimationEditor::stepped);
	// the three frames of a row played 0 1 2 1; with both hands, a cycle each
	const int columns[] = {0, 1, 2, 1, 0, 1, 2, 1, 0};
	const int hands[] = {0, 0, 0, 0, 1, 1, 1, 1, 0};
	for (int i = 0; i < 9; ++i) {
		QCOMPARE(editor.step(), i);
		QCOMPARE(editor.frameColumn(), columns[i]);
		QCOMPARE(editor.shownHand(BattlerAnimationEditor::BothHands), hands[i]);
		QCOMPARE(editor.shownHand(BattlerAnimationEditor::LeftHand), int(BattlerAnimationEditor::LeftHand));
		QCOMPARE(editor.shownHand(BattlerAnimationEditor::WeaponOnly), int(BattlerAnimationEditor::WeaponOnly));
		editor.nextStep();
	}
	QCOMPARE(stepped.count(), 9);
	editor.restart();
	QCOMPARE(editor.step(), 0);
	QVERIFY(BattlerAnimationEditor::isWeaponBehind(BattlerAnimationEditor::LeftHand));
	QVERIFY(!BattlerAnimationEditor::isWeaponBehind(BattlerAnimationEditor::RightHand));

	// the rows of a graphic, 4 by 2
	constexpr int size = BattlerAnimationEditor::kBattlerSize;
	QCOMPARE(BattlerAnimationEditor::rowAt(QPoint(0, 0), size), 0);
	QCOMPARE(BattlerAnimationEditor::rowAt(QPoint(3 * size + 1, size), size), 7);
	QCOMPARE(BattlerAnimationEditor::rowAt(QPoint(size, size - 1), size), 1);
	QCOMPARE(BattlerAnimationEditor::rowAt(QPoint(-1, 0), size), -1);
	QCOMPARE(BattlerAnimationEditor::rowAt(QPoint(4 * size, 0), size), -1);
	QCOMPARE(BattlerAnimationEditor::rowAt(QPoint(0, 2 * size), size), -1);
}

QTEST_GUILESS_MAIN(AnimationEditorTest)
#include "animation_editor_test.moc"
