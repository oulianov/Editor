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

#include "ui/event/event_command_base_widget.h"

class QButtonGroup;
class QComboBox;
class QPlainTextEdit;
class QSpinBox;
class QStackedWidget;
class RpgComboBoxBase;

/** Wait: a time, or (RPG Maker 2003) until a key is pressed */
class WaitWidget : public EventCommandBaseWidget
{
	Q_OBJECT

public:
	WaitWidget(ProjectData& project, QWidget* parent);
	void setData(lcf::rpg::EventCommand* cmd) override;
};

/** Comment: free text over several lines */
class CommentWidget : public EventCommandBaseWidget
{
	Q_OBJECT

public:
	CommentWidget(ProjectData& project, QWidget* parent);
	void setData(EventCommandList* commands) override;
	void apply();

private:
	QPlainTextEdit* m_text = nullptr;
};

/** Call Event: a common event, an event of the map, or one chosen by variables */
class CallEventWidget : public EventCommandBaseWidget
{
	Q_OBJECT

public:
	CallEventWidget(ProjectData& project, QWidget* parent);
	void setData(lcf::rpg::EventCommand* cmd) override;

private:
	void store();

	QButtonGroup* m_kind = nullptr;
	QStackedWidget* m_pages = nullptr;
	RpgComboBoxBase* m_commonEvent = nullptr;
	QComboBox* m_mapEvent = nullptr;
	QSpinBox* m_page = nullptr;
	RpgComboBoxBase* m_eventVariable = nullptr;
	RpgComboBoxBase* m_pageVariable = nullptr;
	bool m_loading = false;
};
