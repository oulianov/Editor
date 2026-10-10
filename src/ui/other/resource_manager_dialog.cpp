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

#include "resource_manager_dialog.h"
#include "ui/common/resource_info.h"
#include "ui/common/preview_zoom.h"
#include "common/audio_player.h"
#include "ui_resource_manager_dialog.h"
#include <QAction>
#include <QCollator>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QHash>
#include <QLocale>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QScrollBar>
#include <QSettings>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QStyle>
#include <QTreeWidgetItem>
#include <algorithm>
#include <iterator>
#include "core.h"
#include "defines.h"
#include "model/project.h"
#include "ui/common/help.h"
#include "ui/common/import_image_dialog.h"

namespace {
	const char GEOMETRY_KEY[] = "resource_manager/geometry";
	const char FOLDER_KEY[] = "resource_manager/folder";
	const char IMPORT_AS_KEY[] = "resource_manager/import_as";
	const char IMPORT_DIR_KEY[] = "resource_manager/import_dir";
	const char EXPORT_DIR_KEY[] = "resource_manager/export_dir";

	constexpr int PathRole = Qt::UserRole;
	constexpr int RtpRole = Qt::UserRole + 1;
	constexpr int NameRole = Qt::UserRole + 2;

	/** Gray squares behind the image, where it is transparent */
	QBrush checkerBrush() {
		QPixmap tile(16, 16);
		tile.fill(QColor(204, 204, 204));
		QPainter painter(&tile);
		painter.fillRect(0, 0, 8, 8, Qt::white);
		painter.fillRect(8, 8, 8, 8, Qt::white);
		return QBrush(tile);
	}

	bool isSameDir(const QString& a, const QString& b) {
		const QString ca = QFileInfo(a).canonicalFilePath();
		return !ca.isEmpty() && ca == QFileInfo(b).canonicalFilePath();
	}
}

using FileType = FileFinder::FileType;

// The folders of RPG Maker 2003's Resource Manager, in its order
const ResourceManagerDialog::Folder ResourceManagerDialog::folders[] = {
	{ BACKDROP, FileType::Image, false, QT_TRANSLATE_NOOP("ResourceManagerDialog", "Battle backgrounds") },
	{ BATTLE, FileType::Image, true, QT_TRANSLATE_NOOP("ResourceManagerDialog", "Battle animations") },
	{ BATTLE2, FileType::Image, true, QT_TRANSLATE_NOOP("ResourceManagerDialog", "Large battle animations (RPG Maker 2003)") },
	{ BATTLECHARSET, FileType::Image, true, QT_TRANSLATE_NOOP("ResourceManagerDialog", "Battle charsets (RPG Maker 2003)") },
	{ BATTLEWEAPON, FileType::Image, true, QT_TRANSLATE_NOOP("ResourceManagerDialog", "Battle weapons (RPG Maker 2003)") },
	{ CHARSET, FileType::Image, true, QT_TRANSLATE_NOOP("ResourceManagerDialog", "Character sets") },
	{ CHIPSET, FileType::Image, true, QT_TRANSLATE_NOOP("ResourceManagerDialog", "Tilesets") },
	{ FACESET, FileType::Image, true, QT_TRANSLATE_NOOP("ResourceManagerDialog", "Face sets") },
	{ FRAME, FileType::Image, true, QT_TRANSLATE_NOOP("ResourceManagerDialog", "Screen frames (RPG Maker 2003)") },
	{ GAMEOVER, FileType::Image, false, QT_TRANSLATE_NOOP("ResourceManagerDialog", "Game over screens") },
	{ MONSTER, FileType::Image, true, QT_TRANSLATE_NOOP("ResourceManagerDialog", "Monster graphics") },
	{ MOVIE, FileType::Video, false, QT_TRANSLATE_NOOP("ResourceManagerDialog", "Movies") },
	{ MUSIC, FileType::Music, false, QT_TRANSLATE_NOOP("ResourceManagerDialog", "Background music") },
	{ PANORAMA, FileType::Image, false, QT_TRANSLATE_NOOP("ResourceManagerDialog", "Panorama backgrounds") },
	{ PICTURE, FileType::Image, true, QT_TRANSLATE_NOOP("ResourceManagerDialog", "Pictures") },
	{ SOUND, FileType::Sound, false, QT_TRANSLATE_NOOP("ResourceManagerDialog", "Sound effects") },
	{ SYSTEM, FileType::Image, true, QT_TRANSLATE_NOOP("ResourceManagerDialog", "System graphics (windows, cursor, fonts colors)") },
	{ SYSTEM2, FileType::Image, true, QT_TRANSLATE_NOOP("ResourceManagerDialog", "Battle system graphics (RPG Maker 2003)") },
	{ TITLE, FileType::Image, false, QT_TRANSLATE_NOOP("ResourceManagerDialog", "Title screens") },
};

ResourceManagerDialog::ResourceManagerDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::ResourceManagerDialog)
{
	ui->setupUi(this);
	// the separator gives the preview more room: 2/5 for the files at first
	ui->splitterResources->setStretchFactor(0, 2);
	ui->splitterResources->setStretchFactor(1, 3);

	const QIcon folderIcon = style()->standardIcon(QStyle::SP_DirIcon);
	for (const Folder& folder : folders) {
		auto* item = new QTreeWidgetItem(ui->listFolders, { QString::fromLatin1(folder.name) });
		item->setIcon(0, folderIcon);
	}
	ui->previewImage->setBackgroundRole(QPalette::Base);
	ui->previewImage->setAutoFillBackground(true);
	// images in a view, to zoom on them as in the pickers; the label says
	// what the other files are
	m_previewScene = new QGraphicsScene(this);
	m_previewScene->setBackgroundBrush(ui->previewView->palette().brush(QPalette::Base));
	m_previewPixmap = m_previewScene->addPixmap(QPixmap());
	ui->previewView->setScene(m_previewScene);
	m_zoom = new PreviewZoom(ui->previewView, this);
	// what the file is, the zoom buttons on its right (as in the pickers)
	const int info = ui->layoutRight->indexOf(ui->previewInfo);
	ui->layoutRight->removeWidget(ui->previewInfo);
	ui->layoutRight->insertLayout(info, PreviewZoom::infoRow(ui->previewInfo, m_zoom));
	showImage(false);

	connect(ui->listFolders, &QTreeWidget::currentItemChanged, this, &ResourceManagerDialog::folderChanged);
	connect(ui->listFiles, &QTreeWidget::currentItemChanged, this, &ResourceManagerDialog::fileChanged);
	connect(ui->listFiles, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item) {
		const Folder* folder = currentFolder();
		if (item && folder && (folder->type == FileType::Music || folder->type == FileType::Sound)) {
			previewAudio(item->data(0, PathRole).toString());
		}
	});
	connect(ui->filterFiles, &QLineEdit::textChanged, this, &ResourceManagerDialog::applyFilter);
	connect(ui->pushImport, &QPushButton::clicked, this, &ResourceManagerDialog::importFiles);
	connect(ui->pushExport, &QPushButton::clicked, this, &ResourceManagerDialog::exportFile);
	connect(ui->pushDelete, &QPushButton::clicked, this, &ResourceManagerDialog::deleteFile);
	connect(ui->pushPlay, &QPushButton::clicked, this, [this] { previewAudio(currentPath()); });
	connect(ui->pushStop, &QPushButton::clicked, this, [this] { previewAudio(QString()); });
	connect(ui->buttonBox, &QDialogButtonBox::helpRequested, this, [this] {
		Help::open(this, QStringLiteral("resource-manager"));
	});

	auto* deleteAction = new QAction(this);
	deleteAction->setShortcuts({ QKeySequence::Delete, QKeySequence(Qt::Key_Backspace) });
	deleteAction->setShortcutContext(Qt::WidgetShortcut);
	ui->listFiles->addAction(deleteAction);
	connect(deleteAction, &QAction::triggered, this, [this] {
		if (ui->pushDelete->isEnabled()) {
			deleteFile();
		}
	});

	QSettings settings;
	resize(860, 540);
	setProperty("dialog_geometry_key", QString::fromLatin1(GEOMETRY_KEY));
	if (const QScreen* screen = parent ? parent->screen() : this->screen()) {
		resize(size().boundedTo(screen->availableGeometry().size() * 0.95));
	}
	if (settings.value(IMPORT_AS_KEY).toString() == QLatin1String("xyz")) {
		ui->radioImportXyz->setChecked(true);
	}
	connect(ui->radioImportXyz, &QRadioButton::toggled, this, [](bool xyz) {
		QSettings().setValue(IMPORT_AS_KEY, xyz ? "xyz" : "png");
	});

	// the folder of last time; the files are listed when the dialog is shown
	const QString lastFolder = settings.value(FOLDER_KEY).toString();
	QTreeWidgetItem* current = ui->listFolders->topLevelItem(0);
	for (int i = 0; i < ui->listFolders->topLevelItemCount(); ++i) {
		if (ui->listFolders->topLevelItem(i)->text(0) == lastFolder) {
			current = ui->listFolders->topLevelItem(i);
		}
	}
	{
		const QSignalBlocker blocker(ui->listFolders);
		ui->listFolders->setCurrentItem(current);
	}
	refreshFolders();
	updateButtons();
}

ResourceManagerDialog::~ResourceManagerDialog()
{
	delete ui;
}

void ResourceManagerDialog::previewAudio(const QString& path)
{
	// the player of the Jukebox and the pickers; an empty path stops
	if (!m_player) {
		m_player = new AudioPlayer(this);
	}
	if (path.isEmpty()) {
		m_player->stop();
	} else {
		m_player->play(path);
	}
	emit audioPreviewRequested(path);
}

void ResourceManagerDialog::showEvent(QShowEvent *event)
{
	// the dialog lives as long as the main window: list the files of the
	// project open now, which may not be the one of last time
	refreshFolders();
	auto* item = ui->listFiles->currentItem();
	refreshFiles(item ? item->data(0, NameRole).toString() : QString());
	QDialog::showEvent(event);
}

void ResourceManagerDialog::hideEvent(QHideEvent *event)
{
	previewAudio(QString());
	QSettings settings;
	settings.setValue(GEOMETRY_KEY, saveGeometry());
	if (const Folder* folder = currentFolder()) {
		settings.setValue(FOLDER_KEY, QString::fromLatin1(folder->name));
	}
	QDialog::hideEvent(event);
}

const ResourceManagerDialog::Folder* ResourceManagerDialog::currentFolder() const
{
	const int row = ui->listFolders->indexOfTopLevelItem(ui->listFolders->currentItem());
	if (row < 0 || row >= static_cast<int>(std::size(folders))) {
		return nullptr;
	}
	return &folders[row];
}

QString ResourceManagerDialog::gameFolderPath(const Folder& folder) const
{
	const auto& project = core().project();
	if (!project) {
		return QString();
	}
	// an existing folder whatever its case (Charset, CHARSET...), else the usual name
	const QString found = project->findDirectory(QString::fromLatin1(folder.name));
	return found.isEmpty()
		? FileFinder::CombinePath(project->projectDir().absolutePath(), QString::fromLatin1(folder.name))
		: found;
}

QString ResourceManagerDialog::rtpFolderPath(const Folder& folder) const
{
	const QString rtp = core().rtpPath(QString());
	if (rtp.isEmpty() || !QDir(rtp).exists()) {
		return QString();
	}
	return FileFinder::Find(QDir(rtp), QString::fromLatin1(folder.name), FileType::Default, QDir::Dirs);
}

QStringList ResourceManagerDialog::extensions(FileType type)
{
	QStringList list = FileFinder::GetFiltersForType(type);
	if (type == FileType::Video) {
		// the movies of RPG Maker 2003
		list << QStringLiteral(".mpg") << QStringLiteral(".mpeg");
	}
	return list;
}

void ResourceManagerDialog::refreshFolders()
{
	const bool project = core().project() != nullptr;
	for (int i = 0; i < ui->listFolders->topLevelItemCount(); ++i) {
		const Folder& folder = folders[i];
		QTreeWidgetItem* item = ui->listFolders->topLevelItem(i);
		const bool missing = project && !QDir(gameFolderPath(folder)).exists();
		QFont font = ui->listFolders->font();
		font.setItalic(missing);
		item->setFont(0, font);
		QString tip = tr(folder.description);
		if (missing) {
			tip += QLatin1Char('\n') + tr("This game has no %1 folder yet: Import creates it.").arg(QString::fromLatin1(folder.name));
		}
		item->setToolTip(0, tip);
	}
	// as wide as the longest folder name (italic ones too), the files and the preview get the rest
	ui->listFolders->resizeColumnToContents(0);
	ui->listFolders->setFixedWidth(ui->listFolders->columnWidth(0)
		+ 2 * ui->listFolders->frameWidth()
		+ ui->listFolders->verticalScrollBar()->sizeHint().width());
}

void ResourceManagerDialog::folderChanged()
{
	previewAudio(QString());
	refreshFiles();
}

void ResourceManagerDialog::refreshFiles(const QString& select)
{
	const Folder* folder = currentFolder();
	QTreeWidgetItem* toSelect = nullptr;
	{
		const QSignalBlocker blocker(ui->listFiles);
		ui->listFiles->clear();

		struct Entry {
			QString name;
			QString path;
			bool rtp;
			int rank;
		};
		QList<Entry> entries;
		QHash<QString, int> seen;
		const QStringList exts = folder ? extensions(folder->type) : QStringList();

		// The game's files hide the RTP's of the same name; in one folder, the
		// extension that the Player tries first wins (as FileFinder::Find)
		auto scan = [&](const QString& dir, bool rtp) {
			if (dir.isEmpty() || !QDir(dir).exists()) {
				return;
			}
			const QFileInfoList files = QDir(dir).entryInfoList(QDir::Files);
			for (const QFileInfo& info : files) {
				const int rank = info.suffix().isEmpty() ? -1 : static_cast<int>(exts.indexOf(QLatin1Char('.') + info.suffix().toLower()));
				if (rank < 0) {
					continue;
				}
				const QString name = info.completeBaseName();
				const QString key = name.toLower();
				auto it = seen.constFind(key);
				if (it == seen.constEnd()) {
					seen.insert(key, static_cast<int>(entries.size()));
					entries.append({ name, info.absoluteFilePath(), rtp, rank });
				} else if (entries[*it].rtp == rtp && rank < entries[*it].rank) {
					entries[*it] = { name, info.absoluteFilePath(), rtp, rank };
				}
			}
		};
		if (folder) {
			scan(gameFolderPath(*folder), false);
			scan(rtpFolderPath(*folder), true);
		}

		QCollator collator;
		collator.setNumericMode(true);
		collator.setCaseSensitivity(Qt::CaseInsensitive);
		std::sort(entries.begin(), entries.end(), [&](const Entry& a, const Entry& b) {
			return collator.compare(a.name, b.name) < 0;
		});

		const QColor rtpColor = palette().color(QPalette::PlaceholderText);
		int rtpCount = 0;
		for (const Entry& entry : entries) {
			auto* item = new QTreeWidgetItem(ui->listFiles,
				{ entry.rtp ? tr("%1 (RTP)").arg(entry.name) : entry.name });
			item->setData(0, PathRole, entry.path);
			item->setData(0, RtpRole, entry.rtp);
			item->setData(0, NameRole, entry.name);
			item->setToolTip(0, QDir::toNativeSeparators(entry.path));
			if (entry.rtp) {
				item->setForeground(0, rtpColor);
				rtpCount++;
			}
			if (!toSelect && !select.isEmpty() && entry.name.compare(select, Qt::CaseInsensitive) == 0) {
				toSelect = item;
			}
		}

		QString count = tr("Files: %1").arg(entries.size());
		if (rtpCount > 0) {
			count += QStringLiteral(", ") + tr("from the RTP: %1").arg(rtpCount);
		} else if (core().rtpPath(QString()).isEmpty()) {
			count += QStringLiteral(", ") + tr("no RTP path set");
		}
		ui->labelCount->setText(count);
		applyFilter();
		if (toSelect) {
			ui->listFiles->setCurrentItem(toSelect);
			ui->listFiles->scrollToItem(toSelect);
		}
	}
	fileChanged();
}

void ResourceManagerDialog::applyFilter()
{
	const QString text = ui->filterFiles->text().trimmed();
	for (int i = 0; i < ui->listFiles->topLevelItemCount(); ++i) {
		QTreeWidgetItem* item = ui->listFiles->topLevelItem(i);
		item->setHidden(!text.isEmpty() && !item->data(0, NameRole).toString().contains(text, Qt::CaseInsensitive));
	}
}

QString ResourceManagerDialog::currentPath() const
{
	const auto* item = ui->listFiles->currentItem();
	return item ? item->data(0, PathRole).toString() : QString();
}

bool ResourceManagerDialog::currentIsRtp() const
{
	const auto* item = ui->listFiles->currentItem();
	return item && item->data(0, RtpRole).toBool();
}

void ResourceManagerDialog::fileChanged()
{
	updateButtons();
	showPreview(currentPath(), currentIsRtp());
}

void ResourceManagerDialog::updateButtons()
{
	const bool project = core().project() != nullptr;
	const Folder* folder = currentFolder();
	const bool selected = !currentPath().isEmpty();
	const bool audio = folder && (folder->type == FileType::Music || folder->type == FileType::Sound);

	ui->pushImport->setEnabled(project && folder);
	ui->pushExport->setEnabled(selected);
	ui->pushDelete->setEnabled(project && selected && !currentIsRtp());
	ui->pushDelete->setToolTip(selected && currentIsRtp() ? tr("Files of the RTP cannot be deleted.") : QString());
	ui->groupImportAs->setEnabled(project && folder && folder->type == FileType::Image);
	ui->pushPlay->setVisible(audio);
	ui->pushStop->setVisible(audio);
	ui->pushPlay->setEnabled(selected);
}

void ResourceManagerDialog::showImage(bool image)
{
	ui->previewView->setVisible(image);
	m_zoom->setVisible(image);
	ui->preview->setVisible(!image);
}

void ResourceManagerDialog::showPreview(const QString& path, bool rtp)
{
	const Folder* folder = currentFolder();
	ui->previewImage->clear();
	m_previewPixmap->setPixmap(QPixmap());
	showImage(folder && folder->type == FileType::Image);
	if (path.isEmpty() || !folder) {
		ui->previewInfo->setText(core().project() ? QString() : tr("No project is open."));
		return;
	}

	const QImage image = folder->type == FileType::Image ? ImportImageDialog::loadImage(path) : QImage();
	QStringList lines = resourceInfo(path, rtp, image);

	if (folder->type == FileType::Image) {
		if (image.isNull()) {
			lines << tr("This image cannot be read.");
		} else {
			if (ImportImageDialog::paletteSize(image) > 0 && folder->transparent) {
				lines << tr("Shown as in the game: palette color 0 is transparent.");
			}
			const QString rule = sizeRule(*folder, image.size());
			if (!rule.isEmpty()) {
				lines << rule;
			}
			// palette color 0 transparent in folders where the game makes it so
			const QImage shown = folder->transparent
				? ImportImageDialog::withTransparentIndex0(image).convertToFormat(QImage::Format_ARGB32)
				: image.convertToFormat(QImage::Format_ARGB32);
			QPixmap pixmap(shown.size());
			QPainter painter(&pixmap);
			painter.fillRect(pixmap.rect(), checkerBrush());
			painter.drawImage(0, 0, shown);
			painter.end();
			m_previewPixmap->setPixmap(pixmap);
			m_previewScene->setSceneRect(pixmap.rect());
		}
	} else {
		lines << tr("Format: %1").arg(QFileInfo(path).suffix().toUpper());
		if (folder->type == FileType::Video) {
			ui->previewImage->setText(tr("Movies have no preview."));
		} else {
			ui->previewImage->setText(tr("Press Play, or double-click the file, to listen to it."));
		}
	}
	ui->previewInfo->setText(lines.join(QLatin1Char('\n')));
}

QString ResourceManagerDialog::sizeRule(const Folder& folder, QSize size)
{
	// the sizes of RPG Maker 2003's help (Image Specifications)
	const QLatin1String name(folder.name);
	auto exact = [&](int w, int h) {
		if (size == QSize(w, h)) {
			return QString();
		}
		return tr("RPG Maker's %1 images are %2 x %3 pixels; this one is %4 x %5.")
			.arg(name).arg(w).arg(h).arg(size.width()).arg(size.height());
	};
	auto range = [&](int min, int max, int step) {
		auto fits = [&](int v) { return v >= min && v <= max && v % step == 0; };
		if (fits(size.width()) && fits(size.height())) {
			return QString();
		}
		const QString rule = step > 1
			? tr("RPG Maker's %1 images are %2 to %3 pixels wide and high, in multiples of %4; this one is %5 x %6.")
				.arg(name).arg(min).arg(max).arg(step).arg(size.width()).arg(size.height())
			: tr("RPG Maker's %1 images are %2 to %3 pixels wide and high; this one is %4 x %5.")
				.arg(name).arg(min).arg(max).arg(size.width()).arg(size.height());
		return rule;
	};

	if (name == QLatin1String(BACKDROP) || name == QLatin1String(FRAME)
			|| name == QLatin1String(GAMEOVER) || name == QLatin1String(TITLE)) {
		return exact(320, 240);
	} else if (name == QLatin1String(BATTLE)) {
		return range(96, 480, 96);
	} else if (name == QLatin1String(BATTLE2)) {
		return range(128, 640, 128);
	} else if (name == QLatin1String(BATTLECHARSET)) {
		return exact(144, 384);
	} else if (name == QLatin1String(BATTLEWEAPON)) {
		return exact(192, 512);
	} else if (name == QLatin1String(CHARSET)) {
		return exact(288, 256);
	} else if (name == QLatin1String(CHIPSET)) {
		return exact(480, 256);
	} else if (name == QLatin1String(FACESET)) {
		return exact(192, 192);
	} else if (name == QLatin1String(MONSTER)) {
		return range(16, 240, 1);
	} else if (name == QLatin1String(PANORAMA)) {
		return range(80, 640, 1);
	} else if (name == QLatin1String(PICTURE)) {
		return range(1, 640, 1);
	} else if (name == QLatin1String(SYSTEM)) {
		return exact(160, 80);
	} else if (name == QLatin1String(SYSTEM2)) {
		return exact(80, 96);
	}
	return QString();
}

void ResourceManagerDialog::importFiles()
{
	const Folder* folder = currentFolder();
	if (!folder || !core().project()) {
		return;
	}
	const QString folderName = QString::fromLatin1(folder->name);

	QStringList patterns;
	for (const QString& ext : extensions(folder->type)) {
		patterns << QLatin1Char('*') + ext;
	}
	const QString filter = tr("%1 files (%2)").arg(folderName, patterns.join(QLatin1Char(' ')))
		+ QStringLiteral(";;") + tr("All files (*)");

	QSettings settings;
	QString start = settings.value(IMPORT_DIR_KEY).toString();
	if (start.isEmpty() || !QDir(start).exists()) {
		const auto location = folder->type == FileType::Image ? QStandardPaths::PicturesLocation
			: folder->type == FileType::Video ? QStandardPaths::MoviesLocation
			: QStandardPaths::MusicLocation;
		start = QStandardPaths::writableLocation(location);
	}
	const QStringList sources = QFileDialog::getOpenFileNames(this,
		tr("Import into %1").arg(folderName), start, filter);
	if (sources.isEmpty()) {
		return;
	}
	settings.setValue(IMPORT_DIR_KEY, QFileInfo(sources.first()).absolutePath());

	// RPG Maker creates the folder when it is missing
	const QString dir = gameFolderPath(*folder);
	if (!QDir().mkpath(dir)) {
		QMessageBox::warning(this, tr("Import"), tr("The folder %1 cannot be created.").arg(QDir::toNativeSeparators(dir)));
		return;
	}

	QString imported;
	for (const QString& source : sources) {
		const bool done = folder->type == FileType::Image
			? importImage(*folder, source, dir)
			: importFile(*folder, source, dir);
		if (done) {
			imported = QFileInfo(source).completeBaseName();
		}
	}
	refreshFolders();
	auto* item = ui->listFiles->currentItem();
	refreshFiles(!imported.isEmpty() ? imported : item ? item->data(0, NameRole).toString() : QString());
}

bool ResourceManagerDialog::makeRoom(const Folder& folder, const QString& dir, const QString& name)
{
	const QStringList exts = extensions(folder.type);
	QStringList existing;
	for (const QFileInfo& info : QDir(dir).entryInfoList(QDir::Files)) {
		if (info.completeBaseName().compare(name, Qt::CaseInsensitive) == 0
				&& exts.contains(QLatin1Char('.') + info.suffix().toLower())) {
			existing << info.absoluteFilePath();
		}
	}
	if (existing.isEmpty()) {
		return true;
	}
	if (QMessageBox::question(this, tr("Import"),
			tr("The %1 folder already has a file named %2. Replace it?").arg(QString::fromLatin1(folder.name), name),
			QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
		return false;
	}
	// every extension: the game would otherwise keep using the old .bmp of a new .png
	for (const QString& path : existing) {
		if (!QFile::remove(path)) {
			QMessageBox::warning(this, tr("Import"), tr("%1 cannot be replaced.").arg(QDir::toNativeSeparators(path)));
			return false;
		}
	}
	return true;
}

bool ResourceManagerDialog::importFile(const Folder& folder, const QString& source, const QString& dir)
{
	const QFileInfo info(source);
	const QStringList exts = extensions(folder.type);
	if (!exts.contains(QLatin1Char('.') + info.suffix().toLower())) {
		QMessageBox::warning(this, tr("Import"), tr("%1 is not a file the game can use in %2 (%3).")
			.arg(info.fileName(), QString::fromLatin1(folder.name), exts.join(QStringLiteral(", "))));
		return false;
	}
	if (isSameDir(info.absolutePath(), dir)) {
		QMessageBox::information(this, tr("Import"), tr("%1 is already in the game.").arg(info.fileName()));
		return false;
	}
	if (!makeRoom(folder, dir, info.completeBaseName())) {
		return false;
	}
	const QString target = FileFinder::CombinePath(dir, info.fileName());
	if (!QFile::copy(source, target)) {
		QMessageBox::warning(this, tr("Import"), tr("%1 cannot be copied to %2.")
			.arg(info.fileName(), QDir::toNativeSeparators(dir)));
		return false;
	}
	return true;
}

bool ResourceManagerDialog::importImage(const Folder& folder, const QString& source, const QString& dir)
{
	const QFileInfo info(source);
	QImage image = ImportImageDialog::loadImage(source);
	if (image.isNull()) {
		QMessageBox::warning(this, tr("Import"), tr("%1 is not an image the Editor can read (PNG, BMP or XYZ).").arg(info.fileName()));
		return false;
	}
	if (isSameDir(info.absolutePath(), dir)) {
		QMessageBox::information(this, tr("Import"), tr("%1 is already in the game.").arg(info.fileName()));
		return false;
	}

	const QString rule = sizeRule(folder, image.size());
	if (!rule.isEmpty() && QMessageBox::warning(this, tr("Import"),
			rule + QStringLiteral("\n\n") + tr("Import %1 anyway?").arg(info.fileName()),
			QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
		return false;
	}

	// The games use palette images: convert when it loses nothing, else ask
	QImage converted = ImportImageDialog::toIndexed(image);
	if (converted.isNull()) {
		QMessageBox box(QMessageBox::Question, tr("Import"),
			tr("%1 has more than 256 colors. RPG Maker, and the EasyRPG Player without its extensions, only use images of 256 colors.")
				.arg(info.fileName()), QMessageBox::NoButton, this);
		QPushButton* reduce = box.addButton(tr("Reduce to 256 colors"), QMessageBox::AcceptRole);
		QPushButton* keep = box.addButton(tr("Keep all colors"), QMessageBox::AcceptRole);
		box.addButton(QMessageBox::Cancel);
		box.setDefaultButton(reduce);
		box.exec();
		if (box.clickedButton() == reduce) {
			converted = ImportImageDialog::reduceColors(image);
		} else if (box.clickedButton() == keep) {
			converted = image.convertToFormat(QImage::Format_ARGB32);
		} else {
			return false;
		}
	}
	image = converted;

	if (folder.transparent) {
		ImportImageDialog dialog(image, this);
		dialog.setWindowTitle(tr("Transparent color of %1").arg(info.fileName()));
		if (dialog.exec() != QDialog::Accepted) {
			return false;
		}
		image = dialog.image();
	}

	const bool palette = image.format() == QImage::Format_Indexed8;
	if (palette) {
		// transparency is palette index 0, not alpha in the palette
		QVector<QRgb> table = image.colorTable();
		for (QRgb& color : table) {
			color |= 0xff000000u;
		}
		image.setColorTable(table);
	}
	bool xyz = ui->radioImportXyz->isChecked();
	if (xyz && !palette) {
		QMessageBox::information(this, tr("Import"),
			tr("%1 is saved as PNG: XYZ images have 256 colors at most.").arg(info.fileName()));
		xyz = false;
	}

	const QString name = info.completeBaseName();
	if (!makeRoom(folder, dir, name)) {
		return false;
	}
	const QString target = FileFinder::CombinePath(dir, name + (xyz ? QStringLiteral(".xyz") : QStringLiteral(".png")));
	const bool saved = xyz ? ImportImageDialog::saveXyz(image, target) : image.save(target, "PNG");
	if (!saved) {
		QMessageBox::warning(this, tr("Import"), tr("%1 cannot be written.").arg(QDir::toNativeSeparators(target)));
		return false;
	}
	return true;
}

void ResourceManagerDialog::exportFile()
{
	const QString path = currentPath();
	if (path.isEmpty()) {
		return;
	}
	const QFileInfo info(path);
	QSettings settings;
	QString dir = settings.value(EXPORT_DIR_KEY).toString();
	if (dir.isEmpty() || !QDir(dir).exists()) {
		dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
	}
	QString target = QFileDialog::getSaveFileName(this, tr("Export %1").arg(info.fileName()),
		FileFinder::CombinePath(dir, info.fileName()));
	if (target.isEmpty()) {
		return;
	}
	if (QFileInfo(target).suffix().isEmpty()) {
		target += QLatin1Char('.') + info.suffix();
	}
	settings.setValue(EXPORT_DIR_KEY, QFileInfo(target).absolutePath());
	if (QFileInfo(target).canonicalFilePath() == info.canonicalFilePath()) {
		return;
	}
	// the file dialog asked before replacing the file
	if (QFile::exists(target) && !QFile::remove(target)) {
		QMessageBox::warning(this, tr("Export"), tr("%1 cannot be replaced.").arg(QDir::toNativeSeparators(target)));
		return;
	}
	if (!QFile::copy(path, target)) {
		QMessageBox::warning(this, tr("Export"), tr("%1 cannot be written.").arg(QDir::toNativeSeparators(target)));
	}
}

void ResourceManagerDialog::deleteFile()
{
	const Folder* folder = currentFolder();
	QTreeWidgetItem* item = ui->listFiles->currentItem();
	if (!folder || !item || currentIsRtp() || !core().project()) {
		return;
	}
	const QString path = currentPath();
	const QString name = item->data(0, NameRole).toString();
	if (QMessageBox::question(this, tr("Delete"),
			tr("Delete %1 from the %2 folder of the game?").arg(QFileInfo(path).fileName(), QString::fromLatin1(folder->name)),
			QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
		return;
	}
	previewAudio(QString());
	// to the trash when the system has one
	if (!QFile::moveToTrash(path) && !QFile::remove(path)) {
		QMessageBox::warning(this, tr("Delete"), tr("%1 cannot be deleted.").arg(QDir::toNativeSeparators(path)));
		return;
	}

	// the same line again (an RTP file of the same name shows again), else the next one
	QTreeWidgetItem* next = ui->listFiles->itemBelow(item);
	if (!next) {
		next = ui->listFiles->itemAbove(item);
	}
	const QString rtpDir = rtpFolderPath(*folder);
	const bool inRtp = !rtpDir.isEmpty() && !FileFinder::Find(QDir(rtpDir), name, folder->type).isEmpty();
	const QString select = inRtp || !next ? name : next->data(0, NameRole).toString();
	refreshFiles(select);
}
