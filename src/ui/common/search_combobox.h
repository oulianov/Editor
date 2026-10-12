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

#include <QComboBox>

class SearchComboPopup;

/**
 * A list of database entries ("0012: Potion") that can be searched: its
 * pop-up has a search field, and typing on the closed list opens it. An
 * entry matches when, for every word typed, its number starts with the word
 * (digits, leading zeros ignored) or its name contains it.
 */
class SearchComboBox : public QComboBox {
	Q_OBJECT
public:
	explicit SearchComboBox(QWidget* parent = nullptr);
	~SearchComboBox() override;

	void showPopup() override;
	void hidePopup() override;

	/** Whether the entry of this text (and number, -1: read from the text) matches the search */
	static bool matches(const QString& text, int id, const QString& search);

protected:
	void keyPressEvent(QKeyEvent* event) override;

private:
	void openSearch(const QString& search);

	SearchComboPopup* m_popup = nullptr;
};
