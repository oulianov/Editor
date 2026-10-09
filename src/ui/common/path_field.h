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

/** A path and its "..." button, the same in every window (Settings, Export
 *  Game...): the button takes the height of the field, whatever the style
 *  draws for a tool button on its own */
inline QHBoxLayout* pathFieldLayout(QLineEdit* field, QToolButton* button) {
	button->setText(QStringLiteral("..."));
	button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Ignored);
	auto* row = new QHBoxLayout;
	row->setContentsMargins(0, 0, 0, 0);
	row->addWidget(field, 1);
	row->addWidget(button);
	return row;
}
