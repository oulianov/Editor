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

#include "qml_shell_dialogs.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QPointer>
#include <QQuickWindow>
#include <memory>
#include "core.h"
#include "model/project.h"
#include "qmlbinding/project_windows.h"
#include "ui/maptree/map_tree_dialogs.h"
#include "qmlbinding/qml_dialog_service.h"
#include "qmlbinding/qml_host.h"

QmlShellDialogService::QmlShellDialogService(std::function<QWindow*()> main) :
	m_main(std::move(main))
{
}

std::unique_ptr<DialogService> QmlShellDialogService::front() const
{
	QWindow* over = QmlHost::frontWindow();
	return std::make_unique<QmlDialogService>(over ? over : m_main());
}

StaleChoice QmlShellDialogService::askStale(const QString& what, bool can_reload, const QStringList& conflicts)
{
	return front()->askStale(what, can_reload, conflicts);
}

DialogService::Answer QmlShellDialogService::question(const QString& title, const QString& text, Buttons buttons)
{
	return front()->question(title, text, buttons);
}

bool QmlShellDialogService::confirm(const QString& title, const QString& text, const QString& accept, const QString& reject)
{
	return front()->confirm(title, text, accept, reject);
}

void QmlShellDialogService::information(const QString& title, const QString& text)
{
	front()->information(title, text);
}

void QmlShellDialogService::warning(const QString& title, const QString& text)
{
	front()->warning(title, text);
}

void QmlShellDialogService::error(const QString& title, const QString& text)
{
	front()->error(title, text);
}

QmlShellDialogs::QmlShellDialogs(std::function<QWindow*()> window) :
	m_window(std::move(window))
{
}

std::optional<ShellDialogs::NewProject> QmlShellDialogs::newProject(const QString& def_dir)
{
	const auto chosen = ProjectWindows::newProject(m_window(), def_dir);
	if (!chosen)
		return std::nullopt;
	return NewProject{chosen->folder, chosen->title, chosen->format, chosen->tileSize, chosen->defDir};
}

ShellDialogs::OpenProject QmlShellDialogs::openProject(const QString& def_dir)
{
	const auto chosen = ProjectWindows::openProject(m_window(), def_dir,
		core().project() ? core().project()->projectDir().absolutePath() : QString());
	OpenProject result;
	result.defDir = chosen.defDir;
	result.folder = chosen.folder;
	switch (chosen.choice) {
		case ProjectWindows::OpenProject::Choice::Open:
			result.choice = OpenProject::Choice::Open;
			break;
		case ProjectWindows::OpenProject::Choice::Import:
			result.choice = OpenProject::Choice::Import;
			break;
		case ProjectWindows::OpenProject::Choice::Cancelled:
			result.choice = OpenProject::Choice::Cancelled;
			break;
	}
	return result;
}

ShellDialogs::ImportProject QmlShellDialogs::importProject(const QString& def_dir)
{
	const auto chosen = ProjectWindows::importProject(m_window(), def_dir);
	ImportProject result;
	result.accepted = chosen.accepted;
	result.defDir = chosen.defDir;
	result.folder = chosen.folder;
	result.sourceFolder = chosen.sourceFolder;
	result.convertXyz = chosen.convertXyz;
	return result;
}

void QmlShellDialogs::withImportProgress(const std::function<void(const ProjectController::ImportProgress&)>& work)
{
	// The work runs here, its window (QML) over the main window is drawn
	// between its steps. As QProgressDialog: shown once it has taken a
	// while (a short import shows nothing), then drawn now and then, the
	// keys and clicks waiting until the end
	QPointer<QQuickWindow> progress;
	QVariantMap state;
	QElapsedTimer started;
	QElapsedTimer drawn;
	auto refresh = [&](bool force) {
		if (!started.isValid() || (!progress && started.elapsed() < 500) || (!force && drawn.isValid() && drawn.elapsed() < 50))
			return;
		if (!progress)
			progress = QmlHost::open(QStringLiteral("ImportProgressWindow"), m_window(), state);
		for (auto it = state.cbegin(); progress && it != state.cend(); ++it)
			progress->setProperty(it.key().toUtf8().constData(), it.value());
		QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
		drawn.start();
	};
	ProjectController::ImportProgress shown;
	shown.begin = [&](const QString& label, int maximum) {
		state = {{QStringLiteral("label"), label}, {QStringLiteral("maximum"), maximum}, {QStringLiteral("value"), 0}};
		started.start();
	};
	shown.label = [&](const QString& label) {
		state[QStringLiteral("label")] = label;
		refresh(true);
	};
	shown.maximum = [&](int maximum) {
		state[QStringLiteral("maximum")] = maximum;
		refresh(false);
	};
	shown.value = [&](int value) {
		state[QStringLiteral("value")] = value;
		refresh(false);
	};
	work(shown);
	delete progress.data();
}

std::optional<ProjectConvert::Options> QmlShellDialogs::convertProject(const Project& project)
{
	return ProjectWindows::convertProject(m_window(), project);
}

void QmlShellDialogs::convertResult(const QString& error, const QString& backup)
{
	ProjectWindows::convertResult(m_window(), error, backup);
}

bool QmlShellDialogs::mapProperties(lcf::rpg::MapInfo& info, lcf::rpg::Map& map)
{
	return MapTreeDialogs::mapProperties(m_window(), core().project()->projectData(), info, map);
}

bool QmlShellDialogs::areaProperties(lcf::rpg::MapInfo& info, const QSize& map_size)
{
	return MapTreeDialogs::areaProperties(m_window(), core().project()->projectData(), info, map_size);
}

std::optional<ShellDialogs::Dungeon> QmlShellDialogs::generateDungeon(const lcf::rpg::Map& source, const QString& name)
{
	return MapTreeDialogs::generateDungeon(m_window(), core().project()->projectData(), source, name);
}

std::optional<QPoint> QmlShellDialogs::shiftMap()
{
	return MapTreeDialogs::shiftMap(m_window());
}

void QmlShellDialogs::settings()
{
	QmlHost::exec(QStringLiteral("SettingsWindow"), m_window());
}

void QmlShellDialogs::help(const QString& topic)
{
	QmlHost::help(m_window(), topic);
}

void QmlShellDialogs::about()
{
	// the texts of the About box, in the context MainWindow
	QmlHost::exec(QStringLiteral("MessageWindow"), m_window(), {
		{QStringLiteral("title"), QCoreApplication::translate("MainWindow", "About EasyRPG Editor")},
		{QStringLiteral("text"), QCoreApplication::translate("MainWindow", "<h3>EasyRPG Editor %1</h3>"
			"<p>A game editor for EasyRPG and RPG Maker 2000/2003 projects.</p>"
			"<p>Built with Qt %2.</p>"
			"<p>Free software, licensed under the GNU General Public License "
			"(version 3 or later).</p>"
			"<p><a href=\"https://easyrpg.org/\">EasyRPG website</a></p>")
			.arg(QCoreApplication::applicationVersion(), QString::fromLatin1(qVersion()))},
	});
}
