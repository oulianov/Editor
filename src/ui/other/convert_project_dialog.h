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
#include "model/project_convert.h"

class QCheckBox;
class Project;

/**
 * RPG Maker 2003's question when an RPG Maker 2000 project is opened:
 * what the conversion changes, where the backup goes, and the choices of the
 * EasyRPG Player. The conversion itself is ProjectConvert::to2003.
 */
class ConvertProjectDialog : public QDialog
{
	Q_OBJECT

public:
	/** @param at_open asked when the project is opened: offers "Don't ask again" */
	ConvertProjectDialog(const Project& project, bool at_open, QWidget* parent = nullptr);

	ProjectConvert::Options options() const;
	/** "Don't ask again for this project" checked (only when asked at open) */
	bool dontAskAgain() const;

	/**
	 * Whether to ask when this project is opened: an RPG Maker 2000 project,
	 * not declined with "Don't ask again", not yet asked in this session.
	 * Asking is remembered for the session.
	 */
	static bool askAtOpen(const Project& project);
	/** Remembers "Don't ask again" for the folder of the project */
	static void setAskAtOpen(const Project& project, bool ask);

	/** The outcome of ProjectConvert::to2003 */
	static void showResult(QWidget* parent, const QString& error, const QString& backup_path);

private:
	QCheckBox* m_keepBattle;
	QCheckBox* m_keepExperience;
	QCheckBox* m_dontAsk = nullptr;
};
