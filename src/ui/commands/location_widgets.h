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

#include "ui/event/event_command_base_widget.h"

class QCheckBox;
class QComboBox;
class QFormLayout;
class QGroupBox;
class QSpinBox;

/** Native editors for map locations and map settings. Bind only after loading:
 * optional parameters and unsupported values survive an unchanged acceptance. */
class LocationCommandWidget : public EventCommandBaseWidget
{
	Q_OBJECT
public:
	LocationCommandWidget(ProjectData& project, QWidget* parent);
	void setData(lcf::rpg::EventCommand* cmd) override;

private:
	int parameter(int index) const;
	void setParameter(int index, int value);
	QFormLayout* section(const QString& title);
	QSpinBox* spin(QFormLayout* form, const QString& label, const QString& name,
		int index, int minimum, int maximum);
	QCheckBox* check(QFormLayout* form, const QString& label, const QString& name, int index);
	QComboBox* choices(QFormLayout* form, const QString& label, const QString& name,
		int index, const QStringList& labels);
	QComboBox* variable(QFormLayout* form, const QString& label, const QString& name, int index);
	QComboBox* map(QFormLayout* form, const QString& label, const QString& name, int index);
	QComboBox* character(QFormLayout* form, const QString& label, const QString& name, int index);
	void location(int mode_index, int map_index, int x_index, int y_index, int direction_index = -1);
	QComboBox* direction(QFormLayout* form, int index, QComboBox* mode = nullptr);
	void tile(QFormLayout* form, const QString& label, const QString& name, int index);
};

using TeleportWidget = LocationCommandWidget;
using MemorizeLocationWidget = LocationCommandWidget;
using RecallToLocationWidget = LocationCommandWidget;
using SetVehicleLocationWidget = LocationCommandWidget;
using ChangeEventLocationWidget = LocationCommandWidget;
using TradeEventLocationsWidget = LocationCommandWidget;
using StoreTerrainIDWidget = LocationCommandWidget;
using StoreEventIDWidget = LocationCommandWidget;
using ChangeMapTilesetWidget = LocationCommandWidget;
using ChangePBGWidget = LocationCommandWidget;
using ChangeEncounterStepsWidget = LocationCommandWidget;
using TileSubstitutionWidget = LocationCommandWidget;
using TeleportTargetsWidget = LocationCommandWidget;
using EscapeTargetWidget = LocationCommandWidget;
