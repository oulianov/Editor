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

#include <QString>
#include <vector>
#include <lcf/rpg/eventcommand.h>

/**
 * The commands an event can be given, as RPG Maker 2003's "Event Commands"
 * window shows them: pages of buttons in groups, in its order, the battle
 * events' own page, and the Maniac Patch's. Their names are the window's
 * (context EventCommandPalette): CommandPalette.qml shows them.
 */
namespace CommandCatalog {
	using Code = lcf::rpg::EventCommand::Code;

	struct Entry {
		/** Code::END: the gap between two groups of buttons */
		Code code;
		/** The name (QT_TRANSLATE_NOOP, context EventCommandPalette) */
		const char* name;
		bool rpg2k3_only;
		bool maniac_only = false;
	};

	struct Page {
		const char* title;
		std::vector<Entry> entries;
	};

	/** The pages of a map event, without the Maniac Patch's */
	const std::vector<Page>& mapPages();
	/** The commands of the Maniac Patch, by theme */
	const Page& maniacPage();
	/** The commands of battle events (RPG Maker 2003's 37 buttons) */
	const Page& battlePage();
	/** The pages of the window: those of a map event and the Maniac Patch's,
	 *  else for a battle its page (and the Maniac Patch's when the game uses
	 *  it, which also runs its commands in battles) */
	std::vector<const Page*> pages(bool battle, bool maniac);

	/** The name of a kind of command, as on its button (translated); empty when none */
	QString commandName(Code code);
	/** A text of the catalog, translated */
	QString translate(const char* text);
}
