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

#include <QFontDatabase>
#include <QKeyEvent>
#include <QPainter>
#include <QTextBlock>
#include <QScrollBar>
#include <QTextEdit>
#include <QtMath>

/** The monospaced font of the system, at the size of the given font */
inline QFont monospaceFont(const QFont& base) {
	QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
	if (!QFontInfo(font).fixedPitch()) {
		// no monospaced font of the system (some Linux desktops)
		font.setFamilies({QStringLiteral("Menlo"), QStringLiteral("Consolas"), QStringLiteral("DejaVu Sans Mono"),
			QStringLiteral("Liberation Mono"), QStringLiteral("Courier New"), QStringLiteral("monospace")});
		font.setStyleHint(QFont::Monospace);
	}
	font.setPointSizeF(base.pointSizeF());
	return font;
}

/**
 * The text of a message, in a monospaced font and not wrapped, with a line
 * after the 38th character (the width of a message with a face) and one after
 * the 50th (without a face). Longer lines can still be typed. Ctrl+Backspace
 * (Cmd or Control on macOS) deletes the line before the cursor, as in VS Code.
 */
class MessageTextEdit : public QTextEdit {
public:
	explicit MessageTextEdit(QWidget* parent = nullptr) : QTextEdit(parent) {
		setAcceptRichText(false);
		setLineWrapMode(QTextEdit::NoWrap);
		setFont(monospaceFont(QWidget::font()));
		// a little wider than the longest message
		const int margins = 2 * qCeil(document()->documentMargin()) + 2 * frameWidth()
			+ verticalScrollBar()->sizeHint().width();
		setMinimumWidth(margins + qCeil(columnX(60)));
	}

protected:
	void keyPressEvent(QKeyEvent* event) override {
		const auto modifiers = event->modifiers() & ~Qt::KeypadModifier;
		const bool line_key = event->key() == Qt::Key_Backspace
			&& (modifiers == Qt::ControlModifier || modifiers == Qt::MetaModifier);
		if (!line_key || isReadOnly()) {
			QTextEdit::keyPressEvent(event);
			return;
		}
		// the selection, else the line up to the cursor, else (at its start) the line break before it
		QTextCursor cursor = textCursor();
		if (!cursor.hasSelection()) {
			if (cursor.atBlockStart()) {
				cursor.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor);
			} else {
				cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
			}
		}
		cursor.removeSelectedText();
		setTextCursor(cursor);
		event->accept();
	}

	void paintEvent(QPaintEvent* event) override {
		QTextEdit::paintEvent(event);
		QPainter painter(viewport());
		const QColor color = palette().color(QPalette::PlaceholderText);
		for (const auto& [columns, style] : {std::pair{38, Qt::DashLine}, std::pair{50, Qt::SolidLine}}) {
			const qreal x = document()->documentMargin() + columnX(columns) - horizontalScrollBar()->value();
			painter.setPen(QPen(color, 1, style));
			painter.drawLine(QPointF(x, 0), QPointF(x, viewport()->height()));
		}
	}

private:
	/** Where the text of so many characters ends */
	qreal columnX(int columns) const {
		return columns * QFontMetricsF(font()).horizontalAdvance(QLatin1Char('0'));
	}
};
