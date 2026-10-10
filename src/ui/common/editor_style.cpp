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
#include "view_state.h"
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
#include <QFormLayout>
#include <QTabBar>
#include <QScrollBar>
#include <cmath>
#include <QStyleOptionTab>
#include <QToolButton>
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
	if (auto* dialog = qobject_cast<QDialog*>(widget)) ViewState::rememberDialog(dialog);

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

	if (auto* view = qobject_cast<QAbstractItemView*>(widget)) {
		// lists, trees and tables scroll by pixels: a trackpad follows the
		// fingers (eventFilter), not three lines a step
		view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
		view->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
		view->viewport()->setProperty("editor_view_viewport", true);
		view->viewport()->installEventFilter(this);
	}

	if (auto* bar = qobject_cast<QTabBar*>(widget)) {
		// the wheel scrolls the tabs (scrollTabs)
		widget->installEventFilter(this);
		// A click on a tab of a dialog (the database...) lets the arrows go
		// to the next tabs, as on Windows: macOS gives tab bars no focus. The
		// map tabs leave it to the map, whose cursor the arrows move.
		if (!bar->documentMode()) {
			bar->setFocusPolicy(Qt::StrongFocus);
		}
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

namespace {
/** The wheel over a tab bar scrolls its tabs, one tab a step (a notch of a
 *  mouse wheel, 40 pixels of a trackpad, up or sideways), and never changes
 *  the tab shown. false: not a horizontal bar. */
bool scrollTabs(QTabBar* bar, QWheelEvent* wheel) {
	if (bar->shape() != QTabBar::RoundedNorth && bar->shape() != QTabBar::RoundedSouth
			&& bar->shape() != QTabBar::TriangularNorth && bar->shape() != QTabBar::TriangularSouth) {
		return false;
	}
	const QPoint pixels = wheel->pixelDelta();
	const QPoint delta = pixels.isNull() ? wheel->angleDelta() : pixels;
	const int step = pixels.isNull() ? QWheelEvent::DefaultDeltasPerStep : 40;
	// down or to the left: the next tabs
	int accumulated = bar->property("editor_wheel").toInt()
		+ (std::abs(delta.y()) >= std::abs(delta.x()) ? delta.y() : delta.x());
	const int steps = accumulated / step;
	accumulated -= steps * step;
	bar->setProperty("editor_wheel", accumulated);
	auto* arrow = bar->findChild<QToolButton*>(steps > 0 ? QStringLiteral("ScrollLeftButton") : QStringLiteral("ScrollRightButton"));
	for (int i = 0; arrow && i < std::abs(steps); ++i) {
		if (arrow->isVisible() && arrow->isEnabled()) {
			arrow->click();
		}
	}
	return true;
}
}

bool EditorStyle::eventFilter(QObject* watched, QEvent* event) {
	if (auto* bar = qobject_cast<QTabBar*>(watched); bar && event->type() == QEvent::Wheel) {
		return scrollTabs(bar, static_cast<QWheelEvent*>(event));
	}
	if (event->type() == QEvent::Wheel && watched->property("editor_view_viewport").toBool()) {
		// A trackpad moves a list by the pixels it reports, as in a text
		// editor; Qt turns them into lines (three a step), much faster than
		// the fingers. A mouse wheel keeps its steps.
		auto* view = qobject_cast<QAbstractItemView*>(static_cast<QWidget*>(watched)->parentWidget());
		auto* wheel = static_cast<QWheelEvent*>(event);
		const QPoint pixels = wheel->pixelDelta();
		if (view && !pixels.isNull()) {
			const bool vertical = std::abs(pixels.y()) >= std::abs(pixels.x());
			QScrollBar* scroll = vertical ? view->verticalScrollBar() : view->horizontalScrollBar();
			const int before = scroll->value();
			scroll->setValue(before - (vertical ? pixels.y() : pixels.x()));
			// at an end, the scroll area around takes the rest
			if (scroll->value() != before || wheel->phase() == Qt::ScrollMomentum) {
				return true;
			}
			event->ignore();
			return false;
		}
		return false;
	}
	if (event->type() == QEvent::Wheel && (qobject_cast<QAbstractSpinBox*>(watched) || qobject_cast<QComboBox*>(watched))
			&& !static_cast<QWidget*>(watched)->hasFocus()) {
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
	if (hint == SH_TabBar_AllowWheelScrolling) {
		// macOS does not let the wheel scroll tabs
		return true;
	}
	if (hint == SH_TabBar_CloseButtonPosition) {
		// the close button after the name, as in most applications (macOS
		// puts it before)
		return QTabBar::RightSide;
	}
	if (hint == SH_FormLayoutFieldGrowthPolicy) {
		// Fields that can grow fill their row (a path, a name), as in
		// RPG Maker's windows; macOS keeps every field at its size hint,
		// which left short paths beside empty space
		return QFormLayout::ExpandingFieldsGrow;
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

namespace {
// The space before the name of a document tab (the maps), and after its close button
constexpr int tab_padding = 8;

/** The room of the close button of a document tab, with its gap */
int documentTabButton(const QTabBar* bar) {
	return bar->tabsClosable() ? bar->style()->pixelMetric(QStyle::PM_TabCloseIndicatorWidth, nullptr, bar) + 4 : 0;
}

}


void EditorStyle::drawControl(ControlElement element, const QStyleOption* option, QPainter* painter,
		const QWidget* widget) const {
	if (element == CE_TabBarTabLabel) {
		auto* bar = qobject_cast<const QTabBar*>(widget);
		const int index = bar ? bar->tabAt(option->rect.center()) : -1;
		// a temporary tab (the map only looked at): its name in italics
		const bool temporary = index >= 0 && bar->tabData(index).toString() == QLatin1String("temporary");
		auto* tab = qstyleoption_cast<const QStyleOptionTab*>(option);
		if (bar && tab && bar->documentMode() && tab->icon.isNull()) {
			// document tabs (the maps): the name from the left of a tab as
			// wide as it is (CT_TabBarTab); macOS centers it with wide margins.
			// Its colors are the ones of macOS: in dark mode, white for the
			// tab shown in the active window, gray for the others
			painter->save();
			QFont font = painter->font();
			font.setItalic(temporary);
			painter->setFont(font);
			const bool dark = tab->palette.color(QPalette::Window).lightness() < 128;
			const bool shown = (tab->state & State_Selected) && (tab->state & State_Active);
			QColor color = tab->palette.color(QPalette::WindowText);
			if (dark) {
				color = shown ? QColor(Qt::white) : QColor(Qt::gray);
			} else if (!(tab->state & State_Selected)) {
				color.setAlphaF(0.7);
			}
			const QRect text = tab->rect.adjusted(tab_padding, 0, -documentTabButton(bar), 0);
			painter->setPen(color);
			painter->drawText(text, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
				QFontMetrics(font).elidedText(tab->text, Qt::ElideRight, text.width()));
			painter->restore();
			return;
		}
		if (temporary) {
			painter->save();
			QFont font = painter->font();
			font.setItalic(true);
			painter->setFont(font);
			QProxyStyle::drawControl(element, option, painter, widget);
			painter->restore();
			return;
		}
	}
	QProxyStyle::drawControl(element, option, painter, widget);
}

QRect EditorStyle::subElementRect(SubElement element, const QStyleOption* option, const QWidget* widget) const {
	if (element == SE_CheckBoxLayoutItem || element == SE_RadioButtonLayoutItem) {
		// macOS lays check boxes and radio buttons out by a smaller rectangle
		// than the widget, letting neighbours overlap their margins; its
		// boxes are drawn into those margins, so a column of them overlapped.
		// They are laid out by their whole rectangle.
		return option->rect;
	}
	return QProxyStyle::subElementRect(element, option, widget);
}

QSize EditorStyle::sizeFromContents(ContentsType type, const QStyleOption* option,
		const QSize& size, const QWidget* widget) const {
	if (type == CT_TabBarTab) {
		auto* bar = qobject_cast<const QTabBar*>(widget);
		auto* tab = qstyleoption_cast<const QStyleOptionTab*>(option);
		if (bar && tab && bar->documentMode() && tab->icon.isNull()) {
			// as wide as the name (in italics, a little wider) and the close
			// button: the style draws the name at the left, with its colors
			QFont font = bar->font();
			font.setItalic(true);
			const int name = std::max(QFontMetrics(bar->font()).horizontalAdvance(tab->text),
				QFontMetrics(font).horizontalAdvance(tab->text));
			const QSize native = QProxyStyle::sizeFromContents(type, option, size, widget);
			// short names (the pages of an event: 1, 2...) still make a tab
			// easy to click: four lines of text wide at least
			const int minimum = 4 * QFontMetrics(bar->font()).height();
			return QSize(std::max(minimum, tab_padding + name + documentTabButton(bar) + tab_padding), native.height());
		}
	}
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
