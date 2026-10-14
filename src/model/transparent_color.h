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
#include <QObject>
#include <QSize>
#include <QString>

/**
 * The transparent color of an image imported into the game, as the Preview
 * window of RPG Maker's Resource Manager chooses it: a click on the image
 * picks the color (the Import Image window).
 *
 * The games use palette images (256 colors at most) where palette index 0
 * is transparent: for those, result() moves the chosen color to index 0
 * (index 0 at first). A true color image (EasyRPG extensions) gets the
 * chosen color as alpha 0 instead (none at first).
 *
 * The texts are in the context ImportImageDialog, as ImportImageWindow.qml's.
 */
class TransparentColorChoice : public QObject
{
	Q_OBJECT
	/** The image as it will be: the transparent color shown transparent */
	Q_PROPERTY(QImage shown READ shown NOTIFY changed)
	Q_PROPERTY(QSize size READ size NOTIFY imageChanged)
	/** A palette image: the chosen color is a palette index */
	Q_PROPERTY(bool palette READ isPalette NOTIFY imageChanged)
	/** The palette index of the transparent color (palette images) */
	Q_PROPERTY(int index READ index NOTIFY changed)
	/** The transparent color, invalid when there is none */
	Q_PROPERTY(QColor color READ color NOTIFY changed)
	Q_PROPERTY(bool hasColor READ hasColor NOTIFY changed)
	/** "Transparent color: #ff00ff (palette index 3)" */
	Q_PROPERTY(QString text READ text NOTIFY changed)

public:
	explicit TransparentColorChoice(QObject* parent = nullptr);

	/** The image to choose in (a palette image, or any other: as ARGB) */
	void setImage(const QImage& image);
	/** setImage() of a file of the game (PNG, BMP, XYZ); false when it cannot be read */
	Q_INVOKABLE bool load(const QString& path);

	QImage shown() const;
	QSize size() const;
	bool isPalette() const;
	int index() const;
	QColor color() const;
	bool hasColor() const;
	QString text() const;

	/** The color of this pixel becomes the transparent one (outside: nothing) */
	Q_INVOKABLE void pick(int x, int y);

	/** The image with the chosen transparent color: palette index 0, or alpha 0 */
	QImage result() const;

signals:
	void imageChanged();
	void changed();

private:
	QImage m_source;
	int m_index = 0;
	bool m_hasColor = false;
	QRgb m_color = 0;
};
