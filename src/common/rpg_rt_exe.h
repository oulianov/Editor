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

#include <QByteArray>
#include <QString>
#include <QStringList>

/**
 * What RPG_RT.exe tells, read the way EasyRPG Player does (exe_reader.cpp):
 * its ExFont, and the engine it is (Player::EngineRpg2k...), from its
 * version resource, its logos and the sizes of its sections.
 */
namespace RpgRtExe {
	/** The flags of Player::EngineType */
	enum Engine {
		EngineNone = 0,
		EngineRpg2k = 1,
		EngineRpg2k3 = 2,
		EngineMajorUpdated = 4,
		EngineEnglish = 8,
	};

	struct Info {
		/** A Windows program (x86 or x64) */
		bool program = false;
		/** EasyRPG Player under the name of RPG_RT.exe */
		bool easyrpgPlayer = false;
		/** Its VERSIONINFO ("1.0.8.0"), empty when it has none */
		QString version;
		/** The logos shown at its start (the XYZ resources) */
		int logos = 0;
		quint32 codeSize = 0;
		quint32 cherrySize = 0;
		quint32 geepSize = 0;
		/** Compressed with UPX: what it is may be wrong */
		bool upx = false;
		/** The Maniac Patch: its version (a date, 240101...), else 0 */
		int maniac = 0;
		/** Engine flags, EngineNone when not known */
		int engine = EngineNone;
	};

	Info read(const QString& path);
	/** The bitmap resource named EXFONT, as a BMP file */
	QByteArray exfont(const QString& path);

	/** The fonts of Windows it asks for: font 1 and font 2 of the database
	 *  (MS Gothic, MS Mincho in Japanese; translations change them), empty when not found */
	QStringList fontNames(const QString& path);

	/** "RPG Maker 2003 1.05-1.09"... */
	QString engineName(int engine);
	/** The flags of an Engine= value of EasyRPG.ini ("2003v105", "rpg2ke"...), else EngineNone */
	int engineFromSetting(const QString& value);
}
