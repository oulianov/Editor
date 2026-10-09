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

#include "export_dialog.h"
#include "launcher_download.h"
#include "core.h"
#include "common/filefinder.h"
#include "ui/common/path_field.h"
#include <QCheckBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QMenu>
#include <QStyle>
#include <algorithm>
#include <QToolButton>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QtConcurrentRun>

ExportDialog::ExportDialog(QWidget* parent) :
	QDialog(parent)
{
	setWindowTitle(tr("Export Game"));
	const auto& project = core().project();
	m_projectDir = project->projectDir();
	m_hasRuntime = !FileFinder::Find(m_projectDir, QStringLiteral("RPG_RT.exe")).isEmpty();
	m_settings = ExportSettings::load(m_projectDir, project->gameTitle());

	// 1. The game
	auto* game = new QGroupBox(tr("Game"), this);
	m_title = new QLineEdit(m_settings.title, game);
	m_title->setObjectName(QStringLiteral("gameTitle"));
	m_version = new QLineEdit(m_settings.version, game);
	m_version->setObjectName(QStringLiteral("gameVersion"));
	m_version->setPlaceholderText(QStringLiteral("1.0"));
	m_icon = new QLineEdit(m_settings.icon, game);
	m_icon->setObjectName(QStringLiteral("iconPath"));
	m_icon->setPlaceholderText(tr("A PNG image"));
	m_icon->setToolTip(tr("The icon of the program (Windows), of the application (macOS), "
		"of the desktop entry (Linux) and of the web page"));
	auto* choose_icon = new QToolButton(game);
	choose_icon->setObjectName(QStringLiteral("chooseIcon"));
	m_iconPreview = new QLabel(game);
	m_iconPreview->setObjectName(QStringLiteral("iconPreview"));
	auto* icon_row = pathFieldLayout(m_icon, choose_icon);
	icon_row->addWidget(m_iconPreview);
	auto* game_form = new QFormLayout(game);
	game_form->addRow(tr("Title:"), m_title);
	game_form->addRow(tr("Version:"), m_version);
	game_form->addRow(tr("Icon:"), icon_row);

	// 2. The platforms: one line each, its folder and its Player
	auto* platforms = new QGroupBox(tr("Platforms"), this);
	auto* grid = new QGridLayout(platforms);
	grid->setColumnStretch(1, 1);
	for (ExportPreset& preset : m_settings.presets) {
		const QString id = ExportPreset::id(preset.platform);
		Row row{preset.platform};
		row.check = new QCheckBox(ExportPreset::name(preset.platform), platforms);
		row.check->setObjectName(QStringLiteral("export_") + id);
		row.check->setChecked(preset.enabled);
		row.path = new QLineEdit(preset.path, platforms);
		row.path->setObjectName(QStringLiteral("path_") + id);
		row.path->setToolTip(tr("Emptied and written at each export. A relative path starts from the game folder."));
		row.browse = new QToolButton(platforms);
		row.browse->setObjectName(QStringLiteral("browse_") + id);
		row.browse->setToolTip(tr("Choose the folder"));
		const int line = static_cast<int>(m_rows.size());
		grid->addWidget(row.check, line, 0);
		grid->addLayout(pathFieldLayout(row.path, row.browse), line, 1);
		// the Player used: a pull-down to change it, get it or locate it
		row.launcher = new QPushButton(platforms);
		row.launcher->setObjectName(QStringLiteral("launcher_") + id);
		row.launcher->setMenu(new QMenu(row.launcher));
		grid->addWidget(row.launcher, line, 2);
		m_rows.append(row);
	}

	// 3. The options, one line each: the details are in their tooltips
	auto* options = new QGroupBox(tr("Options"), this);
	m_rtp = new QCheckBox(tr("Include the RTP files the game uses"), options);
	m_rtp->setObjectName(QStringLiteral("includeRtp"));
	m_rtp->setChecked(m_settings.include_rtp);
	m_rtp->setToolTip(tr("The graphics, music and sounds of the RTP that the game uses are copied with it: "
		"players do not need the RTP. Needed for the web."));
	m_clean = new QCheckBox(tr("Remove unused database entries"), options);
	m_clean->setObjectName(QStringLiteral("cleanDatabase"));
	m_clean->setChecked(m_settings.clean_database);
	m_clean->setToolTip(tr("Entries nothing uses are emptied; the others keep their numbers. "
		"Skipped for games with Maniac Patch, EasyRPG or DynRPG commands. Your project does not change."));
	m_protect = new QCheckBox(tr("Prevent opening the game in editors"), options);
	m_protect->setObjectName(QStringLiteral("protectFiles"));
	m_protect->setChecked(m_settings.protect);
	m_protect->setToolTip(tr("The map tree is written so that only the EasyRPG Player reads it: "
		"RPG Maker and the EasyRPG Editor cannot open the exported game. A deterrent, not encryption."));
	if (project->projectType() != FileFinder::ProjectType::Legacy) {
		m_protect->setEnabled(false);
		m_protect->setToolTip(tr("Only for games in the RPG Maker format"));
	}
	m_zip = new QCheckBox(tr("Make a zip of each export"), options);
	m_zip->setObjectName(QStringLiteral("makeZip"));
	m_zip->setToolTip(tr("Next to each folder; for the web, the zip to upload to itch.io"));
	m_zip->setChecked(std::any_of(m_settings.presets.cbegin(), m_settings.presets.cend(),
		[](const ExportPreset& preset) { return preset.zip; }));
	m_customize = new QCheckBox(tr("Give the macOS application the game's title and icon"), options);
	m_customize->setObjectName(QStringLiteral("customizeApp"));
	m_customize->setToolTip(tr("The application is signed again for this computer: other Macs then ask to allow it."));
	m_customize->setChecked(m_settings.preset(ExportPlatform::MacOS).customize);
	auto* options_layout = new QVBoxLayout(options);
	for (QCheckBox* option : {m_rtp, m_clean, m_protect, m_zip, m_customize}) {
		options_layout->addWidget(option);
	}

	// 4. Export: what it does, then the buttons
	m_log = new QPlainTextEdit(this);
	m_log->setObjectName(QStringLiteral("exportLog"));
	m_log->setReadOnly(true);
	m_log->setPlaceholderText(tr("The export reports here what it copies and adds."));
	m_progress = new QProgressBar(this);
	m_progress->setObjectName(QStringLiteral("exportProgress"));
	m_progress->setRange(0, 1);
	m_progress->setValue(0);
	m_progress->setTextVisible(false);
	m_open = new QPushButton(tr("Open Folder"), this);
	m_open->setObjectName(QStringLiteral("openFolder"));
	m_export = new QPushButton(tr("Export"), this);
	m_export->setObjectName(QStringLiteral("exportButton"));
	m_export->setDefault(true);
	m_cancel = new QPushButton(tr("Cancel"), this);
	m_cancel->setObjectName(QStringLiteral("cancelExport"));
	m_close = new QPushButton(tr("Close"), this);
	m_close->setObjectName(QStringLiteral("closeButton"));
	auto* buttons = new QHBoxLayout;
	buttons->addWidget(m_open);
	buttons->addStretch();
	buttons->addWidget(m_export);
	buttons->addWidget(m_cancel);
	buttons->addWidget(m_close);

	auto* layout = new QVBoxLayout(this);
	layout->addWidget(game);
	layout->addWidget(platforms);
	layout->addWidget(options);
	layout->addWidget(m_log, 1);
	layout->addWidget(m_progress);
	layout->addLayout(buttons);

	for (Row& row : m_rows) {
		const ExportPlatform platform = row.platform;
		auto preset = [this, platform]() -> ExportPreset& { return m_settings.preset(platform); };
		connect(row.check, &QCheckBox::toggled, this, [this, platform, preset](bool on) {
			preset().enabled = on;
			m_dirty = true;
			for (Row& row : m_rows) {
				if (row.platform == platform) {
					updateRow(row);
				}
			}
		});
		connect(row.path, &QLineEdit::textChanged, this, [this, preset](const QString& text) {
			preset().path = text;
			m_dirty = true;
		});
		connect(row.browse, &QToolButton::clicked, this, [this, platform] {
			for (Row& row : m_rows) {
				if (row.platform == platform) {
					chooseFolder(row);
				}
			}
		});
		updateRow(row);
	}
	connect(m_title, &QLineEdit::textChanged, this, [this](const QString& text) {
		m_settings.title = text;
		m_dirty = true;
	});
	connect(m_version, &QLineEdit::textChanged, this, [this](const QString& text) {
		m_settings.version = text;
		m_dirty = true;
	});
	connect(m_icon, &QLineEdit::textChanged, this, [this](const QString& text) {
		m_settings.icon = text;
		m_dirty = true;
		updateIcon();
	});
	connect(choose_icon, &QToolButton::clicked, this, &ExportDialog::chooseIcon);
	connect(m_rtp, &QCheckBox::toggled, this, [this](bool on) {
		m_settings.include_rtp = on;
		m_dirty = true;
	});
	connect(m_clean, &QCheckBox::toggled, this, [this](bool on) {
		m_settings.clean_database = on;
		m_dirty = true;
	});
	connect(m_protect, &QCheckBox::toggled, this, [this](bool on) {
		m_settings.protect = on;
		m_dirty = true;
	});
	connect(m_zip, &QCheckBox::toggled, this, [this](bool on) {
		for (ExportPreset& preset : m_settings.presets) {
			preset.zip = on;
		}
		m_dirty = true;
	});
	connect(m_customize, &QCheckBox::toggled, this, [this](bool on) {
		m_settings.preset(ExportPlatform::MacOS).customize = on;
		m_dirty = true;
	});
	connect(&m_watcher, &QFutureWatcher<bool>::finished, this, &ExportDialog::exportFinished);
	connect(m_export, &QPushButton::clicked, this, &ExportDialog::startExport);
	connect(m_cancel, &QPushButton::clicked, this, &ExportDialog::cancel);
	connect(m_close, &QPushButton::clicked, this, &ExportDialog::reject);
	connect(m_open, &QPushButton::clicked, this, &ExportDialog::openFolder);

	updateIcon();
	m_dirty = false;
	setBusy(false);
	// wide enough for the paths of the folders
	resize(std::max(sizeHint().width(), 760), sizeHint().height());
}

ExportDialog::~ExportDialog()
{
	// the export works on this window's exporter: it stops first
	if (m_exporter) {
		m_exporter->cancel();
		m_watcher.waitForFinished();
	}
}

void ExportDialog::updateRow(Row& row)
{
	// an unchecked platform keeps its settings, greyed
	const bool on = row.check->isChecked();
	row.path->setEnabled(on);
	row.browse->setEnabled(on);
	if (row.launcher) {
		row.launcher->setEnabled(on);
		updateLauncher(row);
	}
	if (row.platform == ExportPlatform::MacOS) {
		m_customize->setEnabled(on);
	}
}

void ExportDialog::updateLauncher(Row& row)
{
	ExportPreset& preset = m_settings.preset(row.platform);
	const QStringList found = Launchers::find(row.platform);
	if (preset.launcher.isEmpty() && !found.isEmpty()) {
		preset.launcher = found.first();
	}
	const bool runtime = preset.launcher == QLatin1String(ExportPreset::kGameRuntime);
	const bool ready = runtime || GameExporter::isLauncher(row.platform, preset.launcher);
	const QString downloads = Launchers::folder(row.platform);
	auto describe = [&](const QString& path) {
		if (path == QLatin1String(ExportPreset::kGameRuntime)) {
			return tr("RPG_RT.exe of the game");
		}
		return path.startsWith(downloads)
			? tr("EasyRPG Player %1").arg(QDir(downloads).relativeFilePath(path).section(QLatin1Char('/'), 0, 0))
			: QFileInfo(path).fileName();
	};
	// the Player used, or what is missing; the menu changes it
	row.launcher->setText(ready ? describe(preset.launcher) : tr("Player missing"));
	row.launcher->setToolTip(ready ? QDir::toNativeSeparators(runtime ? m_projectDir.filePath(preset.launcher) : preset.launcher)
		: tr("Download the EasyRPG Player %1 for %2, or locate it").arg(Launchers::version(), ExportPreset::name(row.platform)));
	row.launcher->setIcon(ready ? QIcon() : style()->standardIcon(QStyle::SP_MessageBoxWarning));

	QMenu* menu = row.launcher->menu();
	menu->clear();
	QStringList choices = found;
	if (row.platform == ExportPlatform::Windows && m_hasRuntime) {
		choices << QString::fromLatin1(ExportPreset::kGameRuntime);
	}
	if (ready && !choices.contains(preset.launcher)) {
		choices << preset.launcher;
	}
	const ExportPlatform platform = row.platform;
	for (const QString& choice : choices) {
		QAction* action = menu->addAction(describe(choice), this, [this, platform, choice] {
			m_settings.preset(platform).launcher = choice;
			m_dirty = true;
			for (Row& row : m_rows) {
				if (row.platform == platform) {
					updateLauncher(row);
				}
			}
		});
		action->setCheckable(true);
		action->setChecked(choice == preset.launcher);
		action->setToolTip(QDir::toNativeSeparators(choice));
	}
	if (!choices.isEmpty()) {
		menu->addSeparator();
	}
	QAction* get = menu->addAction(tr("Download EasyRPG Player %1").arg(Launchers::version()), this, [this, platform] { download(platform); });
	get->setEnabled(!m_download && !m_exporter);
	menu->addAction(tr("Locate..."), this, [this, platform] { chooseLauncher(platform); });
}

void ExportDialog::updateIcon()
{
	// the icon chosen, as high as its field (none: nothing shown)
	const QImage image(m_settings.icon.isEmpty() ? QString() : m_projectDir.absoluteFilePath(m_settings.icon));
	const int size = m_icon->sizeHint().height();
	m_iconPreview->setVisible(!image.isNull());
	m_iconPreview->setFixedSize(size, size);
	m_iconPreview->setPixmap(image.isNull() ? QPixmap()
		: QPixmap::fromImage(image.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
}

void ExportDialog::chooseFolder(Row& row)
{
	const QString dir = QFileDialog::getExistingDirectory(this, tr("Export Folder"), m_projectDir.absoluteFilePath(row.path->text()));
	if (!dir.isEmpty()) {
		row.path->setText(dir);
	}
}

void ExportDialog::chooseLauncher(ExportPlatform platform)
{
	ExportPreset& preset = m_settings.preset(platform);
	const QString title = tr("EasyRPG Player for %1").arg(ExportPreset::name(platform));
	const QString start = QFileInfo(preset.launcher).absolutePath();
#ifdef Q_OS_MACOS
	// an application is a file in the dialogs of macOS
	const bool folder = platform == ExportPlatform::Web;
#else
	const bool folder = platform == ExportPlatform::Web || platform == ExportPlatform::MacOS;
#endif
	QString path = folder ? QFileDialog::getExistingDirectory(this, title, start) : QFileDialog::getOpenFileName(this, title, start);
	if (!path.isEmpty() && !GameExporter::isLauncher(platform, path)) {
		// a folder with the Player in it (an extracted archive)
		const QString inside = QFileInfo(path).isDir() ? Launchers::findIn(platform, path) : QString();
		if (inside.isEmpty()) {
			QMessageBox::warning(this, windowTitle(), tr("%1 is not an EasyRPG Player for %2.")
				.arg(QDir::toNativeSeparators(path), ExportPreset::name(platform)));
			path.clear();
		} else {
			path = inside;
		}
	}
	if (!path.isEmpty()) {
		preset.launcher = path;
		m_dirty = true;
	}
	for (Row& row : m_rows) {
		if (row.platform == platform) {
			updateLauncher(row);
		}
	}
}

void ExportDialog::chooseIcon()
{
	const QString path = QFileDialog::getOpenFileName(this, tr("Icon"), m_projectDir.absoluteFilePath(m_icon->text()), tr("PNG images (*.png)"));
	if (!path.isEmpty()) {
		m_icon->setText(path);
	}
}

void ExportDialog::download(ExportPlatform platform)
{
	const QUrl url = Launchers::url(platform);
	if (m_download || url.isEmpty()) {
		return;
	}
	if (QMessageBox::question(this, tr("Download"),
			tr("Download the EasyRPG Player %1 for %2?\n\n%3\n\nIt is saved in %4.")
				.arg(Launchers::version(), ExportPreset::name(platform), url.toString(), QDir::toNativeSeparators(Launchers::folder(platform))))
			!= QMessageBox::Yes) {
		return;
	}
	m_download = new LauncherDownload(platform, this);
	connect(m_download, &LauncherDownload::progress, this, [this](qint64 received, qint64 total) {
		// in KB: the bar counts with int
		m_progress->setRange(0, total > 0 ? static_cast<int>(total / 1024) : 0);
		m_progress->setValue(static_cast<int>(received / 1024));
	});
	connect(m_download, &LauncherDownload::finished, this, [this, platform](const QString& launcher, const QString& error) {
		if (launcher.isEmpty()) {
			appendLog(error, GameExporter::Error);
		} else {
			appendLog(tr("EasyRPG Player for %1 downloaded: %2").arg(ExportPreset::name(platform), QDir::toNativeSeparators(launcher)), GameExporter::Info);
			m_settings.preset(platform).launcher = launcher;
			m_dirty = true;
		}
		m_download->deleteLater();
		m_download = nullptr;
		m_progress->setRange(0, 1);
		m_progress->setValue(launcher.isEmpty() ? 0 : 1);
		setBusy(false);
		for (Row& row : m_rows) {
			if (row.platform == platform) {
				updateLauncher(row);
			}
		}
		if (m_closeWhenDone) {
			QDialog::reject();
		}
	});
	appendLog(tr("Downloading %1").arg(url.toString()), GameExporter::Info);
	setBusy(true);
	m_download->start();
}

void ExportDialog::startExport()
{
	if (m_exporter || m_download) {
		return;
	}
	QStringList replaced;
	int enabled = 0;
	for (const ExportPreset& preset : m_settings.presets) {
		if (!preset.enabled) {
			continue;
		}
		++enabled;
		const QString error = GameExporter::checkFolder(m_projectDir, preset.path);
		if (!error.isEmpty()) {
			QMessageBox::warning(this, windowTitle(), ExportPreset::name(preset.platform) + QStringLiteral(": ") + error);
			return;
		}
		const QDir dir(m_projectDir.absoluteFilePath(preset.path));
		if (dir.exists() && !dir.isEmpty()) {
			replaced << QDir::toNativeSeparators(dir.absolutePath());
		}
	}
	if (enabled == 0) {
		QMessageBox::information(this, windowTitle(), tr("Check the platforms to export."));
		return;
	}
	if (!replaced.isEmpty() && QMessageBox::question(this, windowTitle(),
			tr("These folders are not empty. Replace everything in them?\n\n%1").arg(replaced.join(QLatin1Char('\n')))) != QMessageBox::Yes) {
		return;
	}
	saveSettings();
	m_log->clear();
	m_folders.clear();

	const auto& project = core().project();
	ExportSource source;
	source.dir = m_projectDir;
	source.type = project->projectType();
	source.encoding = project->encoding();
	source.rtp = core().rtpPath(QString());
	m_exporter = new GameExporter(source, m_settings, this);
	// emitted in the worker thread: queued to this window
	connect(m_exporter, &GameExporter::message, this, &ExportDialog::appendLog);
	connect(m_exporter, &GameExporter::progress, this, [this](int value, int maximum) {
		m_progress->setRange(0, maximum);
		m_progress->setValue(value);
	});
	setBusy(true);
	GameExporter* exporter = m_exporter;
	m_watcher.setFuture(QtConcurrent::run([exporter] { return exporter->run(); }));
}

void ExportDialog::exportFinished()
{
	const bool ok = m_watcher.result();
	m_folders = m_exporter->folders();
	m_exporter->deleteLater();
	m_exporter = nullptr;
	appendLog(ok ? tr("Export finished.") : tr("Export finished with errors."), ok ? GameExporter::Info : GameExporter::Error);
	setBusy(false);
	if (m_closeWhenDone) {
		QDialog::reject();
	}
}

void ExportDialog::cancel()
{
	if (m_exporter) {
		m_exporter->cancel();
	}
	if (m_download) {
		m_download->cancel();
	}
}

void ExportDialog::reject()
{
	if (m_exporter || m_download) {
		// closes when the work has stopped
		m_closeWhenDone = true;
		cancel();
		return;
	}
	if (m_dirty) {
		saveSettings();
	}
	QDialog::reject();
}

void ExportDialog::openFolder()
{
	if (m_folders.isEmpty()) {
		return;
	}
	// several platforms: the folder that holds them
	const QString folder = m_folders.size() == 1 ? m_folders.first() : QFileInfo(m_folders.first()).absolutePath();
	QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
}

void ExportDialog::appendLog(const QString& text, int level)
{
	switch (level) {
	case GameExporter::Added:
		m_log->appendPlainText(QStringLiteral("+ ") + text);
		break;
	case GameExporter::Warning:
		m_log->appendPlainText(tr("Warning: %1").arg(text));
		break;
	case GameExporter::Error:
		m_log->appendPlainText(tr("Error: %1").arg(text));
		break;
	default:
		m_log->appendPlainText(text);
		break;
	}
}

void ExportDialog::setBusy(bool busy)
{
	m_export->setEnabled(!busy);
	for (Row& row : m_rows) {
		if (row.launcher) {
			// the menu offers the download while nothing runs
			row.launcher->setEnabled(!busy && row.check->isChecked());
		}
	}
	m_cancel->setEnabled(busy);
	m_open->setEnabled(!busy && !m_folders.isEmpty());
}

void ExportDialog::saveSettings()
{
	if (!m_settings.save(m_projectDir)) {
		appendLog(tr("The export settings cannot be saved in %1.").arg(QDir::toNativeSeparators(m_projectDir.absolutePath())), GameExporter::Warning);
	}
	m_dirty = false;
}
