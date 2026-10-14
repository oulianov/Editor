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

#include "transparent_color.h"
#include "common/image_import.h"

#include <QCoreApplication>

TransparentColorChoice::TransparentColorChoice(QObject* parent) : QObject(parent)
{
}

void TransparentColorChoice::setImage(const QImage& image)
{
	m_source = image;
	if (m_source.format() == QImage::Format_Mono || m_source.format() == QImage::Format_MonoLSB) {
		m_source = m_source.convertToFormat(QImage::Format_Indexed8);
	}
	if (!m_source.isNull() && m_source.format() != QImage::Format_Indexed8) {
		m_source = m_source.convertToFormat(QImage::Format_ARGB32);
	}
	m_index = 0;
	m_hasColor = false;
	m_color = 0;
	emit imageChanged();
	emit changed();
}

bool TransparentColorChoice::load(const QString& path)
{
	setImage(ImageImport::loadImage(path));
	return !m_source.isNull();
}

QImage TransparentColorChoice::shown() const
{
	if (m_source.isNull()) {
		return QImage();
	}
	QImage shown = m_source.convertToFormat(QImage::Format_ARGB32);
	if (isPalette()) {
		for (int y = 0; y < m_source.height(); ++y) {
			const uchar* src = m_source.constScanLine(y);
			auto* dst = reinterpret_cast<QRgb*>(shown.scanLine(y));
			for (int x = 0; x < m_source.width(); ++x) {
				dst[x] = src[x] == m_index ? 0 : (dst[x] | 0xff000000u);
			}
		}
	} else if (m_hasColor) {
		for (int y = 0; y < shown.height(); ++y) {
			auto* dst = reinterpret_cast<QRgb*>(shown.scanLine(y));
			for (int x = 0; x < shown.width(); ++x) {
				if ((dst[x] & 0xffffffu) == (m_color & 0xffffffu)) {
					dst[x] = 0;
				}
			}
		}
	}
	return shown;
}

QSize TransparentColorChoice::size() const
{
	return m_source.size();
}

bool TransparentColorChoice::isPalette() const
{
	return m_source.format() == QImage::Format_Indexed8;
}

int TransparentColorChoice::index() const
{
	return m_index;
}

QColor TransparentColorChoice::color() const
{
	if (isPalette()) {
		return m_index < m_source.colorCount() ? QColor(m_source.color(m_index)) : QColor();
	}
	return m_hasColor ? QColor(m_color) : QColor();
}

bool TransparentColorChoice::hasColor() const
{
	return color().isValid();
}

QString TransparentColorChoice::text() const
{
	const QColor swatch = color();
	if (isPalette()) {
		return QCoreApplication::translate("ImportImageDialog", "Transparent color: %1 (palette index %2)")
			.arg(swatch.name()).arg(m_index);
	}
	return swatch.isValid() ? QCoreApplication::translate("ImportImageDialog", "Transparent color: %1").arg(swatch.name())
		: QCoreApplication::translate("ImportImageDialog", "No transparent color");
}

void TransparentColorChoice::pick(int x, int y)
{
	if (!m_source.valid(x, y)) {
		return;
	}
	if (isPalette()) {
		m_index = m_source.pixelIndex(x, y);
	} else {
		const QRgb color = m_source.pixel(x, y);
		// a pixel already transparent: keep the alpha of the image only
		m_hasColor = qAlpha(color) != 0;
		m_color = color | 0xff000000u;
	}
	emit changed();
}

QImage TransparentColorChoice::result() const
{
	if (isPalette()) {
		QImage result = m_source;
		ImageImport::swapIndexes(result, 0, m_index);
		return ImageImport::opaquePalette(result);
	}
	return shown();
}
