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

#include <QPixmap>
#include <QQuickPaintedItem>
#include "project_data_gadget.h"

/**
 * Base class for spritesheet based painted items.
 *
 * The picker mode decides whether it displays a single item (false) or is used
 * in a picker context (true): the cells of the sheet, the one chosen framed,
 * a click choosing another. In a picker the sheet is shown as in its file
 * (palette color 0 is the background, as in RPG Maker's pickers), else as
 * the game draws it (palette color 0 transparent where the folder makes it
 * so). Pixel art is scaled without blur.
 *
 * The picture: the file path, else the file of directory and filename found
 * in projectData (the game's, else the RTP's).
 */
class SpritePaintedItem : public QQuickPaintedItem {
	Q_OBJECT
	QML_ELEMENT
	Q_PROPERTY(QString filename READ filename WRITE setFilename NOTIFY filenameChanged)
	Q_PROPERTY(QString directory READ directory WRITE setDirectory NOTIFY directoryChanged)
	/** The file of the picture (its path), instead of filename */
	Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged)
	Q_PROPERTY(ProjectDataGadget* projectData READ projectData WRITE setProjectData NOTIFY projectDataChanged)
	Q_PROPERTY(bool pickerMode READ pickerMode WRITE setPickerMode NOTIFY pickerModeChanged)
	Q_PROPERTY(int cellIndex READ cellIndex WRITE setCellIndex NOTIFY cellIndexChanged)
	Q_PROPERTY(int cellWidth READ cellWidth WRITE setCellWidth NOTIFY cellWidthChanged)
	Q_PROPERTY(int cellHeight READ cellHeight WRITE setCellHeight NOTIFY cellHeightChanged)
	Q_PROPERTY(bool transparent READ transparent WRITE setTransparent NOTIFY transparentChanged)
	Q_PROPERTY(int skipCellsX READ skipCellsX WRITE setSkipCellsX NOTIFY skipCellsXChanged)
	Q_PROPERTY(int skipCellsY READ skipCellsY WRITE setSkipCellsY NOTIFY skipCellsYChanged)
	Q_PROPERTY(int gridColumns READ gridColumns WRITE setGridColumns NOTIFY gridColumnsChanged)
	/** The picture was found and read (false: no file, or one missing from the game and the RTP) */
	Q_PROPERTY(bool loaded READ loaded NOTIFY loadedChanged)

public:
	explicit SpritePaintedItem(QQuickItem* parent = nullptr);

	QString filename() const;
	void setFilename(const QString& filename);

	bool loaded() const;

	QString directory() const;
	void setDirectory(const QString& directory);

	QString path() const;
	void setPath(const QString& path);

	ProjectDataGadget* projectData() const;
	void setProjectData(ProjectDataGadget* projectData);

	bool pickerMode() const;
	void setPickerMode(bool pickerMode);

	int cellIndex() const;
	void setCellIndex(int cellIndex);

	int cellWidth() const;
	void setCellWidth(int cellWidth);

	int cellHeight() const;
	void setCellHeight(int cellHeight);

	bool transparent() const;
	void setTransparent(bool transparent);

	int cellOffsetX() const;
	void setCellOffsetX(int cellOffsetX);

	int cellOffsetY() const;
	void setCellOffsetY(int cellOffsetY);

	int skipCellsX() const;
	void setSkipCellsX(int skipCellsX);

	int skipCellsY() const;
	void setSkipCellsY(int skipCellsY);

	int gridColumns() const;
	void setGridColumns(int gridColumns);

	void paint(QPainter* painter) override;

signals:
	void filenameChanged();
	void directoryChanged();
	void pathChanged();
	void projectDataChanged();
	void pickerModeChanged();
	void cellIndexChanged();
	void cellWidthChanged();
	void cellHeightChanged();
	void transparentChanged();
	void cellOffsetXChanged();
	void cellOffsetYChanged();
	void skipCellsXChanged();
	void skipCellsYChanged();
	void gridColumnsChanged();
	void loadedChanged();

protected:
	/** Compute the source rectangle in image coordinates for the current cell. */
	virtual QRectF sourceRect() const;

	void mousePressEvent(QMouseEvent *event) override;

	virtual void reload();
	/** The picture, from path or from the project (null: none) */
	virtual QPixmap loadImage() const;
	/** The file of the picture (path, else the one of the project) */
	QString imagePath() const;
	/** Frames the cell chosen in a picker */
	static void drawSelection(QPainter* painter, const QRectF& rect);

	QString m_filename;
	QString m_directory;
	QString m_path;
	ProjectDataGadget* m_projectData = nullptr;
	bool m_pickerMode = false;
	bool m_transparent = false;
	QPixmap m_image;
	bool m_loaded = false;

	int m_cellIndex = 0;
	int m_cellWidth = 0;
	int m_cellHeight = 0;
	int m_cellOffsetX = 0;
	int m_cellOffsetY = 0;
	int m_skipCellsX = 0;
	int m_skipCellsY = 0;
	int m_gridColumns = 4;
};

