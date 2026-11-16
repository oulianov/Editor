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
 * and groups of RPG Maker with the terms of the game's engine only, the
 * messages as RPG Maker has them and, for those the Maniac Patch adds parts
 * to, as it has them (the page chooses: greyed without it, or as RPG
 * Maker's when the Editor Settings hide what the game does not run).
 *
 *   Ez.VocabularyTable { id: table; rpg2k3: true; maniac: false }
 *   Repeater { model: table.pages ... qsTranslate("VocabularyWidget", modelData.title) }
 *
 * pages: [{title, groups: [{title, column, pairs, origin, rows: [{label,
 * lines: [[{term, limit, own, origin, fallback, fallbackHint} or {hint}]],
 * maniacLines, maniacTerms}]}]}]: a row's fields and what the game puts
 * between them, in the order of the message, on one line or more; the
 * same with the Maniac Patch (maniacLines, for the rows it adds to: own,
 * not a part it adds, whose origin is "maniac"), and the terms it adds
 * (maniacTerms). The origin of a group
 * (Ez.Origins): "player" for the terms only EasyRPG Player reads,
 * "maniac" for those the Maniac Patch reads too in its games, else empty
 * (RPG Maker's). A term with a fallback is left to EasyRPG Player's text
 * ("default_term") while empty. The texts are the source texts of the
 * context "VocabularyWidget", translated by the page (qsTranslate;
 * fallback only with fallbackHint), so a new language shows at once.
 */
class VocabularyTable : public QObject {
	Q_OBJECT
	QML_ELEMENT
	/** The game is made with RPG Maker 2003 (else 2000) */
	Q_PROPERTY(bool rpg2k3 READ rpg2k3 WRITE setRpg2k3 NOTIFY pagesChanged)
	/** The game uses the Maniac Patch (the origin of the groups it reads too) */
	Q_PROPERTY(bool maniac READ maniac WRITE setManiac NOTIFY pagesChanged)
	Q_PROPERTY(QVariantList pages READ pages NOTIFY pagesChanged)

public:
	explicit VocabularyTable(QObject* parent = nullptr);

	bool rpg2k3() const {
		return m_rpg2k3;
	}
	void setRpg2k3(bool on);
	bool maniac() const {
		return m_maniac;
	}
	void setManiac(bool on);
	QVariantList pages() const;

signals:
	void pagesChanged();

private:
	bool m_rpg2k3 = true;
	bool m_maniac = false;
};
