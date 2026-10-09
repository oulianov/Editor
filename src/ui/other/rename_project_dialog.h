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
#include <QDir>

class QLineEdit;

/** RPG Maker's Rename Project: the folder of a game and its title */
class RenameProjectDialog : public QDialog
{
	Q_OBJECT

public:
	RenameProjectDialog(const QDir& folder, const QString& title, QWidget* parent = nullptr);

	QString folderName() const;
	QString gameTitle() const;

	void accept() override;

private:
	QDir m_folder;
	QLineEdit* m_folderName;
	QLineEdit* m_title;
};
