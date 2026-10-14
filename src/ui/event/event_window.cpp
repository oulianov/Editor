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

#include "event_window.h"
#include "common/editor_settings.h"
#include "model/event_command_list_model.h"
#include "model/event_editor_controller.h"
#include "model/project.h"
#include "model/project_data.h"
#include "qmlbinding/qml_host.h"
#include <QCryptographicHash>
#include <QQuickWindow>
#include <QSettings>
#include <QSize>
#include <QVariantMap>

namespace {
// the window's size and its page settings' width: Settings > Reset local state forgets them
const char SIZE_KEY[] = "event_dialog_geometry";
const char SETTINGS_WIDTH_KEY[] = "event_page_settings_width";

/** The views of the pages of an event of a game */
QString viewStateKey(ProjectData& project, int map_id, int event_id)
{
	const QString game = project.hasProject() ? project.project().projectDir().absolutePath() : QString();
	return QStringLiteral("event_views/%1/%2/%3").arg(QString::fromLatin1(QCryptographicHash::hash(
		game.toUtf8(), QCryptographicHash::Sha256).toHex())).arg(map_id).arg(event_id);
}
}

void EventWindow::normalizeCommands(EventEditorController& controller)
{
	EventCommandListModel lines;
	for (int i = 0; i < controller.pageCount(); ++i) {
		lines.setCommands(controller.commands(i));
	}
	lines.setCommands(nullptr);
	controller.resetModified();
}

void EventWindow::exec(QWindow* over, ProjectData& project, EventEditorController& controller, const Options& options)
{
	normalizeCommands(controller);
	// a version read again from disk is shown the same way
	const auto reloaded = QObject::connect(&controller, &EventEditorController::eventChanged, &controller, [&controller] {
		normalizeCommands(controller);
	});

	QSettings settings;
	const QString key = viewStateKey(project, controller.mapId(), controller.eventId());
	QVariantMap state = settings.value(key).toMap();
	if (options.page > 0) {
		state.insert(QStringLiteral("page"), options.page - 1);
	}
	QVariantMap properties{
		{QStringLiteral("controller"), QVariant::fromValue(&controller)},
		{QStringLiteral("chipsetId"), options.chipsetId},
		{QStringLiteral("viewState"), state},
		{QStringLiteral("selectLine"), options.line - 1},
		{QStringLiteral("settingsWidth"), settings.value(SETTINGS_WIDTH_KEY, 0).toInt()},
	};
	const QSize size = settings.value(SIZE_KEY).toSize();
	if (size.isValid()) {
		properties.insert(QStringLiteral("savedSize"), size);
	}

	controller.setOpen(true);
	QmlHost::exec(QStringLiteral("EventEditorWindow"), over, properties, [&](QQuickWindow* window) {
		// what it showed, for the next time (read before the window goes)
		QVariant shown;
		QMetaObject::invokeMethod(window, "saveViewState", Q_RETURN_ARG(QVariant, shown));
		QSettings settings;
		settings.setValue(key, shown.toMap());
		settings.setValue(SIZE_KEY, window->size());
		const int width = window->property("settingsWidth").toInt();
		if (width > 0) {
			settings.setValue(SETTINGS_WIDTH_KEY, width);
		}
	});
	controller.setOpen(false);
	QObject::disconnect(reloaded);
}
