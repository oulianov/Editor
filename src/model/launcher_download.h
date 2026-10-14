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

#include <QFile>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QStringList>
#include <QUrl>
#include "model/game_export.h"

class QNetworkReply;

/**
 * The EasyRPG Player builds the export puts next to the game: found on this
 * computer, or downloaded from easyrpg.org into the Editor's data folder
 * (launchers/<platform>/<version>).
 */
namespace Launchers {
	/** The stable version downloaded */
	QString version();
	/**
	 * The official build of the platform. EASYRPG_EDITOR_LAUNCHER_BASE (a
	 * folder URL, file:// too) replaces easyrpg.org: mirrors and tests.
	 */
	QUrl url(ExportPlatform platform);
	/**
	 * Where the downloads of the platform go: the Editor's data folder, else
	 * EASYRPG_EDITOR_LAUNCHER_DIR (the scripts: their own, empty at start)
	 */
	QString folder(ExportPlatform platform);
	/** The launchers of this computer, best first: downloaded, then the Player of the settings */
	QStringList find(ExportPlatform platform);
	/** The launcher in a folder (an extracted archive), empty if none */
	QString findIn(ExportPlatform platform, const QString& dir);
	/** The launchers downloaded and extracted completely, the newest first */
	QStringList downloads(ExportPlatform platform);
	/** The launcher of version() downloaded, empty if none */
	QString downloaded(ExportPlatform platform);
	/** The platform of this computer (macOS, Windows, else Linux) */
	ExportPlatform hostPlatform();
}

/**
 * Downloads and extracts the EasyRPG Player of a platform. Archives are
 * extracted by the system's tools (ditto on macOS, unzip and tar on Linux,
 * tar.exe of Windows 10 and later): Qt has no public archive reader.
 */
class LauncherDownload : public QObject
{
	Q_OBJECT
public:
	explicit LauncherDownload(ExportPlatform platform, QObject* parent = nullptr);
	~LauncherDownload() override;

	void start();
	void cancel();

signals:
	void progress(qint64 received, qint64 total);
	/** launcher: the program found in the archive, empty on error or cancel */
	void finished(const QString& launcher, const QString& error);

private:
	void downloaded();
	void extracted(int exit_code, QProcess::ExitStatus status);
	void fail(const QString& error);

	ExportPlatform m_platform;
	QNetworkAccessManager m_network;
	QPointer<QNetworkReply> m_reply;
	QPointer<QProcess> m_process;
	QFile m_file;
	QString m_dir;
	bool m_cancelled = false;
};
