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

#include <QDialog>
#include <QDir>
#include <QFutureWatcher>
#include "model/game_export.h"

class LauncherDownload;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QToolButton;

/**
 * Game Project > Export Game: writes the game ready to play for each
 * platform checked (the game folder, Windows, macOS, Linux, the web), like
 * RPG Maker's Create Gamedisk. One page, read from top to bottom: the game,
 * the platforms with their folder and Player, the options, then Export.
 * The settings are kept in the project (export_presets.json, never exported).
 */
class ExportDialog : public QDialog
{
	Q_OBJECT

public:
	explicit ExportDialog(QWidget* parent = nullptr);
	~ExportDialog() override;

public slots:
	void reject() override;

private:
	/** The line of a platform: checked, its folder, its Player */
	struct Row {
		ExportPlatform platform;
		QCheckBox* check = nullptr;
		QLineEdit* path = nullptr;
		QToolButton* browse = nullptr;
		QPushButton* launcher = nullptr;
	};

	void updateRow(Row& row);
	void updateLauncher(Row& row);
	void updateIcon();
	void chooseFolder(Row& row);
	void chooseLauncher(ExportPlatform platform);
	void chooseIcon();
	void download(ExportPlatform platform);
	void startExport();
	void exportFinished();
	void cancel();
	void openFolder();
	void appendLog(const QString& text, int level);
	void setBusy(bool busy);
	void saveSettings();

	QDir m_projectDir;
	ExportSettings m_settings;
	bool m_hasRuntime = false;
	bool m_dirty = false;
	bool m_closeWhenDone = false;
	QStringList m_folders;
	GameExporter* m_exporter = nullptr;
	QFutureWatcher<bool> m_watcher;
	LauncherDownload* m_download = nullptr;

	QList<Row> m_rows;
	QLineEdit* m_title;
	QLineEdit* m_version;
	QLineEdit* m_icon;
	QLabel* m_iconPreview;
	QCheckBox* m_rtp;
	QCheckBox* m_clean;
	QCheckBox* m_protect;
	QCheckBox* m_zip;
	QCheckBox* m_customize;
	QPlainTextEdit* m_log;
	QProgressBar* m_progress;
	QPushButton* m_open;
	QPushButton* m_export;
	QPushButton* m_cancel;
	QPushButton* m_close;
};
