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

#include <QObject>

class QToolBar;

/**
 * The "»" button of a toolbar too narrow for its buttons opens a menu of the
 * hidden ones, which stays open until one is chosen. Qt's own behaviour (the
 * toolbar unfolding over the window and folding back as soon as the pointer
 * leaves it) makes them hard to reach.
 */
class ToolBarOverflowMenu : public QObject
{
	Q_OBJECT

public:
	static void install(QToolBar* toolbar);

protected:
	bool eventFilter(QObject* watched, QEvent* event) override;

private:
	explicit ToolBarOverflowMenu(QToolBar* toolbar);
	QToolBar* m_toolbar;
};
