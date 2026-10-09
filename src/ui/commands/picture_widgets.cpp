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

#include "picture_widgets.h"
#include "common/dbstring.h"
#include "core.h"
#include "defines.h"
#include "model/project_data.h"
#include "ui/common/rpg_combobox.h"
#include "ui/common/rpg_spinbox.h"
#include "ui/picker/picker_backdrop_widget.h"
#include "ui/picker/picker_dialog.h"
#include <QButtonGroup>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPalette>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>

namespace {
int parameter(const lcf::rpg::EventCommand* command, int index, int fallback = 0) {
	return index < static_cast<int>(command->parameters.size()) ? command->parameters[index] : fallback;
}

void setParameter(lcf::rpg::EventCommand* command, int index, int value, int mask = -1) {
	if (index >= static_cast<int>(command->parameters.size())) {
		auto parameters = lcf::DBArray<int32_t>(index + 1);
		std::copy(command->parameters.begin(), command->parameters.end(), parameters.begin());
		command->parameters = parameters;
	}
	command->parameters[index] = (command->parameters[index] & ~mask) | (value & mask);
}

RpgSpinBox* spin(QWidget* parent, lcf::rpg::EventCommand* command, const QString& name,
	int index, int minimum, int maximum, int fallback = 0, int mask = -1) {
	auto* result = new RpgSpinBox(parent);
	result->setObjectName(name);
	const int value = parameter(command, index, fallback) & mask;
	// Keep unusual and patched values visible without rewriting them on load.
	result->setRange(std::min(minimum, value), std::max(maximum, value));
	result->setValue(value);
	QObject::connect(result, &QSpinBox::valueChanged, parent, [=](int selected) {
		setParameter(command, index, selected, mask);
	});
	return result;
}

QCheckBox* check(QWidget* parent, lcf::rpg::EventCommand* command, const QString& name,
	const QString& label, int index, int mask = -1) {
	auto* result = new QCheckBox(label, parent);
	result->setObjectName(name);
	result->setChecked((parameter(command, index) & mask) != 0);
	QObject::connect(result, &QCheckBox::toggled, parent, [=](bool selected) {
		setParameter(command, index, selected ? 1 : 0, mask);
	});
	return result;
}

void selectValue(QComboBox* combo, int value) {
	int index = combo->findData(value);
	if (index == -1) {
		combo->addItem(PictureWidget::tr("Unknown (%1)").arg(value), value);
		index = combo->count() - 1;
	}
	combo->setCurrentIndex(index);
}

void bindCombo(QComboBox* combo, QWidget* parent, lcf::rpg::EventCommand* command,
	int index, int fallback = 0, int mask = -1) {
	selectValue(combo, parameter(command, index, fallback) & mask);
	QObject::connect(combo, &QComboBox::currentIndexChanged, parent, [=](int selected) {
		if (selected >= 0) {
			setParameter(command, index, combo->itemData(selected).toInt(), mask);
		}
	});
}

QComboBox* characterCombo(QWidget* parent, lcf::rpg::EventCommand* command, const QString& name, int index) {
	auto* result = new QComboBox(parent);
	result->setObjectName(name);
	result->addItem(PictureWidget::tr("Hero"), 10001);
	result->addItem(PictureWidget::tr("Boat"), 10002);
	result->addItem(PictureWidget::tr("Ship"), 10003);
	result->addItem(PictureWidget::tr("Airship"), 10004);
	result->addItem(PictureWidget::tr("This Event"), 10005);
	if (auto* events = core().currentMapEvents()) {
		for (auto it = events->cbegin(); it != events->cend(); ++it) {
			result->addItem(QStringLiteral("%1: %2").arg(it.key(), 4, 10, QChar('0')).arg(ToQString(it.value()->name)), it.key());
		}
	}
	bindCombo(result, parent, command, index, 10001);
	return result;
}

QStringList transitions(bool show) {
	if (show) {
		return {ScreenTransitionWidget::tr("Fade In"), ScreenTransitionWidget::tr("Reconstitute Blocks"),
			ScreenTransitionWidget::tr("Unwipe Downward"), ScreenTransitionWidget::tr("Unwipe Upward"),
			ScreenTransitionWidget::tr("Venetian Blinds"), ScreenTransitionWidget::tr("Vertical Blinds"),
			ScreenTransitionWidget::tr("Horizontal Blinds"), ScreenTransitionWidget::tr("Receding Square"),
			ScreenTransitionWidget::tr("Expanding Square"), ScreenTransitionWidget::tr("Screen Moves Up"),
			ScreenTransitionWidget::tr("Screen Moves Down"), ScreenTransitionWidget::tr("Screen Moves Left"),
			ScreenTransitionWidget::tr("Screen Moves Right"), ScreenTransitionWidget::tr("Vertical Unify"),
			ScreenTransitionWidget::tr("Horizontal Unify"), ScreenTransitionWidget::tr("Unify Quadrants"),
			ScreenTransitionWidget::tr("Zoom Out"), ScreenTransitionWidget::tr("Mosaic"),
			ScreenTransitionWidget::tr("Waver Screen"), ScreenTransitionWidget::tr("Instantaneous")};
	}
	return {ScreenTransitionWidget::tr("Fade Out"), ScreenTransitionWidget::tr("Remove Blocks"),
		ScreenTransitionWidget::tr("Wipe Downward"), ScreenTransitionWidget::tr("Wipe Upward"),
		ScreenTransitionWidget::tr("Venetian Blinds"), ScreenTransitionWidget::tr("Vertical Blinds"),
		ScreenTransitionWidget::tr("Horizontal Blinds"), ScreenTransitionWidget::tr("Receding Square"),
		ScreenTransitionWidget::tr("Expanding Square"), ScreenTransitionWidget::tr("Screen Moves Up"),
		ScreenTransitionWidget::tr("Screen Moves Down"), ScreenTransitionWidget::tr("Screen Moves Left"),
		ScreenTransitionWidget::tr("Screen Moves Right"), ScreenTransitionWidget::tr("Vertical Division"),
		ScreenTransitionWidget::tr("Horizontal Division"), ScreenTransitionWidget::tr("Quadrasection"),
		ScreenTransitionWidget::tr("Zoom In"), ScreenTransitionWidget::tr("Mosaic"),
		ScreenTransitionWidget::tr("Waver Screen"), ScreenTransitionWidget::tr("Instantaneous")};
}

void fillTransitions(QComboBox* combo, bool show, bool useDefault, bool noTransition) {
	combo->clear();
	if (useDefault) {
		combo->addItem(ScreenTransitionWidget::tr("Use Default"), -1);
	}
	const auto names = transitions(show);
	for (int index = 0; index < names.size(); ++index) {
		combo->addItem(names[index], index);
	}
	if (noTransition) {
		combo->addItem(ScreenTransitionWidget::tr("No Transition"), 20);
	}
}
}

PictureWidget::PictureWidget(ProjectData& project, bool move, QWidget* parent) :
	EventCommandBaseWidget(project, parent), m_move(move) {}

ShowPictureWidget::ShowPictureWidget(ProjectData& project, QWidget* parent) : PictureWidget(project, false, parent) {
	setWindowTitle(tr("Show Picture"));
}

MovePictureWidget::MovePictureWidget(ProjectData& project, QWidget* parent) : PictureWidget(project, true, parent) {
	setWindowTitle(tr("Move Picture"));
}

void PictureWidget::setData(lcf::rpg::EventCommand* cmd) {
	EventCommandBaseWidget::setData(cmd);
	const bool rpg2003 = m_project.database().system.ldb_id == 2003;
	auto* layout = new QVBoxLayout(this);
	auto* tabs = new QTabWidget(this);
	tabs->setObjectName(QStringLiteral("pictureTabs"));
	layout->addWidget(tabs);
	auto* general = new QWidget(tabs);
	auto* columns = new QHBoxLayout(general);
	auto* left = new QVBoxLayout();
	auto* right = new QVBoxLayout();
	columns->addLayout(left, 1);
	columns->addLayout(right, 1);
	tabs->addTab(general, tr("Picture"));

	auto* picture = new QGroupBox(tr("Picture No."), general);
	auto* pictureForm = new QFormLayout(picture);
	pictureForm->addRow(tr("Picture No."), spin(picture, cmd, QStringLiteral("pictureId"), 0, 1, 9999, 1));
	left->addWidget(picture);
	if (!m_move) {
		auto* image = new QGroupBox(tr("Image File"), general);
		auto* imageRow = new QHBoxLayout(image);
		auto* file = new QLineEdit(ToQString(cmd->string), image);
		file->setObjectName(QStringLiteral("pictureFile"));
		auto* picker = new QToolButton(image);
		picker->setObjectName(QStringLiteral("pictureFilePicker"));
		picker->setText(QStringLiteral("..."));
		picker->setToolTip(tr("Choose an image"));
		imageRow->addWidget(file, 1);
		imageRow->addWidget(picker);
		left->addWidget(image);
		connect(file, &QLineEdit::textEdited, this, [cmd](const QString& value) { cmd->string = ToDBString(value); });
		connect(picker, &QToolButton::clicked, this, [=, this] {
			auto* preview = new PickerBackdropWidget(this);
			PickerDialog dialog(m_project, FileFinder::FileType::Image, preview, this);
			dialog.setDirectoryAndFile(PICTURE, ToQString(cmd->string));
			connect(&dialog, &PickerDialog::fileSelected, this, [=](const QString& selected) {
				cmd->string = ToDBString(selected);
				file->setText(selected);
			});
			dialog.exec();
		});
	}

	auto* coordinates = new QGroupBox(m_move ? tr("Target Coordinates") : tr("Display Coordinates"), general);
	auto* coordinateLayout = new QVBoxLayout(coordinates);
	auto* modes = new QButtonGroup(coordinates);
	auto* specific = new QRadioButton(tr("Specific Coordinates"), coordinates);
	auto* variables = new QRadioButton(tr("Variable Reference"), coordinates);
	specific->setObjectName(QStringLiteral("pictureCoordinatesConstant"));
	variables->setObjectName(QStringLiteral("pictureCoordinatesVariable"));
	modes->addButton(specific, 0);
	modes->addButton(variables, 1);
	coordinateLayout->addWidget(specific);
	coordinateLayout->addWidget(variables);
	auto* coordinatePages = new QStackedWidget(coordinates);
	coordinateLayout->addWidget(coordinatePages);
	auto* valuesPage = new QWidget(coordinatePages);
	auto* valuesForm = new QFormLayout(valuesPage);
	auto* x = spin(valuesPage, cmd, QStringLiteral("pictureX"), 2, -10000, 10000, 160);
	auto* y = spin(valuesPage, cmd, QStringLiteral("pictureY"), 3, -10000, 10000, 120);
	valuesForm->addRow(tr("X"), x);
	valuesForm->addRow(tr("Y"), y);
	coordinatePages->addWidget(valuesPage);
	auto* variablesPage = new QWidget(coordinatePages);
	auto* variablesForm = new QFormLayout(variablesPage);
	auto* xVariable = new VariableRpgComboBox(variablesPage);
	auto* yVariable = new VariableRpgComboBox(variablesPage);
	xVariable->makeModel(m_project);
	yVariable->makeModel(m_project);
	xVariable->comboBox()->setObjectName(QStringLiteral("pictureXVariable"));
	yVariable->comboBox()->setObjectName(QStringLiteral("pictureYVariable"));
	xVariable->setCurrentIndex(parameter(cmd, 2, 1) - 1);
	yVariable->setCurrentIndex(parameter(cmd, 3, 1) - 1);
	variablesForm->addRow(tr("X"), xVariable);
	variablesForm->addRow(tr("Y"), yVariable);
	coordinatePages->addWidget(variablesPage);
	const int positionMode = parameter(cmd, 1) & 0xff;
	if (auto* button = modes->button(positionMode)) {
		button->setChecked(true);
	}
	coordinatePages->setCurrentIndex(positionMode == 1 ? 1 : 0);
	connect(modes, &QButtonGroup::idToggled, this, [=](int mode, bool selected) {
		if (!selected) {
			return;
		}
		setParameter(cmd, 1, mode, 0xff);
		coordinatePages->setCurrentIndex(mode);
		if (mode == 0) {
			setParameter(cmd, 2, x->value());
			setParameter(cmd, 3, y->value());
		} else {
			if (xVariable->currentIndex() >= 0) {
				setParameter(cmd, 2, xVariable->currentIndex() + 1);
			}
			if (yVariable->currentIndex() >= 0) {
				setParameter(cmd, 3, yVariable->currentIndex() + 1);
			}
		}
	});
	connect(xVariable->comboBox(), &QComboBox::currentIndexChanged, this, [=](int index) {
		if (index >= 0 && variables->isChecked()) {
			setParameter(cmd, 2, index + 1);
		}
	});
	connect(yVariable->comboBox(), &QComboBox::currentIndexChanged, this, [=](int index) {
		if (index >= 0 && variables->isChecked()) {
			setParameter(cmd, 3, index + 1);
		}
	});
	if (!m_move) {
		coordinateLayout->addWidget(check(coordinates, cmd, QStringLiteral("pictureScroll"), tr("Picture Scrolls with Map"), 4));
	}
	left->addWidget(coordinates);
	if (!m_move) {
		auto* transparent = new QGroupBox(tr("Transparent Color"), general);
		auto* transparentForm = new QFormLayout(transparent);
		auto* transparencyMode = new QComboBox(transparent);
		transparencyMode->setObjectName(QStringLiteral("pictureTransparentColor"));
		transparencyMode->addItem(tr("Use"), 1);
		transparencyMode->addItem(tr("None"), 0);
		bindCombo(transparencyMode, this, cmd, 7, 1);
		transparentForm->addRow(transparencyMode);
		left->addWidget(transparent);
	}

	auto* magnification = new QGroupBox(tr("Magnification"), general);
	auto* magnificationForm = new QFormLayout(magnification);
	auto* scale = spin(magnification, cmd, QStringLiteral("pictureMagnification"), 5, 0, 2000, 100);
	scale->setSuffix(tr("%"));
	magnificationForm->addRow(scale);
	right->addWidget(magnification);
	auto* transparency = new QGroupBox(tr("Transparency"), general);
	auto* transparencyForm = new QFormLayout(transparency);
	auto* top = spin(transparency, cmd, QStringLiteral("pictureTopTransparency"), 6, 0, 100);
	auto* bottom = spin(transparency, cmd, QStringLiteral("pictureBottomTransparency"), m_move ? 16 : 14,
		0, 100, parameter(cmd, 6), 0xff);
	top->setSuffix(tr("%"));
	bottom->setSuffix(tr("%"));
	bottom->setEnabled(rpg2003);
	transparencyForm->addRow(tr("Top Half"), top);
	transparencyForm->addRow(tr("Bottom Half"), bottom);
	right->addWidget(transparency);
	if (m_move) {
		auto* timing = new QGroupBox(tr("Transition Time"), general);
		auto* timingForm = new QFormLayout(timing);
		auto* duration = spin(timing, cmd, QStringLiteral("pictureDuration"), 14, 0, 10000);
		duration->setSuffix(tr(" × 0.1 s"));
		timingForm->addRow(duration);
		right->addWidget(timing);
		auto* options = new QGroupBox(tr("Option"), general);
		auto* optionsLayout = new QVBoxLayout(options);
		optionsLayout->addWidget(check(options, cmd, QStringLiteral("pictureWait"), tr("Wait for completion"), 15, 1));
		right->addWidget(options);
	}
	left->addStretch();
	right->addStretch();

	auto* attributes = new QWidget(tabs);
	auto* attributesLayout = new QVBoxLayout(attributes);
	tabs->addTab(attributes, tr("Additional Attributes"));
	auto* color = new QGroupBox(tr("Color Tint"), attributes);
	auto* colorForm = new QFormLayout(color);
	QStringList colorNames = {tr("Red"), tr("Green"), tr("Blue"), tr("Saturation")};
	QList<RpgSpinBox*> tint;
	for (int index = 0; index < colorNames.size(); ++index) {
		auto* value = spin(color, cmd, QStringLiteral("pictureColor%1").arg(index), 8 + index, 0, 200, 100);
		value->setSuffix(tr("%"));
		colorForm->addRow(colorNames[index], value);
		tint.append(value);
	}
	auto* sample = new QLabel(tr("Sample"), color);
	sample->setAutoFillBackground(true);
	sample->setAlignment(Qt::AlignCenter);
	colorForm->addRow(sample);
	auto updateSample = [=] {
		const auto tintColor = QColor(std::clamp(tint[0]->value() * 255 / 200, 0, 255),
			std::clamp(tint[1]->value() * 255 / 200, 0, 255), std::clamp(tint[2]->value() * 255 / 200, 0, 255));
		auto palette = sample->palette();
		palette.setColor(QPalette::Window, tintColor);
		palette.setColor(QPalette::WindowText, tintColor.lightness() > 128 ? Qt::black : Qt::white);
		sample->setPalette(palette);
	};
	for (auto* value : tint) {
		connect(value, &QSpinBox::valueChanged, this, updateSample);
	}
	updateSample();
	attributesLayout->addWidget(color);
	auto* effects = new QGroupBox(tr("Effects"), attributes);
	auto* effectsForm = new QFormLayout(effects);
	auto* effect = new QComboBox(effects);
	effect->setObjectName(QStringLiteral("pictureEffect"));
	effect->addItem(tr("None"), 0);
	effect->addItem(tr("Rotation"), 1);
	effect->addItem(tr("Waver"), 2);
	bindCombo(effect, this, cmd, 12);
	auto* strength = spin(effects, cmd, QStringLiteral("pictureEffectPower"), 13, 0, 10000);
	auto* powerLabel = new QLabel(effect->currentData().toInt() == 1 ? tr("Speed") : tr("Magnitude"), effects);
	effectsForm->addRow(tr("Effect"), effect);
	effectsForm->addRow(powerLabel, strength);
	strength->setEnabled(effect->currentData().toInt() != 0);
	connect(effect, &QComboBox::currentIndexChanged, this, [=](int) {
		const int mode = effect->currentData().toInt();
		strength->setEnabled(mode != 0);
		powerLabel->setText(mode == 1 ? tr("Speed") : tr("Magnitude"));
	});
	attributesLayout->addWidget(effects);
	attributesLayout->addStretch();
}

ErasePictureWidget::ErasePictureWidget(ProjectData& project, QWidget* parent) : EventCommandBaseWidget(project, parent) {
	setWindowTitle(tr("Erase Picture"));
}

void ErasePictureWidget::setData(lcf::rpg::EventCommand* cmd) {
	EventCommandBaseWidget::setData(cmd);
	auto* layout = new QVBoxLayout(this);
	auto* group = new QGroupBox(tr("Picture No."), this);
	auto* form = new QFormLayout(group);
	auto* id = spin(group, cmd, QStringLiteral("erasePictureId"), 0, 1, 9999, 1);
	form->addRow(tr("Picture No."), id);
	// The optional ID selector belongs to later versions; do not add it to
	// a legacy command merely by opening its editor.
	if (cmd->parameters.size() > 1) {
		auto* kind = new QComboBox(group);
		kind->setObjectName(QStringLiteral("erasePictureMode"));
		kind->addItem(tr("Specific Picture"), 0);
		kind->addItem(tr("Variable Reference"), 1);
		kind->addItem(tr("Range"), 2);
		const int currentMode = parameter(cmd, 1);
		if (currentMode == 3) {
			kind->addItem(tr("Range from Variables"), 3);
		} else if (currentMode == 4) {
			kind->addItem(tr("Indirect Variable Reference"), 4);
		} else if (currentMode == 5) {
			kind->addItem(tr("All Pictures"), 5);
		}
		bindCombo(kind, this, cmd, 1);
		form->insertRow(0, tr("Selection"), kind);
		auto* lastId = spin(group, cmd, QStringLiteral("erasePictureLastId"), 2, 1, 9999, parameter(cmd, 0, 1));
		form->addRow(tr("Through Picture No."), lastId);
		auto* variable = new VariableRpgComboBox(group);
		auto* lastVariable = new VariableRpgComboBox(group);
		variable->makeModel(m_project);
		lastVariable->makeModel(m_project);
		variable->comboBox()->setObjectName(QStringLiteral("erasePictureVariable"));
		lastVariable->comboBox()->setObjectName(QStringLiteral("erasePictureLastVariable"));
		variable->setCurrentIndex(parameter(cmd, 0, 1) - 1);
		lastVariable->setCurrentIndex(parameter(cmd, 2, 1) - 1);
		form->addRow(tr("Variable"), variable);
		form->addRow(tr("Through Variable"), lastVariable);
		connect(variable->comboBox(), &QComboBox::currentIndexChanged, this, [=](int index) {
			if (index >= 0) {
				setParameter(cmd, 0, index + 1);
			}
		});
		connect(lastVariable->comboBox(), &QComboBox::currentIndexChanged, this, [=](int index) {
			if (index >= 0) {
				setParameter(cmd, 2, index + 1);
			}
		});
		auto update = [=](int mode) {
			form->setRowVisible(id, mode == 0 || mode == 2);
			form->setRowVisible(lastId, mode == 2);
			form->setRowVisible(variable, mode == 1 || mode == 3 || mode == 4);
			form->setRowVisible(lastVariable, mode == 3);
		};
		update(currentMode);
		connect(kind, &QComboBox::currentIndexChanged, this, [=](int index) {
			if (index < 0) {
				return;
			}
			const int mode = kind->currentData().toInt();
			update(mode);
			if (mode == 0 || mode == 2) {
				setParameter(cmd, 0, id->value());
				if (mode == 2) {
					setParameter(cmd, 2, lastId->value());
				}
			} else if (mode == 1 || mode == 3 || mode == 4) {
				if (variable->currentIndex() >= 0) {
					setParameter(cmd, 0, variable->currentIndex() + 1);
				}
				if (mode == 3 && lastVariable->currentIndex() >= 0) {
					setParameter(cmd, 2, lastVariable->currentIndex() + 1);
				}
			}
		});
		const bool rpg2003 = m_project.database().system.ldb_id == 2003;
		kind->setEnabled(rpg2003);
		lastId->setEnabled(rpg2003);
		variable->setEnabled(rpg2003);
		lastVariable->setEnabled(rpg2003);
	}
	layout->addWidget(group);
}

ScreenTransitionWidget::ScreenTransitionWidget(ProjectData& project, bool show, QWidget* parent) :
	EventCommandBaseWidget(project, parent), m_show(show) {}

EraseScreenWidget::EraseScreenWidget(ProjectData& project, QWidget* parent) : ScreenTransitionWidget(project, false, parent) {
	setWindowTitle(tr("Erase Screen"));
}

ShowScreenWidget::ShowScreenWidget(ProjectData& project, QWidget* parent) : ScreenTransitionWidget(project, true, parent) {
	setWindowTitle(tr("Show Screen"));
}

void ScreenTransitionWidget::setData(lcf::rpg::EventCommand* cmd) {
	EventCommandBaseWidget::setData(cmd);
	auto* layout = new QVBoxLayout(this);
	auto* section = new QGroupBox(tr("Transition Method"), this);
	auto* form = new QFormLayout(section);
	auto* transition = new QComboBox(section);
	transition->setObjectName(QStringLiteral("screenTransition"));
	fillTransitions(transition, m_show, true, false);
	bindCombo(transition, this, cmd, 0, -1);
	form->addRow(transition);
	layout->addWidget(section);
}

PanScreenWidget::PanScreenWidget(ProjectData& project, QWidget* parent) : EventCommandBaseWidget(project, parent) {
	setWindowTitle(tr("Pan Screen"));
}

void PanScreenWidget::setData(lcf::rpg::EventCommand* cmd) {
	EventCommandBaseWidget::setData(cmd);
	auto* layout = new QVBoxLayout(this);
	auto* operation = new QGroupBox(tr("Operation"), this);
	auto* operationLayout = new QVBoxLayout(operation);
	auto* operations = new QButtonGroup(operation);
	QStringList operationNames = {tr("Lock Pan State"), tr("Unlock Pan State"), tr("Pan Screen"), tr("Return to Origin")};
	for (int index = 0; index < operationNames.size(); ++index) {
		auto* radio = new QRadioButton(operationNames[index], operation);
		radio->setObjectName(QStringLiteral("panOperation%1").arg(index));
		operations->addButton(radio, index);
		operationLayout->addWidget(radio);
	}
	if (auto* button = operations->button(parameter(cmd, 0, 2))) {
		button->setChecked(true);
	}
	layout->addWidget(operation);
	auto* settings = new QGroupBox(tr("Direction and Distance"), this);
	auto* form = new QFormLayout(settings);
	auto* direction = new QComboBox(settings);
	direction->setObjectName(QStringLiteral("panDirection"));
	direction->addItem(tr("Up"), 0);
	direction->addItem(tr("Left"), 3);
	direction->addItem(tr("Right"), 1);
	direction->addItem(tr("Down"), 2);
	bindCombo(direction, this, cmd, 1);
	auto* distance = spin(settings, cmd, QStringLiteral("panDistance"), 2, 0, 9999, 1);
	auto* speed = new QComboBox(settings);
	speed->setObjectName(QStringLiteral("panSpeed"));
	QStringList speedNames = {tr("1: One-Eighth Normal"), tr("2: One-Fourth Normal"), tr("3: One-Half Normal"),
		tr("4: Normal"), tr("5: Twice Normal"), tr("6: Four Times Normal")};
	for (int index = 0; index < speedNames.size(); ++index) {
		speed->addItem(speedNames[index], index + 1);
	}
	bindCombo(speed, this, cmd, 3, 4);
	auto* wait = check(settings, cmd, QStringLiteral("panWait"), tr("Wait for completion"), 4, 1);
	form->addRow(tr("Direction"), direction);
	form->addRow(tr("Tiles"), distance);
	form->addRow(tr("Transition Speed"), speed);
	form->addRow(wait);
	layout->addWidget(settings);
	auto update = [=](int operationId) {
		direction->setEnabled(operationId == 2);
		distance->setEnabled(operationId == 2);
		speed->setEnabled(operationId == 2 || operationId == 3);
		wait->setEnabled(operationId == 2 || operationId == 3);
	};
	update(parameter(cmd, 0, 2));
	connect(operations, &QButtonGroup::idToggled, this, [=](int id, bool selected) {
		if (selected) {
			setParameter(cmd, 0, id);
			update(id);
		}
	});
}

ChangeScreenTransitionsWidget::ChangeScreenTransitionsWidget(ProjectData& project, QWidget* parent) :
	EventCommandBaseWidget(project, parent) {
	setWindowTitle(tr("Change Screen Transitions"));
}

void ChangeScreenTransitionsWidget::setData(lcf::rpg::EventCommand* cmd) {
	EventCommandBaseWidget::setData(cmd);
	auto* layout = new QVBoxLayout(this);
	auto* event = new QGroupBox(tr("Event"), this);
	auto* eventForm = new QFormLayout(event);
	auto* context = new QComboBox(event);
	context->setObjectName(QStringLiteral("transitionEvent"));
	QStringList contexts = {tr("Teleport/Erase"), tr("Teleport/Show"), tr("Begin Battle/Erase"),
		tr("Begin Battle/Show"), tr("End Battle/Erase"), tr("End Battle/Show")};
	for (int index = 0; index < contexts.size(); ++index) {
		context->addItem(contexts[index], index);
	}
	bindCombo(context, this, cmd, 0);
	eventForm->addRow(context);
	layout->addWidget(event);
	auto* associated = new QGroupBox(tr("Associated Transition"), this);
	auto* transitionForm = new QFormLayout(associated);
	auto* transition = new QComboBox(associated);
	transition->setObjectName(QStringLiteral("associatedTransition"));
	fillTransitions(transition, (parameter(cmd, 0) & 1) != 0, false, true);
	bindCombo(transition, this, cmd, 1);
	transitionForm->addRow(transition);
	layout->addWidget(associated);
	connect(context, &QComboBox::currentIndexChanged, this, [=](int) {
		const QSignalBlocker blocker(transition);
		const int value = parameter(cmd, 1);
		fillTransitions(transition, (context->currentData().toInt() & 1) != 0, false, true);
		selectValue(transition, value);
	});
}

ShowBattleAnimationWidget::ShowBattleAnimationWidget(ProjectData& project, QWidget* parent) :
	EventCommandBaseWidget(project, parent) {
	setWindowTitle(tr("Show Battle Animation"));
}

void ShowBattleAnimationWidget::setData(lcf::rpg::EventCommand* cmd) {
	EventCommandBaseWidget::setData(cmd);
	auto* layout = new QVBoxLayout(this);
	auto* selection = new QGroupBox(tr("Battle Animation"), this);
	auto* form = new QFormLayout(selection);
	auto* animation = new AnimationRpgComboBox(selection);
	animation->makeModel(m_project);
	animation->comboBox()->setObjectName(QStringLiteral("battleAnimation"));
	animation->setCurrentIndex(parameter(cmd, 0, 1) - 1);
	connect(animation->comboBox(), &QComboBox::currentIndexChanged, this, [=](int index) {
		if (index >= 0) {
			setParameter(cmd, 0, index + 1);
		}
	});
	form->addRow(tr("Battle Animation"), animation);
	form->addRow(tr("Target"), characterCombo(selection, cmd, QStringLiteral("battleAnimationTarget"), 1));
	layout->addWidget(selection);
	auto* options = new QGroupBox(tr("Options"), this);
	auto* optionsLayout = new QVBoxLayout(options);
	optionsLayout->addWidget(check(options, cmd, QStringLiteral("battleAnimationWait"), tr("Wait for completion"), 2));
	optionsLayout->addWidget(check(options, cmd, QStringLiteral("battleAnimationGlobal"), tr("Global Scope"), 3));
	layout->addWidget(options);
}

FlashSpriteWidget::FlashSpriteWidget(ProjectData& project, QWidget* parent) : EventCommandBaseWidget(project, parent) {
	setWindowTitle(tr("Flash Sprite"));
}

void FlashSpriteWidget::setData(lcf::rpg::EventCommand* cmd) {
	EventCommandBaseWidget::setData(cmd);
	auto* layout = new QVBoxLayout(this);
	auto* target = new QGroupBox(tr("Target"), this);
	auto* targetForm = new QFormLayout(target);
	targetForm->addRow(characterCombo(target, cmd, QStringLiteral("flashSpriteTarget"), 0));
	layout->addWidget(target);
	auto* color = new QGroupBox(tr("Color"), this);
	auto* colorForm = new QFormLayout(color);
	QStringList names = {tr("Red"), tr("Green"), tr("Blue"), tr("Strength")};
	QList<RpgSpinBox*> tint;
	for (int index = 0; index < names.size(); ++index) {
		auto* value = spin(color, cmd, QStringLiteral("flashSpriteColor%1").arg(index), index + 1, 0, 31, 31);
		colorForm->addRow(names[index], value);
		tint.append(value);
	}
	auto* sample = new QLabel(tr("Sample"), color);
	sample->setAutoFillBackground(true);
	sample->setAlignment(Qt::AlignCenter);
	colorForm->addRow(sample);
	auto updateSample = [=] {
		const auto flashColor = QColor(std::clamp(tint[0]->value() * 255 / 31, 0, 255),
			std::clamp(tint[1]->value() * 255 / 31, 0, 255), std::clamp(tint[2]->value() * 255 / 31, 0, 255));
		auto palette = sample->palette();
		palette.setColor(QPalette::Window, flashColor);
		palette.setColor(QPalette::WindowText, flashColor.lightness() > 128 ? Qt::black : Qt::white);
		sample->setPalette(palette);
	};
	for (auto* value : tint) {
		connect(value, &QSpinBox::valueChanged, this, updateSample);
	}
	updateSample();
	layout->addWidget(color);
	auto* timing = new QGroupBox(tr("Flash Cycle Time"), this);
	auto* timingForm = new QFormLayout(timing);
	auto* duration = spin(timing, cmd, QStringLiteral("flashSpriteDuration"), 5, 0, 10000, 5);
	duration->setSuffix(tr(" × 0.1 s"));
	timingForm->addRow(duration);
	timingForm->addRow(check(timing, cmd, QStringLiteral("flashSpriteWait"), tr("Wait for completion"), 6));
	layout->addWidget(timing);
}

PlayerVisibilityWidget::PlayerVisibilityWidget(ProjectData& project, QWidget* parent) : EventCommandBaseWidget(project, parent) {
	setWindowTitle(tr("Player Visibility"));
}

void PlayerVisibilityWidget::setData(lcf::rpg::EventCommand* cmd) {
	EventCommandBaseWidget::setData(cmd);
	auto* layout = new QVBoxLayout(this);
	auto* state = new QGroupBox(tr("Transparency State"), this);
	auto* stateLayout = new QVBoxLayout(state);
	auto* choices = new QButtonGroup(state);
	auto* transparent = new QRadioButton(tr("Transparent"), state);
	auto* opaque = new QRadioButton(tr("Opaque"), state);
	transparent->setObjectName(QStringLiteral("playerTransparent"));
	opaque->setObjectName(QStringLiteral("playerOpaque"));
	choices->addButton(transparent, 0);
	choices->addButton(opaque, 1);
	stateLayout->addWidget(transparent);
	stateLayout->addWidget(opaque);
	if (auto* button = choices->button(parameter(cmd, 0, 1))) {
		button->setChecked(true);
	}
	connect(choices, &QButtonGroup::idToggled, this, [=](int value, bool selected) {
		if (selected) {
			setParameter(cmd, 0, value);
		}
	});
	layout->addWidget(state);
}
