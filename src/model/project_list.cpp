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

#include "project_list.h"
#include "common/filefinder.h"
#include "controller/dialog_service.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFutureWatcher>
#include <QtConcurrentRun>

ProjectListModel::ProjectListModel(QObject* parent) : QAbstractTableModel(parent)
{
}

ProjectListModel::~ProjectListModel() = default;

int ProjectListModel::rowCount(const QModelIndex& parent) const
{
	return parent.isValid() ? 0 : static_cast<int>(m_projects.size());
}

int ProjectListModel::columnCount(const QModelIndex& parent) const
{
	return parent.isValid() ? 0 : ColumnCount;
}

QVariant ProjectListModel::data(const QModelIndex& index, int role) const
{
	if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
		return QVariant();
	}
	const Project& project = *m_projects[static_cast<size_t>(index.row())];
	if (role == Qt::DisplayRole) {
		role = index.column() == TitleColumn ? TitleRole : FolderRole;
	}
	switch (role) {
	case FolderRole:
		return project.projectDir().dirName();
	case TitleRole:
		return project.gameTitle();
	case PathRole:
	case Qt::ToolTipRole:
		return QDir::toNativeSeparators(project.projectDir().absolutePath());
	default:
		return QVariant();
	}
}

QVariant ProjectListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
		return QVariant();
	}
	return section == TitleColumn ? QCoreApplication::translate("OpenProjectDialog", "Game Title")
		: QCoreApplication::translate("OpenProjectDialog", "Folder");
}

QHash<int, QByteArray> ProjectListModel::roleNames() const
{
	QHash<int, QByteArray> names = QAbstractTableModel::roleNames();
	names.insert(FolderRole, "folder");
	names.insert(TitleRole, "title");
	names.insert(PathRole, "path");
	return names;
}

QString ProjectListModel::folder() const
{
	return m_folder;
}

void ProjectListModel::setFolder(const QString& folder)
{
	if (folder == m_folder) {
		return;
	}
	m_folder = folder;
	emit folderChanged();
	refresh();
}

bool ProjectListModel::busy() const
{
	return m_scan != nullptr;
}

void ProjectListModel::refresh()
{
	beginResetModel();
	m_projects.clear();
	endResetModel();
	setCurrentRow(-1);

	const bool was_busy = busy();
	auto* scan = new QFutureWatcher<Project::ProjectList>(this);
	// another folder can be asked for before this one is listed: its list only
	m_scan = scan;
	connect(scan, &QFutureWatcher<Project::ProjectList>::finished, this, [this, scan] {
		scan->deleteLater();
		if (m_scan != scan) {
			return;
		}
		m_scan = nullptr;
		beginResetModel();
		m_projects = scan->result();
		endResetModel();
		setCurrentRow(m_projects.empty() ? -1 : 0);
		emit busyChanged();
		emit listed();
	});
	// only the files are read in the background
	scan->setFuture(QtConcurrent::run(&Project::enumerate, QDir(m_folder)));
	if (!was_busy) {
		emit busyChanged();
	}
}

int ProjectListModel::currentRow() const
{
	return m_current;
}

void ProjectListModel::setCurrentRow(int row)
{
	if (row < -1 || row >= rowCount()) {
		row = -1;
	}
	if (row != m_current) {
		m_current = row;
		emit currentRowChanged();
	}
}

QString ProjectListModel::currentPath() const
{
	const auto current = project(m_current);
	return current ? current->projectDir().absolutePath() : QString();
}

std::shared_ptr<Project> ProjectListModel::project(int row) const
{
	if (row < 0 || row >= rowCount()) {
		return nullptr;
	}
	return m_projects[static_cast<size_t>(row)];
}

QString ProjectListModel::openPath() const
{
	return m_openPath;
}

void ProjectListModel::setOpenPath(const QString& path)
{
	if (path != m_openPath) {
		m_openPath = path;
		emit openPathChanged();
	}
}

bool ProjectListModel::isOpen(int row) const
{
	const auto chosen = project(row);
	return chosen && !m_openPath.isEmpty() && QDir(m_openPath) == QDir(chosen->projectDir().absolutePath());
}

QString ProjectListModel::openNotice(int row) const
{
	if (!isOpen(row)) {
		return QString();
	}
	return QCoreApplication::translate("OpenProjectDialog",
		"%1 is the project open in the Editor: change it with Game Project > Project Settings, "
		"or close it to delete it.").arg(project(row)->projectDir().dirName());
}

QString ProjectListModel::trash(int row)
{
	const auto chosen = project(row);
	if (!chosen || isOpen(row)) {
		return openNotice(row);
	}
	const QString path = chosen->projectDir().absolutePath();
	if (DialogService::instance().question(QCoreApplication::translate("OpenProjectDialog", "Open existing Project"),
			QCoreApplication::translate("OpenProjectDialog", "Move the project %1 (%2) and all its files to the trash?")
				.arg(chosen->gameTitle(), QDir::toNativeSeparators(path))) != DialogService::Answer::Yes) {
		return QString();
	}
	// to the trash, never erased: RPG Maker asks the same, the files stay recoverable
	const bool gone = m_useTrash ? QFile::moveToTrash(path) : QDir(path).removeRecursively();
	refresh();
	return gone ? QString() : QCoreApplication::translate("OpenProjectDialog", "%1 could not be moved to the trash.")
		.arg(QDir::toNativeSeparators(path));
}

void ProjectListModel::setUseTrash(bool trash)
{
	m_useTrash = trash;
}

void ProjectListModel::setSettingsEditor(SettingsEditor editor)
{
	m_settingsEditor = std::move(editor);
}

QString ProjectListModel::editSettings(int row)
{
	const auto chosen = project(row);
	if (!chosen || isOpen(row)) {
		return openNotice(row);
	}
	if (!m_settingsEditor) {
		return QString();
	}
	// the title and the code page as the game is read
	chosen->detectEncoding();
	const QString error = m_settingsEditor(*chosen);
	// its folder or its title may have changed
	refresh();
	return error;
}

bool ProjectListModel::isLegacyProject(const QString& folder) const
{
	return !folder.isEmpty() && FileFinder::IsRPG2kProject(QDir(folder));
}
