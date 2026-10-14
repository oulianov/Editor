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

#include "launcher_download.h"
#include "common/editor_settings.h"
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QVersionNumber>
#include <algorithm>

namespace {
// the latest stable release of the EasyRPG Player (easyrpg.org/player/downloads)
const char kVersion[] = "0.8.1.1";
const char kBaseUrl[] = "https://easyrpg.org/downloads/player/0.8.1.1/";

QString archiveName(ExportPlatform platform)
{
	const QString version = QString::fromLatin1(kVersion);
	switch (platform) {
	case ExportPlatform::Windows: return QStringLiteral("easyrpg-player-%1-windows-x64.zip").arg(version);
	case ExportPlatform::MacOS: return QStringLiteral("easyrpg-player-%1-macos.app.zip").arg(version);
	case ExportPlatform::Linux: return QStringLiteral("easyrpg-player-%1-linux.tar.gz").arg(version);
	case ExportPlatform::Web: return QStringLiteral("easyrpg-player-%1-js.tar.gz").arg(version);
	}
	return QString();
}
}

QString Launchers::version()
{
	return QString::fromLatin1(kVersion);
}

QUrl Launchers::url(ExportPlatform platform)
{
	const QString name = archiveName(platform);
	if (name.isEmpty()) {
		return QUrl();
	}
	QString base = qEnvironmentVariable("EASYRPG_EDITOR_LAUNCHER_BASE", QString::fromLatin1(kBaseUrl));
	if (!base.endsWith(QLatin1Char('/'))) {
		base += QLatin1Char('/');
	}
	return QUrl(base).resolved(QUrl(name));
}

QString Launchers::folder(ExportPlatform platform)
{
	QString root = qEnvironmentVariable("EASYRPG_EDITOR_LAUNCHER_DIR");
	if (root.isEmpty()) {
		root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/launchers");
	}
	return root + QLatin1Char('/') + ExportPreset::id(platform);
}

namespace {
/** The launcher extracted in the folder of a version: none while its
 *  archive is there (removed once extracted, kept by an interrupted one) */
QString extracted(ExportPlatform platform, const QString& dir)
{
	const QString archive = Launchers::url(platform).fileName();
	if (!archive.isEmpty() && QFileInfo::exists(dir + QLatin1Char('/') + archive)) {
		return QString();
	}
	return Launchers::findIn(platform, dir);
}
}

QStringList Launchers::downloads(ExportPlatform platform)
{
	const QDir dir(folder(platform));
	QStringList versions = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
	std::sort(versions.begin(), versions.end(), [](const QString& a, const QString& b) {
		return QVersionNumber::fromString(a) > QVersionNumber::fromString(b);
	});
	QStringList found;
	for (const QString& v : versions) {
		const QString launcher = extracted(platform, dir.filePath(v));
		if (!launcher.isEmpty() && GameExporter::isLauncher(platform, launcher)) {
			found << QDir::cleanPath(launcher);
		}
	}
	return found;
}

QString Launchers::downloaded(ExportPlatform platform)
{
	const QString launcher = extracted(platform, folder(platform) + QLatin1Char('/') + version());
	return GameExporter::isLauncher(platform, launcher) ? QDir::cleanPath(launcher) : QString();
}

ExportPlatform Launchers::hostPlatform()
{
#if defined(Q_OS_MACOS)
	return ExportPlatform::MacOS;
#elif defined(Q_OS_WIN)
	return ExportPlatform::Windows;
#else
	return ExportPlatform::Linux;
#endif
}

QString Launchers::findIn(ExportPlatform platform, const QString& dir)
{
	if (platform == ExportPlatform::Web && GameExporter::isLauncher(platform, dir)) {
		return dir;
	}
	QString any_exe;
	QDirIterator it(dir, QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
	while (it.hasNext()) {
		const QFileInfo info(it.next());
		const QString name = info.fileName();
		switch (platform) {
		case ExportPlatform::Windows:
			if (name.compare(QStringLiteral("Player.exe"), Qt::CaseInsensitive) == 0) {
				return info.filePath();
			}
			if (any_exe.isEmpty() && GameExporter::isLauncher(platform, info.filePath())) {
				any_exe = info.filePath();
			}
			break;
		case ExportPlatform::MacOS:
		case ExportPlatform::Web:
			if (info.isDir() && GameExporter::isLauncher(platform, info.filePath())) {
				return info.filePath();
			}
			break;
		case ExportPlatform::Linux:
			if (name == QStringLiteral("easyrpg-player") && info.isFile()) {
				return info.filePath();
			}
			break;
		}
	}
	return any_exe;
}

QStringList Launchers::find(ExportPlatform platform)
{
	QStringList found;
	auto add = [&](const QString& path) {
		const QString clean = path.isEmpty() ? QString() : QDir::cleanPath(path);
		if (!clean.isEmpty() && !found.contains(clean) && GameExporter::isLauncher(platform, clean)) {
			found << clean;
		}
	};

	// downloaded, the newest first
	for (const QString& launcher : downloads(platform)) {
		add(launcher);
	}

	// the Player of this computer (test play), for the platform of this computer
	const QString player = EditorSettings::instance().playerPath();
	const QString app_dir = QCoreApplication::applicationDirPath();
#if defined(Q_OS_MACOS)
	if (platform == ExportPlatform::MacOS) {
		// the settings name the program inside the bundle
		QString bundle = player;
		while (!bundle.isEmpty() && !bundle.endsWith(QStringLiteral(".app")) && bundle != QFileInfo(bundle).path()) {
			bundle = QFileInfo(bundle).path();
		}
		add(bundle);
		add(app_dir + QStringLiteral("/../../../EasyRPG Player.app"));
		add(QStringLiteral("/Applications/EasyRPG Player.app"));
		add(QDir::homePath() + QStringLiteral("/Applications/EasyRPG Player.app"));
	}
#elif defined(Q_OS_WIN)
	if (platform == ExportPlatform::Windows) {
		add(player);
		add(app_dir + QStringLiteral("/Player.exe"));
	}
#else
	if (platform == ExportPlatform::Linux) {
		add(player);
		add(app_dir + QStringLiteral("/easyrpg-player"));
		add(QStandardPaths::findExecutable(QStringLiteral("easyrpg-player")));
	}
#endif
	Q_UNUSED(player);
	Q_UNUSED(app_dir);
	return found;
}

LauncherDownload::LauncherDownload(ExportPlatform platform, QObject* parent) :
	QObject(parent), m_platform(platform)
{
}

LauncherDownload::~LauncherDownload()
{
	// the window that listens is closing
	blockSignals(true);
	cancel();
}

void LauncherDownload::start()
{
	const QUrl url = Launchers::url(m_platform);
	m_dir = Launchers::folder(m_platform) + QLatin1Char('/') + Launchers::version();
	// a fresh folder: what an interrupted download left is replaced
	QDir(m_dir).removeRecursively();
	if (url.isEmpty() || !QDir().mkpath(m_dir)) {
		fail(tr("Cannot create %1.").arg(QDir::toNativeSeparators(m_dir)));
		return;
	}
	m_file.setFileName(m_dir + QLatin1Char('/') + url.fileName());
	if (!m_file.open(QIODevice::WriteOnly)) {
		fail(tr("Cannot write %1.").arg(QDir::toNativeSeparators(m_file.fileName())));
		return;
	}
	m_reply = m_network.get(QNetworkRequest(url));
	connect(m_reply.data(), &QNetworkReply::readyRead, this, [this] {
		// none after an abort (cancel)
		if (m_reply->isOpen()) {
			m_file.write(m_reply->readAll());
		}
	});
	connect(m_reply.data(), &QNetworkReply::downloadProgress, this, &LauncherDownload::progress);
	connect(m_reply.data(), &QNetworkReply::finished, this, &LauncherDownload::downloaded);
}

void LauncherDownload::cancel()
{
	m_cancelled = true;
	if (m_reply) {
		m_reply->abort();
	}
	if (m_process) {
		m_process->kill();
	}
}

void LauncherDownload::fail(const QString& error)
{
	m_file.close();
	if (!m_dir.isEmpty()) {
		QDir(m_dir).removeRecursively();
	}
	emit finished(QString(), error);
}

void LauncherDownload::downloaded()
{
	m_reply->deleteLater();
	if (m_cancelled) {
		fail(tr("Download cancelled."));
		return;
	}
	m_file.write(m_reply->readAll());
	m_file.close();
	const int status = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	if (m_reply->error() != QNetworkReply::NoError || status >= 400) {
		fail(tr("Download of %1 failed: %2").arg(m_reply->url().toString(), m_reply->errorString()));
		return;
	}

	const QString archive = m_file.fileName();
	QString program;
	QStringList args;
	if (archive.endsWith(QStringLiteral(".zip"))) {
#if defined(Q_OS_MACOS)
		// keeps what an application bundle needs (links, permissions, signature)
		program = QStringLiteral("/usr/bin/ditto");
		args = { QStringLiteral("-x"), QStringLiteral("-k"), archive, m_dir };
#elif defined(Q_OS_WIN)
		// the tar.exe of Windows 10 and later reads zip archives
		program = QStandardPaths::findExecutable(QStringLiteral("tar"));
		args = { QStringLiteral("-xf"), archive, QStringLiteral("-C"), m_dir };
#else
		program = QStandardPaths::findExecutable(QStringLiteral("unzip"));
		args = { QStringLiteral("-q"), QStringLiteral("-o"), archive, QStringLiteral("-d"), m_dir };
#endif
	} else {
		program = QStandardPaths::findExecutable(QStringLiteral("tar"));
		args = { QStringLiteral("-xzf"), archive, QStringLiteral("-C"), m_dir };
	}
	if (program.isEmpty()) {
		fail(tr("No program to extract %1 was found (unzip or tar).").arg(QFileInfo(archive).fileName()));
		return;
	}
	m_process = new QProcess(this);
	connect(m_process.data(), &QProcess::finished, this, &LauncherDownload::extracted);
	connect(m_process.data(), &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
		if (error == QProcess::FailedToStart) {
			fail(tr("The archive cannot be extracted: %1").arg(m_process->errorString()));
		}
	});
	m_process->start(program, args);
}

void LauncherDownload::extracted(int exit_code, QProcess::ExitStatus status)
{
	m_process->deleteLater();
	if (m_cancelled) {
		fail(tr("Download cancelled."));
		return;
	}
	if (status != QProcess::NormalExit || exit_code != 0) {
		fail(tr("The archive cannot be extracted: %1").arg(QString::fromLocal8Bit(m_process->readAllStandardError())));
		return;
	}
	QFile::remove(m_file.fileName());
	const QString launcher = Launchers::findIn(m_platform, m_dir);
	if (launcher.isEmpty()) {
		fail(tr("The archive has no EasyRPG Player for %1.").arg(ExportPreset::name(m_platform)));
		return;
	}
	emit finished(launcher, QString());
}
