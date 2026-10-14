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

#include <QPointer>
#include <QWindow>
#include <functional>
#include "controller/dialog_service.h"
#include "controller/shell_dialogs.h"

/**
 * The questions and messages of the controllers: in QML windows
 * (QmlDialogService) over the window in front, else over the main window.
 */
class QmlShellDialogService : public DialogService
{
public:
	/** main: the main window, which the questions go over by default */
	explicit QmlShellDialogService(std::function<QWindow*()> main);

	StaleChoice askStale(const QString& what, bool can_reload, const QStringList& conflicts) override;
	Answer question(const QString& title, const QString& text, Buttons buttons = Buttons::YesNo) override;
	void information(const QString& title, const QString& text) override;
	void warning(const QString& title, const QString& text) override;
	void error(const QString& title, const QString& text) override;

private:
	/** The service of the window in front, made for one question (a nested
	 *  question has its own) */
	std::unique_ptr<DialogService> front() const;

	std::function<QWindow*()> m_main;
};

/**
 * The editing windows of the QML main window's actions (ShellDialogs),
 * over its window: the project windows (ProjectWindows) and the map tree's
 * (MapTreeDialogs), Settings, Help and About, the progress of an import.
 */
class QmlShellDialogs : public ShellDialogs
{
public:
	/** window: the main window, the QML windows go over it */
	explicit QmlShellDialogs(std::function<QWindow*()> window);

	std::optional<NewProject> newProject(const QString& def_dir) override;
	OpenProject openProject(const QString& def_dir) override;
	ImportProject importProject(const QString& def_dir) override;
	void withImportProgress(const std::function<void(const ProjectController::ImportProgress&)>& work) override;
	std::optional<ProjectConvert::Options> convertProject(const Project& project) override;
	void convertResult(const QString& error, const QString& backup) override;
	bool mapProperties(lcf::rpg::MapInfo& info, lcf::rpg::Map& map) override;
	bool areaProperties(lcf::rpg::MapInfo& info, const QSize& map_size) override;
	std::optional<Dungeon> generateDungeon(const lcf::rpg::Map& source, const QString& name) override;
	std::optional<QPoint> shiftMap() override;

	/** The QML windows of the actions */
	void settings();
	void help(const QString& topic);
	void about();

private:
	std::function<QWindow*()> m_window;
};
