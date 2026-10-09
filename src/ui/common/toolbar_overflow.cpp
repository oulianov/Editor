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

#include "toolbar_overflow.h"
#include <QAbstractButton>
#include <QEvent>
#include <QMenu>
#include <QToolBar>

ToolBarOverflowMenu::ToolBarOverflowMenu(QToolBar* toolbar) : QObject(toolbar), m_toolbar(toolbar)
{
	// Qt's extension button of the toolbar
	if (auto* button = toolbar->findChild<QAbstractButton*>(QStringLiteral("qt_toolbar_ext_button"))) {
		button->installEventFilter(this);
	}
}

void ToolBarOverflowMenu::install(QToolBar* toolbar)
{
	new ToolBarOverflowMenu(toolbar);
}

bool ToolBarOverflowMenu::eventFilter(QObject* watched, QEvent* event)
{
	if (event->type() != QEvent::MouseButtonPress && event->type() != QEvent::MouseButtonDblClick) {
		return QObject::eventFilter(watched, event);
	}
	// the actions whose buttons do not fit, in their order
	QMenu menu(m_toolbar);
	for (QAction* action : m_toolbar->actions()) {
		QWidget* widget = m_toolbar->widgetForAction(action);
		if (!widget || widget->isVisible()) {
			continue;
		}
		if (action->isSeparator()) {
			if (!menu.isEmpty()) {
				menu.addSeparator();
			}
		} else if (action->isVisible()) {
			menu.addAction(action);
		}
	}
	auto* button = static_cast<QWidget*>(watched);
	if (!menu.isEmpty()) {
		menu.exec(button->mapToGlobal(QPoint(0, button->height())));
	}
	return true;
}
