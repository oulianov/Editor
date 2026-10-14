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

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include "common/image_import.h"
#include "model/transparent_color.h"

namespace {

/** A palette image: index 0 magenta (the background), 1 dark, 2 green in a square */
QImage paletteImage()
{
	QImage image(16, 8, QImage::Format_Indexed8);
	image.setColorTable({ qRgb(255, 0, 255), qRgb(10, 20, 30), qRgb(0, 200, 0) });
	image.fill(0);
	for (int y = 2; y < 6; ++y) {
		for (int x = 2; x < 6; ++x) {
			image.setPixel(x, y, x == 2 ? 1 : 2);
		}
	}
	return image;
}

} // namespace

/** The transparent color of an image imported (the Import Image window, without it) */
class TransparentColorTest : public QObject
{
	Q_OBJECT

private slots:
	void paletteImage();
	void trueColorImage();
	void file();
};

void TransparentColorTest::paletteImage()
{
	TransparentColorChoice choice;
	QSignalSpy changed(&choice, &TransparentColorChoice::changed);
	choice.setImage(::paletteImage());
	QVERIFY(choice.isPalette());
	QCOMPARE(choice.size(), QSize(16, 8));
	// index 0 at first: the background is transparent
	QCOMPARE(choice.index(), 0);
	QCOMPARE(choice.color(), QColor(255, 0, 255));
	QCOMPARE(choice.text(), QStringLiteral("Transparent color: #ff00ff (palette index 0)"));
	QCOMPARE(qAlpha(choice.shown().pixel(0, 0)), 0);
	QCOMPARE(choice.shown().pixel(3, 3), qRgb(0, 200, 0));

	// the green of the square picked: transparent, the background shown
	changed.clear();
	choice.pick(4, 4);
	QCOMPARE(changed.count(), 1);
	QCOMPARE(choice.index(), 2);
	QVERIFY(choice.hasColor());
	QCOMPARE(choice.text(), QStringLiteral("Transparent color: #00c800 (palette index 2)"));
	QCOMPARE(qAlpha(choice.shown().pixel(4, 4)), 0);
	QCOMPARE(choice.shown().pixel(0, 0), qRgb(255, 0, 255));
	// outside: nothing
	choice.pick(16, 0);
	QCOMPARE(choice.index(), 2);

	// the result: the green at index 0 (the game's transparent one), the pixels the same colors
	const QImage result = choice.result();
	QCOMPARE(result.format(), QImage::Format_Indexed8);
	QCOMPARE(result.pixelIndex(4, 4), 0);
	QCOMPARE(QColor(result.color(0)), QColor(0, 200, 0));
	QCOMPARE(result.pixel(0, 0), qRgb(255, 0, 255));
	QCOMPARE(result.pixel(2, 3), qRgb(10, 20, 30));
	// the palette is opaque: transparency is index 0, not alpha
	QCOMPARE(qAlpha(result.color(result.pixelIndex(0, 0))), 255);
}

void TransparentColorTest::trueColorImage()
{
	QImage image(8, 8, QImage::Format_ARGB32);
	image.fill(qRgb(1, 2, 3));
	image.setPixel(1, 1, qRgb(200, 100, 50));
	image.setPixel(2, 2, qRgba(0, 0, 0, 0));
	TransparentColorChoice choice;
	choice.setImage(image);
	QVERIFY(!choice.isPalette());
	// no color at first: only the image's own alpha
	QVERIFY(!choice.hasColor());
	QCOMPARE(choice.text(), QStringLiteral("No transparent color"));
	QCOMPARE(choice.result().pixel(0, 0), qRgb(1, 2, 3));

	choice.pick(0, 0);
	QVERIFY(choice.hasColor());
	QCOMPARE(choice.text(), QStringLiteral("Transparent color: #010203"));
	const QImage result = choice.result();
	QCOMPARE(qAlpha(result.pixel(5, 5)), 0);
	QCOMPARE(result.pixel(1, 1), qRgb(200, 100, 50));

	// a pixel already transparent: no color, the image's alpha only
	choice.pick(2, 2);
	QVERIFY(!choice.hasColor());
	QCOMPARE(choice.result().pixel(5, 5), qRgb(1, 2, 3));
	QCOMPARE(qAlpha(choice.result().pixel(2, 2)), 0);
}

void TransparentColorTest::file()
{
	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	const QString path = dir.filePath(QStringLiteral("image.png"));
	QVERIFY(::paletteImage().save(path));
	TransparentColorChoice choice;
	QSignalSpy image_changed(&choice, &TransparentColorChoice::imageChanged);
	QVERIFY(choice.load(path));
	QCOMPARE(image_changed.count(), 1);
	QVERIFY(choice.isPalette());
	QCOMPARE(choice.size(), QSize(16, 8));
	QVERIFY(!choice.load(dir.filePath(QStringLiteral("none.png"))));
	QVERIFY(choice.shown().isNull());
}

QTEST_MAIN(TransparentColorTest)
#include "transparent_color_test.moc"
