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
#include <QPointer>
#include <QString>

class DialogService;
class LauncherDownload;

/**
 * The EasyRPG Player of this computer for test play, downloaded from the
 * Settings or when Run Game finds none: the build of easyrpg.org for this
 * platform (LauncherDownload, as the export's), extracted in the Editor's
 * data folder. The one of this version already there is used
 * without downloading it again. The failures go through
 * the DialogService given, else the installed one.
 */
class PlayerDownload : public QObject
{
	Q_OBJECT
	Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)
	/** In KB, out of progressMaximum (0: unknown yet) */
	Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
	Q_PROPERTY(int progressMaximum READ progressMaximum NOTIFY progressChanged)
	/** The version downloaded ("0.8.1.1") */
	Q_PROPERTY(QString version READ version CONSTANT)
	/** The archive downloaded, its file name */
	Q_PROPERTY(QString fileName READ fileName CONSTANT)

public:
	explicit PlayerDownload(QObject* parent = nullptr);
	/** A download still running is cancelled */
	~PlayerDownload() override;

	bool isBusy() const;
	int progress() const;
	int progressMaximum() const;
	QString version() const;
	QString fileName() const;

	/** Where the failures go (nullptr: the installed DialogService) */
	void setDialogService(DialogService* dialogs);

	/**
	 * The Player of this version already downloaded (finished at once),
	 * else downloads it
	 *
	 * @return false when nothing starts (refused, already running)
	 */
	Q_INVOKABLE bool start();
	/** Stops the download: finished with no program, nothing told */
	Q_INVOKABLE void cancel();

	/** The program of the Player downloaded, as the Settings keep it; empty if none */
	static QString downloadedProgram();

signals:
	void busyChanged();
	void progressChanged();
	/** The program to run; empty when cancelled or failed (the failure told) */
	void finished(const QString& program);

private:
	DialogService& dialogs() const;
	void setProgress(int value, int maximum);

	DialogService* m_dialogs = nullptr;
	QPointer<LauncherDownload> m_download;
	bool m_cancelled = false;
	int m_progress = 0;
	int m_progressMaximum = 0;
};
