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

#pragma once

#include <QObject>
#include <QPointF>
#include <QRectF>
#include <QStringList>
#include <vector>
#include <lcf/rpg/animation.h>

class QTimer;

/**
 * The editing of a battle animation apart from its page: the frame and the
 * cel shown, the cels placed, dragged and changed, the frames and the effect
 * timings with RPG Maker's Cut/Copy/Paste, and the playback. Nothing here
 * asks the user or draws: the page loads the graphic and the target monster
 * (it gives their heights), draws the preview with origin() and cellRect(),
 * and shows again what the signals say changed.
 *
 * A change of the frames or of the cels of the frame shown is told from the
 * largest to the smallest: framesChanged(), currentFrameChanged(),
 * cellsChanged(), currentCellChanged(), then previewChanged().
 * As in RPG Maker, what is cut or copied goes from one animation to another.
 */
class AnimationEditor : public QObject
{
	Q_OBJECT
	Q_PROPERTY(int frameCount READ frameCount NOTIFY framesChanged)
	Q_PROPERTY(int currentFrame READ currentFrame WRITE setCurrentFrame NOTIFY currentFrameChanged)
	Q_PROPERTY(int currentCell READ currentCell WRITE setCurrentCell NOTIFY currentCellChanged)
	Q_PROPERTY(int pattern READ pattern WRITE setPattern NOTIFY patternChanged)
	Q_PROPERTY(int currentTiming READ currentTiming WRITE setCurrentTiming NOTIFY currentTimingChanged)
	Q_PROPERTY(bool playing READ isPlaying NOTIFY playingChanged)
	Q_PROPERTY(bool useGrid READ useGrid WRITE setUseGrid NOTIFY useGridChanged)

public:
	/** The fields of a cel */
	enum CellField { Pattern, X, Y, Zoom, Transparency, Red, Green, Blue, Saturation };
	Q_ENUM(CellField)

	// the battle screen
	static constexpr int kScreenWidth = 320;
	static constexpr int kScreenHeight = 240;
	// patterns per row of an animation graphic, as the Player reads them
	static constexpr int kPatternColumns = 5;
	// the patterns a cel may use without the graphic
	static constexpr int kMaxPatterns = 999;
	// the step of Use Grid
	static constexpr int kGridStep = 8;
	// the Player shows a frame of an animation for 2 frames at 60 per second
	static constexpr int kPlayInterval = 1000 / 30;

	explicit AnimationEditor(QObject* parent = nullptr);

	lcf::rpg::Animation* animation() const;
	/** Shows the first frame of anim (none: nothing is edited); playing stops */
	void setAnimation(lcf::rpg::Animation* anim);

	/*
	 * The preview
	 */

	/** The side of a pattern: 96 pixels, 128 for a large animation */
	int cellSize() const;
	/** The height of the animation graphic, 0 without one: it gives the patterns */
	void setGraphicHeight(int height);
	/** The patterns of the graphic; 0 without it */
	int patternCount() const;
	/** The patterns a cel may use: those of the graphic, any without it */
	int patternLimit() const;
	/** The height of the monster the preview shows, 0 without one */
	void setTargetHeight(int height);
	/** Where the cel coordinates start from on the screen */
	QPointF origin() const;
	/** Where a cel is drawn on the screen */
	QRectF cellRect(const lcf::rpg::AnimationCellData& cell) const;
	/** The cel of the frame shown under a point of the screen: the top one,
	 *  or with behind the one below the current cel; -1 when none */
	int cellAt(const QPointF& pos, bool behind) const;
	/** Use Grid: cels are placed and moved by steps of kGridStep pixels */
	bool useGrid() const;
	void setUseGrid(bool on);
	/** A coordinate on the grid when it is used */
	int snap(double value) const;
	/** What an arrow key moves a cel by */
	int moveStep() const;

	/*
	 * Frames
	 */

	int frameCount() const;
	/** The frame shown, -1 without frames */
	int currentFrame() const;
	/** The same cel stays selected from frame to frame, when it is used */
	void setCurrentFrame(int index);
	const lcf::rpg::AnimationFrame* frame() const;
	void previousFrame();
	void nextFrame();
	/** Number of Frames: frames are added (empty) or removed at the end */
	void resizeFrames(int count);
	/** The cels of the previous frame replace those of the frame shown */
	void clonePrevious();
	/** A blank frame before the one shown; the effects stay on their frames */
	void insertFrame();
	/** Removes the frame shown, never the last one left */
	void deleteFrame();
	void copyFrame();
	/** Copy, then Clear */
	void cutFrame();
	void pasteFrame();
	/** Empties the frame shown */
	void clearFrame();
	static bool canPasteFrame();
	/** After a tool (AnimationTools) changed the frames */
	void framesEdited();

	/*
	 * Cels of the frame shown
	 */

	/** The cel slot selected, used or not; -1 when none */
	int currentCell() const;
	void setCurrentCell(int index);
	const lcf::rpg::AnimationCellData* cell() const;
	/** The current cel is used: its fields can be changed */
	bool isCellEditable() const;
	int cellCount() const;
	/** "Cel 1: pattern 3", or unused */
	QString cellText(int index) const;
	/** A field of the current cel (the pattern from 0, like cell_id) */
	int cellValue(CellField field) const;
	void setCellValue(CellField field, int value);
	/** In the first unused slot of the frame shown, then selected */
	void addCell(const lcf::rpg::AnimationCellData& cell);
	/** A deleted cel leaves an unused slot, so the next cels keep their number
	 *  (RPG Maker does the same); unused slots at the end are dropped */
	void deleteCell();
	void copyCell();
	/** Copy, then Delete */
	void cutCell();
	/** Not while playing */
	void pasteCell();
	static bool canPasteCell();
	/** The pattern of the current cel, step patterns further, kept in patternLimit() */
	void changePattern(int step);
	/** Moves the current cel, or all the cels of the frame; with the grid to
	 *  its next line in the direction of the move. Not while playing */
	void moveCells(int dx, int dy, bool all);

	/** The pattern selected: the one of the current cel, given to new cels */
	int pattern() const;
	/** Selects a pattern of the graphic, given to the current cel */
	void setPattern(int index);

	/**
	 * A press on the screen: the cel under it is selected and dragged (with
	 * behind the one behind the current cel, with all every cel of the frame
	 * moves); elsewhere a new cel of the selected pattern is placed and
	 * dragged. Not while playing.
	 */
	void beginDrag(const QPointF& pos, bool behind, bool all);
	void dragTo(const QPointF& pos);
	void endDrag();
	bool isDragging() const;

	/*
	 * Effect timings, kept sorted by frame
	 */

	/** The timing selected, -1 without timings */
	int currentTiming() const;
	void setCurrentTiming(int index);
	/** A new timing, on the frame shown */
	lcf::rpg::AnimationTiming newTiming() const;
	/** Replaces the timing at index (added when negative), then selects it */
	void putTiming(const lcf::rpg::AnimationTiming& timing, int index);
	void deleteTiming();
	void copyTiming();
	/** Copy, then Delete */
	void cutTiming();
	/** On the frame shown, like a new effect */
	void pasteTiming();
	static bool canPasteTiming();
	/** The columns of a timing in the list: frame, sound effect, flash, shake */
	static QStringList timingTexts(const lcf::rpg::AnimationTiming& timing);

	/*
	 * Playback
	 */

	bool isPlaying() const;
	/** From the first frame to the last, a frame every kPlayInterval ms */
	void play();
	void stop();
	void togglePlay();
	/** The next frame of the playback; it stops after the last */
	void step();

signals:
	/** Frames were added or removed, or a tool changed their cels */
	void framesChanged();
	/** Another frame is shown, or its cels were replaced */
	void currentFrameChanged();
	/** Cels of the frame shown were added or deleted */
	void cellsChanged();
	/** Another cel is selected, or the same slot of another frame */
	void currentCellChanged();
	/** Fields of the cels of the frame shown changed (moved, pattern...) */
	void cellEdited();
	void patternChanged();
	/** Timings were added, removed or changed */
	void timingsChanged();
	void currentTimingChanged();
	void playingChanged(bool playing);
	void useGridChanged(bool on);
	/** The preview must be drawn again */
	void previewChanged();

private:
	lcf::rpg::AnimationFrame* mutableFrame();
	lcf::rpg::AnimationCellData* mutableCell();
	/** The frame list from scratch, select (clamped) shown */
	void refreshFrames(int select);
	/** The frame index shown, and the cel kept selected */
	void showFrame(int index);
	/** The cels from scratch: select when used, else the first used cel */
	void refreshCells(int select);
	void selectCell(int select);
	/** The selected pattern follows the current cel */
	void followCell();
	/** From currentFrameChanged() to previewChanged() */
	void emitFrameShown();
	void refreshTimings(int select);
	void cellsEdited();

	lcf::rpg::Animation* m_anim = nullptr;
	int m_graphicHeight = 0;
	int m_targetHeight = 0;
	bool m_grid = false;
	int m_frame = -1;
	int m_cell = -1;
	int m_pattern = 0;
	int m_timing = -1;
	QTimer* m_playTimer = nullptr;

	bool m_dragging = false;
	/** Shift when the drag started: all the cels move, like in RPG Maker */
	bool m_dragAll = false;
	QPointF m_dragStart;
	std::vector<lcf::rpg::AnimationCellData> m_dragCells;
	QPointF m_dragOffset;
};
