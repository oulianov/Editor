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

#include "export_controller.h"
#include "launcher_download.h"
#include "controller/dialog_service.h"
#include "model/project.h"
#include <QCoreApplication>
#include <QDesktopServices>
#include <QFileInfo>
#include <QUrl>
#include <QtConcurrentRun>
#include <algorithm>

namespace {
	/** The window's title, for its questions */
	QString windowTitle() {
		return QCoreApplication::translate("ExportDialog", "Export Game");
	}
}

ExportPlatformModel::ExportPlatformModel(ExportController* controller) :
	QAbstractListModel(controller), m_controller(controller)
{
}

int ExportPlatformModel::rowCount(const QModelIndex& parent) const
{
	return parent.isValid() ? 0 : static_cast<int>(m_controller->settings().presets.size());
}

QVariant ExportPlatformModel::data(const QModelIndex& index, int role) const
{
	if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
		return QVariant();
	}
	const ExportPreset& preset = m_controller->settings().presets[index.row()];
	const ExportPlatform platform = preset.platform;
	switch (role) {
	case Qt::DisplayRole:
	case NameRole:
		return ExportPreset::name(platform);
	case Qt::ToolTipRole:
	case LauncherToolTipRole:
		return m_controller->launcherToolTip(platform);
	case Qt::CheckStateRole:
		return preset.enabled ? Qt::Checked : Qt::Unchecked;
	case PlatformRole:
		return static_cast<int>(platform);
	case IdRole:
		return ExportPreset::id(platform);
	case EnabledRole:
		return preset.enabled;
	case PathRole:
		return preset.path;
	case LauncherRole:
		return preset.launcher;
	case LauncherReadyRole:
		return m_controller->launcherReady(platform);
	case LauncherTextRole:
		return m_controller->launcherText(platform);
	case LauncherChoicesRole:
		return m_controller->launcherChoices(platform);
	default:
		return QVariant();
	}
}

bool ExportPlatformModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
	if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
		return false;
	}
	const ExportPlatform platform = m_controller->settings().presets[index.row()].platform;
	switch (role) {
	case Qt::CheckStateRole:
		m_controller->setPlatformEnabled(platform, value.toInt() == Qt::Checked);
		return true;
	case EnabledRole:
		m_controller->setPlatformEnabled(platform, value.toBool());
		return true;
	case Qt::EditRole:
	case PathRole:
		m_controller->setPlatformPath(platform, value.toString());
		return true;
	case LauncherRole:
		m_controller->setLauncher(platform, value.toString());
		return true;
	default:
		return false;
	}
}

Qt::ItemFlags ExportPlatformModel::flags(const QModelIndex& index) const
{
	if (!index.isValid()) {
		return Qt::NoItemFlags;
	}
	return QAbstractListModel::flags(index) | Qt::ItemIsUserCheckable | Qt::ItemIsEditable;
}

QHash<int, QByteArray> ExportPlatformModel::roleNames() const
{
	QHash<int, QByteArray> names = QAbstractListModel::roleNames();
	names.insert(PlatformRole, "platform");
	names.insert(IdRole, "id");
	names.insert(NameRole, "name");
	names.insert(EnabledRole, "enabled");
	names.insert(PathRole, "path");
	names.insert(LauncherRole, "launcher");
	names.insert(LauncherReadyRole, "launcherReady");
	names.insert(LauncherTextRole, "launcherText");
	names.insert(LauncherToolTipRole, "launcherToolTip");
	names.insert(LauncherChoicesRole, "launcherChoices");
	return names;
}

bool ExportPlatformModel::download(int row)
{
	const auto& presets = m_controller->settings().presets;
	return row >= 0 && row < presets.size() && m_controller->download(presets[row].platform);
}

QString ExportPlatformModel::locate(int row, const QString& path)
{
	const auto& presets = m_controller->settings().presets;
	return row >= 0 && row < presets.size() ? m_controller->locateLauncher(presets[row].platform, path) : QString();
}

void ExportPlatformModel::reset()
{
	beginResetModel();
	endResetModel();
}

void ExportPlatformModel::platformChanged(ExportPlatform platform)
{
	const auto& presets = m_controller->settings().presets;
	for (int row = 0; row < presets.size(); ++row) {
		if (presets[row].platform == platform) {
			emit dataChanged(index(row), index(row));
		}
	}
}

ExportController::ExportController(QObject* parent) :
	QObject(parent)
{
	m_platforms = new ExportPlatformModel(this);
	connect(&m_watcher, &QFutureWatcher<bool>::finished, this, &ExportController::exportFinished);
	connect(this, &ExportController::platformChanged, m_platforms, &ExportPlatformModel::platformChanged);
}

ExportController::~ExportController()
{
	// the export works on this controller's exporter: it stops first
	if (m_exporter) {
		m_exporter->cancel();
		m_watcher.waitForFinished();
	}
}

void ExportController::setSource(const ExportSource& source, const QString& game_title)
{
	m_source = source;
	m_hasRuntime = !FileFinder::Find(m_source.dir, QStringLiteral("RPG_RT.exe")).isEmpty();
	m_settings = ExportSettings::load(m_source.dir, game_title);
	pickLaunchers();
	m_dirty = false;
	m_platforms->reset();
	emit sourceChanged();
	emit settingsChanged();
	emit dirtyChanged();
}

void ExportController::setProject(const Project& project, const QString& rtp)
{
	ExportSource source;
	source.dir = project.projectDir();
	source.type = project.projectType();
	source.encoding = project.encoding();
	source.rtp = rtp;
	setSource(source, project.gameTitle());
}

const ExportSource& ExportController::source() const
{
	return m_source;
}

QString ExportController::sourceFolder() const
{
	return m_source.dir.absolutePath();
}

QString ExportController::playerVersion() const
{
	return Launchers::version();
}

const ExportSettings& ExportController::settings() const
{
	return m_settings;
}

void ExportController::markDirty()
{
	const bool was = m_dirty;
	m_dirty = true;
	emit settingsChanged();
	if (!was) {
		emit dirtyChanged();
	}
}

QString ExportController::title() const
{
	return m_settings.title;
}

void ExportController::setTitle(const QString& title)
{
	if (title != m_settings.title) {
		m_settings.title = title;
		markDirty();
	}
}

QString ExportController::version() const
{
	return m_settings.version;
}

void ExportController::setVersion(const QString& version)
{
	if (version != m_settings.version) {
		m_settings.version = version;
		markDirty();
	}
}

QString ExportController::icon() const
{
	return m_settings.icon;
}

void ExportController::setIcon(const QString& icon)
{
	if (icon != m_settings.icon) {
		m_settings.icon = icon;
		markDirty();
	}
}

QString ExportController::iconPath() const
{
	return m_settings.icon.isEmpty() ? QString() : m_source.dir.absoluteFilePath(m_settings.icon);
}

bool ExportController::includeRtp() const
{
	return m_settings.include_rtp;
}

void ExportController::setIncludeRtp(bool on)
{
	if (on != m_settings.include_rtp) {
		m_settings.include_rtp = on;
		markDirty();
	}
}

bool ExportController::cleanDatabase() const
{
	return m_settings.clean_database;
}

void ExportController::setCleanDatabase(bool on)
{
	if (on != m_settings.clean_database) {
		m_settings.clean_database = on;
		markDirty();
	}
}

bool ExportController::protect() const
{
	return m_settings.protect;
}

void ExportController::setProtect(bool on)
{
	if (on != m_settings.protect) {
		m_settings.protect = on;
		markDirty();
	}
}

bool ExportController::canProtect() const
{
	return m_source.type == FileFinder::ProjectType::Legacy;
}

bool ExportController::zip() const
{
	return std::any_of(m_settings.presets.cbegin(), m_settings.presets.cend(),
		[](const ExportPreset& preset) { return preset.zip; });
}

void ExportController::setZip(bool on)
{
	for (ExportPreset& preset : m_settings.presets) {
		preset.zip = on;
	}
	markDirty();
}

bool ExportController::customizeApp() const
{
	const ExportPreset* mac = findPreset(ExportPlatform::MacOS);
	return mac && mac->customize;
}

void ExportController::setCustomizeApp(bool on)
{
	if (on != customizeApp()) {
		preset(ExportPlatform::MacOS).customize = on;
		markDirty();
	}
}

bool ExportController::hasRuntime() const
{
	return m_hasRuntime;
}

bool ExportController::isDirty() const
{
	return m_dirty;
}

bool ExportController::save()
{
	const bool saved = m_settings.save(m_source.dir);
	if (!saved) {
		appendLog(QCoreApplication::translate("ExportDialog", "The export settings cannot be saved in %1.")
			.arg(QDir::toNativeSeparators(m_source.dir.absolutePath())), GameExporter::Warning);
	}
	if (m_dirty) {
		m_dirty = false;
		emit dirtyChanged();
	}
	return saved;
}

ExportPreset& ExportController::preset(ExportPlatform platform)
{
	return m_settings.preset(platform);
}

const ExportPreset* ExportController::findPreset(ExportPlatform platform) const
{
	for (const ExportPreset& preset : m_settings.presets) {
		if (preset.platform == platform) {
			return &preset;
		}
	}
	return nullptr;
}

ExportPlatformModel* ExportController::platforms() const
{
	return m_platforms;
}

bool ExportController::platformEnabled(ExportPlatform platform) const
{
	const ExportPreset* found = findPreset(platform);
	return found && found->enabled;
}

void ExportController::setPlatformEnabled(ExportPlatform platform, bool on)
{
	if (on != platformEnabled(platform)) {
		preset(platform).enabled = on;
		markDirty();
		emit platformChanged(platform);
	}
}

QString ExportController::platformPath(ExportPlatform platform) const
{
	const ExportPreset* found = findPreset(platform);
	return found ? found->path : QString();
}

void ExportController::setPlatformPath(ExportPlatform platform, const QString& path)
{
	if (path != platformPath(platform)) {
		preset(platform).path = path;
		markDirty();
		emit platformChanged(platform);
	}
}

QString ExportController::launcher(ExportPlatform platform) const
{
	const ExportPreset* found = findPreset(platform);
	return found ? found->launcher : QString();
}

void ExportController::setLauncher(ExportPlatform platform, const QString& launcher)
{
	if (launcher != this->launcher(platform)) {
		preset(platform).launcher = launcher;
		markDirty();
		emit platformChanged(platform);
	}
}

void ExportController::pickLaunchers()
{
	for (ExportPreset& preset : m_settings.presets) {
		if (preset.launcher.isEmpty()) {
			const QStringList found = Launchers::find(preset.platform);
			if (!found.isEmpty()) {
				preset.launcher = found.first();
				emit platformChanged(preset.platform);
			}
		}
	}
}

bool ExportController::launcherReady(ExportPlatform platform) const
{
	const QString path = launcher(platform);
	return path == QLatin1String(ExportPreset::kGameRuntime) || GameExporter::isLauncher(platform, path);
}

QString ExportController::describeLauncher(const QString& path) const
{
	if (path == QLatin1String(ExportPreset::kGameRuntime)) {
		return QCoreApplication::translate("ExportDialog", "RPG_RT.exe of the game");
	}
	for (ExportPlatform platform : { ExportPlatform::Windows, ExportPlatform::MacOS, ExportPlatform::Linux, ExportPlatform::Web }) {
		const QString downloads = Launchers::folder(platform);
		if (path.startsWith(downloads)) {
			return QCoreApplication::translate("ExportDialog", "EasyRPG Player %1")
				.arg(QDir(downloads).relativeFilePath(path).section(QLatin1Char('/'), 0, 0));
		}
	}
	return QFileInfo(path).fileName();
}

QString ExportController::launcherText(ExportPlatform platform) const
{
	// the Player used, or what is missing
	return launcherReady(platform) ? describeLauncher(launcher(platform))
		: QCoreApplication::translate("ExportDialog", "Player missing");
}

QString ExportController::launcherToolTip(ExportPlatform platform) const
{
	if (!launcherReady(platform)) {
		return QCoreApplication::translate("ExportDialog", "Download the EasyRPG Player %1 for %2, or locate it")
			.arg(Launchers::version(), ExportPreset::name(platform));
	}
	const QString path = launcher(platform);
	return QDir::toNativeSeparators(path == QLatin1String(ExportPreset::kGameRuntime) ? m_source.dir.filePath(path) : path);
}

QStringList ExportController::launcherChoices(ExportPlatform platform) const
{
	QStringList choices = Launchers::find(platform);
	if (platform == ExportPlatform::Windows && m_hasRuntime) {
		choices << QString::fromLatin1(ExportPreset::kGameRuntime);
	}
	const QString current = launcher(platform);
	if (launcherReady(platform) && !choices.contains(current)) {
		choices << current;
	}
	return choices;
}

QString ExportController::locateLauncher(ExportPlatform platform, const QString& located)
{
	if (located.isEmpty()) {
		return QString();
	}
	QString path = located;
	if (!GameExporter::isLauncher(platform, path)) {
		// a folder with the Player in it (an extracted archive)
		const QString inside = QFileInfo(path).isDir() ? Launchers::findIn(platform, path) : QString();
		if (inside.isEmpty()) {
			return QCoreApplication::translate("ExportDialog", "%1 is not an EasyRPG Player for %2.")
				.arg(QDir::toNativeSeparators(path), ExportPreset::name(platform));
		}
		path = inside;
	}
	setLauncher(platform, path);
	return QString();
}

bool ExportController::isRunning() const
{
	return !m_exporter.isNull();
}

bool ExportController::isDownloading() const
{
	return !m_download.isNull();
}

bool ExportController::isBusy() const
{
	return isRunning() || isDownloading();
}

bool ExportController::canDownload() const
{
	return !isBusy();
}

int ExportController::progress() const
{
	return m_progress;
}

int ExportController::progressMaximum() const
{
	return m_progressMaximum;
}

void ExportController::setProgress(int value, int maximum)
{
	if (value != m_progress || maximum != m_progressMaximum) {
		m_progress = value;
		m_progressMaximum = maximum;
		emit progressChanged();
	}
}

QStringList ExportController::log() const
{
	return m_log;
}

QString ExportController::logLine(const QString& text, int level)
{
	switch (level) {
	case GameExporter::Added:
		return QStringLiteral("+ ") + text;
	case GameExporter::Warning:
		return QCoreApplication::translate("ExportDialog", "Warning: %1").arg(text);
	case GameExporter::Error:
		return QCoreApplication::translate("ExportDialog", "Error: %1").arg(text);
	default:
		return text;
	}
}

void ExportController::appendLog(const QString& text, int level)
{
	const QString line = logLine(text, level);
	m_log << line;
	emit lineAdded(line, level);
	emit logChanged();
}

QStringList ExportController::folders() const
{
	return m_folders;
}

QString ExportController::openFolderPath() const
{
	if (m_folders.isEmpty()) {
		return QString();
	}
	// several platforms: the folder that holds them
	return m_folders.size() == 1 ? m_folders.first() : QFileInfo(m_folders.first()).absolutePath();
}

bool ExportController::openFolder() const
{
	const QString folder = openFolderPath();
	return !folder.isEmpty() && QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
}

bool ExportController::start()
{
	if (isBusy()) {
		return false;
	}
	DialogService& dialogs = DialogService::instance();
	QStringList replaced;
	int enabled = 0;
	for (const ExportPreset& preset : m_settings.presets) {
		if (!preset.enabled) {
			continue;
		}
		++enabled;
		const QString error = GameExporter::checkFolder(m_source.dir, preset.path);
		if (!error.isEmpty()) {
			dialogs.warning(windowTitle(), ExportPreset::name(preset.platform) + QStringLiteral(": ") + error);
			return false;
		}
		const QDir dir(m_source.dir.absoluteFilePath(preset.path));
		if (dir.exists() && !dir.isEmpty()) {
			replaced << QDir::toNativeSeparators(dir.absolutePath());
		}
	}
	if (enabled == 0) {
		dialogs.information(windowTitle(), QCoreApplication::translate("ExportDialog", "Check the platforms to export."));
		return false;
	}
	if (!replaced.isEmpty() && dialogs.question(windowTitle(),
			QCoreApplication::translate("ExportDialog", "These folders are not empty. Replace everything in them?\n\n%1")
				.arg(replaced.join(QLatin1Char('\n')))) != DialogService::Answer::Yes) {
		return false;
	}
	save();
	m_log.clear();
	emit logCleared();
	emit logChanged();
	m_folders.clear();
	emit foldersChanged();

	m_exporter = new GameExporter(m_source, m_settings, this);
	// emitted in the worker thread: queued to this controller
	connect(m_exporter, &GameExporter::message, this, &ExportController::appendLog);
	connect(m_exporter, &GameExporter::progress, this, &ExportController::setProgress);
	emit busyChanged();
	GameExporter* exporter = m_exporter;
	m_watcher.setFuture(QtConcurrent::run([exporter] { return exporter->run(); }));
	return true;
}

void ExportController::exportFinished()
{
	if (!m_exporter) {
		return;
	}
	const bool ok = m_watcher.result();
	m_folders = m_exporter->folders();
	m_exporter->deleteLater();
	m_exporter = nullptr;
	appendLog(ok ? QCoreApplication::translate("ExportDialog", "Export finished.")
		: QCoreApplication::translate("ExportDialog", "Export finished with errors."), ok ? GameExporter::Info : GameExporter::Error);
	emit foldersChanged();
	emit busyChanged();
	if (ok) {
		emit finished();
	} else {
		emit failed();
	}
}

void ExportController::cancel()
{
	if (m_exporter) {
		m_exporter->cancel();
	}
	if (m_download) {
		m_download->cancel();
	}
}

bool ExportController::download(ExportPlatform platform)
{
	const QUrl url = Launchers::url(platform);
	if (isBusy() || url.isEmpty()) {
		return false;
	}
	m_download = new LauncherDownload(platform, this);
	connect(m_download, &LauncherDownload::progress, this, [this](qint64 received, qint64 total) {
		// in KB: the bars count with int
		setProgress(static_cast<int>(received / 1024), total > 0 ? static_cast<int>(total / 1024) : 0);
	});
	connect(m_download, &LauncherDownload::finished, this, [this, platform](const QString& launcher, const QString& error) {
		if (launcher.isEmpty()) {
			appendLog(error, GameExporter::Error);
		} else {
			appendLog(QCoreApplication::translate("ExportDialog", "EasyRPG Player for %1 downloaded: %2")
				.arg(ExportPreset::name(platform), QDir::toNativeSeparators(launcher)), GameExporter::Info);
			setLauncher(platform, launcher);
		}
		m_download->deleteLater();
		m_download = nullptr;
		setProgress(launcher.isEmpty() ? 0 : 1, 1);
		emit busyChanged();
		emit platformChanged(platform);
		emit downloadFinished(platform, launcher, error);
	});
	appendLog(QCoreApplication::translate("ExportDialog", "Downloading %1").arg(url.toString()), GameExporter::Info);
	emit busyChanged();
	m_download->start();
	return true;
}
