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
#include <array>

class QButtonGroup;
class QLabel;
class QLineEdit;
class FaceSetGraphicsItem;

/** Show Choices: up to four choices and the cancel behavior, with their branches */
class ShowChoiceWidget : public EventCommandBaseWidget
{
	Q_OBJECT

public:
	ShowChoiceWidget(ProjectData& project, QWidget* parent);
	void setData(EventCommandList* commands) override;
	bool apply();

private:
	void updateCancel();

	lcf::rpg::EventCommand m_edited;
	std::array<QLineEdit*, 4> m_choices{};
	std::array<QString, 4> m_texts;
	std::array<bool, 4> m_present{};
	QButtonGroup* m_cancel = nullptr;
};

/** Input Number: digits and the variable receiving the number */
class InputNumberWidget : public EventCommandBaseWidget
{
	Q_OBJECT

public:
	InputNumberWidget(ProjectData& project, QWidget* parent);
	void setData(lcf::rpg::EventCommand* cmd) override;
};

/** Change Face Graphic: the face of the next messages, its side and flip */
class ChangeFaceGraphicWidget : public EventCommandBaseWidget
{
	Q_OBJECT

public:
	ChangeFaceGraphicWidget(ProjectData& project, QWidget* parent);
	void setData(lcf::rpg::EventCommand* cmd) override;

private:
	void refreshFace();

	FaceSetGraphicsItem* m_face = nullptr;
	QLabel* m_file = nullptr;
};

/** Enter Hero Name: the hero, the first keyboard page and the default name */
class EnterHeroNameWidget : public EventCommandBaseWidget
{
	Q_OBJECT

public:
	EnterHeroNameWidget(ProjectData& project, QWidget* parent);
	void setData(lcf::rpg::EventCommand* cmd) override;
};
