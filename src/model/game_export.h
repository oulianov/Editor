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
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <atomic>
#include "common/filefinder.h"

/** The platforms a game is exported to */
enum class ExportPlatform { Windows, MacOS, Linux, Web };

/** One target of the export: where, and with which program */
struct ExportPreset
{
	ExportPlatform platform = ExportPlatform::Windows;
	bool enabled = false;
	/** The folder written (emptied first) */
	QString path;
	/**
	 * The EasyRPG Player of the platform: Player.exe, the "EasyRPG Player.app"
	 * bundle, the easyrpg-player program, or the folder of the web Player
	 * (a release with index.html, or a Player build folder: see WebPlayer).
	 * kGameRuntime keeps the game's RPG_RT.exe (Windows).
	 */
	QString launcher;
	/** A .zip of the folder next to it (web: the zip to upload to itch.io) */
	bool zip = false;
	/** macOS: the application gets the name and the icon of the game (signed again for this computer) */
	bool customize = false;

	static constexpr const char* kGameRuntime = "RPG_RT.exe";
	/** Id in the presets file and folder of the launchers: windows, macos, linux, web */
	static QString id(ExportPlatform platform);
	static QString name(ExportPlatform platform);
};

/**
 * The export settings of a project, kept in the project folder
 * (kFileName, never exported) so that they travel with the game.
 */
struct ExportSettings
{
	QString title;
	QString version;
	/** A PNG image */
	QString icon;
	bool include_rtp = true;
	bool clean_database = false;
	bool protect = false;
	QList<ExportPreset> presets;

	static constexpr const char* kFileName = "export_presets.json";

	/** The saved settings of the project, else the defaults (export folders next to the project) */
	static ExportSettings load(const QDir& project_dir, const QString& title);
	bool save(const QDir& project_dir) const;
	ExportPreset& preset(ExportPlatform platform);
};

/** The project read by the export, from its files */
struct ExportSource
{
	QDir dir;
	FileFinder::ProjectType type = FileFinder::ProjectType::Legacy;
	QString encoding;
	/** The RTP folder of the settings, empty if none */
	QString rtp;
};

/**
 * Writes a game ready to play, for each enabled preset: the files of the
 * game (no tool files), the RTP files it uses, the launcher. run() works in
 * a worker thread: the signals reach the window in its own thread.
 */
class GameExporter : public QObject
{
	Q_OBJECT
public:
	enum Level { Info, Added, Warning, Error };

	GameExporter(const ExportSource& source, const ExportSettings& settings, QObject* parent = nullptr);

	/** Exports every enabled preset; false on error or cancel */
	bool run();
	void cancel();

	/** The written folders, after run() */
	QStringList folders() const;

	/** Whether a launcher path is the program the platform needs */
	static bool isLauncher(ExportPlatform platform, const QString& path);
	/** Why an export folder cannot be used (in the project, the project in it...), else empty */
	static QString checkFolder(const QDir& project_dir, const QString& folder);
	/** Whether a path of the game (relative, with /) is a tool file left out of exports */
	static bool isExcluded(const QString& path, bool rpgsync);
	/** Whether the game is edited with rpgsync (Scripts/pyproject.toml with [tool.rpgsync]) */
	static bool usesRpgsync(const QDir& project_dir);

signals:
	void progress(int value, int maximum);
	void message(const QString& text, int level);

private:
	struct PlannedFile {
		QString source;
		QString path;  // in the game, with /
	};

	bool prepare();
	bool exportPreset(const ExportPreset& preset);
	bool writeGame(const QString& dir);
	bool writeLauncher(const ExportPreset& preset, const QString& dir);
	bool writeWeb(const ExportPreset& preset, const QString& dir);
	bool step();
	void log(const QString& text, int level = Info);

	ExportSource m_source;
	ExportSettings m_settings;
	std::atomic<bool> m_cancelled{ false };
	QList<PlannedFile> m_files;
	QByteArray m_ini;
	QString m_iniName;
	QByteArray m_database;
	QByteArray m_mapTree;
	QString m_treeName;
	QStringList m_folders;
	int m_step = 0;
	int m_steps = 0;
};
