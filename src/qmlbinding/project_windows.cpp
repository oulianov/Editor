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

#include "project_windows.h"
#include "model/project.h"
#include "model/project_list.h"
#include "model/project_settings.h"
#include "qml_dialog_service.h"
#include "qml_host.h"

#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QQuickWindow>

namespace {

/** The folder of the games as the windows show it: ending with "/" */
QString withSlash(QString folder) {
	if (!folder.isEmpty() && !folder.endsWith(QLatin1Char('/'))) {
		folder += QLatin1Char('/');
	}
	return folder;
}

} // namespace

std::optional<ProjectWindows::NewProject> ProjectWindows::newProject(QWindow* over, const QString& def_dir)
{
	std::optional<NewProject> chosen;
	QmlHost::exec(QStringLiteral("NewProjectWindow"), over, {{QStringLiteral("defDir"), withSlash(def_dir)}},
			[&chosen](QQuickWindow* window) {
		if (window->property("result").toInt() != 1) {
			return;
		}
		NewProject project;
		project.folder = window->property("gameFolder").toString();
		project.title = window->property("gameTitle").toString();
		if (project.title.isEmpty()) {
			project.title = QCoreApplication::translate("NewProjectDialog", "Untitled");
		}
		project.format = window->property("format").toInt();
		project.tileSize = window->property("tileSize").toInt();
		project.defDir = window->property("defDir").toString();
		chosen = project;
	});
	return chosen;
}

ProjectWindows::OpenProject ProjectWindows::openProject(QWindow* over, const QString& def_dir, const QString& open_path)
{
	ProjectListModel projects;
	projects.setOpenPath(open_path);
	projects.setSettingsEditor([](Project& project) {
		// over the Open Project window, modal
		return editSettings(QGuiApplication::modalWindow(), project);
	});
	projects.setFolder(withSlash(def_dir));

	OpenProject chosen;
	const int result = QmlHost::exec(QStringLiteral("OpenProjectWindow"), over,
		{{QStringLiteral("projects"), QVariant::fromValue(&projects)}});
	chosen.defDir = projects.folder();
	// 2: Import... was clicked
	if (result == 2) {
		chosen.choice = OpenProject::Choice::Import;
	} else if (result == 1 && !projects.currentPath().isEmpty()) {
		chosen.choice = OpenProject::Choice::Open;
		chosen.folder = projects.currentPath();
	}
	return chosen;
}

ProjectWindows::ImportProject ProjectWindows::importProject(QWindow* over, const QString& def_dir)
{
	ImportProject chosen;
	chosen.defDir = withSlash(def_dir);
	QmlHost::exec(QStringLiteral("ImportProjectWindow"), over, {{QStringLiteral("defDir"), chosen.defDir}},
			[&chosen](QQuickWindow* window) {
		chosen.accepted = window->property("result").toInt() == 1;
		chosen.defDir = window->property("defDir").toString();
		chosen.folder = window->property("gameFolder").toString();
		chosen.sourceFolder = window->property("sourceFolder").toString();
		chosen.convertXyz = window->property("convertXyz").toBool();
	});
	return chosen;
}

std::optional<ProjectConvert::Options> ProjectWindows::convertProject(QWindow* over, const Project& project)
{
	int maps = 0;
	for (const auto& info : project.treeMap().maps) {
		maps += info.ID > 0 && info.type == lcf::rpg::TreeMap::MapType_map;
	}
	const QString backup = project.projectDir().filePath(ProjectConvert::backupFolderName());
	std::optional<ProjectConvert::Options> options;
	QmlHost::exec(QStringLiteral("ConvertProjectWindow"), over, {
		{QStringLiteral("maps"), maps},
		{QStringLiteral("backupPath"), QDir::toNativeSeparators(backup)},
	}, [&options](QQuickWindow* window) {
		if (window->property("result").toInt() != 1) {
			return;
		}
		ProjectConvert::Options chosen;
		chosen.keep2kBattleSystem = window->property("keepBattle").toBool();
		chosen.keep2kExperience = window->property("keepExperience").toBool();
		options = chosen;
	});
	return options;
}

void ProjectWindows::convertResult(QWindow* over, const QString& error, const QString& backup)
{
	QmlDialogService dialogs(over);
	if (!error.isEmpty()) {
		dialogs.error(QCoreApplication::translate("ConvertProjectDialog", "Convert to RPG Maker 2003"),
			QCoreApplication::translate("ConvertProjectDialog", "The project was not converted.\n\n%1").arg(error));
		return;
	}
	dialogs.information(QCoreApplication::translate("ConvertProjectDialog", "Convert to RPG Maker 2003"),
		QCoreApplication::translate("ConvertProjectDialog", "The project is now an RPG Maker 2003 project.\n\n"
			"The files of RPG Maker 2000 are in:\n%1").arg(QDir::toNativeSeparators(backup)));
}

QString ProjectWindows::editSettings(QWindow* over, Project& project)
{
	// the project is not open: its folder is renamed here, not reopened
	ProjectSettings settings(project, false);
	if (QmlHost::exec(QStringLiteral("ProjectSettingsWindow"), over,
			{{QStringLiteral("settings"), QVariant::fromValue(&settings)}}) != 1) {
		return QString();
	}
	QStringList errors;
	if (const QString error = settings.write(project); !error.isEmpty()) {
		errors << error;
	}
	if (settings.renamesFolder() && settings.renameFolder().isEmpty()) {
		errors << QCoreApplication::translate("OpenProjectDialog", "The folder could not be renamed.");
	}
	return errors.join(QStringLiteral("\n\n"));
}
