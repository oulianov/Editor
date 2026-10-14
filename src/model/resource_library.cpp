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

#include "resource_library.h"
#include <QBrush>
#include <QCollator>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QGuiApplication>
#include <QHash>
#include <QLocale>
#include <QPalette>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <algorithm>
#include <iterator>
#include "common/image_import.h"
#include "controller/dialog_service.h"
#include "defines.h"

namespace {
	// the window's keys of before (EditorSettings resets them)
	const char FOLDER_KEY[] = "resource_manager/folder";
	const char IMPORT_AS_KEY[] = "resource_manager/import_as";
	const char IMPORT_DIR_KEY[] = "resource_manager/import_dir";
	const char EXPORT_DIR_KEY[] = "resource_manager/export_dir";

	using FileType = FileFinder::FileType;
	using Kind = ResourceLibraryModel::Kind;

	struct Folder {
		const char* name;
		FileType type;
		/** palette index 0 is transparent in the game */
		bool transparent;
		const char* description;
	};

	// The folders of RPG Maker 2003's Resource Manager, in its order
	const Folder kFolders[] = {
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
	constexpr int FolderCount = static_cast<int>(std::size(kFolders));

	const Folder* folderAt(int folder) {
		return folder >= 0 && folder < FolderCount ? &kFolders[folder] : nullptr;
	}

	bool isSameDir(const QString& a, const QString& b) {
		const QString ca = QFileInfo(a).canonicalFilePath();
		return !ca.isEmpty() && ca == QFileInfo(b).canonicalFilePath();
	}

	/** An image the Player reads: palette transparency is index 0, not alpha */
	QImage withOpaquePalette(QImage image) {
		return image.format() == QImage::Format_Indexed8 ? ImageImport::opaquePalette(image) : image;
	}
}

ResourceResult ResourceResult::done(const QString& name)
{
	ResourceResult result;
	result.ok = true;
	result.name = name;
	return result;
}

ResourceResult ResourceResult::failed(const QString& error)
{
	ResourceResult result;
	result.error = error;
	return result;
}

ResourceResult ResourceResult::skipped(const QString& notice)
{
	ResourceResult result;
	result.notice = notice;
	return result;
}

ResourceResult ResourceResult::declined()
{
	ResourceResult result;
	result.cancelled = true;
	return result;
}

ResourceImportChoices::Colors ResourceImportChoices::askColors(const QString&, const QString&)
{
	return Colors::Cancel;
}

QImage ResourceImportChoices::chooseTransparent(const QImage&, const QString&)
{
	return QImage();
}

ResourceFolderModel::ResourceFolderModel(ResourceLibraryModel* library) :
	QAbstractListModel(library), m_library(library)
{
	m_missing.fill(false, FolderCount);
}

int ResourceFolderModel::rowCount(const QModelIndex& parent) const
{
	return parent.isValid() ? 0 : FolderCount;
}

QVariant ResourceFolderModel::data(const QModelIndex& index, int role) const
{
	if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
		return QVariant();
	}
	const int row = index.row();
	const Folder& folder = kFolders[row];
	const bool missing = m_missing[row];
	switch (role) {
	case Qt::DisplayRole:
	case NameRole:
		return QString::fromLatin1(folder.name);
	case Qt::ToolTipRole: {
		QString tip = ResourceLibraryModel::folderDescription(row);
		if (missing) {
			tip += QLatin1Char('\n') + QCoreApplication::translate("ResourceManagerDialog",
				"This game has no %1 folder yet: Import creates it.").arg(QString::fromLatin1(folder.name));
		}
		return tip;
	}
	case Qt::FontRole:
		if (missing) {
			// only italic: the rest is the view's font
			QFont font;
			font.setItalic(true);
			return font;
		}
		return QVariant();
	case DescriptionRole:
		return ResourceLibraryModel::folderDescription(row);
	case KindRole:
		return QVariant::fromValue(ResourceLibraryModel::folderKind(row));
	case TransparentRole:
		return folder.transparent;
	case MissingRole:
		return missing;
	case PathRole:
		return m_library->gameFolderPath(row);
	default:
		return QVariant();
	}
}

QHash<int, QByteArray> ResourceFolderModel::roleNames() const
{
	QHash<int, QByteArray> names = QAbstractListModel::roleNames();
	names.insert(NameRole, "name");
	names.insert(DescriptionRole, "description");
	names.insert(KindRole, "kind");
	names.insert(TransparentRole, "transparent");
	names.insert(MissingRole, "missing");
	names.insert(PathRole, "path");
	return names;
}

void ResourceFolderModel::update()
{
	const bool game = m_library->hasGame();
	for (int row = 0; row < FolderCount; ++row) {
		const bool missing = game && !QDir(m_library->gameFolderPath(row)).exists();
		if (missing != m_missing[row]) {
			m_missing[row] = missing;
			emit dataChanged(index(row), index(row));
		}
	}
}

ResourceLibraryModel::ResourceLibraryModel(QObject* parent) :
	QAbstractListModel(parent)
{
	m_folders = new ResourceFolderModel(this);
	QSettings settings;
	m_importXyz = settings.value(QLatin1String(IMPORT_AS_KEY)).toString() == QLatin1String("xyz");
	// the folder of last time
	m_folder = std::max(0, folderOf(settings.value(QLatin1String(FOLDER_KEY)).toString()));
}

ResourceLibraryModel::~ResourceLibraryModel() = default;

int ResourceLibraryModel::rowCount(const QModelIndex& parent) const
{
	return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant ResourceLibraryModel::data(const QModelIndex& index, int role) const
{
	if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
		return QVariant();
	}
	const Entry& entry = m_entries[m_rows[index.row()]];
	switch (role) {
	case Qt::DisplayRole:
		return entry.rtp ? QCoreApplication::translate("ResourceManagerDialog", "%1 (RTP)").arg(entry.name) : entry.name;
	case Qt::ToolTipRole:
		return QDir::toNativeSeparators(entry.path);
	case Qt::ForegroundRole:
		// the RTP's dimmed
		return entry.rtp ? QVariant(QBrush(QGuiApplication::palette().color(QPalette::PlaceholderText))) : QVariant();
	case NameRole:
		return entry.name;
	case PathRole:
		return entry.path;
	case OriginRole:
		return QVariant::fromValue(entry.rtp ? Origin::Rtp : Origin::Game);
	case SizeRole:
		return entry.size;
	case FormatRole:
		return QFileInfo(entry.path).suffix().toUpper();
	default:
		return QVariant();
	}
}

QHash<int, QByteArray> ResourceLibraryModel::roleNames() const
{
	QHash<int, QByteArray> names = QAbstractListModel::roleNames();
	names.insert(NameRole, "name");
	names.insert(PathRole, "path");
	names.insert(OriginRole, "origin");
	names.insert(SizeRole, "size");
	names.insert(FormatRole, "format");
	return names;
}

QString ResourceLibraryModel::gamePath() const
{
	return m_gamePath;
}

void ResourceLibraryModel::setGamePath(const QString& path)
{
	if (path == m_gamePath) {
		return;
	}
	m_gamePath = path;
	emit gamePathChanged();
	// another game: none of its files is shown yet
	reload(QString(), false);
}

QString ResourceLibraryModel::rtpPath() const
{
	return m_rtpPath;
}

void ResourceLibraryModel::setRtpPath(const QString& path)
{
	if (path == m_rtpPath) {
		return;
	}
	m_rtpPath = path;
	emit rtpPathChanged();
	reload(QString(), true);
}

bool ResourceLibraryModel::hasGame() const
{
	return !m_gamePath.isEmpty();
}

ResourceFolderModel* ResourceLibraryModel::folders() const
{
	return m_folders;
}

int ResourceLibraryModel::folderCount()
{
	return FolderCount;
}

QString ResourceLibraryModel::folderName(int folder)
{
	const Folder* found = folderAt(folder);
	return found ? QString::fromLatin1(found->name) : QString();
}

int ResourceLibraryModel::folderOf(const QString& name)
{
	for (int i = 0; i < FolderCount; ++i) {
		if (name == QLatin1String(kFolders[i].name)) {
			return i;
		}
	}
	return -1;
}

ResourceLibraryModel::Kind ResourceLibraryModel::folderKind(int folder)
{
	switch (folderType(folder)) {
	case FileType::Music:
		return Kind::Music;
	case FileType::Sound:
		return Kind::Sound;
	case FileType::Video:
		return Kind::Movie;
	default:
		return Kind::Image;
	}
}

FileFinder::FileType ResourceLibraryModel::folderType(int folder)
{
	const Folder* found = folderAt(folder);
	return found ? found->type : FileType::Default;
}

bool ResourceLibraryModel::folderTransparent(int folder)
{
	const Folder* found = folderAt(folder);
	return found && found->transparent;
}

QString ResourceLibraryModel::folderDescription(int folder)
{
	const Folder* found = folderAt(folder);
	return found ? QCoreApplication::translate("ResourceManagerDialog", found->description) : QString();
}

QString ResourceLibraryModel::gameFolderPath(int folder) const
{
	const Folder* found = folderAt(folder);
	if (!found || m_gamePath.isEmpty()) {
		return QString();
	}
	// an existing folder whatever its case (Charset, CHARSET...), else the usual name
	const QString name = QString::fromLatin1(found->name);
	const QString dir = FileFinder::Find(QDir(m_gamePath), name, FileType::Default, QDir::Dirs);
	return dir.isEmpty() ? FileFinder::CombinePath(QDir(m_gamePath).absolutePath(), name) : dir;
}

QString ResourceLibraryModel::rtpFolderPath(int folder) const
{
	const Folder* found = folderAt(folder);
	if (!found || m_rtpPath.isEmpty() || !QDir(m_rtpPath).exists()) {
		return QString();
	}
	return FileFinder::Find(QDir(m_rtpPath), QString::fromLatin1(found->name), FileType::Default, QDir::Dirs);
}

int ResourceLibraryModel::folder() const
{
	return m_folder;
}

void ResourceLibraryModel::setFolder(int folder)
{
	if (!folderAt(folder) || folder == m_folder) {
		return;
	}
	m_folder = folder;
	QSettings().setValue(QLatin1String(FOLDER_KEY), folderName(folder));
	emit folderChanged();
	reload(QString(), false);
}

QString ResourceLibraryModel::folderName() const
{
	return folderName(m_folder);
}

ResourceLibraryModel::Kind ResourceLibraryModel::kind() const
{
	return folderKind(m_folder);
}

QString ResourceLibraryModel::filter() const
{
	return m_filter;
}

void ResourceLibraryModel::setFilter(const QString& text)
{
	if (text == m_filter) {
		return;
	}
	m_filter = text;
	beginResetModel();
	applyFilter();
	endResetModel();
	emit filterChanged();
}

QString ResourceLibraryModel::countText() const
{
	const auto rtp = std::count_if(m_entries.cbegin(), m_entries.cend(), [](const Entry& e) { return e.rtp; });
	QString count = QCoreApplication::translate("ResourceManagerDialog", "Files: %1").arg(m_entries.size());
	if (rtp > 0) {
		count += QStringLiteral(", ") + QCoreApplication::translate("ResourceManagerDialog", "from the RTP: %1").arg(rtp);
	} else if (m_rtpPath.isEmpty()) {
		count += QStringLiteral(", ") + QCoreApplication::translate("ResourceManagerDialog", "no RTP path set");
	}
	return count;
}

void ResourceLibraryModel::refresh()
{
	reload(QString(), true);
}

void ResourceLibraryModel::refresh(const QString& select)
{
	reload(select, select.isEmpty());
}

void ResourceLibraryModel::reload(const QString& select, bool keep)
{
	const QString shown = keep && m_current >= 0 ? m_entries[m_current].name : select;
	m_folders->update();

	beginResetModel();
	m_entries.clear();
	m_current = -1;
	QHash<QString, int> seen;
	const QStringList exts = extensions(folderType(m_folder));

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
				seen.insert(key, static_cast<int>(m_entries.size()));
				m_entries.append({ name, info.absoluteFilePath(), rtp, rank, info.size() });
			} else if (m_entries[*it].rtp == rtp && rank < m_entries[*it].rank) {
				m_entries[*it] = { name, info.absoluteFilePath(), rtp, rank, info.size() };
			}
		}
	};
	if (hasGame()) {
		scan(gameFolderPath(m_folder), false);
	}
	scan(rtpFolderPath(m_folder), true);

	QCollator collator;
	collator.setNumericMode(true);
	collator.setCaseSensitivity(Qt::CaseInsensitive);
	std::sort(m_entries.begin(), m_entries.end(), [&](const Entry& a, const Entry& b) {
		return collator.compare(a.name, b.name) < 0;
	});
	if (!shown.isEmpty()) {
		for (int i = 0; i < m_entries.size(); ++i) {
			if (m_entries[i].name.compare(shown, Qt::CaseInsensitive) == 0) {
				m_current = i;
				break;
			}
		}
	}
	applyFilter();
	endResetModel();
	emit filesChanged();
	updateCurrent();
}

void ResourceLibraryModel::applyFilter()
{
	m_rows.clear();
	const QString text = m_filter.trimmed();
	for (int i = 0; i < m_entries.size(); ++i) {
		if (text.isEmpty() || m_entries[i].name.contains(text, Qt::CaseInsensitive)) {
			m_rows.append(i);
		}
	}
}

int ResourceLibraryModel::currentRow() const
{
	return m_current < 0 ? -1 : static_cast<int>(m_rows.indexOf(m_current));
}

void ResourceLibraryModel::setCurrentRow(int row)
{
	const int current = row >= 0 && row < m_rows.size() ? m_rows[row] : -1;
	if (current == m_current) {
		return;
	}
	m_current = current;
	updateCurrent();
}

bool ResourceLibraryModel::selectName(const QString& name)
{
	for (int i = 0; i < m_entries.size(); ++i) {
		if (m_entries[i].name.compare(name, Qt::CaseInsensitive) == 0) {
			if (i != m_current) {
				m_current = i;
				updateCurrent();
			}
			return true;
		}
	}
	return false;
}

QString ResourceLibraryModel::currentName() const
{
	return m_current < 0 ? QString() : m_entries[m_current].name;
}

QString ResourceLibraryModel::currentPath() const
{
	return m_current < 0 ? QString() : m_entries[m_current].path;
}

bool ResourceLibraryModel::currentIsRtp() const
{
	return m_current >= 0 && m_entries[m_current].rtp;
}

QStringList ResourceLibraryModel::info() const
{
	return m_info;
}

QString ResourceLibraryModel::previewNote() const
{
	return m_previewNote;
}

QImage ResourceLibraryModel::previewImage() const
{
	if (m_image.isNull()) {
		return QImage();
	}
	// palette color 0 transparent in folders where the game makes it so
	return folderTransparent(m_folder)
		? ImageImport::withTransparentIndex0(m_image).convertToFormat(QImage::Format_ARGB32)
		: m_image.convertToFormat(QImage::Format_ARGB32);
}

void ResourceLibraryModel::updateCurrent()
{
	m_image = QImage();
	m_info.clear();
	m_previewNote.clear();
	const QString path = currentPath();
	if (path.isEmpty()) {
		if (!hasGame()) {
			m_info << QCoreApplication::translate("ResourceManagerDialog", "No project is open.");
		}
		emit currentChanged();
		return;
	}

	const Kind shown = kind();
	if (shown == Kind::Image) {
		m_image = ImageImport::loadImage(path);
	}
	m_info = fileInfo(path, currentIsRtp(), m_image);
	if (shown == Kind::Image) {
		if (m_image.isNull()) {
			m_info << QCoreApplication::translate("ResourceManagerDialog", "This image cannot be read.");
		} else {
			if (ImageImport::paletteSize(m_image) > 0 && folderTransparent(m_folder)) {
				m_info << QCoreApplication::translate("ResourceManagerDialog", "Shown as in the game: palette color 0 is transparent.");
			}
			const QString rule = sizeRule(folderName(), m_image.size());
			if (!rule.isEmpty()) {
				m_info << rule;
			}
		}
	} else {
		m_info << QCoreApplication::translate("ResourceManagerDialog", "Format: %1").arg(QFileInfo(path).suffix().toUpper());
		m_previewNote = shown == Kind::Movie
			? QCoreApplication::translate("ResourceManagerDialog", "Movies have no preview.")
			: QCoreApplication::translate("ResourceManagerDialog", "Press Play, or double-click the file, to listen to it.");
	}
	emit currentChanged();
}

QStringList ResourceLibraryModel::extensions(FileFinder::FileType type)
{
	QStringList list = FileFinder::GetFiltersForType(type);
	if (type == FileType::Video) {
		// the movies of RPG Maker 2003
		list << QStringLiteral(".mpg") << QStringLiteral(".mpeg");
	}
	return list;
}

QString ResourceLibraryModel::sizeRule(const QString& folder, QSize size)
{
	// the sizes of RPG Maker 2003's help (Image Specifications)
	auto exact = [&](int w, int h) {
		if (size == QSize(w, h)) {
			return QString();
		}
		return QCoreApplication::translate("ResourceManagerDialog", "RPG Maker's %1 images are %2 x %3 pixels; this one is %4 x %5.")
			.arg(folder).arg(w).arg(h).arg(size.width()).arg(size.height());
	};
	auto range = [&](int min, int max, int step) {
		auto fits = [&](int v) { return v >= min && v <= max && v % step == 0; };
		if (fits(size.width()) && fits(size.height())) {
			return QString();
		}
		const QString rule = step > 1
			? QCoreApplication::translate("ResourceManagerDialog", "RPG Maker's %1 images are %2 to %3 pixels wide and high, in multiples of %4; this one is %5 x %6.")
				.arg(folder).arg(min).arg(max).arg(step).arg(size.width()).arg(size.height())
			: QCoreApplication::translate("ResourceManagerDialog", "RPG Maker's %1 images are %2 to %3 pixels wide and high; this one is %4 x %5.")
				.arg(folder).arg(min).arg(max).arg(size.width()).arg(size.height());
		return rule;
	};

	if (folder == QLatin1String(BACKDROP) || folder == QLatin1String(FRAME)
			|| folder == QLatin1String(GAMEOVER) || folder == QLatin1String(TITLE)) {
		return exact(320, 240);
	} else if (folder == QLatin1String(BATTLE)) {
		return range(96, 480, 96);
	} else if (folder == QLatin1String(BATTLE2)) {
		return range(128, 640, 128);
	} else if (folder == QLatin1String(BATTLECHARSET)) {
		return exact(144, 384);
	} else if (folder == QLatin1String(BATTLEWEAPON)) {
		return exact(192, 512);
	} else if (folder == QLatin1String(CHARSET)) {
		return exact(288, 256);
	} else if (folder == QLatin1String(CHIPSET)) {
		return exact(480, 256);
	} else if (folder == QLatin1String(FACESET)) {
		return exact(192, 192);
	} else if (folder == QLatin1String(MONSTER)) {
		return range(16, 240, 1);
	} else if (folder == QLatin1String(PANORAMA)) {
		return range(80, 640, 1);
	} else if (folder == QLatin1String(PICTURE)) {
		return range(1, 640, 1);
	} else if (folder == QLatin1String(SYSTEM)) {
		return exact(160, 80);
	} else if (folder == QLatin1String(SYSTEM2)) {
		return exact(80, 96);
	}
	return QString();
}

QStringList ResourceLibraryModel::fileInfo(const QString& path, bool rtp, const QImage& image)
{
	const QFileInfo info(path);
	QStringList lines;
	lines << (rtp ? QCoreApplication::translate("ResourceManagerDialog", "RTP file: %1")
		: QCoreApplication::translate("ResourceManagerDialog", "Game file: %1")).arg(info.fileName());
	lines << QCoreApplication::translate("ResourceManagerDialog", "Size: %1").arg(QLocale().formattedDataSize(info.size()));
	if (!image.isNull()) {
		lines << QCoreApplication::translate("ResourceManagerDialog", "%1 x %2 pixels").arg(image.width()).arg(image.height());
		const int colors = ImageImport::paletteSize(image);
		lines << (colors > 0 ? QCoreApplication::translate("ResourceManagerDialog", "Palette image, %1 colors").arg(colors)
			: QCoreApplication::translate("ResourceManagerDialog", "True color image: the game needs the EasyRPG extensions (RPG Maker only uses 256 color images)."));
	}
	return lines;
}

bool ResourceLibraryModel::canImport() const
{
	return hasGame();
}

bool ResourceLibraryModel::canExport() const
{
	return m_current >= 0;
}

bool ResourceLibraryModel::canDelete() const
{
	return hasGame() && m_current >= 0 && !currentIsRtp();
}

bool ResourceLibraryModel::importXyz() const
{
	return m_importXyz;
}

void ResourceLibraryModel::setImportXyz(bool xyz)
{
	if (xyz == m_importXyz) {
		return;
	}
	m_importXyz = xyz;
	QSettings().setValue(QLatin1String(IMPORT_AS_KEY), xyz ? "xyz" : "png");
	emit importXyzChanged();
}

void ResourceLibraryModel::setImportChoices(ResourceImportChoices* choices)
{
	m_choices = choices;
}

void ResourceLibraryModel::setUseTrash(bool trash)
{
	m_useTrash = trash;
}

QString ResourceLibraryModel::importDirectory() const
{
	const QString dir = QSettings().value(QLatin1String(IMPORT_DIR_KEY)).toString();
	if (!dir.isEmpty() && QDir(dir).exists()) {
		return dir;
	}
	const Kind shown = kind();
	const auto location = shown == Kind::Image ? QStandardPaths::PicturesLocation
		: shown == Kind::Movie ? QStandardPaths::MoviesLocation
		: QStandardPaths::MusicLocation;
	return QStandardPaths::writableLocation(location);
}

void ResourceLibraryModel::rememberImportDirectory(const QString& file)
{
	QSettings().setValue(QLatin1String(IMPORT_DIR_KEY), QFileInfo(file).absolutePath());
}

QStringList ResourceLibraryModel::importFilters() const
{
	QStringList patterns;
	for (const QString& ext : extensions(folderType(m_folder))) {
		patterns << QLatin1Char('*') + ext;
	}
	return { QCoreApplication::translate("ResourceManagerDialog", "%1 files (%2)").arg(folderName(), patterns.join(QLatin1Char(' '))),
		QCoreApplication::translate("ResourceManagerDialog", "All files (*)") };
}

QString ResourceLibraryModel::exportDirectory() const
{
	const QString dir = QSettings().value(QLatin1String(EXPORT_DIR_KEY)).toString();
	return dir.isEmpty() || !QDir(dir).exists()
		? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
		: dir;
}

ResourceResult ResourceLibraryModel::createFolder()
{
	if (!hasGame()) {
		return ResourceResult::failed(QCoreApplication::translate("ResourceManagerDialog", "No project is open."));
	}
	// RPG Maker creates the folder when it is missing
	const QString dir = gameFolderPath(m_folder);
	if (!QDir().mkpath(dir)) {
		return ResourceResult::failed(QCoreApplication::translate("ResourceManagerDialog", "The folder %1 cannot be created.")
			.arg(QDir::toNativeSeparators(dir)));
	}
	m_folders->update();
	return ResourceResult::done(folderName());
}

ResourceResult ResourceLibraryModel::importFile(const QString& source)
{
	const ResourceResult folder = createFolder();
	if (!folder.ok) {
		return folder;
	}
	const QString dir = gameFolderPath(m_folder);
	return kind() == Kind::Image ? importImage(source, dir) : importOther(source, dir);
}

QVariantList ResourceLibraryModel::importFiles(const QStringList& sources)
{
	QVariantList results;
	if (sources.isEmpty()) {
		return results;
	}
	rememberImportDirectory(sources.first());
	const ResourceResult folder = createFolder();
	if (!folder.ok) {
		results << QVariant::fromValue(folder);
		return results;
	}
	QString imported;
	for (const QString& source : sources) {
		const ResourceResult result = importFile(source);
		if (result.ok) {
			imported = result.name;
		}
		results << QVariant::fromValue(result);
	}
	refresh(imported.isEmpty() ? currentName() : imported);
	return results;
}

ResourceResult ResourceLibraryModel::makeRoom(const QString& dir, const QString& name)
{
	const QStringList exts = extensions(folderType(m_folder));
	QStringList existing;
	for (const QFileInfo& info : QDir(dir).entryInfoList(QDir::Files)) {
		if (info.completeBaseName().compare(name, Qt::CaseInsensitive) == 0
				&& exts.contains(QLatin1Char('.') + info.suffix().toLower())) {
			existing << info.absoluteFilePath();
		}
	}
	if (existing.isEmpty()) {
		return ResourceResult::done(name);
	}
	if (DialogService::instance().question(QCoreApplication::translate("ResourceManagerDialog", "Import"),
			QCoreApplication::translate("ResourceManagerDialog", "The %1 folder already has a file named %2. Replace it?").arg(folderName(), name))
			!= DialogService::Answer::Yes) {
		return ResourceResult::declined();
	}
	// every extension: the game would otherwise keep using the old .bmp of a new .png
	for (const QString& path : existing) {
		if (!QFile::remove(path)) {
			return ResourceResult::failed(QCoreApplication::translate("ResourceManagerDialog", "%1 cannot be replaced.")
				.arg(QDir::toNativeSeparators(path)));
		}
	}
	return ResourceResult::done(name);
}

ResourceResult ResourceLibraryModel::importOther(const QString& source, const QString& dir)
{
	const QFileInfo info(source);
	const QStringList exts = extensions(folderType(m_folder));
	if (!exts.contains(QLatin1Char('.') + info.suffix().toLower())) {
		return ResourceResult::failed(QCoreApplication::translate("ResourceManagerDialog", "%1 is not a file the game can use in %2 (%3).")
			.arg(info.fileName(), folderName(), exts.join(QStringLiteral(", "))));
	}
	if (isSameDir(info.absolutePath(), dir)) {
		return ResourceResult::skipped(QCoreApplication::translate("ResourceManagerDialog", "%1 is already in the game.").arg(info.fileName()));
	}
	const ResourceResult room = makeRoom(dir, info.completeBaseName());
	if (!room.ok) {
		return room;
	}
	const QString target = FileFinder::CombinePath(dir, info.fileName());
	if (!QFile::copy(source, target)) {
		return ResourceResult::failed(QCoreApplication::translate("ResourceManagerDialog", "%1 cannot be copied to %2.")
			.arg(info.fileName(), QDir::toNativeSeparators(dir)));
	}
	return ResourceResult::done(info.completeBaseName());
}

ResourceResult ResourceLibraryModel::importImage(const QString& source, const QString& dir)
{
	const QFileInfo info(source);
	QImage image = ImageImport::loadImage(source);
	if (image.isNull()) {
		return ResourceResult::failed(QCoreApplication::translate("ResourceManagerDialog", "%1 is not an image the Editor can read (PNG, BMP or XYZ).")
			.arg(info.fileName()));
	}
	if (isSameDir(info.absolutePath(), dir)) {
		return ResourceResult::skipped(QCoreApplication::translate("ResourceManagerDialog", "%1 is already in the game.").arg(info.fileName()));
	}

	const QString title = QCoreApplication::translate("ResourceManagerDialog", "Import");
	const QString rule = sizeRule(folderName(), image.size());
	if (!rule.isEmpty() && DialogService::instance().question(title,
			rule + QStringLiteral("\n\n") + QCoreApplication::translate("ResourceManagerDialog", "Import %1 anyway?").arg(info.fileName()))
			!= DialogService::Answer::Yes) {
		return ResourceResult::declined();
	}

	// The games use palette images: convert when it loses nothing, else ask
	QImage converted = ImageImport::toIndexed(image);
	if (converted.isNull()) {
		ResourceImportChoices fallback;
		ResourceImportChoices& choices = m_choices ? *m_choices : fallback;
		switch (choices.askColors(title, QCoreApplication::translate("ResourceManagerDialog",
				"%1 has more than 256 colors. RPG Maker, and the EasyRPG Player without its extensions, only use images of 256 colors.")
				.arg(info.fileName()))) {
		case ResourceImportChoices::Colors::Reduce:
			converted = ImageImport::reduceColors(image);
			break;
		case ResourceImportChoices::Colors::Keep:
			converted = image.convertToFormat(QImage::Format_ARGB32);
			break;
		case ResourceImportChoices::Colors::Cancel:
			return ResourceResult::declined();
		}
	}
	image = converted;

	if (folderTransparent(m_folder)) {
		image = m_choices ? m_choices->chooseTransparent(image, info.fileName()) : QImage();
		if (image.isNull()) {
			return ResourceResult::declined();
		}
	}

	const bool palette = image.format() == QImage::Format_Indexed8;
	// transparency is palette index 0, not alpha in the palette
	image = withOpaquePalette(image);
	bool xyz = m_importXyz;
	if (xyz && !palette) {
		DialogService::instance().information(title,
			QCoreApplication::translate("ResourceManagerDialog", "%1 is saved as PNG: XYZ images have 256 colors at most.").arg(info.fileName()));
		xyz = false;
	}

	const QString name = info.completeBaseName();
	const ResourceResult room = makeRoom(dir, name);
	if (!room.ok) {
		return room;
	}
	const QString target = FileFinder::CombinePath(dir, name + (xyz ? QStringLiteral(".xyz") : QStringLiteral(".png")));
	const bool saved = xyz ? ImageImport::saveXyz(image, target) : image.save(target, "PNG");
	if (!saved) {
		return ResourceResult::failed(QCoreApplication::translate("ResourceManagerDialog", "%1 cannot be written.")
			.arg(QDir::toNativeSeparators(target)));
	}
	return ResourceResult::done(name);
}

ResourceResult ResourceLibraryModel::exportCurrent(const QString& target_path)
{
	const QString path = currentPath();
	if (path.isEmpty() || target_path.isEmpty()) {
		return ResourceResult::declined();
	}
	const QFileInfo info(path);
	QString target = target_path;
	if (QFileInfo(target).suffix().isEmpty()) {
		target += QLatin1Char('.') + info.suffix();
	}
	QSettings().setValue(QLatin1String(EXPORT_DIR_KEY), QFileInfo(target).absolutePath());
	if (QFileInfo(target).canonicalFilePath() == info.canonicalFilePath()) {
		return ResourceResult::done(info.completeBaseName());
	}
	// the file window asked before replacing the file
	if (QFile::exists(target) && !QFile::remove(target)) {
		return ResourceResult::failed(QCoreApplication::translate("ResourceManagerDialog", "%1 cannot be replaced.")
			.arg(QDir::toNativeSeparators(target)));
	}
	if (!QFile::copy(path, target)) {
		return ResourceResult::failed(QCoreApplication::translate("ResourceManagerDialog", "%1 cannot be written.")
			.arg(QDir::toNativeSeparators(target)));
	}
	return ResourceResult::done(QFileInfo(target).completeBaseName());
}

ResourceResult ResourceLibraryModel::renameCurrent(const QString& new_name)
{
	if (!canDelete()) {
		return ResourceResult::declined();
	}
	const Entry entry = m_entries[m_current];
	const QString name = new_name.trimmed();
	static const QString forbidden = QStringLiteral("/\\:*?\"<>|");
	if (name.isEmpty() || name.startsWith(QLatin1Char('.'))
			|| std::any_of(name.cbegin(), name.cend(), [](QChar c) { return forbidden.contains(c) || c.unicode() < 32; })) {
		return ResourceResult::failed(QCoreApplication::translate("ResourceManagerDialog", "%1 cannot be the name of a file.").arg(name));
	}
	if (name == entry.name) {
		return ResourceResult::done(name);
	}
	// another file of the game with this name: the Player would not know which
	const QFileInfo info(entry.path);
	const QStringList exts = extensions(folderType(m_folder));
	for (const QFileInfo& other : QDir(info.absolutePath()).entryInfoList(QDir::Files)) {
		if (other.completeBaseName().compare(name, Qt::CaseInsensitive) == 0
				&& exts.contains(QLatin1Char('.') + other.suffix().toLower())
				&& other.absoluteFilePath() != info.absoluteFilePath()) {
			return ResourceResult::failed(QCoreApplication::translate("ResourceManagerDialog", "The %1 folder already has a file named %2.")
				.arg(folderName(), name));
		}
	}
	const QString target = FileFinder::CombinePath(info.absolutePath(), name + QLatin1Char('.') + info.suffix());
	emit aboutToRemoveFile(entry.path);
	if (!QFile::rename(entry.path, target)) {
		return ResourceResult::failed(QCoreApplication::translate("ResourceManagerDialog", "%1 cannot be renamed.")
			.arg(QDir::toNativeSeparators(entry.path)));
	}
	refresh(name);
	return ResourceResult::done(name);
}

ResourceResult ResourceLibraryModel::deleteCurrent()
{
	if (!canDelete()) {
		return ResourceResult::declined();
	}
	const Entry entry = m_entries[m_current];
	if (DialogService::instance().question(QCoreApplication::translate("ResourceManagerDialog", "Delete"),
			QCoreApplication::translate("ResourceManagerDialog", "Delete %1 from the %2 folder of the game?")
				.arg(QFileInfo(entry.path).fileName(), folderName()))
			!= DialogService::Answer::Yes) {
		return ResourceResult::declined();
	}
	emit aboutToRemoveFile(entry.path);
	// to the trash when the system has one
	if (!(m_useTrash && QFile::moveToTrash(entry.path)) && !QFile::remove(entry.path)) {
		return ResourceResult::failed(QCoreApplication::translate("ResourceManagerDialog", "%1 cannot be deleted.")
			.arg(QDir::toNativeSeparators(entry.path)));
	}

	// the same line again (an RTP file of the same name shows again), else the next one
	const int row = currentRow();
	int next = -1;
	if (row >= 0 && row + 1 < m_rows.size()) {
		next = m_rows[row + 1];
	} else if (row > 0) {
		next = m_rows[row - 1];
	}
	const QString rtp_dir = rtpFolderPath(m_folder);
	const bool in_rtp = !rtp_dir.isEmpty() && !FileFinder::Find(QDir(rtp_dir), entry.name, folderType(m_folder)).isEmpty();
	refresh(in_rtp || next < 0 ? entry.name : m_entries[next].name);
	return ResourceResult::done(entry.name);
}

QString ResourceLibraryModel::openFolderPath() const
{
	if (m_current >= 0) {
		return QFileInfo(m_entries[m_current].path).absolutePath();
	}
	const QString game = gameFolderPath(m_folder);
	if (!game.isEmpty() && QDir(game).exists()) {
		return game;
	}
	return rtpFolderPath(m_folder);
}

bool ResourceLibraryModel::openFolder() const
{
	const QString path = openFolderPath();
	return !path.isEmpty() && QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}
