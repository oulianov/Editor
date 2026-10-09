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

#include <QDialog>
#include <QList>
#include <lcf/rpg/eventcommand.h>

class QLineEdit;
class QPushButton;
class QTabWidget;

/**
 * Picks the kind of command to insert, like the RPG Maker "Event Commands"
 * window: tabs of buttons, plus a search field.
 */
class EventCommandPalette : public QDialog
{
	Q_OBJECT

public:
	using Code = lcf::rpg::EventCommand::Code;

	EventCommandPalette(bool rpg2k3, bool battle, QWidget* parent = nullptr);

	Code selectedCode() const;

	/** Name of a command kind, as on its button */
	static QString commandName(Code code);

private:
	void filter(const QString& text);

	QLineEdit* m_search = nullptr;
	QTabWidget* m_tabs = nullptr;
	QList<QPushButton*> m_buttons;
	Code m_code = Code::END;
};
