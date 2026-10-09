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

#include "location_widgets.h"
#include "common/dbstring.h"
#include "core.h"
#include "defines.h"
#include "model/project_data.h"
#include "ui/common/rpg_combobox.h"
#include "ui/common/rpg_spinbox.h"
#include "ui/picker/picker_backdrop_widget.h"
#include "ui/picker/picker_dialog.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QListWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>

LocationCommandWidget::LocationCommandWidget(ProjectData& project, QWidget* parent) :
	EventCommandBaseWidget(project, parent)
{
	new QVBoxLayout(this);
}

int LocationCommandWidget::parameter(int index) const {
	return index < static_cast<int>(m_cmd->parameters.size()) ? m_cmd->parameters[index] : 0;
}

void LocationCommandWidget::setParameter(int index, int value) {
	if (index < static_cast<int>(m_cmd->parameters.size()) && parameter(index) == value) {
		return;
	}
	if (index >= static_cast<int>(m_cmd->parameters.size())) {
		std::vector<int32_t> parameters(m_cmd->parameters.begin(), m_cmd->parameters.end());
		parameters.resize(index + 1);
		m_cmd->parameters = lcf::DBArray<int32_t>(parameters.begin(), parameters.end());
	}
	m_cmd->parameters[index] = value;
}

QFormLayout* LocationCommandWidget::section(const QString& title) {
	auto* group = new QGroupBox(title, this);
	auto* form = new QFormLayout(group);
	form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	layout()->addWidget(group);
	return form;
}

QSpinBox* LocationCommandWidget::spin(QFormLayout* form, const QString& label,
	const QString& name, int index, int minimum, int maximum)
{
	auto* value = new RpgSpinBox(this);
	value->setObjectName(name);
	value->setRange(std::min(minimum, parameter(index)), std::max(maximum, parameter(index)));
	value->setValue(parameter(index));
	form->addRow(label, value);
	connect(value, &QSpinBox::valueChanged, this, [this, index](int value) { setParameter(index, value); });
	return value;
}

QCheckBox* LocationCommandWidget::check(QFormLayout* form, const QString& label,
	const QString& name, int index)
{
	auto* value = new QCheckBox(label, this);
	value->setObjectName(name);
	value->setChecked(parameter(index) != 0);
	form->addRow(value);
	connect(value, &QCheckBox::toggled, this, [this, index](bool value) { setParameter(index, value ? 1 : 0); });
	return value;
}

QComboBox* LocationCommandWidget::choices(QFormLayout* form, const QString& label,
	const QString& name, int index, const QStringList& labels)
{
	auto* value = new QComboBox(this);
	value->setObjectName(name);
	for (int i = 0; i < labels.size(); ++i) {
		value->addItem(labels[i], i);
	}
	int selected = value->findData(parameter(index));
	if (selected < 0) {
		value->addItem(tr("Unknown (%1)").arg(parameter(index)), parameter(index));
		selected = value->count() - 1;
	}
	value->setCurrentIndex(selected);
	form->addRow(label, value);
	connect(value, &QComboBox::currentIndexChanged, this, [this, value, index](int row) {
		if (row >= 0) {
			setParameter(index, value->itemData(row).toInt());
		}
	});
	return value;
}

QComboBox* LocationCommandWidget::variable(QFormLayout* form, const QString& label,
	const QString& name, int index)
{
	auto* value = new VariableRpgComboBox(this);
	value->makeModel(m_project);
	auto* combo = value->comboBox();
	combo->setObjectName(name);
	combo->setCurrentIndex(parameter(index) - 1);
	form->addRow(label, value);
	connect(combo, &QComboBox::currentIndexChanged, this, [this, index](int row) {
		if (row >= 0) {
			setParameter(index, row + 1);
		}
	});
	return combo;
}

QComboBox* LocationCommandWidget::map(QFormLayout* form, const QString& label,
	const QString& name, int index)
{
	auto* value = new QComboBox(this);
	value->setObjectName(name);
	value->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
	value->setMinimumContentsLength(14);
	for (const auto& map : m_project.treeMap().maps) {
		if (map.ID > 0) {
			value->addItem(QStringLiteral("%1: %2").arg(map.ID, 4, 10, QChar('0')).arg(ToQString(map.name)), map.ID);
		}
	}
	int selected = value->findData(parameter(index));
	if (selected < 0) {
		value->addItem(tr("Unknown Map (%1)").arg(parameter(index)), parameter(index));
		selected = value->count() - 1;
	}
	value->setCurrentIndex(selected);
	form->addRow(label, value);
	connect(value, &QComboBox::currentIndexChanged, this, [this, index, value](int row) {
		if (row >= 0) {
			setParameter(index, value->itemData(row).toInt());
		}
	});
	return value;
}

QComboBox* LocationCommandWidget::character(QFormLayout* form, const QString& label,
	const QString& name, int index)
{
	auto* value = new QComboBox(this);
	value->setObjectName(name);
	value->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
	value->setMinimumContentsLength(14);
	value->addItem(tr("This Event"), 10005);
	value->addItem(tr("Hero"), 10001);
	value->addItem(tr("Boat"), 10002);
	value->addItem(tr("Ship"), 10003);
	value->addItem(tr("Airship"), 10004);
	if (const auto* events = core().currentMapEvents()) {
		for (auto it = events->cbegin(); it != events->cend(); ++it) {
			value->addItem(QStringLiteral("%1: %2").arg(it.key(), 4, 10, QChar('0')).arg(ToQString(it.value()->name)), it.key());
		}
	}
	int selected = value->findData(parameter(index));
	if (selected < 0) {
		value->addItem(tr("Unknown Event (%1)").arg(parameter(index)), parameter(index));
		selected = value->count() - 1;
	}
	value->setCurrentIndex(selected);
	form->addRow(label, value);
	connect(value, &QComboBox::currentIndexChanged, this, [this, index, value](int row) {
		if (row >= 0) {
			setParameter(index, value->itemData(row).toInt());
		}
	});
	return value;
}

void LocationCommandWidget::direction(QFormLayout* form, int index, QComboBox* mode) {
	auto* facing = choices(form, tr("Facing"), QStringLiteral("facing"), index,
		{tr("Retain Facing"), tr("Up"), tr("Right"), tr("Down"), tr("Left")});
	const bool supported = m_project.database().system.ldb_id == 2003;
	facing->setEnabled(supported && (!mode || mode->currentData().toInt() == 0));
	if (mode) {
		connect(mode, &QComboBox::currentIndexChanged, this, [facing, mode, supported] {
			facing->setEnabled(supported && mode->currentData().toInt() == 0);
		});
	}
}

void LocationCommandWidget::location(int mode_index, int map_index, int x_index,
	int y_index, int direction_index)
{
	auto* form = section(tr("Location"));
	QComboBox* mode = nullptr;
	if (mode_index >= 0) {
		mode = choices(form, tr("Location"), QStringLiteral("locationMode"), mode_index,
			{tr("Specific Location"), tr("Location Referenced from Variables")});
	}
	QComboBox* map_value = nullptr;
	if (map_index >= 0) {
		map_value = map(form, tr("Map"), QStringLiteral("locationMap"), map_index);
	}
	auto* x = spin(form, tr("X Coordinate"), QStringLiteral("locationX"), x_index, 0, 9999);
	auto* y = spin(form, tr("Y Coordinate"), QStringLiteral("locationY"), y_index, 0, 9999);
	if (mode) {
		QComboBox* map_variable = nullptr;
		if (map_index >= 0) {
			map_variable = variable(form, tr("Map ID Stored in"), QStringLiteral("locationMapVariable"), map_index);
		}
		auto* x_variable = variable(form, tr("X Coordinate Stored in"), QStringLiteral("locationXVariable"), x_index);
		auto* y_variable = variable(form, tr("Y Coordinate Stored in"), QStringLiteral("locationYVariable"), y_index);
		auto update = [=, this](bool write) {
			const bool variables = mode->currentData().toInt() != 0;
			if (map_value) {
				map_value->setEnabled(!variables);
				map_variable->parentWidget()->setEnabled(variables);
			}
			x->setEnabled(!variables);
			y->setEnabled(!variables);
			x_variable->parentWidget()->setEnabled(variables);
			y_variable->parentWidget()->setEnabled(variables);
			if (write) {
				if (map_value && (!variables || map_variable->currentIndex() >= 0)) {
					setParameter(map_index, variables ? map_variable->currentIndex() + 1 : map_value->currentData().toInt());
				}
				if (!variables || x_variable->currentIndex() >= 0) {
					setParameter(x_index, variables ? x_variable->currentIndex() + 1 : x->value());
				}
				if (!variables || y_variable->currentIndex() >= 0) {
					setParameter(y_index, variables ? y_variable->currentIndex() + 1 : y->value());
				}
			}
		};
		update(false);
		connect(mode, &QComboBox::currentIndexChanged, this, [update] { update(true); });
	}
	if (direction_index >= 0) {
		direction(form, direction_index, mode);
	}
}

void LocationCommandWidget::tile(QFormLayout* form, const QString& label,
	const QString& name, int index)
{
	auto* row = new QHBoxLayout();
	auto* value = new RpgSpinBox(this);
	value->setObjectName(name);
	value->setRange(std::min(0, parameter(index)), std::max(143, parameter(index)));
	value->setValue(parameter(index));
	auto* pick = new QToolButton(this);
	pick->setObjectName(name + QStringLiteral("Picker"));
	pick->setText(tr("Set"));
	row->addWidget(value, 1);
	row->addWidget(pick);
	form->addRow(label, row);
	connect(value, &QSpinBox::valueChanged, this, [this, index](int value) { setParameter(index, value); });
	connect(pick, &QToolButton::clicked, this, [this, value] {
		QDialog dialog(this);
		dialog.setWindowTitle(tr("Select Tile"));
		auto* layout = new QVBoxLayout(&dialog);
		auto* tiles = new QListWidget(&dialog);
		tiles->setObjectName(QStringLiteral("tilePicker"));
		tiles->setViewMode(QListView::IconMode);
		tiles->setResizeMode(QListView::Adjust);
		tiles->setMovement(QListView::Static);
		tiles->setIconSize(QSize(32, 32));
		const bool upper = parameter(0) != 0;
		for (int i = 0; i < 144; ++i) {
			QPixmap image(32, 32);
			image.fill(Qt::transparent);
			core().beginPainting(image);
			core().renderTile(core().translate(i + (upper ? 162 : 18)), image.rect());
			core().endPainting();
			new QListWidgetItem(QIcon(image), QString::number(i), tiles);
		}
		tiles->setCurrentRow(value->value());
		layout->addWidget(tiles);
		auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
		layout->addWidget(buttons);
		connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
		connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
		connect(tiles, &QListWidget::itemDoubleClicked, &dialog, &QDialog::accept);
		if (dialog.exec() == QDialog::Accepted && tiles->currentRow() >= 0) {
			value->setValue(tiles->currentRow());
		}
	});
}

void LocationCommandWidget::setData(lcf::rpg::EventCommand* cmd) {
	// Base bindings eagerly normalize or append parameters. These editors only
	// write a field when it is edited so old engine and patch tails survive.
	m_cmd = cmd;
	using Code = lcf::rpg::EventCommand::Code;
	switch (static_cast<Code>(cmd->code)) {
	case Code::Teleport:
		setWindowTitle(tr("Teleport"));
		location(-1, 0, 1, 2, 3);
		break;
	case Code::MemorizeLocation:
	case Code::RecallToLocation: {
		const bool memorize = static_cast<Code>(cmd->code) == Code::MemorizeLocation;
		setWindowTitle(memorize ? tr("Memorize Position") : tr("Recall to Memorized Position"));
		auto* form = section(memorize ? tr("Storage Locations") : tr("Recall Location"));
		variable(form, memorize ? tr("Store Map ID in") : tr("Map ID Stored in"), QStringLiteral("mapVariable"), 0);
		variable(form, memorize ? tr("Store X Coordinate in") : tr("X Coordinate Stored in"), QStringLiteral("xVariable"), 1);
		variable(form, memorize ? tr("Store Y Coordinate in") : tr("Y Coordinate Stored in"), QStringLiteral("yVariable"), 2);
		break;
	}
	case Code::SetVehicleLocation: {
		setWindowTitle(tr("Set Vehicle Location"));
		auto* form = section(tr("Vehicle"));
		auto* vehicle = choices(form, tr("Vehicle"), QStringLiteral("vehicle"), 0, {tr("Boat"), tr("Ship"), tr("Airship")});
		if (parameter(0) == -1) {
			vehicle->setItemText(vehicle->currentIndex(), tr("Party"));
		}
		location(1, 2, 3, 4);
		break;
	}
	case Code::ChangeEventLocation: {
		setWindowTitle(tr("Change Event Location"));
		character(section(tr("Event")), tr("Event"), QStringLiteral("event"), 0);
		location(1, -1, 2, 3, 4);
		break;
	}
	case Code::TradeEventLocations: {
		setWindowTitle(tr("Trade Event Locations"));
		for (int index = 0; index < 2; ++index) {
			auto* form = section(index == 0 ? tr("First Event") : tr("Second Event"));
			const QString name = index == 0 ? QStringLiteral("firstEvent") : QStringLiteral("secondEvent");
			auto* event = character(form, tr("Event"), name, index);
			if (cmd->parameters.size() > 2 && (parameter(2) & (1 << index))) {
				event->setEnabled(false);
				variable(form, tr("Event ID Stored in"), name + QStringLiteral("Variable"), index);
			}
		}
		break;
	}
	case Code::StoreTerrainID:
	case Code::StoreEventID: {
		const bool terrain = static_cast<Code>(cmd->code) == Code::StoreTerrainID;
		setWindowTitle(terrain ? tr("Store Terrain ID") : tr("Store Event ID"));
		location(0, -1, 1, 2);
		variable(section(tr("Storage Location")), terrain ? tr("Store Terrain ID in Variable") : tr("Store Event ID in Variable"), QStringLiteral("resultVariable"), 3);
		break;
	}
	case Code::ChangeMapTileset: {
		setWindowTitle(tr("Change Map Tileset"));
		auto* form = section(tr("New Tileset"));
		if (cmd->parameters.size() > 1 && (parameter(1) & 1)) {
			variable(form, tr("Tileset ID Stored in"), QStringLiteral("tilesetVariable"), 0);
		} else {
			auto* chipset = new ChipsetRpgComboBox(this);
			chipset->makeModel(m_project);
			chipset->comboBox()->setObjectName(QStringLiteral("tileset"));
			chipset->setCurrentIndex(parameter(0) - 1);
			form->addRow(tr("Tileset"), chipset);
			connect(chipset->comboBox(), &QComboBox::currentIndexChanged, this, [this](int row) {
				if (row >= 0) {
					setParameter(0, row + 1);
				}
			});
		}
		break;
	}
	case Code::ChangePBG: {
		setWindowTitle(tr("Change Parallax Background"));
		auto* graphic = section(tr("Graphic"));
		if (cmd->parameters.size() > 6 && (parameter(6) & 1)) {
			variable(graphic, tr("Graphic Name Stored in"), QStringLiteral("panoramaVariable"), 7);
		} else {
			auto* row = new QHBoxLayout();
			auto* file = new QLineEdit(ToQString(cmd->string), this);
			file->setObjectName(QStringLiteral("panorama"));
			auto* pick = new QToolButton(this);
			pick->setObjectName(QStringLiteral("panoramaPicker"));
			pick->setText(tr("Set"));
			row->addWidget(file, 1);
			row->addWidget(pick);
			graphic->addRow(tr("Graphic"), row);
			connect(file, &QLineEdit::textEdited, this, [this](const QString& value) { m_cmd->string = ToDBString(value); });
			connect(pick, &QToolButton::clicked, this, [this, file] {
				PickerDialog dialog(m_project, FileFinder::FileType::Image, new PickerBackdropWidget(), this);
				dialog.setDirectoryAndFile(QStringLiteral(PANORAMA), file->text());
				connect(&dialog, &PickerDialog::fileSelected, this, [this, file](const QString& value) {
					file->setText(value);
					m_cmd->string = ToDBString(value);
				});
				dialog.exec();
			});
		}
		auto* options = section(tr("Options"));
		auto* horizontal = check(options, tr("Horizontal Scrolling"), QStringLiteral("horizontalScroll"), 0);
		auto* horizontal_auto = check(options, tr("Horizontal Autoscroll"), QStringLiteral("horizontalAuto"), 2);
		QWidget* horizontal_speed;
		if (cmd->parameters.size() > 6 && (parameter(6) & (1 << 4))) {
			horizontal_speed = variable(options, tr("Horizontal Speed Stored in"), QStringLiteral("horizontalSpeedVariable"), 3)->parentWidget();
		} else {
			horizontal_speed = spin(options, tr("Horizontal Speed"), QStringLiteral("horizontalSpeed"), 3, -8, 8);
		}
		auto* vertical = check(options, tr("Vertical Scrolling"), QStringLiteral("verticalScroll"), 1);
		auto* vertical_auto = check(options, tr("Vertical Autoscroll"), QStringLiteral("verticalAuto"), 4);
		QWidget* vertical_speed;
		if (cmd->parameters.size() > 6 && (parameter(6) & (1 << 6))) {
			vertical_speed = variable(options, tr("Vertical Speed Stored in"), QStringLiteral("verticalSpeedVariable"), 5)->parentWidget();
		} else {
			vertical_speed = spin(options, tr("Vertical Speed"), QStringLiteral("verticalSpeed"), 5, -8, 8);
		}
		auto update = [=] {
			horizontal_auto->setEnabled(horizontal->isChecked());
			horizontal_speed->setEnabled(horizontal->isChecked() && horizontal_auto->isChecked());
			vertical_auto->setEnabled(vertical->isChecked());
			vertical_speed->setEnabled(vertical->isChecked() && vertical_auto->isChecked());
		};
		update();
		for (auto* value : {horizontal, horizontal_auto, vertical, vertical_auto}) {
			connect(value, &QCheckBox::toggled, this, update);
		}
		break;
	}
	case Code::ChangeEncounterSteps:
		setWindowTitle(tr("Change Encounter Rate"));
		spin(section(tr("New Rate")), tr("Steps Between Encounters"), QStringLiteral("encounterSteps"), 0, 0, 9999);
		break;
	case Code::TileSubstitution: {
		setWindowTitle(tr("Tile Substitution"));
		auto* form = section(tr("Tile Layer"));
		choices(form, tr("Tile Layer"), QStringLiteral("tileLayer"), 0, {tr("Lower Layer"), tr("Upper Layer")});
		tile(form, tr("Original Tile"), QStringLiteral("originalTile"), 1);
		tile(form, tr("Substituted Tile"), QStringLiteral("substitutedTile"), 2);
		break;
	}
	case Code::TeleportTargets:
	case Code::EscapeTarget: {
		const bool teleport = static_cast<Code>(cmd->code) == Code::TeleportTargets;
		setWindowTitle(teleport ? tr("Teleport Target Management") : tr("Set Escape Target"));
		QComboBox* operation = nullptr;
		if (teleport) {
			operation = choices(section(tr("Operation")), tr("Operation"), QStringLiteral("targetOperation"), 0,
				{tr("Add Teleport Target"), tr("Delete Teleport Target")});
		}
		const int offset = teleport ? 1 : 0;
		location(-1, offset, offset + 1, offset + 2);
		auto* form = section(teleport ? tr("Turn Switch ON After Teleporting") : tr("Turn Switch ON After Escaping"));
		auto* enabled = check(form, tr("Turn on"), QStringLiteral("targetSwitchEnabled"), offset + 3);
		auto* value = new SwitchRpgComboBox(this);
		value->makeModel(m_project);
		value->comboBox()->setObjectName(QStringLiteral("targetSwitch"));
		value->setCurrentIndex(parameter(offset + 4) - 1);
		form->addRow(tr("Switch"), value);
		connect(value->comboBox(), &QComboBox::currentIndexChanged, this, [this, offset](int row) {
			if (row >= 0) {
				setParameter(offset + 4, row + 1);
			}
		});
		auto update = [=, this] {
			const bool adding = !operation || operation->currentData().toInt() == 0;
			findChild<QSpinBox*>(QStringLiteral("locationX"))->setEnabled(adding);
			findChild<QSpinBox*>(QStringLiteral("locationY"))->setEnabled(adding);
			enabled->setEnabled(adding);
			value->setEnabled(adding && enabled->isChecked());
		};
		update();
		connect(enabled, &QCheckBox::toggled, this, update);
		if (operation) {
			connect(operation, &QComboBox::currentIndexChanged, this, update);
		}
		break;
	}
	default:
		break;
	}
}
