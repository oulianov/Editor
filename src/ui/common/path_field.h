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

#include <QHBoxLayout>
#include <QLineEdit>
#include <QToolButton>

/**
 * The "..." button of a field (a list of the database, a path, a sound...),
 * the same everywhere: as tall as the field (the style gives it the height
 * of the one-line fields, EditorStyle::controlHeight)
 */
inline QToolButton* fieldButton(QWidget* parent, QToolButton* button = nullptr) {
	if (!button) {
		button = new QToolButton(parent);
	}
	// (a button with its own text keeps it: the presets of Settings...)
	if (button->text().isEmpty()) {
		button->setText(QStringLiteral("..."));
	}
	button->setProperty("editor_field_button", true);
	button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	return button;
}

/**
 * A field and its "..." button, the same in every window (the lists of the
 * database, paths in Settings and Export Game, sounds): no margins, it sits
 * in forms like a plain field; the button close to the field it belongs to.
 * With parent, the layout of that widget
 */
inline QHBoxLayout* fieldRow(QWidget* field, QToolButton* button, QWidget* parent = nullptr) {
	fieldButton(nullptr, button);
	auto* row = parent ? new QHBoxLayout(parent) : new QHBoxLayout;
	row->setContentsMargins(0, 0, 0, 0);
	row->setSpacing(2);
	row->addWidget(field, 1);
	row->addWidget(button);
	return row;
}

/** A path and its "..." button (Settings, Export Game...) */
inline QHBoxLayout* pathFieldLayout(QLineEdit* field, QToolButton* button) {
	return fieldRow(field, button);
}
