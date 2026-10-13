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

#include "rpg_combobox.h"
#include "rpg_model.h"
#include "widget_as_dialog_model_wrapper.h"

namespace RpgComboBoxDetail {

template <typename LCF>
QAbstractItemModel* makeModel(ProjectData& project, QObject* parent) {
	return new RpgModel<LCF>(project, parent);
}

template <typename LCF>
void editInDatabase(ProjectData& project, int id, QWidget* parent, const std::function<void(int)>& chosen) {
	// FIXME: This copies the entire project, kinda slow
	WidgetAsDialogModelWrapper<LCF> dialog(project, id, parent);
	QObject::connect(&dialog, &WidgetAsDialogWrapperBase::valueSelected, parent, chosen);
	dialog.exec();
}

// the types of the aliases of rpg_combobox.h
#define EDITOR_RPG_COMBOBOX(LCF) \
	template QAbstractItemModel* makeModel<LCF>(ProjectData&, QObject*); \
	template void editInDatabase<LCF>(ProjectData&, int, QWidget*, const std::function<void(int)>&);

EDITOR_RPG_COMBOBOX(lcf::rpg::Actor)
EDITOR_RPG_COMBOBOX(lcf::rpg::Skill)
EDITOR_RPG_COMBOBOX(lcf::rpg::Item)
EDITOR_RPG_COMBOBOX(lcf::rpg::Enemy)
EDITOR_RPG_COMBOBOX(lcf::rpg::Troop)
EDITOR_RPG_COMBOBOX(lcf::rpg::Terrain)
EDITOR_RPG_COMBOBOX(lcf::rpg::Attribute)
EDITOR_RPG_COMBOBOX(lcf::rpg::State)
EDITOR_RPG_COMBOBOX(lcf::rpg::Animation)
EDITOR_RPG_COMBOBOX(lcf::rpg::Chipset)
EDITOR_RPG_COMBOBOX(lcf::rpg::CommonEvent)
EDITOR_RPG_COMBOBOX(lcf::rpg::Class)
EDITOR_RPG_COMBOBOX(lcf::rpg::BattlerAnimation)
EDITOR_RPG_COMBOBOX(lcf::rpg::Switch)
EDITOR_RPG_COMBOBOX(lcf::rpg::Variable)

}
