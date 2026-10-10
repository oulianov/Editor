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

#include <QDialog>
#include <QImage>

#include "import_item.h"

class PreviewZoom;
class QGraphicsScene;

namespace Ui {
class ImportImageDialog;
}

/**
 * Chooses the transparent color of an image imported into the game, as the
 * Preview window of RPG Maker's Resource Manager: a click on the image picks
 * the color.
 *
 * The games use palette images (256 colors at most) where palette index 0 is
 * transparent: for those, image() moves the chosen color to index 0. A true
 * color image (EasyRPG extensions) gets the chosen color as alpha 0 instead.
 */
class ImportImageDialog : public QDialog
{
	Q_OBJECT

public:
	explicit ImportImageDialog(const QImage& image, QWidget *parent = nullptr);
	explicit ImportImageDialog(const QString& filepath, QWidget *parent = nullptr);
	~ImportImageDialog();

	/** The image with the chosen transparent color */
	QImage image() const;

	/** Palette index (palette images) of the transparent color */
	int transparentIndex() const { return m_keyIndex; }

	/** A PNG, BMP or XYZ file; palette images (and XYZ) as Format_Indexed8 */
	static QImage loadImage(const QString& path);

	/** Writes a palette image in RPG Maker's XYZ format (zlib compressed) */
	static bool saveXyz(const QImage& image, const QString& path);

	/** The same pixels as a palette image, or a null image above 256 colors.
	 *  Fully transparent pixels become index 0, otherwise colors are numbered
	 *  in the order they appear, so the top left pixel's color is index 0. */
	static QImage toIndexed(const QImage& image);

	/** Reduced to 256 colors (dithered); transparent pixels become index 0 */
	static QImage reduceColors(const QImage& image);

	/** As the game shows it: the pixels of palette index 0 transparent */
	static QImage withTransparentIndex0(const QImage& image);

	/** Palette images: the number of colors, else 0 */
	static int paletteSize(const QImage& image);

private:
	void init();
	void pick(QPoint pixel);
	void updateView();
	QImage displayImage() const;

	Ui::ImportImageDialog *ui;

	QImage m_source;
	PreviewZoom* m_zoom = nullptr;
	int m_keyIndex = 0;
	bool m_hasKeyColor = false;
	QRgb m_keyColor = 0;
	QGraphicsScene *m_scene = nullptr;
	ImportItem *m_pixmap = nullptr;
};
