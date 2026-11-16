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

#include <QAbstractTableModel>
#include <QList>
#include <QPointer>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <functional>
#include <memory>
#include "model/project.h"

template<typename T> class QFutureWatcher;

/**
 * Game Project > Open Project, without its window: the projects of the
 * folder of the games (one row each: its folder, its title, its profile:
 * its engine and its add-ons, GameProfile), listed in the background, and
 * what can be done with them there (Delete).
 *
 * The project open in the Editor (openPath) is not changed from there. The
 * texts are in the context OpenProjectDialog, as OpenProjectWindow.qml's.
 */
class ProjectListModel : public QAbstractTableModel
{
	Q_OBJECT
	/** The folder of the games */
	Q_PROPERTY(QString folder READ folder WRITE setFolder NOTIFY folderChanged)
	/** The folder of the games is a folder (a path typed may be none: nothing listed) */
	Q_PROPERTY(bool folderExists READ folderExists NOTIFY folderExistsChanged)
	/** The projects are being listed */
	Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
	Q_PROPERTY(int currentRow READ currentRow WRITE setCurrentRow NOTIFY currentRowChanged)
	/** The folder of the project selected, empty when none */
	Q_PROPERTY(QString currentPath READ currentPath NOTIFY currentRowChanged)
	/** The project open in the Editor, empty when none */
	Q_PROPERTY(QString openPath READ openPath WRITE setOpenPath NOTIFY openPathChanged)

public:
	enum Column { FolderColumn, TitleColumn, ProfileColumn, ColumnCount };
	enum Role {
		/** The name of its folder (DisplayRole of the first column) */
		FolderRole = Qt::UserRole + 1,
		/** The title of the game (DisplayRole of the second column) */
		TitleRole,
		/** The folder of the project */
		PathRole,
		/** Its profile: the chips of GameProfile::chipsOf, read from its files */
		ProfileRole,
		/** Their texts (DisplayRole of the third column), joined by commas */
		ProfileTextRole,
	};

	explicit ProjectListModel(QObject* parent = nullptr);
	~ProjectListModel() override;

	// QAbstractItemModel
	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	int columnCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
	QHash<int, QByteArray> roleNames() const override;

	QString folder() const;
	/** Lists the projects of another folder (in the background) */
	void setFolder(const QString& folder);
	bool folderExists() const;
	bool busy() const;
	/** Lists the projects again (none when the folder does not exist); the first one is selected */
	Q_INVOKABLE void refresh();

	int currentRow() const;
	void setCurrentRow(int row);
	QString currentPath() const;
	/** The project of a row, nullptr out of the list */
	std::shared_ptr<Project> project(int row) const;

	QString openPath() const;
	void setOpenPath(const QString& path);
	/** The project of the row is the one open in the Editor */
	Q_INVOKABLE bool isOpen(int row) const;
	/** Why the project of the row cannot be changed here (the one open), else empty */
	Q_INVOKABLE QString openNotice(int row) const;

	/**
	 * Moves the project of the row and all its files to the trash, after
	 * asking (DialogService; No is the safe answer), then lists the projects
	 * again: why it could not, else empty
	 */
	Q_INVOKABLE QString trash(int row);
	/** Deleted projects go to the trash of the system (tests: removed) */
	void setUseTrash(bool trash);

	/**
	 * Game Project > Project Settings of a project of the list, by the
	 * interface: its errors, empty when it worked or was cancelled
	 */
	using SettingsEditor = std::function<QString(Project& project)>;
	void setSettingsEditor(SettingsEditor editor);
	/** The settings of the project of the row (not the one open), then the projects listed again: what went wrong */
	Q_INVOKABLE QString editSettings(int row);

	/** An RPG Maker 2000 or 2003 project is in this folder (what Import copies) */
	Q_INVOKABLE bool isLegacyProject(const QString& folder) const;

signals:
	void folderChanged();
	void folderExistsChanged();
	void busyChanged();
	void currentRowChanged();
	void openPathChanged();
	/** The projects were listed */
	void listed();

private:
	/** What the listing reads in the background: the projects, and the setup of each (GameProfile::read) */
	struct Listing {
		Project::ProjectList projects;
		QList<QVariantMap> setups;
	};
	static Listing list(const QDir& folder);

	Project::ProjectList m_projects;
	/** The chips of the profile of each project */
	QList<QVariantList> m_profiles;
	QString m_folder;
	bool m_folderExists = false;
	QString m_openPath;
	int m_current = -1;
	bool m_useTrash = true;
	SettingsEditor m_settingsEditor;
	/** The listing running, the last one asked for */
	QPointer<QFutureWatcher<Listing>> m_scan;
};
