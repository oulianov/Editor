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

#include "wrap_check_box.h"

#include <QStyleOptionButton>
#include <QStylePainter>
#include <algorithm>

WrapCheckBox::WrapCheckBox(QWidget* parent) : WrapCheckBox(QString(), parent) {
}

WrapCheckBox::WrapCheckBox(const QString& text, QWidget* parent) : QCheckBox(text, parent) {
	QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	policy.setHeightForWidth(true);
	setSizePolicy(policy);
}

int WrapCheckBox::textLeft() const {
	QStyleOptionButton option;
	initStyleOption(&option);
	return style()->pixelMetric(QStyle::PM_IndicatorWidth, &option, this)
		+ style()->pixelMetric(QStyle::PM_CheckBoxLabelSpacing, &option, this);
}

int WrapCheckBox::lineHeight() const {
	QStyleOptionButton option;
	initStyleOption(&option);
	return std::max(style()->pixelMetric(QStyle::PM_IndicatorHeight, &option, this), fontMetrics().height());
}

QSize WrapCheckBox::minimumSizeHint() const {
	// its longest word on a line
	int word = 0;
	for (const QString& part : text().remove(QLatin1Char('&')).split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
		word = std::max(word, fontMetrics().horizontalAdvance(part));
	}
	const int width = textLeft() + word;
	return QSize(width, heightForWidth(width));
}

QSize WrapCheckBox::sizeHint() const {
	// it does not widen its column: it takes the width the other controls
	// give it (its policy grows), and its text wraps to fit
	return minimumSizeHint();
}

bool WrapCheckBox::hasHeightForWidth() const {
	return true;
}

int WrapCheckBox::heightForWidth(int width) const {
	const QRect text = fontMetrics().boundingRect(QRect(0, 0, std::max(1, width - textLeft()), 1 << 20),
		Qt::TextWordWrap | Qt::TextShowMnemonic, QCheckBox::text());
	// the first line is centered on the box
	const int first = (lineHeight() - fontMetrics().height()) / 2;
	return std::max(lineHeight(), first + text.height());
}

void WrapCheckBox::paintEvent(QPaintEvent*) {
	QStylePainter painter(this);
	QStyleOptionButton option;
	initStyleOption(&option);
	// the box, with its focus ring, on the first line
	const QString label = option.text;
	option.text.clear();
	option.rect.setHeight(lineHeight());
	painter.drawControl(QStyle::CE_CheckBox, option);
	// the text, on as many lines as needed
	const int first = (lineHeight() - fontMetrics().height()) / 2;
	const QRect text(textLeft(), first, width() - textLeft(), height() - first);
	style()->drawItemText(&painter, text, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap | Qt::TextShowMnemonic,
		palette(), isEnabled(), label, QPalette::WindowText);
}

bool WrapCheckBox::hitButton(const QPoint& pos) const {
	// the box and every line of its text
	return rect().contains(pos);
}
