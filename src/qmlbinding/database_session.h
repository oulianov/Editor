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
#include <QVariantMap>
#include <QWindow>
#include <QtQmlIntegration>
#include <functional>
#include <memory>
#include <lcf/rpg/database.h>

#include "controller/database_apply.h"
#include "controller/edit_locks.h"
#include "json_t.h"
#include "json_view.h"
#include "model/project_data.h"
#include "project_data_gadget.h"

class DialogService;

/**
 * What a database window edits: a copy of the project's database, put into
 * the project and its file by apply() (OK, Apply), with DatabaseApply: the
 * file changed meanwhile by another program is merged, the conflicts
 * asked.
 *
 *   Ez.DatabaseSession { id: session; projectData: Ez.ProjectData }
 *   ... session.database.list("actors") ...
 *   onAccepted: if (session.apply() === Ez.DatabaseSession.Applied) close()
 */
class DatabaseSession : public QObject {
	Q_OBJECT
	QML_ELEMENT
	/** The project edited (its copy is made when it is set) */
	Q_PROPERTY(ProjectDataGadget* projectData READ projectData WRITE setProjectData NOTIFY projectDataChanged)
	/** The root of the copy */
	Q_PROPERTY(JsonView* database READ database NOTIFY projectDataChanged)
	/** The copy differs from what was read or last applied */
	Q_PROPERTY(bool modified READ modified NOTIFY modifiedChanged)
	/** What the last apply() said (a save that failed) */
	Q_PROPERTY(QString message READ message NOTIFY messageChanged)
	/** The game is made with RPG Maker 2003 (else 2000) */
	Q_PROPERTY(bool rpg2k3 READ isRpg2k3 NOTIFY projectDataChanged)
	/**
	 * A window edits the copy: the project does not read the database
	 * again from disk meanwhile (EditLock::Database); what another program
	 * wrote is merged by apply()
	 */
	Q_PROPERTY(bool editing READ editing WRITE setEditing NOTIFY editingChanged)
	/** The window the questions of apply() are asked over (its own dialogs, see setWindowDialogs) */
	Q_PROPERTY(QWindow* window READ window WRITE setWindow NOTIFY windowChanged)

public:
	enum Status {
		/** In the project and written */
		Applied = static_cast<int>(DatabaseApply::Status::Applied),
		/** Changed on disk and cancelled: nothing changed, the window stays */
		Cancelled = static_cast<int>(DatabaseApply::Status::Cancelled),
		/** Changed on disk and the changes discarded: the window closes, the
		 *  project reads the file again once it did (reloadRequested) */
		Reload = static_cast<int>(DatabaseApply::Status::Reload),
		/** In the project but not written (message) */
		SaveFailed = static_cast<int>(DatabaseApply::Status::SaveFailed),
	};
	Q_ENUM(Status)

	explicit DatabaseSession(QObject* parent = nullptr);
	~DatabaseSession() override;

	ProjectDataGadget* projectData() const {
		return m_gadget;
	}
	void setProjectData(ProjectDataGadget* gadget);
	/** Edits a copy of the project (C++; the file: ProjectController's unless setFile()) */
	void open(ProjectData* live);

	JsonView* database();
	/** The session whose copy json is (a view's json()), nullptr for none */
	static DatabaseSession* owning(const Json* json);
	Json& json() {
		return m_json;
	}
	/** The copy edited */
	ProjectData& copy() {
		return m_copy;
	}
	bool modified() const {
		return m_modified;
	}
	QString message() const {
		return m_message;
	}

	/** OK, Apply: the copy into the project and its file */
	Q_INVOKABLE Status apply();
	/** The copy as the project is now (Cancel of a window that stays) */
	Q_INVOKABLE void discard();

	/**
	 * What a database window showed last time in this project (its page,
	 * its entries...), kept in the settings ("database_views/<SHA-256>");
	 * "active_page", also the page a project not opened yet shows
	 */
	Q_INVOKABLE QVariantMap viewState() const;
	Q_INVOKABLE void setViewState(const QVariantMap& state);

	bool isRpg2k3() const;
	bool editing() const {
		return m_lock.isHeld();
	}
	void setEditing(bool editing);
	QWindow* window() const {
		return m_window;
	}
	void setWindow(QWindow* window);

	/** Makes the dialogs of a window (QML: questions in windows of their own over it) */
	using WindowDialogs = std::function<std::unique_ptr<DialogService>(QWindow*)>;
	/** How the sessions ask over their window (set once by the QML interface) */
	static void setWindowDialogs(WindowDialogs dialogs);

	/** The file of the database (tests); by default ProjectController::databaseFile() */
	void setFile(std::function<DatabaseApply::File()> file);
	/** How the questions are asked (tests); by default DialogService::instance() */
	void setDialogs(DialogService* dialogs);

signals:
	void projectDataChanged();
	void modifiedChanged();
	void messageChanged();
	/** apply() put the copy into the project */
	void applied();
	/** apply() discarded the changes for the file on disk: close, the project reads it */
	void reloadRequested();
	void editingChanged();
	void windowChanged();
	/** The settings forgot the windows' state (Settings, Reset local state): show the first page */
	void viewStateReset();

private:
	void setModified(bool modified);
	void setMessage(const QString& message);
	/** The key of the settings of this project's database windows */
	QString viewStateKey() const;

	ProjectDataGadget* m_gadget = nullptr;
	ProjectData* m_live = nullptr;
	ProjectData m_copy;
	/** The database as read or last applied: the base of the merge with the file */
	lcf::rpg::Database m_base;
	JsonT<lcf::rpg::Database> m_json;
	std::function<DatabaseApply::File()> m_file;
	DialogService* m_dialogs = nullptr;
	QPointer<QWindow> m_window;
	EditLocks::Lock m_lock;
	bool m_modified = false;
	/** The copy changed by the session itself (discard, apply's merge) */
	bool m_quiet = false;
	QString m_message;
};
