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

#include "editor_update.h"
#include "common/editor_settings.h"
#include "common/logging.h"
#include "controller/dialog_service.h"
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTemporaryDir>
#include <QVersionNumber>
#include <algorithm>

// the GitHub repository of the releases (CMake's EDITOR_UPDATE_REPOSITORY)
#ifndef EDITOR_UPDATE_REPOSITORY
#define EDITOR_UPDATE_REPOSITORY ""
#endif

namespace {

/** The first automatic check, after the start-up (ms) */
constexpr int first_check_delay = 4000;
/** Then one every hour */
constexpr int check_interval = 60 * 60 * 1000;

QByteArray userAgent()
{
	return QStringLiteral("EasyRPG-Editor/%1").arg(QCoreApplication::applicationVersion()).toUtf8();
}

/** A build in its build folder (CMake's), not a package: replaced by none */
[[maybe_unused]] bool inBuildFolder(const QString& dir)
{
	return QFileInfo::exists(dir + QStringLiteral("/CMakeCache.txt"));
}

/** The application bundle running (macOS), empty if none */
QString macBundle()
{
	// Contents/MacOS/<program>
	const QString bundle = QDir::cleanPath(QCoreApplication::applicationDirPath() + QStringLiteral("/../.."));
	return bundle.endsWith(QStringLiteral(".app")) ? bundle : QString();
}

/** The names of this computer's processor in the files of a release */
QStringList archNames()
{
	const QString arch = QSysInfo::currentCpuArchitecture();
	if (arch == QLatin1String("x86_64"))
		return {QStringLiteral("x86_64"), QStringLiteral("amd64"), QStringLiteral("x64")};
	if (arch == QLatin1String("arm64"))
		return {QStringLiteral("aarch64"), QStringLiteral("arm64")};
	return {arch};
}

/** The team of a signature (codesign -dv), empty if none */
QString teamOf(const QString& codesign_output)
{
	for (const QString& line : codesign_output.split(QLatin1Char('\n'))) {
		if (line.startsWith(QStringLiteral("TeamIdentifier="))) {
			const QString team = line.mid(15).trimmed();
			return team == QLatin1String("not set") ? QString() : team;
		}
	}
	return QString();
}

/*
 * The helpers that put the update in place once the Editor has quit:
 * $1 its process, $2 the update made ready, $3 what it replaces, $4 the
 * folder the update was made ready in (removed), $5 1 to start it again.
 * The old one is moved into $4 (the same volume: renamed), then removed.
 */
const char mac_helper[] = R"(n=0
while kill -0 "$1" 2>/dev/null && [ $n -lt 600 ]; do sleep 0.2; n=$((n+1)); done
old="$4/old.app"
rm -rf "$old"
if mv "$3" "$old"; then
  if mv "$2" "$3"; then rm -rf "$4"; else mv "$old" "$3"; fi
fi
if [ "$5" = 1 ]; then /usr/bin/open "$3"; fi
)";

const char appimage_helper[] = R"(n=0
while kill -0 "$1" 2>/dev/null && [ $n -lt 600 ]; do sleep 0.2; n=$((n+1)); done
if mv -f "$2" "$3"; then rm -rf "$4"; fi
if [ "$5" = 1 ]; then
  unset APPDIR APPIMAGE ARGV0 OWD
  exec "$3"
fi
)";

// PowerShell, its paths in the environment (EZ_UPDATE_...): no quoting
const char windows_helper[] = R"(
try { Wait-Process -Id ([int]$env:EZ_UPDATE_PID) -Timeout 120 -ErrorAction SilentlyContinue } catch {}
$target = $env:EZ_UPDATE_TARGET
$old = Join-Path $env:EZ_UPDATE_STAGING 'old'
$moved = $false
for ($i = 0; $i -lt 60 -and -not $moved; $i++) {
  try { Move-Item -LiteralPath $target -Destination $old -ErrorAction Stop; $moved = $true }
  catch { Start-Sleep -Milliseconds 500 }
}
if ($moved) {
  try {
    Move-Item -LiteralPath $env:EZ_UPDATE_STAGED -Destination $target -ErrorAction Stop
    Remove-Item -LiteralPath $env:EZ_UPDATE_STAGING -Recurse -Force -ErrorAction SilentlyContinue
  } catch { Move-Item -LiteralPath $old -Destination $target }
}
if ($env:EZ_UPDATE_RELAUNCH -eq '1') { Start-Process -FilePath (Join-Path $target $env:EZ_UPDATE_PROGRAM) }
)";

QString powershell()
{
	const QString found = QStandardPaths::findExecutable(QStringLiteral("powershell"));
	return found.isEmpty() ? QStringLiteral("powershell.exe") : found;
}

#if defined(Q_OS_WIN)
/** No console window for the programs run: CREATE_NO_WINDOW, which
 *  CREATE_NEW_CONSOLE (startDetached's) would cancel */
void noConsole(QProcess& process)
{
	process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) {
		args->flags &= ~0x00000010u;
		args->flags |= 0x08000000u;
	});
}
#else
void noConsole(QProcess&) {}
#endif

}

/*
 * EditorUpdates
 */

QString EditorUpdates::repository()
{
	return QStringLiteral(EDITOR_UPDATE_REPOSITORY).trimmed();
}

QUrl EditorUpdates::feedUrl()
{
	const QString feed = qEnvironmentVariable("EDITOR_UPDATE_FEED").trimmed();
	if (!feed.isEmpty())
		return QUrl::fromUserInput(feed, QDir::currentPath(), QUrl::AssumeLocalFile);
	const QString repo = repository();
	if (repo.isEmpty())
		return QUrl();
	return QUrl(QStringLiteral("https://api.github.com/repos/%1/releases/latest").arg(repo));
}

bool EditorUpdates::enabled()
{
	return feedUrl().isValid() && !feedUrl().isEmpty();
}

QString EditorUpdates::versionOf(const QString& tag)
{
	QString version = tag.trimmed();
	if (version.startsWith(QLatin1Char('v')) || version.startsWith(QLatin1Char('V')))
		version.remove(0, 1);
	return version;
}

int EditorUpdates::compareVersions(const QString& a, const QString& b)
{
	return QVersionNumber::compare(QVersionNumber::fromString(versionOf(a)).normalized(),
		QVersionNumber::fromString(versionOf(b)).normalized());
}

EditorUpdates::Method EditorUpdates::method()
{
#if defined(Q_OS_MACOS)
	const QString bundle = macBundle();
	if (!bundle.isEmpty() && !inBuildFolder(QFileInfo(bundle).absolutePath()))
		return Method::MacBundle;
#elif defined(Q_OS_WIN)
	// <folder>/bin/Editor.exe, as the zip has it
	const QDir dir(QCoreApplication::applicationDirPath());
	if (dir.dirName().compare(QStringLiteral("bin"), Qt::CaseInsensitive) == 0 && !inBuildFolder(dir.path())
			&& !inBuildFolder(QDir::cleanPath(dir.path() + QStringLiteral("/.."))))
		return Method::WindowsFolder;
#else
	// a Flatpak is updated by Flatpak
	if (qEnvironmentVariableIsEmpty("FLATPAK_ID")) {
		const QString appimage = qEnvironmentVariable("APPIMAGE");
		if (!appimage.isEmpty() && QFileInfo(appimage).isFile())
			return Method::AppImage;
	}
#endif
	return Method::Browser;
}

QString EditorUpdates::target()
{
	switch (method()) {
	case Method::MacBundle:
		return macBundle();
	case Method::WindowsFolder:
		return QDir::cleanPath(QCoreApplication::applicationDirPath() + QStringLiteral("/.."));
	case Method::AppImage:
		return QFileInfo(qEnvironmentVariable("APPIMAGE")).absoluteFilePath();
	case Method::Browser:
		break;
	}
	return QString();
}

int EditorUpdates::pickAsset(const QStringList& names, Method method)
{
	auto ends = [](const QString& name, const char* suffix) {
		return name.endsWith(QLatin1String(suffix), Qt::CaseInsensitive);
	};
	auto has = [](const QString& name, const char* part) {
		return name.contains(QLatin1String(part), Qt::CaseInsensitive);
	};
	int fallback = -1;
	for (int i = 0; i < names.size(); i++) {
		const QString& name = names[i];
		switch (method) {
		case Method::MacBundle:
			if (ends(name, ".dmg")) {
				if (has(name, "mac"))
					return i;
				if (fallback < 0)
					fallback = i;
			}
			break;
		case Method::WindowsFolder:
			if (ends(name, "-windows.zip"))
				return i;
			if (ends(name, ".zip") && has(name, "win") && fallback < 0)
				fallback = i;
			break;
		case Method::AppImage:
			if (ends(name, ".AppImage")) {
				for (const QString& arch : archNames()) {
					if (name.contains(arch, Qt::CaseInsensitive))
						return i;
				}
				// one of another processor is not this one's
				static const char* const any_arch[] = {"x86_64", "amd64", "x64", "aarch64", "arm64", "i386", "i686", "armhf"};
				bool other = false;
				for (const char* arch : any_arch)
					other = other || has(name, arch);
				if (!other && fallback < 0)
					fallback = i;
			}
			break;
		case Method::Browser:
			return -1;
		}
	}
	return fallback;
}

std::optional<EditorUpdates::Release> EditorUpdates::parseRelease(const QByteArray& json, Method method, QString* error)
{
	QJsonParseError parse_error;
	const QJsonDocument document = QJsonDocument::fromJson(json, &parse_error);
	QJsonObject release;
	if (document.isObject()) {
		release = document.object();
	} else if (document.isArray()) {
		// a list of releases, the newest first: the first published one
		for (const QJsonValue& value : document.array()) {
			const QJsonObject object = value.toObject();
			if (!object.value(QStringLiteral("draft")).toBool() && !object.value(QStringLiteral("prerelease")).toBool()) {
				release = object;
				break;
			}
		}
	}
	const QString tag = release.value(QStringLiteral("tag_name")).toString();
	if (tag.isEmpty() || QVersionNumber::fromString(versionOf(tag)).isNull()) {
		if (error) {
			// GitHub's own error ("Not Found", "API rate limit exceeded...")
			const QString message = release.value(QStringLiteral("message")).toString();
			*error = !message.isEmpty() ? message
				: parse_error.error != QJsonParseError::NoError ? parse_error.errorString()
				: QCoreApplication::translate("EditorUpdater", "No release was found.");
		}
		return std::nullopt;
	}
	Release found;
	found.version = versionOf(tag);
	found.page = QUrl(release.value(QStringLiteral("html_url")).toString());
	const QJsonArray assets = release.value(QStringLiteral("assets")).toArray();
	QStringList names;
	for (const QJsonValue& asset : assets)
		names << asset.toObject().value(QStringLiteral("name")).toString();
	const int index = pickAsset(names, method);
	if (index >= 0) {
		const QJsonObject asset = assets[index].toObject();
		// a name, not a path
		found.assetName = QFileInfo(names[index]).fileName();
		found.assetUrl = QUrl(asset.value(QStringLiteral("browser_download_url")).toString());
		found.assetSize = asset.value(QStringLiteral("size")).toInteger();
	}
	return found;
}

/*
 * EditorUpdater
 */

EditorUpdater& EditorUpdater::instance()
{
	// the application's child: gone with it, before the static objects
	static QPointer<EditorUpdater> updater;
	if (!updater)
		updater = new EditorUpdater(QCoreApplication::instance());
	return *updater;
}

EditorUpdater::EditorUpdater(QObject* parent) : QObject(parent)
{
	m_timer.setSingleShot(true);
	connect(&m_timer, &QTimer::timeout, this, [this] {
		check(false);
		m_timer.start(check_interval);
	});
	connect(qApp, &QCoreApplication::aboutToQuit, this, &EditorUpdater::installOnQuit);
}

EditorUpdater::~EditorUpdater()
{
	if (m_reply) {
		m_reply->disconnect(this);
		m_reply->abort();
	}
}

bool EditorUpdater::isEnabled() const
{
	return EditorUpdates::enabled();
}

QString EditorUpdater::currentVersion() const
{
	return QCoreApplication::applicationVersion();
}

QString EditorUpdater::latestVersion() const
{
	return m_release ? m_release->version : QString();
}

EditorUpdater::State EditorUpdater::state() const
{
	return m_state;
}

bool EditorUpdater::isUpdateAvailable() const
{
	return m_release.has_value();
}

bool EditorUpdater::isBusy() const
{
	return m_state == State::Downloading || m_state == State::Preparing;
}

bool EditorUpdater::isReady() const
{
	return m_state == State::Ready;
}

bool EditorUpdater::installsInPlace() const
{
	return EditorUpdates::method() != EditorUpdates::Method::Browser;
}

int EditorUpdater::progress() const
{
	return m_progress;
}

void EditorUpdater::setDialogService(DialogService* dialogs)
{
	m_dialogs = dialogs;
}

DialogService& EditorUpdater::dialogs() const
{
	return m_dialogs ? *m_dialogs : DialogService::instance();
}

void EditorUpdater::setState(State state)
{
	if (m_state == state)
		return;
	m_state = state;
	emit stateChanged();
}

void EditorUpdater::setProgress(int percent)
{
	if (m_progress == percent)
		return;
	m_progress = percent;
	emit progressChanged();
}

QString EditorUpdater::actionText() const
{
	switch (m_state) {
	case State::Idle:
		break;
	case State::Checking:
		// a check while an update is known keeps Install Update
		if (!m_release)
			return tr("Checking for Updates...");
		[[fallthrough]];
	case State::Available:
		return (installsInPlace() ? tr("Install Update (v%1)...") : tr("Download Update (v%1)...")).arg(m_release->version);
	case State::Downloading:
	case State::Preparing:
		return tr("Downloading Update...");
	case State::Ready:
		return tr("Restart to Update");
	}
	return QString();
}

bool EditorUpdater::actionEnabled() const
{
	return m_state == State::Idle || m_state == State::Available || m_state == State::Ready;
}

void EditorUpdater::startAutomaticChecks(bool automation)
{
	m_automatic = true;
	m_automation = automation;
	connect(&EditorSettings::instance(), &EditorSettings::changed, this, &EditorUpdater::scheduleChecks,
		Qt::UniqueConnection);
	scheduleChecks();
}

void EditorUpdater::scheduleChecks()
{
	// a script checks only a feed of its own, never GitHub
	const bool on = m_automatic && isEnabled() && EditorSettings::instance().checkForUpdates()
		&& (!m_automation || !qEnvironmentVariableIsEmpty("EDITOR_UPDATE_FEED"));
	if (!on) {
		if (m_timer.isActive())
			qCDebug(lcUpdate) << "automatic checks off";
		m_timer.stop();
	} else if (!m_timer.isActive()) {
		qCDebug(lcUpdate) << "automatic checks on, the first in" << first_check_delay << "ms";
		m_timer.start(first_check_delay);
	}
}

void EditorUpdater::activate()
{
	if (m_state == State::Ready)
		restart();
	else if (m_state == State::Available)
		install();
	else if (m_state == State::Idle)
		check(true);
}

void EditorUpdater::check(bool interactive)
{
	if (!isEnabled() || (m_state != State::Idle && m_state != State::Available))
		return;
	if (!m_network)
		m_network = new QNetworkAccessManager(this);
	setState(State::Checking);
	QNetworkRequest request(EditorUpdates::feedUrl());
	request.setHeader(QNetworkRequest::UserAgentHeader, userAgent());
	request.setRawHeader("Accept", "application/vnd.github+json");
	request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
	request.setTransferTimeout(30000);
	qCDebug(lcUpdate).noquote() << "checking" << request.url().toString() << (interactive ? "(asked)" : "(automatic)");
	QNetworkReply* reply = m_network->get(request);
	connect(reply, &QNetworkReply::finished, this, [this, reply, interactive] { checked(reply, interactive); });
}

void EditorUpdater::checked(QNetworkReply* reply, bool interactive)
{
	reply->deleteLater();
	const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	QString error;
	std::optional<EditorUpdates::Release> release;
	if (reply->error() != QNetworkReply::NoError && status == 0) {
		error = reply->errorString();
	} else {
		release = EditorUpdates::parseRelease(reply->readAll(), EditorUpdates::method(), &error);
		if (release && status >= 400) {
			release.reset();
			error = reply->errorString();
		}
	}
	const QString title = tr("Check for Updates");
	if (!release) {
		qCInfo(lcUpdate).noquote() << "cannot check for updates:" << error;
		setState(m_release ? State::Available : State::Idle);
		if (interactive) {
			QTimer::singleShot(0, this, [this, title, error] {
				dialogs().warning(title, tr("Cannot check for updates: %1").arg(error));
			});
		}
		return;
	}
	const bool newer = EditorUpdates::compareVersions(release->version, currentVersion()) > 0;
	qCInfo(lcUpdate).noquote() << "latest release" << release->version << "(this one" << currentVersion() + ")"
		<< (newer ? "newer, package: " + (release->assetName.isEmpty() ? QStringLiteral("none") : release->assetName) : "");
	if (newer)
		m_release = release;
	else
		m_release.reset();
	setState(newer ? State::Available : State::Idle);
	if (!interactive)
		return;
	QTimer::singleShot(0, this, [this, title, newer] {
		if (!newer) {
			dialogs().information(title, tr("EasyRPG Editor is up to date (version %1).").arg(currentVersion()));
			return;
		}
		if (!m_release)
			return;
		const QString text = tr("Version %1 is available (you have %2).").arg(m_release->version, currentVersion());
		if (dialogs().confirm(title, text, installsInPlace() ? tr("Install") : tr("Download"), tr("Later")))
			install();
	});
}

void EditorUpdater::install()
{
	if (m_state == State::Ready) {
		restart();
		return;
	}
	if (m_state != State::Available || !m_release) {
		if (m_state == State::Idle)
			check(true);
		return;
	}
	const EditorUpdates::Method method = EditorUpdates::method();
	if (method == EditorUpdates::Method::Browser || m_release->assetUrl.isEmpty()) {
		if (method != EditorUpdates::Method::Browser)
			qCInfo(lcUpdate) << "the release has no package for this system: its page opens";
		openPage();
		return;
	}

	// made ready next to what it replaces (the same volume: renamed in place)
	const QFileInfo target(EditorUpdates::target());
	m_target = target.absoluteFilePath();
	m_staging = target.absolutePath() + QStringLiteral("/.") + target.fileName() + QStringLiteral(".update");
	m_staged.clear();
	m_manual = false;
	QDir(m_staging).removeRecursively();
	bool writable = QFileInfo(target.absolutePath()).isWritable() && QDir().mkpath(m_staging);
	if (writable) {
		// the permissions say little on Windows: a file written tells
		QFile probe(m_staging + QStringLiteral("/probe"));
		writable = probe.open(QIODevice::WriteOnly);
		probe.close();
		probe.remove();
	}
	// macOS runs a downloaded application from a copy elsewhere until it is moved
	if (m_target.contains(QStringLiteral("/AppTranslocation/")))
		writable = false;
	if (method == EditorUpdates::Method::MacBundle && !QFileInfo(m_target).isWritable())
		writable = false;
	if (!writable) {
		QDir(m_staging).removeRecursively();
		m_staging.clear();
		qCInfo(lcUpdate).noquote() << "cannot replace" << m_target;
		if (method != EditorUpdates::Method::MacBundle) {
			dialogs().information(tr("Update"), tr("EasyRPG Editor cannot replace itself in %1: "
				"the page of version %2 opens, to download it.")
				.arg(QDir::toNativeSeparators(target.absolutePath()), m_release->version));
			openPage();
			return;
		}
		// the disk image, opened for the user to copy it
		m_manual = true;
		QString folder = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
		if (folder.isEmpty() || !QDir().mkpath(folder))
			folder = QDir::tempPath();
		startDownload(folder + QLatin1Char('/') + m_release->assetName);
		return;
	}
	startDownload(m_staging + QLatin1Char('/') + m_release->assetName);
}

void EditorUpdater::startDownload(const QString& file)
{
	QFile::remove(file);
	m_file.setFileName(file);
	if (!m_file.open(QIODevice::WriteOnly)) {
		fail(tr("Cannot write %1.").arg(QDir::toNativeSeparators(file)));
		return;
	}
	if (!m_network)
		m_network = new QNetworkAccessManager(this);
	QNetworkRequest request(m_release->assetUrl);
	request.setHeader(QNetworkRequest::UserAgentHeader, userAgent());
	request.setRawHeader("Accept", "application/octet-stream");
	qCInfo(lcUpdate).noquote() << "downloading" << request.url().toString() << "to" << file;
	setProgress(-1);
	setState(State::Downloading);
	m_reply = m_network->get(request);
	connect(m_reply.data(), &QNetworkReply::readyRead, this, [this] {
		if (m_reply && m_reply->isOpen())
			m_file.write(m_reply->readAll());
	});
	connect(m_reply.data(), &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
		if (total <= 0 && m_release)
			total = m_release->assetSize;
		setProgress(total > 0 ? static_cast<int>(std::clamp<qint64>(received * 100 / total, 0, 100)) : -1);
	});
	connect(m_reply.data(), &QNetworkReply::finished, this, &EditorUpdater::downloaded);
}

void EditorUpdater::downloaded()
{
	QNetworkReply* reply = m_reply;
	m_reply = nullptr;
	reply->deleteLater();
	m_file.write(reply->readAll());
	m_file.close();
	const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	if (reply->error() != QNetworkReply::NoError || status >= 400) {
		fail(tr("Download of %1 failed: %2").arg(m_release->assetName, reply->errorString()));
		return;
	}
	setProgress(100);
	setState(State::Preparing);
	prepare();
}

void EditorUpdater::prepare()
{
	switch (EditorUpdates::method()) {
	case EditorUpdates::Method::MacBundle:
		prepareMac();
		break;
	case EditorUpdates::Method::WindowsFolder:
		prepareWindows();
		break;
	case EditorUpdates::Method::AppImage:
		prepareAppImage();
		break;
	case EditorUpdates::Method::Browser:
		openPage();
		break;
	}
}

void EditorUpdater::prepareMac()
{
	const QString image = m_file.fileName();
	if (m_manual) {
		// copied by the user from the disk image, in the Finder
		m_manual = false;
		setProgress(-1);
		setState(State::Available);
		qCInfo(lcUpdate).noquote() << "opening" << image;
		QDesktopServices::openUrl(QUrl::fromLocalFile(image));
		QTimer::singleShot(0, this, [this, image] {
			dialogs().information(tr("Update"), tr("EasyRPG Editor cannot replace itself in %1. "
				"The disk image of version %2 was downloaded (%3) and opened: "
				"quit the Editor and drag EasyRPG Editor from it to that folder to replace this one.")
				.arg(QFileInfo(m_target).absolutePath(), m_release ? m_release->version : QString(), image));
		});
		return;
	}
	m_mount = std::make_unique<QTemporaryDir>(QDir::tempPath() + QStringLiteral("/easyrpg-editor-update-XXXXXX"));
	if (!m_mount->isValid()) {
		m_mount.reset();
		fail(tr("Cannot write %1.").arg(QDir::tempPath()));
		return;
	}
	const QString mount = m_mount->path();
	run(QStringLiteral("/usr/bin/hdiutil"), {QStringLiteral("attach"), QStringLiteral("-nobrowse"), QStringLiteral("-readonly"),
			QStringLiteral("-noautoopen"), QStringLiteral("-mountpoint"), mount, image},
		[this, mount](int code, const QString& output) {
		if (code != 0) {
			m_mount.reset();
			fail(tr("The disk image cannot be opened: %1").arg(output.trimmed()));
			return;
		}
		// the application of the disk image, of the same name first
		const QString name = QFileInfo(m_target).fileName();
		const QFileInfoList apps = QDir(mount).entryInfoList({QStringLiteral("*.app")}, QDir::Dirs | QDir::NoSymLinks);
		QString app;
		for (const QFileInfo& found : apps) {
			if (app.isEmpty() || found.fileName() == name)
				app = found.absoluteFilePath();
		}
		if (app.isEmpty()) {
			detachImage([this] { fail(tr("The disk image has no application.")); });
			return;
		}
		m_staged = m_staging + QLatin1Char('/') + name;
		// ditto keeps what a bundle needs: links, permissions, signature
		run(QStringLiteral("/usr/bin/ditto"), {app, m_staged}, [this](int code, const QString& output) {
			detachImage([this, code, output] {
				if (code != 0)
					fail(tr("The application cannot be copied: %1").arg(output.trimmed()));
				else
					verifyMac();
			});
		});
	});
}

void EditorUpdater::detachImage(std::function<void()> then)
{
	if (!m_mount) {
		then();
		return;
	}
	const QString mount = m_mount->path();
	run(QStringLiteral("/usr/bin/hdiutil"), {QStringLiteral("detach"), mount}, [this, mount, then](int code, const QString&) {
		if (code == 0) {
			m_mount.reset();
			then();
			return;
		}
		run(QStringLiteral("/usr/bin/hdiutil"), {QStringLiteral("detach"), QStringLiteral("-force"), mount},
			[this, then](int code, const QString& output) {
			if (code != 0) {
				// still mounted: its files are not removed with the folder
				qCWarning(lcUpdate).noquote() << "cannot detach the disk image:" << output.trimmed();
				m_mount->setAutoRemove(false);
			}
			m_mount.reset();
			then();
		});
	});
}

void EditorUpdater::verifyMac()
{
	const QString codesign = QStringLiteral("/usr/bin/codesign");
	run(codesign, {QStringLiteral("--verify"), QStringLiteral("--deep"), QStringLiteral("--strict"), m_staged},
		[this, codesign](int code, const QString& output) {
		if (code != 0) {
			fail(tr("The update is not signed correctly: %1").arg(output.trimmed()));
			return;
		}
		// signed by a team: the update must be signed by the same one
		run(codesign, {QStringLiteral("-dv"), QStringLiteral("--verbose=2"), m_target}, [this, codesign](int, const QString& running) {
			const QString team = teamOf(running);
			if (team.isEmpty()) {
				QFile::remove(m_file.fileName());
				readyToRestart();
				return;
			}
			run(codesign, {QStringLiteral("-dv"), QStringLiteral("--verbose=2"), m_staged}, [this, team](int, const QString& staged) {
				if (teamOf(staged) != team) {
					fail(tr("The update is signed by another developer."));
					return;
				}
				QFile::remove(m_file.fileName());
				readyToRestart();
			});
		});
	});
}

void EditorUpdater::prepareWindows()
{
	const QString extract = m_staging + QStringLiteral("/extract");
	QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
	env.insert(QStringLiteral("EZ_UPDATE_ZIP"), QDir::toNativeSeparators(m_file.fileName()));
	env.insert(QStringLiteral("EZ_UPDATE_DIR"), QDir::toNativeSeparators(extract));
	run(powershell(), {QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"), QStringLiteral("-ExecutionPolicy"),
			QStringLiteral("Bypass"), QStringLiteral("-Command"),
			QStringLiteral("$ErrorActionPreference = 'Stop'; "
				"Expand-Archive -LiteralPath $env:EZ_UPDATE_ZIP -DestinationPath $env:EZ_UPDATE_DIR -Force")},
		[this, extract](int code, const QString& output) {
		if (code != 0) {
			fail(tr("The archive cannot be extracted: %1").arg(output.trimmed()));
			return;
		}
		QFile::remove(m_file.fileName());
		// the folder holding the program as this one is (bin/Editor.exe):
		// the zip's top folder, or the zip itself
		const QString program = QDir(m_target).relativeFilePath(QCoreApplication::applicationFilePath());
		QStringList candidates = {extract};
		for (const QFileInfo& dir : QDir(extract).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot))
			candidates << dir.absoluteFilePath();
		for (const QString& candidate : std::as_const(candidates)) {
			if (QFileInfo(candidate + QLatin1Char('/') + program).isFile()) {
				m_staged = candidate;
				readyToRestart();
				return;
			}
		}
		fail(tr("The archive has no EasyRPG Editor."));
	}, &env);
}

void EditorUpdater::prepareAppImage()
{
	m_staged = m_file.fileName();
	QFile file(m_staged);
	if (!file.setPermissions(file.permissions() | QFileDevice::ExeOwner | QFileDevice::ExeUser
			| QFileDevice::ExeGroup | QFileDevice::ExeOther | QFileDevice::ReadOwner | QFileDevice::ReadUser)) {
		fail(tr("Cannot write %1.").arg(m_staged));
		return;
	}
	readyToRestart();
}

void EditorUpdater::readyToRestart()
{
	qCInfo(lcUpdate).noquote() << "update ready:" << m_staged;
	setProgress(-1);
	setState(State::Ready);
	QTimer::singleShot(0, this, [this] {
		if (m_state != State::Ready)
			return;
		const QString text = tr("Version %1 is ready. Restart now to finish the update?")
			.arg(m_release ? m_release->version : QString());
		if (dialogs().confirm(tr("Update"), text, tr("Restart Now"), tr("Later")))
			restart();
	});
}

void EditorUpdater::restart()
{
	if (m_state == State::Ready)
		emit restartRequested();
}

void EditorUpdater::setRelaunchOnQuit(bool relaunch)
{
	m_relaunch = relaunch;
}

void EditorUpdater::fail(const QString& error)
{
	qCWarning(lcUpdate).noquote() << "the update cannot be installed:" << error;
	cleanUp();
	setProgress(-1);
	setState(m_release ? State::Available : State::Idle);
	QTimer::singleShot(0, this, [this, error] {
		dialogs().warning(tr("Update"), tr("The update cannot be installed: %1").arg(error));
	});
}

void EditorUpdater::cleanUp()
{
	m_file.close();
	if (m_manual)
		QFile::remove(m_file.fileName());
	m_manual = false;
	// only the folder made for the update (".<name>.update")
	const QString name = QFileInfo(m_staging).fileName();
	if (!m_staging.isEmpty() && name.startsWith(QLatin1Char('.')) && name.endsWith(QStringLiteral(".update")))
		QDir(m_staging).removeRecursively();
	m_staging.clear();
	m_staged.clear();
}

void EditorUpdater::openPage()
{
	if (!m_release)
		return;
	const QUrl page = m_release->page.isValid() && !m_release->page.isEmpty() ? m_release->page
		: QUrl(QStringLiteral("https://github.com/%1/releases/latest").arg(EditorUpdates::repository()));
	qCInfo(lcUpdate).noquote() << "opening" << page.toString();
	QDesktopServices::openUrl(page);
}

void EditorUpdater::installOnQuit()
{
	if (isBusy()) {
		// quit before it was ready: nothing left behind
		if (m_reply) {
			m_reply->disconnect(this);
			m_reply->abort();
		}
		for (QProcess* process : findChildren<QProcess*>()) {
			process->disconnect(this);
			process->kill();
			process->waitForFinished(2000);
		}
		if (m_mount) {
			QProcess::execute(QStringLiteral("/usr/bin/hdiutil"), {QStringLiteral("detach"), QStringLiteral("-force"), m_mount->path()});
			m_mount.reset();
		}
		cleanUp();
		qCInfo(lcUpdate) << "the update was not ready: not installed";
		return;
	}
	if (m_state != State::Ready || m_staged.isEmpty())
		return;
	const QString pid = QString::number(QCoreApplication::applicationPid());
	const QString relaunch = m_relaunch ? QStringLiteral("1") : QStringLiteral("0");
	QProcess helper;
	// not in what is replaced
	helper.setWorkingDirectory(QFileInfo(m_target).absolutePath());
	switch (EditorUpdates::method()) {
	case EditorUpdates::Method::MacBundle:
	case EditorUpdates::Method::AppImage: {
		const bool mac = EditorUpdates::method() == EditorUpdates::Method::MacBundle;
		helper.setProgram(QStringLiteral("/bin/sh"));
		helper.setArguments({QStringLiteral("-c"), QString::fromLatin1(mac ? mac_helper : appimage_helper),
			QStringLiteral("easyrpg-editor-update"), pid, m_staged, m_target, m_staging, relaunch});
		break;
	}
	case EditorUpdates::Method::WindowsFolder: {
		QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
		env.insert(QStringLiteral("EZ_UPDATE_PID"), pid);
		env.insert(QStringLiteral("EZ_UPDATE_STAGED"), QDir::toNativeSeparators(m_staged));
		env.insert(QStringLiteral("EZ_UPDATE_TARGET"), QDir::toNativeSeparators(m_target));
		env.insert(QStringLiteral("EZ_UPDATE_STAGING"), QDir::toNativeSeparators(m_staging));
		env.insert(QStringLiteral("EZ_UPDATE_PROGRAM"),
			QDir::toNativeSeparators(QDir(m_target).relativeFilePath(QCoreApplication::applicationFilePath())));
		env.insert(QStringLiteral("EZ_UPDATE_RELAUNCH"), relaunch);
		helper.setProcessEnvironment(env);
		// -EncodedCommand: the script as UTF-16LE in base64, nothing to quote
		const QString script = QString::fromLatin1(windows_helper);
		const QByteArray encoded = QByteArray(reinterpret_cast<const char*>(script.utf16()), script.size() * 2).toBase64();
		helper.setProgram(powershell());
		helper.setArguments({QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"), QStringLiteral("-ExecutionPolicy"),
			QStringLiteral("Bypass"), QStringLiteral("-WindowStyle"), QStringLiteral("Hidden"),
			QStringLiteral("-EncodedCommand"), QString::fromLatin1(encoded)});
		noConsole(helper);
		break;
	}
	case EditorUpdates::Method::Browser:
		return;
	}
	qint64 helper_pid = 0;
	const bool started = helper.startDetached(&helper_pid);
	qCInfo(lcUpdate).noquote() << (started ? "installing the update once the Editor has quit" : "cannot start the installer:")
		<< (started ? QStringLiteral("(helper %1, %2)").arg(helper_pid).arg(m_relaunch ? "restarts it" : "no restart") : helper.errorString())
		<< m_staged << "->" << m_target;
}

void EditorUpdater::run(const QString& program, const QStringList& args, std::function<void(int, const QString&)> done,
	const QProcessEnvironment* env)
{
	auto* process = new QProcess(this);
	process->setProcessChannelMode(QProcess::MergedChannels);
	if (env)
		process->setProcessEnvironment(*env);
	noConsole(*process);
	connect(process, &QProcess::finished, this, [process, done](int code, QProcess::ExitStatus status) {
		process->deleteLater();
		done(status == QProcess::NormalExit ? code : -1, QString::fromLocal8Bit(process->readAll()));
	});
	connect(process, &QProcess::errorOccurred, this, [process, done](QProcess::ProcessError error) {
		if (error == QProcess::FailedToStart) {
			process->deleteLater();
			done(-1, process->errorString());
		}
	});
	qCDebug(lcUpdate).noquote() << "running" << program << args.join(QLatin1Char(' '));
	process->start(program, args);
}
