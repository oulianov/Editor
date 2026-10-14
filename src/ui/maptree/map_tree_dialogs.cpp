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

#include "map_tree_dialogs.h"
#include "location_picker.h"
#include "map_tree_forms.h"
#include "model/map_tools.h"
#include "qmlbinding/qml_host.h"
#include <QQuickWindow>

bool MapTreeDialogs::mapProperties(QWindow* over, ProjectData& project, lcf::rpg::MapInfo& info, lcf::rpg::Map& map)
{
	// gone after the window (deleted later), which shows it until then
	auto* form = new MapPropertiesForm(project, info, map);
	const int result = QmlHost::exec(QStringLiteral("MapPropertiesWindow"), over,
		{{QStringLiteral("form"), QVariant::fromValue(form)}});
	form->deleteLater();
	if (result != 1)
		return false;
	map = form->resultMap();
	info = form->resultInfo();
	return true;
}

bool MapTreeDialogs::areaProperties(QWindow* over, ProjectData& project, lcf::rpg::MapInfo& info, const QSize& map_size)
{
	auto* form = new AreaPropertiesForm(project, info, map_size);
	const int result = QmlHost::exec(QStringLiteral("AreaPropertiesWindow"), over,
		{{QStringLiteral("form"), QVariant::fromValue(form)}});
	form->deleteLater();
	if (result != 1)
		return false;
	info = form->resultInfo();
	return true;
}

std::optional<ShellDialogs::Dungeon> MapTreeDialogs::generateDungeon(QWindow* over, ProjectData& project,
	const lcf::rpg::Map& source, const QString& name, std::optional<uint32_t> seed)
{
	auto* form = new DungeonForm(project, source);
	if (seed)
		form->setSeed(*seed);
	QString chosen_name;
	const int result = QmlHost::exec(QStringLiteral("GenerateDungeonWindow"), over, {
		{QStringLiteral("form"), QVariant::fromValue(form)},
		{QStringLiteral("dungeonName"), name},
	}, [&chosen_name](QQuickWindow* window) {
		chosen_name = window->property("dungeonName").toString();
	});
	form->deleteLater();
	// the window generates the dungeon shown once more on OK
	if (result != 1 || !form->preview()->hasMap())
		return std::nullopt;
	return ShellDialogs::Dungeon{chosen_name, form->map()};
}

std::optional<QPoint> MapTreeDialogs::shiftMap(QWindow* over)
{
	QPoint offset;
	const int result = QmlHost::exec(QStringLiteral("ShiftMapWindow"), over, {}, [&offset](QQuickWindow* window) {
		offset = QPoint(window->property("dx").toInt(), window->property("dy").toInt());
	});
	if (result != 1)
		return std::nullopt;
	return offset;
}

std::optional<QRect> MapTreeDialogs::areaBoundaries(QWindow* over, ProjectData& project, int map_id, const QRect& area)
{
	auto* picker = new LocationPicker(project);
	const bool readable = picker->showMap(map_id);
	QRect chosen;
	const int result = QmlHost::exec(QStringLiteral("AreaBoundariesWindow"), over, {
		{QStringLiteral("picker"), QVariant::fromValue(picker)},
		{QStringLiteral("readable"), readable},
		{QStringLiteral("area"), area},
	}, [&chosen](QQuickWindow* window) {
		chosen = window->property("area").toRect();
	});
	picker->deleteLater();
	if (result != 1 || chosen.isEmpty())
		return std::nullopt;
	return chosen;
}

std::optional<QRect> MapTreeDialogs::chipSelector(QWindow* over, int chipset_id, bool lower, const QSize& max_size,
	const QRect& current)
{
	QVariantList tiles;
	for (int id : MapTools::paletteTiles(lower))
		tiles.append(id);
	QRect chosen;
	const int result = QmlHost::exec(QStringLiteral("ChipSelectorWindow"), over, {
		{QStringLiteral("chipsetId"), chipset_id},
		{QStringLiteral("tiles"), tiles},
		{QStringLiteral("maxWidth"), max_size.width()},
		{QStringLiteral("maxHeight"), max_size.height()},
		{QStringLiteral("selection"), current},
	}, [&chosen](QQuickWindow* window) {
		chosen = window->property("selection").toRect();
	});
	if (result != 1 || chosen.isEmpty())
		return std::nullopt;
	return chosen;
}
