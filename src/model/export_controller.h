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

#include <QAbstractListModel>
#include <QFutureWatcher>
#include <QPointer>
#include <QStringList>
#include "model/game_export.h"

class ExportController;
class LauncherDownload;
class Project;

/**
 * The platforms of the export, one row each in the settings' order: checked
 * or not, the folder written, the EasyRPG Player used. Enabled, path and
 * launcher can be set (setData).
 */
class ExportPlatformModel : public QAbstractListModel
{
	Q_OBJECT

public:
	enum Role {
		/** ExportPlatform, as an int */
		PlatformRole = Qt::UserRole + 1,
		/** windows, macos, linux, web */
		IdRole,
		/** Its name (DisplayRole too) */
		NameRole,
		EnabledRole,
		/** The folder written, as set (relative to the game folder or absolute) */
		PathRole,
		/** The Player: a path, or ExportPreset::kGameRuntime */
		LauncherRole,
		/** The Player is there */
		LauncherReadyRole,
		/** The Player used, or what is missing */
		LauncherTextRole,
		LauncherToolTipRole,
		/** The Players to choose from (paths) */
		LauncherChoicesRole,
	};

	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
	Qt::ItemFlags flags(const QModelIndex& index) const override;
	QHash<int, QByteArray> roleNames() const override;

	/** Downloads the Player of a row's platform, after asking (ExportController::download) */
	Q_INVOKABLE bool download(int row);
	/** Uses the Player at path for a row's platform: why it cannot, else empty */
	Q_INVOKABLE QString locate(int row, const QString& path);

private:
	friend class ExportController;
	explicit ExportPlatformModel(ExportController* controller);
	void reset();
	void platformChanged(ExportPlatform platform);

	ExportController* m_controller;
};

/**
 * Game Project > Export Game, without its window: the settings of the
 * export (kept in the project, export_presets.json), its run in a worker
 * thread over GameExporter with its progress and log, and the download of
 * the EasyRPG Players (LauncherDownload).
 *
 * The questions (folders to replace, a download) and what prevents an
 * export from starting go through DialogService.
 */
class ExportController : public QObject
{
	Q_OBJECT
	Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY settingsChanged)
	Q_PROPERTY(QString version READ version WRITE setVersion NOTIFY settingsChanged)
	Q_PROPERTY(QString icon READ icon WRITE setIcon NOTIFY settingsChanged)
	Q_PROPERTY(QString iconPath READ iconPath NOTIFY settingsChanged)
	Q_PROPERTY(bool includeRtp READ includeRtp WRITE setIncludeRtp NOTIFY settingsChanged)
	Q_PROPERTY(bool cleanDatabase READ cleanDatabase WRITE setCleanDatabase NOTIFY settingsChanged)
	Q_PROPERTY(bool protect READ protect WRITE setProtect NOTIFY settingsChanged)
	Q_PROPERTY(bool zip READ zip WRITE setZip NOTIFY settingsChanged)
	Q_PROPERTY(bool customizeApp READ customizeApp WRITE setCustomizeApp NOTIFY settingsChanged)
	Q_PROPERTY(bool canProtect READ canProtect NOTIFY sourceChanged)
	/** The game folder (relative paths start from it) */
	Q_PROPERTY(QString sourceFolder READ sourceFolder NOTIFY sourceChanged)
	/** The version of the EasyRPG Player downloaded */
	Q_PROPERTY(QString playerVersion READ playerVersion CONSTANT)
	Q_PROPERTY(bool hasRuntime READ hasRuntime NOTIFY sourceChanged)
	Q_PROPERTY(bool dirty READ isDirty NOTIFY dirtyChanged)
	Q_PROPERTY(ExportPlatformModel* platforms READ platforms CONSTANT)
	Q_PROPERTY(bool running READ isRunning NOTIFY busyChanged)
	Q_PROPERTY(bool downloading READ isDownloading NOTIFY busyChanged)
	Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)
	Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
	Q_PROPERTY(int progressMaximum READ progressMaximum NOTIFY progressChanged)
	Q_PROPERTY(QStringList log READ log NOTIFY logChanged)
	Q_PROPERTY(QStringList folders READ folders NOTIFY foldersChanged)

public:
	explicit ExportController(QObject* parent = nullptr);
	/** A running export stops first */
	~ExportController() override;

	/** The game to export: its settings are read (the saved ones, else the defaults) */
	void setSource(const ExportSource& source, const QString& game_title);
	/** setSource for a project: its folder, format, code page and title; rtp: the RTP folder */
	void setProject(const Project& project, const QString& rtp);
	const ExportSource& source() const;
	QString sourceFolder() const;
	QString playerVersion() const;
	const ExportSettings& settings() const;

	/*
	 * The settings (dirty until saved)
	 */

	QString title() const;
	void setTitle(const QString& title);
	QString version() const;
	void setVersion(const QString& version);
	/** A PNG image, as set (relative to the game folder or absolute) */
	QString icon() const;
	void setIcon(const QString& icon);
	/** The icon's file, empty when none */
	QString iconPath() const;
	bool includeRtp() const;
	void setIncludeRtp(bool on);
	bool cleanDatabase() const;
	void setCleanDatabase(bool on);
	/** The map tree only the Player reads (games in the RPG Maker format) */
	bool protect() const;
	void setProtect(bool on);
	bool canProtect() const;
	/** A zip of each export: all platforms at once */
	bool zip() const;
	void setZip(bool on);
	/** The macOS application gets the game's title and icon */
	bool customizeApp() const;
	void setCustomizeApp(bool on);
	/** The game has its RPG_RT.exe (a Windows launcher) */
	bool hasRuntime() const;
	bool isDirty() const;
	/** Writes the settings in the project; a warning in the log when it cannot */
	Q_INVOKABLE bool save();

	/*
	 * The platforms
	 */

	ExportPlatformModel* platforms() const;
	bool platformEnabled(ExportPlatform platform) const;
	void setPlatformEnabled(ExportPlatform platform, bool on);
	QString platformPath(ExportPlatform platform) const;
	void setPlatformPath(ExportPlatform platform, const QString& path);
	/** The Player used: a path, or ExportPreset::kGameRuntime */
	QString launcher(ExportPlatform platform) const;
	void setLauncher(ExportPlatform platform, const QString& launcher);
	/** The Player is there */
	bool launcherReady(ExportPlatform platform) const;
	/** The Player used, else "Player missing" */
	QString launcherText(ExportPlatform platform) const;
	QString launcherToolTip(ExportPlatform platform) const;
	/** The Players found on this computer, the game's RPG_RT.exe, the one used */
	QStringList launcherChoices(ExportPlatform platform) const;
	/** A Player as the choices show it: "EasyRPG Player 0.8.1.1", "RPG_RT.exe of the game"... */
	Q_INVOKABLE QString describeLauncher(const QString& path) const;
	/** Uses the Player found at path (a folder with it inside: the Player in
	 *  it); why it cannot, else empty */
	QString locateLauncher(ExportPlatform platform, const QString& path);

	/*
	 * The export and the downloads
	 */

	bool isRunning() const;
	bool isDownloading() const;
	bool isBusy() const;
	/** A download can start: nothing runs */
	bool canDownload() const;
	/** The steps done, out of progressMaximum (0: unknown) */
	int progress() const;
	int progressMaximum() const;
	/** What the export and the downloads did, line by line ("+ ", "Warning: ", "Error: ") */
	QStringList log() const;
	/** The folders written by the last export */
	QStringList folders() const;
	/** The folder to open after an export (several platforms: the one that holds them) */
	QString openFolderPath() const;
	Q_INVOKABLE bool openFolder() const;

	/** Exports to every platform checked, after asking before replacing
	 *  folders that are not empty; false when it does not start */
	Q_INVOKABLE bool start();
	/** Stops the export or the download */
	Q_INVOKABLE void cancel();
	/** Downloads and extracts the Player of a platform, after asking; false when it does not start */
	bool download(ExportPlatform platform);

	/** A line of the log as the log shows it */
	static QString logLine(const QString& text, int level);

signals:
	void sourceChanged();
	void settingsChanged();
	void dirtyChanged();
	/** A platform's settings or Player changed */
	void platformChanged(ExportPlatform platform);
	void busyChanged();
	void progressChanged();
	void logChanged();
	/** A line added to the log (logLine), level: GameExporter::Level */
	void lineAdded(const QString& line, int level);
	void logCleared();
	void foldersChanged();
	/** The export ended without errors */
	void finished();
	/** The export ended with errors, or was cancelled */
	void failed();
	/** A download ended: the Player extracted, else the error (empty when cancelled before) */
	void downloadFinished(ExportPlatform platform, const QString& launcher, const QString& error);

private:
	void exportFinished();
	void appendLog(const QString& text, int level);
	void setProgress(int value, int maximum);
	void markDirty();
	/** The first Player found for the platforms without one */
	void pickLaunchers();
	ExportPreset& preset(ExportPlatform platform);
	const ExportPreset* findPreset(ExportPlatform platform) const;

	ExportSource m_source;
	ExportSettings m_settings;
	bool m_hasRuntime = false;
	bool m_dirty = false;
	ExportPlatformModel* m_platforms;
	QPointer<GameExporter> m_exporter;
	QFutureWatcher<bool> m_watcher;
	QPointer<LauncherDownload> m_download;
	int m_progress = 0;
	int m_progressMaximum = 1;
	QStringList m_log;
	QStringList m_folders;
};
