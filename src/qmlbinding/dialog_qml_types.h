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

// The models of the dialogs of the Editor in the QML module, registered
// from here: editor_core does not link QtQml. The windows are given them.

#include <QObject>
#include <QtQmlIntegration>
#include "model/export_controller.h"
#include "model/project_search.h"
#include "model/project_settings.h"
#include "model/search_results_model.h"
#include "model/search_suggestions.h"
#include "model/test_play_options.h"

/** The search of the project (Search window) */
struct ProjectSearchForeign
{
	Q_GADGET
	QML_FOREIGN(ProjectSearch)
	QML_NAMED_ELEMENT(ProjectSearch)
	QML_UNCREATABLE("A search is made for a project")
};

struct SearchResultsModelForeign
{
	Q_GADGET
	QML_FOREIGN(SearchResultsModel)
	QML_NAMED_ELEMENT(SearchResultsModel)
	QML_UNCREATABLE("The results are a search's")
};

struct SearchSuggestionsForeign
{
	Q_GADGET
	QML_FOREIGN(SearchSuggestions)
	QML_NAMED_ELEMENT(SearchSuggestions)
	QML_UNCREATABLE("The suggestions are a search's")
};

/** Export Game */
struct ExportControllerForeign
{
	Q_GADGET
	QML_FOREIGN(ExportController)
	QML_NAMED_ELEMENT(ExportController)
	QML_UNCREATABLE("An export is made for a project")
};

struct ExportPlatformModelForeign
{
	Q_GADGET
	QML_FOREIGN(ExportPlatformModel)
	QML_NAMED_ELEMENT(ExportPlatformModel)
	QML_UNCREATABLE("The platforms are an export's")
};

/** Project Settings */
struct ProjectSettingsForeign
{
	Q_GADGET
	QML_FOREIGN(ProjectSettings)
	QML_NAMED_ELEMENT(ProjectSettings)
	QML_UNCREATABLE("The settings are a project's")
};

/** Test Play Options */
struct TestPlayOptionsForeign
{
	Q_GADGET
	QML_FOREIGN(TestPlayOptions)
	QML_NAMED_ELEMENT(TestPlayOptions)
	QML_UNCREATABLE("The options are a project's")
};
