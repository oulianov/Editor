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

#include "database_session.h"
#include "common/editor_settings.h"
#include "controller/dialog_service.h"
#include "controller/project_controller.h"
#include "json_view.h"
#include "model/project.h"
#include "project_data_gadget.h"

#include <QCryptographicHash>
#include <QSettings>
#include <QCoreApplication>
#include <QTimer>

namespace {
DatabaseSession::WindowDialogs& windowDialogs() {
	static DatabaseSession::WindowDialogs dialogs;
	return dialogs;
}
}

DatabaseSession::DatabaseSession(QObject* parent) : QObject(parent) {
	m_json.setData(&m_copy.database());
	// its views find the session (owning): the pickers edit its copy
	m_json.setParent(this);
	connect(&m_json, &Json::valueChanged, this, [this] {
		if (!m_quiet) {
			setModified(true);
		}
	});
	connect(&EditorSettings::instance(), &EditorSettings::localStateReset, this, &DatabaseSession::viewStateReset);
}

DatabaseSession::~DatabaseSession() = default;

void DatabaseSession::setProjectData(ProjectDataGadget* gadget) {
	if (m_gadget == gadget) {
		return;
	}
	m_gadget = gadget;
	open(gadget ? gadget->projectData() : nullptr);
}

void DatabaseSession::open(ProjectData* live) {
	m_live = live;
	discard();
	emit projectDataChanged();
}

JsonView* DatabaseSession::database() {
	return m_live ? m_json.view(QString()) : nullptr;
}

DatabaseSession* DatabaseSession::owning(const Json* json) {
	return json ? qobject_cast<DatabaseSession*>(json->parent()) : nullptr;
}

void DatabaseSession::discard() {
	m_quiet = true;
	m_copy = m_live ? *m_live : ProjectData();
	m_base = m_copy.database();
	// the vectors kept their address: the views read the copy again
	m_json.notifyChanged(QString());
	m_quiet = false;
	setModified(false);
	setMessage(QString());
}

DatabaseSession::Status DatabaseSession::apply() {
	if (!m_live) {
		return Cancelled;
	}
	// over the window when it has dialogs of its own (QML), else the interface's
	std::unique_ptr<DialogService> own;
	if (!m_dialogs && m_window && windowDialogs()) {
		own = windowDialogs()(m_window);
	}
	DialogService& dialogs = m_dialogs ? *m_dialogs : own ? *own : DialogService::instance();
	const DatabaseApply::File file = m_file ? m_file() : ProjectController::databaseFile();
	m_quiet = true;
	const auto result = DatabaseApply::apply(*m_live, m_copy, m_base, file, dialogs, this);
	// what was merged from the file is in the copy
	m_json.notifyChanged(QString());
	m_quiet = false;
	setMessage(result.message);
	switch (result.status) {
		case DatabaseApply::Status::Applied:
			setModified(false);
			emit applied();
			break;
		case DatabaseApply::Status::SaveFailed:
			// in the project: the window shows the message
			setModified(false);
			emit applied();
			break;
		case DatabaseApply::Status::Reload: {
			// the window closes first: the reload waits for no window. The
			// window (and this session with it) is gone by then: the timer
			// belongs to the application
			emit reloadRequested();
			const auto reload = file.reload;
			QTimer::singleShot(0, QCoreApplication::instance(), [reload] {
				if (reload) {
					reload();
				}
			});
			break;
		}
		case DatabaseApply::Status::Cancelled:
			break;
	}
	return static_cast<Status>(result.status);
}

void DatabaseSession::setFile(std::function<DatabaseApply::File()> file) {
	m_file = std::move(file);
}

void DatabaseSession::setDialogs(DialogService* dialogs) {
	m_dialogs = dialogs;
}

bool DatabaseSession::isRpg2k3() const {
	return m_json.isRpg2k3();
}

void DatabaseSession::setEditing(bool editing) {
	if (editing == m_lock.isHeld()) {
		return;
	}
	if (editing) {
		m_lock = EditLocks::Lock({EditLock::Database});
	} else {
		m_lock.release();
	}
	emit editingChanged();
}

void DatabaseSession::setWindow(QWindow* window) {
	if (m_window != window) {
		m_window = window;
		emit windowChanged();
	}
}

void DatabaseSession::setWindowDialogs(WindowDialogs dialogs) {
	windowDialogs() = std::move(dialogs);
}

QString DatabaseSession::viewStateKey() const {
	if (!m_live || !m_live->hasProject()) {
		return {};
	}
	// the pages and entries belong to the project
	return QStringLiteral("database_views/%1").arg(QString::fromLatin1(QCryptographicHash::hash(
		m_live->project().projectDir().absolutePath().toUtf8(), QCryptographicHash::Sha256).toHex()));
}

QVariantMap DatabaseSession::viewState() const {
	const QString key = viewStateKey();
	QVariantMap state = key.isEmpty() ? QVariantMap() : QSettings().value(key).toMap();
	// a project not opened yet: the page of the last database window
	if (!state.contains(QStringLiteral("active_page"))) {
		state.insert(QStringLiteral("active_page"), QSettings().value(QStringLiteral("database_page"), 0).toInt());
	}
	return state;
}

void DatabaseSession::setViewState(const QVariantMap& state) {
	const QString key = viewStateKey();
	if (key.isEmpty()) {
		return;
	}
	// the keys not given stay (those older versions saved too)
	QVariantMap kept = QSettings().value(key).toMap();
	for (auto it = state.cbegin(); it != state.cend(); ++it) {
		kept.insert(it.key(), it.value());
	}
	QSettings().setValue(key, kept);
	if (state.contains(QStringLiteral("active_page"))) {
		QSettings().setValue(QStringLiteral("database_page"), state.value(QStringLiteral("active_page")));
	}
}

void DatabaseSession::setModified(bool modified) {
	if (m_modified != modified) {
		m_modified = modified;
		emit modifiedChanged();
	}
}

void DatabaseSession::setMessage(const QString& message) {
	if (m_message != message) {
		m_message = message;
		emit messageChanged();
	}
}
