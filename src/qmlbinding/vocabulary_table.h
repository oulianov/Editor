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

#include <QObject>
#include <QVariantList>
#include <QtQmlIntegration>

/**
 * The terms of the Vocabulary page for QML (VocabularyFields): the pages
 * and groups of RPG Maker with the terms of the game's engine only.
 *
 *   Ez.VocabularyTable { id: table; rpg2k3: true }
 *   Repeater { model: table.pages ... qsTranslate("VocabularyWidget", modelData.title) }
 *
 * pages: [{title, groups: [{title, column, pairs, rows: [{label, term,
 * limit, hint, second, secondLimit}]}]}]; the texts are the source texts
 * of the context "VocabularyWidget", translated by the page (qsTranslate),
 * so a new language shows at once.
 */
class VocabularyTable : public QObject {
	Q_OBJECT
	QML_ELEMENT
	/** The game is made with RPG Maker 2003 (else 2000) */
	Q_PROPERTY(bool rpg2k3 READ rpg2k3 WRITE setRpg2k3 NOTIFY pagesChanged)
	Q_PROPERTY(QVariantList pages READ pages NOTIFY pagesChanged)

public:
	explicit VocabularyTable(QObject* parent = nullptr);

	bool rpg2k3() const {
		return m_rpg2k3;
	}
	void setRpg2k3(bool on);
	QVariantList pages() const;

signals:
	void pagesChanged();

private:
	bool m_rpg2k3 = true;
};
