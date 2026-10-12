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

#include <QFrame>
#include <QPointer>
#include <QTextEdit>

class ProjectData;

/**
 * The codes of a message (\N[n], \C[n], \. ...) as buttons that type them
 * into the text, each explained by its tooltip. A hero name or a variable is
 * chosen in the searchable list of them, a symbol in the game's ExFont ($A...).
 */
class MessageCodePopup : public QFrame {
	Q_OBJECT
public:
	/** Shows the codes under anchor, typing the one chosen into edit */
	static void showCodes(QTextEdit* edit, ProjectData& project, QWidget* anchor);
	/** Shows the symbols */
	static void showSymbols(QTextEdit* edit, ProjectData& project, QWidget* anchor);

private:
	MessageCodePopup(QTextEdit* edit, QWidget* anchor);

	void addCodes(ProjectData& project);
	void addSymbols(ProjectData& project);
	/** Types text at the cursor; the number between [ and ] stays selected to be typed over */
	void insert(const QString& text);
	void place();

	QPointer<QTextEdit> m_edit;
	QWidget* m_anchor;
};
