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

class QCheckBox;
class QComboBox;
class QLineEdit;

/** The options of the Editor, in one window (EditorSettings) */
class SettingsDialog : public QDialog
{
	Q_OBJECT

public:
	explicit SettingsDialog(QWidget* parent = nullptr);

	void accept() override;

protected:
	void resizeEvent(QResizeEvent* event) override;

private:
	QComboBox* m_language;
	QCheckBox* m_reloadExternal;
	QCheckBox* m_searchCache;
	QLineEdit* m_rtpPath;
	QLineEdit* m_playerPath;
	QCheckBox* m_animateTiles;
	QCheckBox* m_commandColors;
	QCheckBox* m_commandStripes;
};
