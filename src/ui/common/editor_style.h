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

#include <QHash>
#include <QProxyStyle>

class QStyleOptionSpinBox;

/**
 * The look of the Editor's forms, on top of the platform style.
 *
 * Group boxes are sections: a title in a muted color above a light rounded
 * border, with little padding. Platform styles draw them differently (macOS
 * as filled cards: a card around a combo box, which has its own frame, makes
 * a card in a card); here they look the same everywhere.
 *
 * Dialogs use the small control size on macOS, dense like the windows of
 * RPG Maker; lists, trees and the map keep the normal size.
 * Spacing is left to the platform style, which adapts it to the controls.
 *
 * Spin boxes and combo boxes take the mouse wheel only once clicked: a swipe
 * over a form scrolls the form, as in native forms, instead of changing the
 * values under the pointer.
 */
class EditorStyle : public QProxyStyle
{
	Q_OBJECT

public:
	using QProxyStyle::QProxyStyle;

	void polish(QWidget* widget) override;
	using QProxyStyle::polish;
	bool eventFilter(QObject* watched, QEvent* event) override;

	void drawControl(ControlElement element, const QStyleOption* option, QPainter* painter,
		const QWidget* widget = nullptr) const override;
	void drawComplexControl(ComplexControl control, const QStyleOptionComplex* option,
		QPainter* painter, const QWidget* widget = nullptr) const override;
	QRect subControlRect(ComplexControl control, const QStyleOptionComplex* option,
		SubControl sub_control, const QWidget* widget = nullptr) const override;
	SubControl hitTestComplexControl(ComplexControl control, const QStyleOptionComplex* option,
		const QPoint& position, const QWidget* widget = nullptr) const override;
	int styleHint(StyleHint hint, const QStyleOption* option = nullptr,
		const QWidget* widget = nullptr, QStyleHintReturn* data = nullptr) const override;
	QRect subElementRect(SubElement element, const QStyleOption* option, const QWidget* widget) const override;
	QSize sizeFromContents(ContentsType type, const QStyleOption* option,
		const QSize& size, const QWidget* widget = nullptr) const override;

	/** Space between the border of a section and its controls */
	/**
	 * The parts of a spin box: its field (drawn by the platform, with its
	 * focus ring) and, after a gap the ring cannot cover, its arrows: a
	 * button as tall as the field and as wide as the "..." of fields, an
	 * arrow in each half. Empty
	 * stepper without arrows
	 */
	void spinParts(const QStyleOptionSpinBox* box, const QWidget* widget, QRect* field, QRect* stepper) const;
	/**
	 * The height of the one-line fields of a form (the design system's
	 * control height) like field: what shows of a drop-down list of the
	 * platform, of the same control size
	 */
	int controlHeight(const QWidget* field) const;
	mutable QHash<QString, int> m_controlHeights;
	/** The width of the "..." button of a field (fieldButton) like field */
	int fieldButtonWidth(const QWidget* field) const;
	mutable QHash<QString, int> m_fieldButtonWidths;
	static constexpr int section_padding = 4;
	/** Space above the title of a section, from what is above it */
	static constexpr int section_title_top = 4;
	/** Space between the title of a section and its border */
	static constexpr int section_title_gap = 2;
	static constexpr qreal section_radius = 6;
};
