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

#include "editor_style.h"
#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QWheelEvent>
#include <QDialog>
#include <QGraphicsView>
#include <QGroupBox>
#include <QLayout>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QStyleOptionGroupBox>
#include <QTextEdit>

namespace {
// Muted text and borders: the text color, partly transparent, so they follow
// the light and dark palettes
QColor muted(const QPalette& palette, qreal alpha) {
	QColor color = palette.color(QPalette::WindowText);
	color.setAlphaF(alpha);
	return color;
}

// Titles of sections: regular weight, a bit smaller than the controls
QFont titleFont(const QWidget* widget) {
	QFont font = widget ? widget->font() : QFont();
	font.setWeight(QFont::Normal);
	if (font.pointSizeF() > 0) {
		font.setPointSizeF(font.pointSizeF() * 0.9);
	} else {
		font.setPixelSize(std::max(1, qRound(font.pixelSize() * 0.9)));
	}
	return font;
}

int titleHeight(const QStyleOptionGroupBox* box, const QWidget* widget) {
	return box->text.isEmpty() && !(box->subControls & QStyle::SC_GroupBoxCheckBox)
		? 0 : QFontMetrics(titleFont(widget)).height();
}
}

void EditorStyle::polish(QWidget* widget) {
	QProxyStyle::polish(widget);

	if (widget->isWindow()) {
		// Windows add widgets after setupUi (database pages...). A hidden
		// widget keeps its old size hints until it is shown, so the window
		// would open with a stale minimum and be resized just after: macOS
		// can keep the first size, below the minimum. Refresh them now,
		// before the size of the window is chosen.
		for (auto* child : widget->findChildren<QWidget*>()) {
			child->updateGeometry();
		}
	}

	if (qobject_cast<QAbstractSpinBox*>(widget) || qobject_cast<QComboBox*>(widget)) {
		// the wheel neither changes them nor gives them the focus
		widget->setFocusPolicy(Qt::StrongFocus);
		widget->installEventFilter(this);
	}

	if (auto* group = qobject_cast<QGroupBox*>(widget)) {
		// the padding comes from the style, the same in every dialog
		if (group->layout()) {
			group->layout()->setContentsMargins(0, 0, 0, 0);
		}
	}

#ifdef Q_OS_MACOS
	// Forms in dialogs; text, lists and pictures stay readable at full size
	const bool content = qobject_cast<QAbstractItemView*>(widget) || qobject_cast<QGraphicsView*>(widget)
		|| qobject_cast<QTextEdit*>(widget) || qobject_cast<QPlainTextEdit*>(widget);
	if (!content && !widget->isWindow() && qobject_cast<QDialog*>(widget->window())) {
		widget->setAttribute(Qt::WA_MacSmallSize);
	}
#endif
}

bool EditorStyle::eventFilter(QObject* watched, QEvent* event) {
	if (event->type() == QEvent::Wheel && !static_cast<QWidget*>(watched)->hasFocus()) {
		// ignored: the scroll area around gets it
		event->ignore();
		return true;
	}
	return QProxyStyle::eventFilter(watched, event);
}

namespace {
// A scroll bar of a scroll area form (QScrollArea), not of a list or a view
bool isFormScrollBar(const QWidget* widget) {
	for (const QWidget* parent = widget; parent; parent = parent->parentWidget()) {
		if (qobject_cast<const QAbstractScrollArea*>(parent)) {
			return qobject_cast<const QScrollArea*>(parent);
		}
	}
	return false;
}
}

int EditorStyle::styleHint(StyleHint hint, const QStyleOption* option,
		const QWidget* widget, QStyleHintReturn* data) const {
	if (hint == SH_ScrollBar_Transient) {
		// Qt moves the form of a scroll area by copying its pixels, only when
		// nothing lies over the viewport. An overlay scroll bar does: every
		// step of a swipe repainted all the native controls of the form, slow
		// on macOS. Its bar is beside the form instead. Lists and views keep
		// the overlay bar: their rows draw fast.
		if (isFormScrollBar(widget)) {
			return false;
		}
	}
	return QProxyStyle::styleHint(hint, option, widget, data);
}

QRect EditorStyle::subControlRect(ComplexControl control, const QStyleOptionComplex* option,
		SubControl sub_control, const QWidget* widget) const {
	auto* box = qstyleoption_cast<const QStyleOptionGroupBox*>(option);
	if (control != CC_GroupBox || !box) {
		return QProxyStyle::subControlRect(control, option, sub_control, widget);
	}

	const QRect rect = box->rect;
	const int title = titleHeight(box, widget);
	const int indicator = (box->subControls & SC_GroupBoxCheckBox)
		? pixelMetric(PM_IndicatorWidth, option, widget) + 4 : 0;
	const int top = title ? section_title_top : 0;
	const QRect frame = rect.adjusted(0, title ? top + title + section_title_gap : 0, 0, 0);

	switch (sub_control) {
		case SC_GroupBoxCheckBox: {
			const int size = pixelMetric(PM_IndicatorWidth, option, widget);
			return QRect(rect.left(), rect.top() + top + (title - size) / 2, size, size);
		}
		case SC_GroupBoxLabel:
			return QRect(rect.left() + indicator, rect.top() + top,
				QFontMetrics(titleFont(widget)).horizontalAdvance(box->text) + 2, title);
		case SC_GroupBoxFrame:
			return frame;
		case SC_GroupBoxContents:
			return frame.adjusted(1 + section_padding, 1 + section_padding, -1 - section_padding, -1 - section_padding);
		default:
			return QProxyStyle::subControlRect(control, option, sub_control, widget);
	}
}

QSize EditorStyle::sizeFromContents(ContentsType type, const QStyleOption* option,
		const QSize& size, const QWidget* widget) const {
	if (type == CT_GroupBox) {
		// the title and the border around the contents
		return size + QSize(2 + 2 * section_padding, 2 + 2 * section_padding + section_title_top + section_title_gap);
	}
	return QProxyStyle::sizeFromContents(type, option, size, widget);
}

void EditorStyle::drawComplexControl(ComplexControl control, const QStyleOptionComplex* option,
		QPainter* painter, const QWidget* widget) const {
	if (control == CC_ScrollBar && isFormScrollBar(widget)) {
		// Beside the form (see styleHint) but drawn like the overlay bars: no
		// track, a slim rounded thumb, away from the form's edge
		const QRect slider = QProxyStyle::subControlRect(control, option, SC_ScrollBarSlider, widget);
		if (slider.isEmpty() || !(option->state & State_Enabled)) {
			return;
		}
		const bool horizontal = option->state & State_Horizontal;
		const bool active = option->activeSubControls & SC_ScrollBarSlider;
		const int thickness = active ? 7 : 5;
		QRectF thumb = slider;
		if (horizontal) {
			thumb.setTop(option->rect.bottom() - 1 - thickness);
			thumb.setHeight(thickness);
		} else {
			thumb.setLeft(option->rect.right() - 1 - thickness);
			thumb.setWidth(thickness);
		}
		QColor color = option->palette.color(QPalette::WindowText);
		color.setAlphaF(active ? 0.5 : 0.3);
		painter->save();
		painter->setRenderHint(QPainter::Antialiasing);
		painter->setPen(Qt::NoPen);
		painter->setBrush(color);
		painter->drawRoundedRect(thumb, thickness / 2.0, thickness / 2.0);
		painter->restore();
		return;
	}

	auto* box = qstyleoption_cast<const QStyleOptionGroupBox*>(option);
	if (control != CC_GroupBox || !box) {
		QProxyStyle::drawComplexControl(control, option, painter, widget);
		return;
	}

	const bool enabled = box->state & State_Enabled;
	painter->save();
	painter->setRenderHint(QPainter::Antialiasing);

	// light border, no fill
	const QRectF frame = QRectF(subControlRect(control, option, SC_GroupBoxFrame, widget)).adjusted(0.5, 0.5, -0.5, -0.5);
	painter->setPen(QPen(muted(box->palette, 0.14), 1));
	painter->setBrush(Qt::NoBrush);
	painter->drawRoundedRect(frame, section_radius, section_radius);

	// title in a muted color, distinct from the contents
	if (!box->text.isEmpty()) {
		painter->setFont(titleFont(widget));
		painter->setPen(muted(box->palette, enabled ? 0.62 : 0.35));
		painter->drawText(subControlRect(control, option, SC_GroupBoxLabel, widget),
			Qt::AlignLeft | Qt::AlignVCenter | Qt::TextShowMnemonic, box->text);
	}
	painter->restore();

	if (box->subControls & SC_GroupBoxCheckBox) {
		QStyleOptionButton check;
		check.QStyleOption::operator=(*box);
		check.rect = subControlRect(control, option, SC_GroupBoxCheckBox, widget);
		check.state = box->state & ~State_HasFocus;
		drawPrimitive(PE_IndicatorCheckBox, &check, painter, widget);
	}
}
