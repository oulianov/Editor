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

// What the Skills and Items pages of the database share.

#include <QCoreApplication>
#include <QListWidget>
#include <QStringList>
#include <vector>
#include <lcf/dbbitarray.h>
#include <lcf/rpg/battleranimationitemskill.h>
#include "common/dbstring.h"

class ProjectData;
class QBoxLayout;
class QButtonGroup;
class QComboBox;
namespace lcf::rpg {
	class Item;
}

/** Fills a combo box whose values are 0, 1, 2... in the order of the texts */
void addChoices(QComboBox* combo, const QStringList& texts);

/** Radio buttons added to a layout, grouped with their values as ids */
QButtonGroup* addRadios(QWidget* parent, QBoxLayout* layout, const std::vector<std::pair<int, QString>>& buttons);

/** The names of database entries, one per line of a checklist */
template<class T>
QStringList databaseNames(const std::vector<T>& entries) {
	QStringList names;
	for (const auto& entry : entries) {
		names << ToQString(entry.name);
	}
	return names;
}

/**
 * A list of checkboxes editing a bit array of the database: the attributes
 * and conditions of a skill or item, the heroes who can use an item...
 */
class BitArrayChecklist : public QListWidget {
public:
	explicit BitArrayChecklist(QWidget* parent = nullptr);

	/**
	 * Shows one line per name, checked from the bits.
	 * RPG Maker stores the arrays only up to the last entry it needs: the
	 * entries past their end have the value "missing".
	 */
	void bind(lcf::DBBitArray& bits, const QStringList& names, bool missing = false);

private:
	lcf::DBBitArray* m_bits = nullptr;
	bool m_missing = false;
};

/**
 * The "Set" button of the animation of skills and weapons (RPG Maker 2003):
 * how each hero moves and is drawn when using it.
 */
class BattlerAnimationDataEditor {
	Q_DECLARE_TR_FUNCTIONS(BattlerAnimationDataEditor)

public:
	/** weapon: the weapon whose ranged attack is edited too, null for a skill */
	static void exec(ProjectData& project, std::vector<lcf::rpg::BattlerAnimationItemSkill>& data,
			lcf::rpg::Item* weapon, QWidget* parent);
};
