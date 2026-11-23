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
#include <QStringList>

/**
 * The names a file of the RTP has in the other RTPs, as EasyRPG Player maps
 * them: Actor1 of the English RPG Maker 2000 RTP (and of the EasyRPG RTP)
 * is 主人公1 in the Japanese ones, hero1 in an English RPG Maker 2003 RTP...
 * A game asking for a file its RTP folders name otherwise finds it as the
 * Player does.
 */
namespace RtpNames {
	/**
	 * The other names of the file name of a folder of the RTP ("CharSet",
	 * any case), the RPG Maker 2000 RTPs first; empty for a file of no RTP
	 */
	QStringList alternatives(const QString& folder, const QString& name);
}
