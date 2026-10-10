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

#include <QCheckBox>

/**
 * A check box whose text wraps on several lines when its column is narrow
 * (long translations): as wide as its longest word at least, as tall as
 * its lines. The box stays beside the first line; a click on any line
 * checks it.
 */
class WrapCheckBox : public QCheckBox
{
	Q_OBJECT

public:
	explicit WrapCheckBox(QWidget* parent = nullptr);
	explicit WrapCheckBox(const QString& text, QWidget* parent = nullptr);

	QSize sizeHint() const override;
	QSize minimumSizeHint() const override;
	bool hasHeightForWidth() const override;
	int heightForWidth(int width) const override;

protected:
	void paintEvent(QPaintEvent* event) override;
	bool hitButton(const QPoint& pos) const override;

private:
	/** Where the text starts: after the box and its spacing */
	int textLeft() const;
	/** The height of the box's line (the box or a line of text) */
	int lineHeight() const;
};
