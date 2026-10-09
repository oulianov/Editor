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

#include "maniac_widgets.h"

class QTabWidget;

/**
 * Show Picture (11110) and Move Picture (11120). With Maniac Patch, as
 * EasyRPG Player reads them: values from variables (modes in p17, p20, p21),
 * origin (p1 >> 8), blend mode and flips (above the bottom transparency),
 * a separate height, the fixed angle effect and, for Show Picture, the file
 * name from a string, the spritesheet, the layers and the erase flags.
 */
class PictureWidget : public ManiacCommandWidget {
	Q_OBJECT
public:
	PictureWidget(ProjectData& project, bool move, QWidget* parent = nullptr);
	void setData(lcf::rpg::EventCommand* cmd) override;

protected:
	void growParameters(int index) override;

private:
	void addManiacTab(QTabWidget* tabs, bool enabled);
	bool m_move;
};

class ShowPictureWidget : public PictureWidget {
	Q_OBJECT
public:
	ShowPictureWidget(ProjectData& project, QWidget* parent = nullptr);
};

class MovePictureWidget : public PictureWidget {
	Q_OBJECT
public:
	MovePictureWidget(ProjectData& project, QWidget* parent = nullptr);
};

class ErasePictureWidget : public EventCommandBaseWidget {
	Q_OBJECT
public:
	ErasePictureWidget(ProjectData& project, QWidget* parent = nullptr);
	void setData(lcf::rpg::EventCommand* cmd) override;
};

class ScreenTransitionWidget : public EventCommandBaseWidget {
	Q_OBJECT
public:
	ScreenTransitionWidget(ProjectData& project, bool show, QWidget* parent = nullptr);
	void setData(lcf::rpg::EventCommand* cmd) override;

private:
	bool m_show;
};

class EraseScreenWidget : public ScreenTransitionWidget {
	Q_OBJECT
public:
	EraseScreenWidget(ProjectData& project, QWidget* parent = nullptr);
};

class ShowScreenWidget : public ScreenTransitionWidget {
	Q_OBJECT
public:
	ShowScreenWidget(ProjectData& project, QWidget* parent = nullptr);
};

class PanScreenWidget : public EventCommandBaseWidget {
	Q_OBJECT
public:
	PanScreenWidget(ProjectData& project, QWidget* parent = nullptr);
	void setData(lcf::rpg::EventCommand* cmd) override;
};

class ChangeScreenTransitionsWidget : public EventCommandBaseWidget {
	Q_OBJECT
public:
	ChangeScreenTransitionsWidget(ProjectData& project, QWidget* parent = nullptr);
	void setData(lcf::rpg::EventCommand* cmd) override;
};

class ShowBattleAnimationWidget : public EventCommandBaseWidget {
	Q_OBJECT
public:
	ShowBattleAnimationWidget(ProjectData& project, QWidget* parent = nullptr);
	void setData(lcf::rpg::EventCommand* cmd) override;
};

class FlashSpriteWidget : public EventCommandBaseWidget {
	Q_OBJECT
public:
	FlashSpriteWidget(ProjectData& project, QWidget* parent = nullptr);
	void setData(lcf::rpg::EventCommand* cmd) override;
};

class PlayerVisibilityWidget : public EventCommandBaseWidget {
	Q_OBJECT
public:
	PlayerVisibilityWidget(ProjectData& project, QWidget* parent = nullptr);
	void setData(lcf::rpg::EventCommand* cmd) override;
};
