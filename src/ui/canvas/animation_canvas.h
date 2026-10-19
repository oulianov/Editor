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

#include <QColor>
#include <QImage>
#include <QPointer>
#include <QQuickPaintedItem>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QtQmlIntegration>

#include "model/animation_editor.h"
#include "qmlbinding/json_view.h"

/**
 * The preview of a battle animation on its database page, and what the page
 * does to it (AnimationEditor), in a database window's copy: the battle
 * screen (320 x 240, the item's own size: scale it to show it larger) with
 * the frame shown over a monster of the database, its cels placed and
 * dragged with the mouse (Shift: all of them, Ctrl: the one behind), moved
 * with the arrows; the frames, cels and effect timings with RPG Maker's
 * Cut/Copy/Paste; the frame tools (AnimationTools); the playback, without
 * sound. The patterns of the graphic are drawn for the strip under it.
 */
class ProjectData;

class AnimationCanvas : public QQuickPaintedItem
{
	Q_OBJECT
	QML_ELEMENT
	/** The animation shown (its view, "/animations/0"), null for none */
	Q_PROPERTY(JsonView* entry READ entry WRITE setEntry NOTIFY entryChanged)
	/** Without an entry: this animation of the project open (from 1, 0
	 *  for none), as a preview (Show Battle Animation) */
	Q_PROPERTY(int animationId READ animationId WRITE setAnimationId NOTIFY entryChanged)
	/** The monster the preview shows the animation on (its index), -1: none */
	Q_PROPERTY(int target READ target WRITE setTarget NOTIFY targetChanged)
	Q_PROPERTY(int frameCount READ frameCount NOTIFY framesChanged)
	Q_PROPERTY(int currentFrame READ currentFrame WRITE setCurrentFrame NOTIFY currentFrameChanged)
	Q_PROPERTY(int currentCell READ currentCell WRITE setCurrentCell NOTIFY currentCellChanged)
	/** "Cel 1: pattern 3"... of the slots of the frame shown */
	Q_PROPERTY(QStringList cellTexts READ cellTexts NOTIFY cellsChanged)
	/** The current cel is used: its fields can be changed */
	Q_PROPERTY(bool cellEditable READ cellEditable NOTIFY currentCellChanged)
	/** The pattern selected (from 0) */
	Q_PROPERTY(int pattern READ pattern WRITE setPattern NOTIFY patternChanged)
	Q_PROPERTY(int patternCount READ patternCount NOTIFY graphicChanged)
	/** The patterns a cel may use: those of the graphic, any without it */
	Q_PROPERTY(int patternLimit READ patternLimit NOTIFY graphicChanged)
	/** The patterns of the graphic side by side, thumbSize a pattern, the selected one framed */
	Q_PROPERTY(QImage patterns READ patterns NOTIFY patternsChanged)
	Q_PROPERTY(int thumbSize READ thumbSize CONSTANT)
	/** The rows of the effect timings: [frame, sound effect, flash, shake] */
	Q_PROPERTY(QVariantList timingRows READ timingRows NOTIFY timingsChanged)
	Q_PROPERTY(int currentTiming READ currentTiming WRITE setCurrentTiming NOTIFY currentTimingChanged)
	Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
	Q_PROPERTY(bool useGrid READ useGrid WRITE setUseGrid NOTIFY useGridChanged)
	/** The cross where the cel coordinates start from is drawn (not in a mere preview) */
	Q_PROPERTY(bool showOrigin READ showOrigin WRITE setShowOrigin NOTIFY showOriginChanged)
	/** What an arrow key moves a cel by */
	Q_PROPERTY(int moveStep READ moveStep NOTIFY useGridChanged)
	/** Grows on every change of the cels: the fields shown read them again */
	Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)
	/** The frame of the selected pattern */
	Q_PROPERTY(QColor highlight READ highlight WRITE setHighlight NOTIFY patternsChanged)
	Q_PROPERTY(QColor patternBackground READ patternBackground WRITE setPatternBackground NOTIFY patternsChanged)

public:
	explicit AnimationCanvas(QQuickItem* parent = nullptr);

	JsonView* entry() const;
	void setEntry(JsonView* entry);
	int target() const;
	void setTarget(int target);
	int frameCount() const;
	int currentFrame() const;
	void setCurrentFrame(int index);
	int currentCell() const;
	void setCurrentCell(int index);
	QStringList cellTexts() const;
	bool cellEditable() const;
	int pattern() const;
	void setPattern(int index);
	int patternCount() const;
	int patternLimit() const;
	QImage patterns() const;
	int thumbSize() const;
	QVariantList timingRows() const;
	int currentTiming() const;
	void setCurrentTiming(int index);
	bool playing() const;
	bool useGrid() const;
	void setUseGrid(bool on);
	bool showOrigin() const;
	void setShowOrigin(bool show);
	int animationId() const;
	void setAnimationId(int id);
	int moveStep() const;
	int revision() const;
	QColor highlight() const;
	void setHighlight(const QColor& color);
	QColor patternBackground() const;
	void setPatternBackground(const QColor& color);

	/** The monster first shown: the first one with a graphic */
	Q_INVOKABLE int firstTarget() const;

	// frames
	Q_INVOKABLE void previousFrame();
	Q_INVOKABLE void nextFrame();
	Q_INVOKABLE void resizeFrames(int count);
	Q_INVOKABLE void clonePrevious();
	Q_INVOKABLE void insertFrame();
	Q_INVOKABLE void deleteFrame();
	Q_INVOKABLE void copyFrame();
	Q_INVOKABLE void cutFrame();
	Q_INVOKABLE void pasteFrame();
	Q_INVOKABLE void clearFrame();
	Q_INVOKABLE bool canPasteFrame() const;

	// cels: field is AnimationEditor::CellField (0 Pattern ... 8 Saturation)
	Q_INVOKABLE int cellValue(int field) const;
	Q_INVOKABLE void setCellValue(int field, int value);
	Q_INVOKABLE void deleteCell();
	Q_INVOKABLE void copyCell();
	Q_INVOKABLE void cutCell();
	Q_INVOKABLE void pasteCell();
	Q_INVOKABLE bool canPasteCell() const;
	Q_INVOKABLE void changePattern(int step);
	Q_INVOKABLE void moveCells(int dx, int dy, bool all);
	/** The cel under a point of the screen, -1 for none */
	Q_INVOKABLE int cellAt(qreal x, qreal y) const;

	// effect timings, as maps: frame (from 1), se {name, volume, tempo,
	// balance}, flash_scope, flash_red, flash_green, flash_blue,
	// flash_power, screen_shake
	Q_INVOKABLE QVariantMap timing(int index) const;
	/** A new timing, on the frame shown */
	Q_INVOKABLE QVariantMap newTiming() const;
	/** Replaces the timing at index (added when negative) */
	Q_INVOKABLE void putTiming(const QVariantMap& timing, int index);
	Q_INVOKABLE void deleteTiming();
	Q_INVOKABLE void copyTiming();
	Q_INVOKABLE void cutTiming();
	Q_INVOKABLE void pasteTiming();
	Q_INVOKABLE bool canPasteTiming() const;

	// the frame tools (AnimationTools), frames from 0
	Q_INVOKABLE int maxCells() const;
	Q_INVOKABLE void massClear(int first, int last);
	Q_INVOKABLE void massCopy(int first, int last, int start);
	Q_INVOKABLE void autocomplete(int first, int last, int first_cel, int last_cel, bool pattern, bool zoom, bool transparency);
	/** cels: the slots chosen; values: pattern (from 0), zoom, transparency, tone [r, g, b, saturation], each when set */
	Q_INVOKABLE void massModify(int first, int last, const QVariantList& cels, const QVariantMap& values);

	// the playback (silent)
	Q_INVOKABLE void togglePlay();
	Q_INVOKABLE void stop();

	void paint(QPainter* painter) override;

signals:
	void entryChanged();
	void targetChanged();
	void framesChanged();
	void currentFrameChanged();
	void cellsChanged();
	void currentCellChanged();
	void patternChanged();
	void graphicChanged();
	void patternsChanged();
	void timingsChanged();
	void currentTimingChanged();
	void playingChanged();
	void useGridChanged();
	void showOriginChanged();
	void revisionChanged();
	/** The right button on the screen (the cel under it selected first), at a point of the item */
	void contextMenuRequested(qreal x, qreal y);
	/** A double-click on a cel: RPG Maker opens its properties */
	void cellDoubleClicked();

protected:
	void mousePressEvent(QMouseEvent* event) override;
	void mouseDoubleClickEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void keyPressEvent(QKeyEvent* event) override;

private:
	/** The project of the entry, else the one open for animationId */
	ProjectData* projectData() const;
	/** The animation of the entry, again (the list may have moved) */
	void findAnimation();
	void loadGraphic();
	void loadTarget();
	/** Draws the screen again (on this thread: paint() only shows it) */
	void render();
	void renderPatterns();
	void onDataChanged(const QString& jsonPtr);
	/** The views of the database hear what the editor changed */
	void changed(const QString& path);
	void bump();
	QPointF screenPoint(const QPointF& pos) const;

	QPointer<JsonView> m_entry;
	AnimationEditor m_editor;
	int m_target = -1;
	QImage m_graphic;
	QImage m_battler;
	QImage m_screen;
	QImage m_patterns;
	QColor m_highlight = QColor(48, 140, 198);
	bool m_showOrigin = true;
	int m_animationId = 0;
	QColor m_patternBackground = Qt::white;
	int m_revision = 0;
	bool m_editing = false;
};
