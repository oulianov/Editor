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

#include "action_catalog.h"
#include <QCoreApplication>
#include <QHash>
#include <initializer_list>

namespace {

using Group = ActionCatalog::Group;
using MenuRole = ActionCatalog::MenuRole;

struct Info {
	const char* name;
	const char* text;
	const char* toolTip = nullptr;
	const char* statusTip = nullptr;
	const char* iconText = nullptr;
	const char* icon = nullptr;
	Group group = Group::None;
	bool checkable = false;
	bool checked = false;
	bool visible = true;
	MenuRole role = MenuRole::None;
};

// The texts in the context "MainWindow": the translations keep matching
const Info actions[] = {
	{.name = "actionProjectOpen", .text = QT_TRANSLATE_NOOP("MainWindow", "&Open Project"),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "Open an existing Game Project"), .icon = "project-open"},
	{.name = "actionProjectNew", .text = QT_TRANSLATE_NOOP("MainWindow", "&New Project"),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "Create a new Game Project"), .icon = "project-new"},
	{.name = "actionProjectClose", .text = QT_TRANSLATE_NOOP("MainWindow", "&Close Project"),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "Close current opened Game Project"), .icon = "project-close"},
	{.name = "actionGameDiskCreate", .text = QT_TRANSLATE_NOOP("MainWindow", "Export &Game..."),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "Export the game for Windows, macOS, Linux and the web (RPG Maker's Create Gamedisk)"),
		.icon = "export"},
	{.name = "actionQuit", .text = QT_TRANSLATE_NOOP("MainWindow", "&Quit"),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "Quit EasyRPG Editor"), .icon = "exit", .role = MenuRole::Quit},
	{.name = "actionMapSave", .text = QT_TRANSLATE_NOOP("MainWindow", "&Save Map"),
		.toolTip = QT_TRANSLATE_NOOP("MainWindow", "Save current Map"),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "Save all changes done in current Map"), .icon = "save"},
	{.name = "actionMapRevert", .text = QT_TRANSLATE_NOOP("MainWindow", "&Revert Map"),
		.toolTip = QT_TRANSLATE_NOOP("MainWindow", "Revert Map: discard the unsaved changes of the current map"),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "Discard every change made to the current map since it was last saved: it is read again from its file"),
		.icon = "revert"},
	{.name = "actionLayerLower", .text = QT_TRANSLATE_NOOP("MainWindow", "&Lower Layer"),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "Edit lower layer mode"), .icon = "layer-low",
		.group = Group::Layer, .checkable = true},
	{.name = "actionLayerUpper", .text = QT_TRANSLATE_NOOP("MainWindow", "&Upper Layer"),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "Edit upper layer mode"), .icon = "layer-high",
		.group = Group::Layer, .checkable = true},
	{.name = "actionLayerEvents", .text = QT_TRANSLATE_NOOP("MainWindow", "&Events"),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "Edit events mode"), .icon = "layer-event",
		.group = Group::Layer, .checkable = true},
	{.name = "actionZoom100", .text = QT_TRANSLATE_NOOP("MainWindow", "&Normal Size (1/2)"),
		.toolTip = QT_TRANSLATE_NOOP("MainWindow", "Normal size (1/2)"),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "Back to the normal size of the map, as maps open (1/2)"),
		.icon = "zoom-original"},
	{.name = "actionZoomIn", .text = QT_TRANSLATE_NOOP("MainWindow", "Zoom &In"),
		.toolTip = QT_TRANSLATE_NOOP("MainWindow", "Zoom In"),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "Zoom In"), .icon = "zoom-in"},
	{.name = "actionZoomOut", .text = QT_TRANSLATE_NOOP("MainWindow", "Zoom &Out"),
		.toolTip = QT_TRANSLATE_NOOP("MainWindow", "Zoom Out"),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "Zoom Out"), .icon = "zoom-out"},
	{.name = "actionDatabase", .text = QT_TRANSLATE_NOOP("MainWindow", "Database"),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "Open Database editor"), .icon = "database"},
	{.name = "actionResourceManager", .text = QT_TRANSLATE_NOOP("MainWindow", "Resource Manager"),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "Open Resource Manager"), .icon = "resources"},
	{.name = "actionJukebox", .text = QT_TRANSLATE_NOOP("MainWindow", "Jukebox"),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "Play music while you work"), .icon = "audio"},
	{.name = "actionSearch", .text = QT_TRANSLATE_NOOP("MainWindow", "Search"),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "Search inside your Game Project"), .icon = "edit-find"},
	{.name = "actionPlayTest", .text = QT_TRANSLATE_NOOP("MainWindow", "&Test Play"),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "Test your Game"), .icon = "play"},
	{.name = "actionFullScreenToggle", .text = QT_TRANSLATE_NOOP("MainWindow", "&Full Screen"),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "Test game in fullscreen mode"), .icon = "fullscreen",
		.checkable = true},
	{.name = "actionTitleBackgroundToggle", .text = QT_TRANSLATE_NOOP("MainWindow", "Show &Title Background"),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "Show title screen's background on Play Test"), .icon = "show-title",
		.checkable = true},
	{.name = "actionContents", .text = QT_TRANSLATE_NOOP("MainWindow", "&Contents"),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "View Help contents"), .icon = "help-contents"},
	{.name = "actionAbout", .text = QT_TRANSLATE_NOOP("MainWindow", "&About EasyRPG Editor"),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "About EasyRPG Editor"), .icon = "help-about",
		.role = MenuRole::About},
	// its text follows the updates (EditorUpdater): Install Update...
	{.name = "actionCheckUpdates", .text = QT_TRANSLATE_NOOP("MainWindow", "Check for &Updates..."),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "Look for a newer version of EasyRPG Editor, and install it"),
		.role = MenuRole::Updates},
	// not written yet
	{.name = "actionScriptEditor", .text = QT_TRANSLATE_NOOP("MainWindow", "&Script Editor"),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "Open Script Editor"), .icon = "code", .visible = false},
	{.name = "actionPaletteToggle", .text = QT_TRANSLATE_NOOP("MainWindow", "Palette"),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "Show/Hide palette window"), .icon = "palette",
		.checkable = true, .checked = true},
	{.name = "actionMapTreeToggle", .text = QT_TRANSLATE_NOOP("MainWindow", "Map Tree"),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "Show/Hide Map Tree window"), .icon = "tree",
		.checkable = true, .checked = true},
	{.name = "actionUndo", .text = QT_TRANSLATE_NOOP("MainWindow", "Undo"),
		.toolTip = QT_TRANSLATE_NOOP("MainWindow", "Undo last action"),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "Undo the last action"), .icon = "edit-undo"},
	{.name = "actionRedo", .text = QT_TRANSLATE_NOOP("MainWindow", "Redo"),
		.toolTip = QT_TRANSLATE_NOOP("MainWindow", "Redo the last action undone"),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "Redo the last action undone"), .icon = "edit-redo"},
	{.name = "actionMapSaveAll", .text = QT_TRANSLATE_NOOP("MainWindow", "Save All Maps"),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "Save the changes of every open map")},
	{.name = "actionProjectSettings", .text = QT_TRANSLATE_NOOP("MainWindow", "Project Settings..."),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "The folder, title, engine, text encoding and patches of the game"),
		.icon = "settings"},
	{.name = "actionProjectConvert2003", .text = QT_TRANSLATE_NOOP("MainWindow", "Convert to RPG Maker 2003..."),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "Convert this RPG Maker 2000 project to the format of RPG Maker 2003")},
	{.name = "actionSettings", .text = QT_TRANSLATE_NOOP("MainWindow", "Settings..."),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "Language, paths, reloading of changed files and appearance of the Editor"),
		.role = MenuRole::Preferences},
	{.name = "actionDrawSelect", .text = QT_TRANSLATE_NOOP("MainWindow", "Select"),
		.toolTip = QT_TRANSLATE_NOOP("MainWindow", "Select map elements"),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "Select map elements"), .icon = "select-area",
		.group = Group::Tool, .checkable = true},
	{.name = "actionZoom", .text = QT_TRANSLATE_NOOP("MainWindow", "Zoom"),
		.toolTip = QT_TRANSLATE_NOOP("MainWindow", "Zoom in and out"),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "Zoom using the mouse buttons"), .icon = "zoom",
		.group = Group::Tool, .checkable = true},
	{.name = "actionDrawPen", .text = QT_TRANSLATE_NOOP("MainWindow", "Draw"),
		.toolTip = QT_TRANSLATE_NOOP("MainWindow", "Draw in the map with a pen"),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "Place map elements freehand"), .icon = "draw-pen",
		.group = Group::Tool, .checkable = true},
	{.name = "actionDrawRectangle", .text = QT_TRANSLATE_NOOP("MainWindow", "Rectangle"),
		.toolTip = QT_TRANSLATE_NOOP("MainWindow", "Draw in the map with a rectangle tool"),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "Place map elements in a rectangular pattern"), .icon = "draw-rectangle",
		.group = Group::Tool, .checkable = true},
	{.name = "actionDrawCircle", .text = QT_TRANSLATE_NOOP("MainWindow", "Circle"),
		.toolTip = QT_TRANSLATE_NOOP("MainWindow", "Draw in a map with a circle tool"),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "Place map elements in an elliptical pattern"), .icon = "draw-circle",
		.group = Group::Tool, .checkable = true},
	{.name = "actionDrawFill", .text = QT_TRANSLATE_NOOP("MainWindow", "Fill"),
		.toolTip = QT_TRANSLATE_NOOP("MainWindow", "Draw in the map with a fill tool"),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "Flood fill map elements"), .icon = "draw-fill",
		.group = Group::Tool, .checkable = true},
	{.name = "actionProjectImport", .text = QT_TRANSLATE_NOOP("MainWindow", "&Import Project to Workspace...")},
	{.name = "actionMapProperties", .text = QT_TRANSLATE_NOOP("MainWindow", "Map Properties"),
		.toolTip = QT_TRANSLATE_NOOP("MainWindow", "Edit map properties"),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "Edit map properties"), .icon = "properties"},
	{.name = "actionMapNew", .text = QT_TRANSLATE_NOOP("MainWindow", "New Map"),
		.toolTip = QT_TRANSLATE_NOOP("MainWindow", "Create a new map and add it as child of the current map."),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "Create a new map and add it as child of the current map."), .icon = "new"},
	{.name = "actionMapCopy", .text = QT_TRANSLATE_NOOP("MainWindow", "Copy Map"),
		.toolTip = QT_TRANSLATE_NOOP("MainWindow", "Copy selected map."),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "Copy selected map."), .icon = "edit-copy"},
	{.name = "actionMapPaste", .text = QT_TRANSLATE_NOOP("MainWindow", "Paste Map"),
		.toolTip = QT_TRANSLATE_NOOP("MainWindow", "Paste current copied map as children of the selected map."),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "Paste current copied map as children of the selected map."),
		.icon = "edit-paste"},
	{.name = "actionMapDelete", .text = QT_TRANSLATE_NOOP("MainWindow", "Delete Map"),
		.toolTip = QT_TRANSLATE_NOOP("MainWindow", "Delete selected map."),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "Delete selected map."), .icon = "edit-delete"},
	{.name = "actionAreaNew", .text = QT_TRANSLATE_NOOP("MainWindow", "Create Area"),
		.toolTip = QT_TRANSLATE_NOOP("MainWindow", "Create an area (a region with its own encounters) on the selected map."),
		.icon = "area-new"},
	{.name = "actionAreaProperties", .text = QT_TRANSLATE_NOOP("MainWindow", "Area Properties"), .icon = "properties"},
	{.name = "actionAreaDelete", .text = QT_TRANSLATE_NOOP("MainWindow", "Delete Area"), .icon = "edit-delete"},
	{.name = "actionMapGenerateDungeon", .text = QT_TRANSLATE_NOOP("MainWindow", "Generate Dungeon..."),
		.toolTip = QT_TRANSLATE_NOOP("MainWindow", "Create a new map with the dungeon generator settings of the selected map.")},
	{.name = "actionMapShift", .text = QT_TRANSLATE_NOOP("MainWindow", "Shift Map"),
		.toolTip = QT_TRANSLATE_NOOP("MainWindow", "Move all the tiles and events of the selected map."), .icon = "map-shift"},
	{.name = "actionMapOpenStart", .text = QT_TRANSLATE_NOOP("MainWindow", "Open Start Map"),
		.toolTip = QT_TRANSLATE_NOOP("MainWindow", "Open the map of the party's start position"),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "Open the map where the game starts: the map of the party's start position"),
		.icon = "place-start"},
	{.name = "actionMapGoTo", .text = QT_TRANSLATE_NOOP("MainWindow", "Go to Map..."),
		.toolTip = QT_TRANSLATE_NOOP("MainWindow", "Open a map by its number or its name"),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "Open a map by its number or its name")},
	{.name = "actionPlayTestOptions", .text = QT_TRANSLATE_NOOP("MainWindow", "Test Play &Options..."),
		.toolTip = QT_TRANSLATE_NOOP("MainWindow", "Test the game with a play mode, start position or party")},
	{.name = "actionZoom1_1", .text = QT_TRANSLATE_NOOP("MainWindow", "1/1 Ratio"),
		.toolTip = QT_TRANSLATE_NOOP("MainWindow", "1/1 Ratio"),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "Edit at a 1/1 view ratio"),
		.iconText = QT_TRANSLATE_NOOP("MainWindow", "1/1"), .icon = "zoom-ratio-1", .group = Group::Zoom, .checkable = true},
	{.name = "actionZoom1_2", .text = QT_TRANSLATE_NOOP("MainWindow", "1/2 Ratio"),
		.toolTip = QT_TRANSLATE_NOOP("MainWindow", "1/2 Ratio"),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "Edit at a 1/2 view ratio"),
		.iconText = QT_TRANSLATE_NOOP("MainWindow", "1/2"), .icon = "zoom-ratio-2", .group = Group::Zoom, .checkable = true},
	{.name = "actionZoom1_4", .text = QT_TRANSLATE_NOOP("MainWindow", "1/4 Ratio"),
		.toolTip = QT_TRANSLATE_NOOP("MainWindow", "1/4 Ratio"),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "Edit at a 1/4 view ratio"),
		.iconText = QT_TRANSLATE_NOOP("MainWindow", "1/4"), .icon = "zoom-ratio-4", .group = Group::Zoom, .checkable = true},
	{.name = "actionZoom1_8", .text = QT_TRANSLATE_NOOP("MainWindow", "1/8 Ratio"),
		.toolTip = QT_TRANSLATE_NOOP("MainWindow", "1/8 Ratio"),
		.statusTip = QT_TRANSLATE_NOOP("MainWindow", "Edit at a 1/8 view ratio"),
		.iconText = QT_TRANSLATE_NOOP("MainWindow", "1/8"), .icon = "zoom-ratio-8", .group = Group::Zoom, .checkable = true},
};

struct MenuInfo {
	const char* name;
	const char* title;
	std::initializer_list<const char*> items;
};

// "": a separator; "menu:": a submenu
const MenuInfo menu_infos[] = {
	{"menuGame", QT_TRANSLATE_NOOP("MainWindow", "Game &Project"), {"actionProjectNew", "actionProjectOpen",
		"actionProjectClose", "", "actionProjectSettings", "actionGameDiskCreate", "actionProjectConvert2003", "",
		"actionProjectImport", "", "actionQuit"}},
	{"menuMaps", QT_TRANSLATE_NOOP("MainWindow", "Maps"), {"actionUndo", "actionRedo", "", "actionMapOpenStart",
		"actionMapGoTo", "", "actionMapSave", "actionMapSaveAll", "actionMapRevert", "", "actionLayerLower",
		"actionLayerUpper", "actionLayerEvents", "",
		"menu:menuMapZoom", "actionZoom100", "actionZoomIn", "actionZoomOut"}},
	{"menuView", QT_TRANSLATE_NOOP("MainWindow", "View"), {"actionPaletteToggle", "actionMapTreeToggle"}},
	{"menuTools", QT_TRANSLATE_NOOP("MainWindow", "Tools"), {"actionDatabase", "actionResourceManager",
		"actionJukebox", "actionSearch", "actionScriptEditor", "", "actionSettings"}},
	{"menuGame2", QT_TRANSLATE_NOOP("MainWindow", "Game"), {"actionPlayTest", "actionPlayTestOptions", "",
		"actionFullScreenToggle", "actionTitleBackgroundToggle"}},
	{"menuHelp", QT_TRANSLATE_NOOP("MainWindow", "Help"), {"actionContents", "", "actionAbout", "actionCheckUpdates"}},
	// submenus
	{"menuMapZoom", QT_TRANSLATE_NOOP("MainWindow", "Map &Zoom"), {"actionZoom1_1", "actionZoom1_2",
		"actionZoom1_4", "actionZoom1_8"}},
	// the right-click menus of the map tree, as RPG Maker's: on a map, an
	// area, the project
	{"menuTreeMap", nullptr, {"actionMapProperties", "", "actionMapNew", "actionAreaNew", "actionMapGenerateDungeon",
		"", "actionMapCopy", "actionMapPaste", "actionMapDelete", "", "actionMapShift"}},
	{"menuTreeArea", nullptr, {"actionAreaProperties", "", "actionAreaDelete"}},
	{"menuTreeProject", nullptr, {"actionMapNew", "", "actionMapPaste"}},
};
constexpr int menu_bar_count = 6;

struct ToolBarInfo {
	const char* name;
	const char* title;
	std::initializer_list<const char*> items;
};

const ToolBarInfo toolbar_infos[] = {
	{"mainToolBar", nullptr, {"actionProjectNew", "actionProjectOpen", "actionProjectClose", "",
		"actionGameDiskCreate", "", "actionMapSave", "actionMapRevert", "actionMapProperties", "",
		"actionLayerLower", "actionLayerUpper", "actionLayerEvents", "", "actionZoomIn", "actionZoomOut",
		"actionZoom100", "actionZoom1_1", "actionZoom1_2", "actionZoom1_4", "actionZoom1_8", "",
		"actionPaletteToggle", "actionMapTreeToggle", "actionJukebox", "", "actionDatabase", "actionScriptEditor",
		"actionResourceManager", "actionSearch", "", "actionPlayTest", "actionFullScreenToggle",
		"actionTitleBackgroundToggle", "", "actionContents", ""}},
	// the palette's
	{"toolBar", QT_TRANSLATE_NOOP("MainWindow", "Toolbar"), {"actionUndo", "actionRedo", "", "actionDrawSelect",
		"actionDrawPen", "actionDrawRectangle", "actionDrawCircle", "actionDrawFill", "", "actionZoom"}},
};

constexpr int action_count = static_cast<int>(std::size(actions));

const QHash<QString, int>& rows()
{
	static const QHash<QString, int> rows = [] {
		QHash<QString, int> rows;
		for (int i = 0; i < action_count; i++)
			rows.insert(QLatin1String(actions[i].name), i);
		return rows;
	}();
	return rows;
}

QString translated(const char* text)
{
	return text ? QCoreApplication::translate("MainWindow", text) : QString();
}

QStringList items(std::initializer_list<const char*> list)
{
	QStringList items;
	for (const char* item : list)
		items.append(QLatin1String(item));
	return items;
}

}

ActionCatalog::ActionCatalog(QObject* parent) :
	QAbstractListModel(parent),
	m_states(action_count)
{
	for (int i = 0; i < action_count; i++) {
		m_states[i].checked = actions[i].checked;
		m_states[i].visible = actions[i].visible;
	}
}

int ActionCatalog::rowCount(const QModelIndex& parent) const
{
	return parent.isValid() ? 0 : action_count;
}

QVariant ActionCatalog::data(const QModelIndex& index, int role) const
{
	if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
		return {};
	const Info& info = actions[index.row()];
	const QString name = QLatin1String(info.name);
	switch (role) {
	case Qt::DisplayRole:
	case TextRole:
		return text(name);
	case NameRole:
		return name;
	case ToolTipRole:
		return translated(info.toolTip);
	case StatusTipRole:
		return translated(info.statusTip);
	case IconTextRole:
		return translated(info.iconText);
	case IconNameRole:
		return QString::fromLatin1(info.icon);
	case ShortcutsRole:
		return shortcutTexts(name);
	case StandardKeyRole:
		return static_cast<int>(ShortcutCatalog::standardKey(name));
	case ShortcutContextRole:
		return static_cast<int>(ShortcutCatalog::context(name));
	case CheckableRole:
		return info.checkable;
	case CheckedRole:
		return m_states[index.row()].checked;
	case EnabledRole:
		return m_states[index.row()].enabled;
	case VisibleRole:
		return m_states[index.row()].visible;
	case GroupRole:
		return static_cast<int>(info.group);
	case MenuRoleRole:
		return static_cast<int>(info.role);
	}
	return {};
}

QHash<int, QByteArray> ActionCatalog::roleNames() const
{
	return {
		{NameRole, "name"},
		{TextRole, "text"},
		{ToolTipRole, "toolTip"},
		{StatusTipRole, "statusTip"},
		{IconTextRole, "iconText"},
		{IconNameRole, "iconName"},
		{ShortcutsRole, "shortcuts"},
		{StandardKeyRole, "standardKey"},
		{ShortcutContextRole, "shortcutContext"},
		{CheckableRole, "checkable"},
		{CheckedRole, "checked"},
		{EnabledRole, "enabled"},
		{VisibleRole, "visible"},
		{GroupRole, "group"},
		{MenuRoleRole, "menuRole"},
	};
}

QStringList ActionCatalog::names() const
{
	QStringList names;
	for (const Info& info : actions)
		names.append(QLatin1String(info.name));
	return names;
}

bool ActionCatalog::contains(const QString& name) const
{
	return rows().contains(name);
}

int ActionCatalog::indexOf(const QString& name) const
{
	return rows().value(name, -1);
}

QString ActionCatalog::text(const QString& name) const
{
	const int row = indexOf(name);
	if (row < 0)
		return QString();
	return m_states[row].text.isEmpty() ? translated(actions[row].text) : m_states[row].text;
}

QString ActionCatalog::toolTip(const QString& name) const
{
	const int row = indexOf(name);
	return row < 0 ? QString() : translated(actions[row].toolTip);
}

QString ActionCatalog::statusTip(const QString& name) const
{
	const int row = indexOf(name);
	return row < 0 ? QString() : translated(actions[row].statusTip);
}

QString ActionCatalog::iconText(const QString& name) const
{
	const int row = indexOf(name);
	return row < 0 ? QString() : translated(actions[row].iconText);
}

QString ActionCatalog::iconName(const QString& name) const
{
	const int row = indexOf(name);
	return row < 0 ? QString() : QString::fromLatin1(actions[row].icon);
}

QList<QKeySequence> ActionCatalog::shortcuts(const QString& name) const
{
	return ShortcutCatalog::shortcuts(name);
}

QStringList ActionCatalog::shortcutTexts(const QString& name) const
{
	QStringList texts;
	for (const QKeySequence& key : ShortcutCatalog::shortcuts(name))
		texts.append(key.toString(QKeySequence::PortableText));
	return texts;
}

ShortcutCatalog::Context ActionCatalog::shortcutContext(const QString& name) const
{
	return ShortcutCatalog::context(name);
}

bool ActionCatalog::isTreeShortcut(const QString& name) const
{
	return ShortcutCatalog::context(name) == ShortcutCatalog::Context::MapTree;
}

bool ActionCatalog::isCheckable(const QString& name) const
{
	const int row = indexOf(name);
	return row >= 0 && actions[row].checkable;
}

bool ActionCatalog::isVisible(const QString& name) const
{
	const int row = indexOf(name);
	return row >= 0 && m_states[row].visible;
}

void ActionCatalog::setVisible(const QString& name, bool visible)
{
	const int row = indexOf(name);
	if (row < 0 || m_states[row].visible == visible)
		return;
	m_states[row].visible = visible;
	emit dataChanged(index(row), index(row), {VisibleRole});
	emit stateChanged(name);
}

void ActionCatalog::setText(const QString& name, const QString& text)
{
	const int row = indexOf(name);
	if (row < 0 || m_states[row].text == text)
		return;
	m_states[row].text = text;
	emit dataChanged(index(row), index(row), {Qt::DisplayRole, TextRole});
	emit stateChanged(name);
}

ActionCatalog::Group ActionCatalog::group(const QString& name) const
{
	const int row = indexOf(name);
	return row < 0 ? Group::None : actions[row].group;
}

ActionCatalog::MenuRole ActionCatalog::menuRole(const QString& name) const
{
	const int row = indexOf(name);
	return row < 0 ? MenuRole::None : actions[row].role;
}

QStringList ActionCatalog::groupActions(Group group) const
{
	QStringList names;
	if (group == Group::None)
		return names;
	for (const Info& info : actions) {
		if (info.group == group)
			names.append(QLatin1String(info.name));
	}
	return names;
}

bool ActionCatalog::isEnabled(const QString& name) const
{
	const int row = indexOf(name);
	return row >= 0 && m_states[row].enabled;
}

bool ActionCatalog::isChecked(const QString& name) const
{
	const int row = indexOf(name);
	return row >= 0 && m_states[row].checked;
}

void ActionCatalog::setEnabled(const QString& name, bool enabled)
{
	const int row = indexOf(name);
	if (row < 0 || m_states[row].enabled == enabled)
		return;
	m_states[row].enabled = enabled;
	emit dataChanged(index(row), index(row), {EnabledRole});
	emit stateChanged(name);
}

void ActionCatalog::setChecked(const QString& name, bool checked)
{
	const int row = indexOf(name);
	if (row < 0 || !actions[row].checkable)
		return;
	// the others of its group first: one checked at most
	if (checked) {
		for (const QString& other : groupActions(actions[row].group)) {
			if (other != name)
				setChecked(other, false);
		}
	}
	if (m_states[row].checked == checked)
		return;
	m_states[row].checked = checked;
	emit dataChanged(index(row), index(row), {CheckedRole});
	emit stateChanged(name);
}

QList<ActionCatalog::Menu> ActionCatalog::menus() const
{
	QList<Menu> menus;
	for (const MenuInfo& info : menu_infos)
		menus.append({QLatin1String(info.name), translated(info.title), items(info.items)});
	return menus;
}

QVariantList ActionCatalog::menuList() const
{
	QVariantList list;
	int i = 0;
	for (const Menu& menu : menus()) {
		list.append(QVariantMap{{"name", menu.name}, {"title", menu.title}, {"items", menu.items},
			{"menuBar", i++ < menu_bar_count}});
	}
	return list;
}

QStringList ActionCatalog::menuBar() const
{
	QStringList names;
	for (int i = 0; i < menu_bar_count; i++)
		names.append(QLatin1String(menu_infos[i].name));
	return names;
}

QString ActionCatalog::menuTitle(const QString& menu) const
{
	for (const MenuInfo& info : menu_infos) {
		if (menu == QLatin1String(info.name))
			return translated(info.title);
	}
	return {};
}

QStringList ActionCatalog::toolBar(const QString& toolbar) const
{
	for (const ToolBarInfo& info : toolbar_infos) {
		if (toolbar == QLatin1String(info.name))
			return items(info.items);
	}
	return {};
}

QStringList ActionCatalog::toolBarNames() const
{
	QStringList names;
	for (const ToolBarInfo& info : toolbar_infos)
		names.append(QLatin1String(info.name));
	return names;
}

QString ActionCatalog::toolBarTitle(const QString& toolbar) const
{
	for (const ToolBarInfo& info : toolbar_infos) {
		if (toolbar == QLatin1String(info.name))
			return translated(info.title);
	}
	return {};
}

void ActionCatalog::trigger(const QString& name)
{
	if (isEnabled(name))
		emit triggered(name);
}

void ActionCatalog::retranslate()
{
	emit dataChanged(index(0), index(action_count - 1), {Qt::DisplayRole, TextRole, ToolTipRole, StatusTipRole, IconTextRole});
	emit retranslated();
}
