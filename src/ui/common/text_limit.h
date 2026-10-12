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

#include "ui/common/message_text_edit.h"

#include <QCoreApplication>
#include <QEvent>
#include <QLineEdit>
#include <QPainter>
#include <QtMath>

/**
 * The text of a field that the game shows in a window of its own size: in a
 * monospaced font, with a line after the last character RPG Maker lets type
 * (the MaxLength of its forms, in bytes). Longer text can still be typed.
 */
class TextLimit : public QWidget {
public:
	/**
	 * Marks the limit of the field; the marker lives and dies with it. With
	 * fit, the field is at least as wide as the limit.
	 */
	static void mark(QLineEdit* line, int columns, bool fit = true) {
		line->setFont(monospaceFont(line->font()));
		auto* limit = new TextLimit(line, columns);
		if (fit) {
			// the whole limit and a little more
			const QMargins margins = line->textMargins();
			line->setMinimumWidth(qMax(line->minimumWidth(),
				qCeil(limit->columnX(columns + 2)) + margins.left() + margins.right() + 16));
		}
	}

protected:
	bool eventFilter(QObject* watched, QEvent* event) override {
		if (watched == m_line && event->type() == QEvent::Resize) {
			setGeometry(m_line->rect());
		}
		return QWidget::eventFilter(watched, event);
	}

	void paintEvent(QPaintEvent*) override {
		// where the first character is, scrolled or not: the cursor tells
		const QRectF cursor = m_line->inputMethodQuery(Qt::ImCursorRectangle).toRectF();
		const QFontMetricsF metrics(m_line->font());
		const qreal origin = cursor.center().x()
			- metrics.horizontalAdvance(m_line->text().left(m_line->cursorPosition()));
		const qreal x = origin + columnX(m_columns);
		if (x <= 1 || x >= width() - 1) {
			return;
		}
		QPainter painter(this);
		painter.setPen(QPen(m_line->palette().color(QPalette::PlaceholderText), 1));
		painter.drawLine(QPointF(x, 3), QPointF(x, height() - 3));
	}

private:
	TextLimit(QLineEdit* line, int columns) : QWidget(line), m_line(line), m_columns(columns) {
		setAttribute(Qt::WA_TransparentForMouseEvents);
		setFocusPolicy(Qt::NoFocus);
		setGeometry(line->rect());
		line->installEventFilter(this);
		connect(line, &QLineEdit::textChanged, this, qOverload<>(&QWidget::update));
		connect(line, &QLineEdit::cursorPositionChanged, this, qOverload<>(&QWidget::update));
		connect(line, &QLineEdit::selectionChanged, this, qOverload<>(&QWidget::update));
		line->setProperty("textLimit", columns);
		if (line->toolTip().isEmpty()) {
			line->setToolTip(QCoreApplication::translate("TextLimit", "RPG Maker allows %n characters here", nullptr, columns));
		}
		show();
	}

	/** Where the text of so many characters ends, from the first */
	qreal columnX(int columns) const {
		return columns * QFontMetricsF(m_line->font()).horizontalAdvance(QLatin1Char('0'));
	}

	QLineEdit* m_line;
	int m_columns;
};
