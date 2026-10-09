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
#include <QCoreApplication>
#include <cstdint>
#include <vector>
#include <lcf/dbstring.h>
#include "common/dbstring.h"

class ProjectData;
class QScrollArea;

namespace lcf::rpg {
	class Music;
	class Sound;
}

// Fields shared by the System, System 2 and Vocabulary pages of the database
namespace SystemFields {
	/** A file of folder dir (an image) and a button opening its picker.
	 * With index, the file is a charset and index is the sprite in it. */
	QWidget* imageField(ProjectData& project, const QString& dir, lcf::DBString& file,
		QWidget* parent, int32_t* index = nullptr);

	/** A music or a sound and a button opening its picker (volume, tempo...) */
	QWidget* audioField(ProjectData& project, lcf::rpg::Music& music, QWidget* parent);
	QWidget* audioField(ProjectData& project, lcf::rpg::Sound& sound, QWidget* parent);

	/** The page in a frameless scroll area, so that it keeps its size on small screens */
	QScrollArea* scrollable(QWidget* page);

	/** Combos edit the IDs of list in order; the empty choices (0) are left out */
	void bindIdList(QWidget* context, const std::vector<QComboBox*>& combos, std::vector<int16_t>& list);

	/** Fills combo with "<none>" (0) and the entries of items accepted by filter, by ID */
	template<class T, class Filter>
	void fillWithNone(QComboBox* combo, const std::vector<T>& items, Filter filter) {
		// long names are elided instead of widening the page
		combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
		combo->setMinimumContentsLength(8);
		combo->addItem(QCoreApplication::translate("SystemFields", "<none>"), 0);
		for (const auto& item : items) {
			if (filter(item)) {
				combo->addItem(QStringLiteral("%1: %2").arg(item.ID, 4, 10, QChar('0')).arg(ToQString(item.name)), item.ID);
			}
		}
	}

	template<class T>
	void fillWithNone(QComboBox* combo, const std::vector<T>& items) {
		fillWithNone(combo, items, [](const T&) { return true; });
	}
}
