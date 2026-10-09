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

#include <QImage>
#include <QPixmap>
#include <QPointF>
#include <QWidget>
#include <lcf/rpg/animation.h>

class ProjectData;
class QComboBox;
class QGraphicsScene;
class QGraphicsView;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QTimer;
class QTreeWidget;

/**
 * The Battle Animations page of the database, like RPG Maker's: the graphic,
 * the scope and the Y axis line, the frames with their cels (a pattern of the
 * graphic placed, zoomed, faded and tinted around the target) and the effect
 * timings (sound effect, flash and shake on a frame). The preview shows the
 * current frame over a monster of the database.
 */
class BattleAnimationWidget : public QWidget
{
	Q_OBJECT

public:
	using value_type = lcf::rpg::Animation;

	explicit BattleAnimationWidget(ProjectData& project, QWidget *parent = nullptr);

	void setData(lcf::rpg::Animation* anim);

	/** A graphic of the game, the first palette color transparent as in the Player */
	static QPixmap loadGraphic(const ProjectData& project, const QString& dir, const QString& name);

protected:
	bool eventFilter(QObject* watched, QEvent* event) override;

private:
	lcf::rpg::AnimationFrame* currentFrame();
	lcf::rpg::AnimationCellData* currentCell();
	int cellSize() const;
	int patternCount() const;
	/** Where the cel coordinates start from in the preview */
	QPointF origin() const;
	QRectF cellRect(const lcf::rpg::AnimationCellData& cell) const;
	QString cellText(int index) const;

	void loadAnimationGraphic();
	void loadTarget();
	void refreshFrames(int select);
	void frameChanged();
	void resizeFrames(int count);
	void refreshCells(int select);
	void refreshCellFields();
	void deleteCell();
	void refreshTimings(int select);
	void editTiming(int index);
	bool timingDialog(lcf::rpg::AnimationTiming& timing);
	void togglePlay();
	void redraw();
	void redrawPatterns();
	void canvasPressed(const QPointF& pos);
	void canvasDragged(const QPointF& pos);
	void patternClicked(const QPointF& pos);

	ProjectData& m_project;
	lcf::rpg::Animation* m_anim = nullptr;
	bool m_rpg2k3 = false;
	QImage m_graphic;
	QPixmap m_battler;
	int m_pattern = 0;
	bool m_dragging = false;
	QPointF m_dragOffset;

	QLineEdit* m_name = nullptr;
	QLineEdit* m_file = nullptr;
	QComboBox* m_scope = nullptr;
	QComboBox* m_position = nullptr;
	QComboBox* m_target = nullptr;
	QListWidget* m_frames = nullptr;
	QPushButton* m_frameCount = nullptr;
	QPushButton* m_clone = nullptr;
	QGraphicsScene* m_scene = nullptr;
	QGraphicsView* m_view = nullptr;
	QPushButton* m_back = nullptr;
	QPushButton* m_next = nullptr;
	QLabel* m_frameLabel = nullptr;
	QPushButton* m_play = nullptr;
	QTimer* m_playTimer = nullptr;
	QGraphicsScene* m_patternScene = nullptr;
	QGraphicsView* m_patternView = nullptr;
	QComboBox* m_cell = nullptr;
	QPushButton* m_deleteCell = nullptr;
	QSpinBox* m_cellPattern = nullptr;
	QSpinBox* m_cellX = nullptr;
	QSpinBox* m_cellY = nullptr;
	QSpinBox* m_cellZoom = nullptr;
	QSpinBox* m_cellTransparency = nullptr;
	QSpinBox* m_cellRed = nullptr;
	QSpinBox* m_cellGreen = nullptr;
	QSpinBox* m_cellBlue = nullptr;
	QSpinBox* m_cellSaturation = nullptr;
	QTreeWidget* m_timings = nullptr;
	QPushButton* m_editTiming = nullptr;
	QPushButton* m_deleteTiming = nullptr;
};
