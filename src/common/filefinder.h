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

#include <QDir>

namespace FileFinder {
	enum class FileType {
		Default,
		Image,
		Sound,
		Music,
		Video,
		Font
	};

	enum class ProjectType {
		None,
		/** EasyRPG project */
		EasyRpg,
		/** Legacy (RPG Maker 2k/2k3) project */
		Legacy
	};

	QString Find(const QDir& directory, const QString& filename, FileType type = FileType::Default, QDir::Filter filter = QDir::Files);
	QString Find(const QDir& baseDir, const QString& subDir, const QString& filename, FileType type = FileType::Default, QDir::Filter filter = QDir::Files);

	ProjectType GetProjectType(const QDir& directory);

	bool IsRPG2kProject(const QDir& directory);

	bool IsEasyRpgProject(const QDir& directory);

	QString CombinePath(const QString& path1, const QString& path2);

	/** Replaces the file to by the file from in one step (on Windows too,
	 *  unlike QFile::rename). from must be closed: Windows does not rename an
	 *  open file. A file another program holds open for a moment (an
	 *  antivirus, a backup tool) is tried again for up to a second. error
	 *  gets why it failed, in the system's words. */
	bool ReplaceFile(const QString& from, const QString& to, QString* error = nullptr);

	QStringList GetFiltersForType(FileType type);
};

