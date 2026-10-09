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
#include "ui/event/event_command_palette.h"
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
	ManiacCommandWidget(project, parent), m_move(move) {}

ShowPictureWidget::ShowPictureWidget(ProjectData& project, QWidget* parent) : PictureWidget(project, false, parent) {
	setWindowTitle(tr("Show Picture"));
}

MovePictureWidget::MovePictureWidget(ProjectData& project, QWidget* parent) : PictureWidget(project, true, parent) {
	setWindowTitle(tr("Move Picture"));
}

void PictureWidget::growParameters(int index) {
	// The parameters of RPG Maker 2003 1.12 and Maniac Patch: a shorter
	// command gets the values EasyRPG Player uses when they are missing
	const int full = m_move ? 22 : 30;
	const int first = m_move ? 16 : 14;
	std::vector<int32_t> parameters(m_cmd->parameters.begin(), m_cmd->parameters.end());
	const int size = static_cast<int>(parameters.size());
	if (size < full && index >= first) {
		parameters.resize(full, 0);
		if (size <= first) {
			// the bottom half as transparent as the top one
			parameters[first] = ::parameter(m_cmd, 6);
		}
		if (!m_move) {
			// erased on map change, flashed and shaken with the screen
			parameters[29] = 1 | 32 | 64;
		}
	}
	parameters.resize(std::max<size_t>(parameters.size(), index + 1), 0);
	m_cmd->parameters = lcf::DBArray<int32_t>(parameters.begin(), parameters.end());
}

void PictureWidget::setData(lcf::rpg::EventCommand* cmd) {
	EventCommandBaseWidget::setData(cmd);
	const bool rpg2003 = m_project.database().system.ldb_id == 2003;
	const bool maniac = this->maniac();
	// low byte: bottom transparency; above: Maniac Patch blend mode and flips
	const int flags = m_move ? 16 : 14;
	// a command written with Maniac Patch shows its options in any game
	const bool showManiac = maniac || (::parameter(cmd, 1) >> 8) != 0 || (::parameter(cmd, flags) >> 8) != 0
		|| ::parameter(cmd, 12) == 3 || (m_move && (::parameter(cmd, 15) & ~1) != 0);
	const auto modes = numberModes();
	// a value whose mode (number, variable...) EasyRPG Player reads in bits of another parameter
	auto modeValue = [&](const QString& name, int modeIndex, int shift, int bits, int index, int minimum, int maximum) {
		auto* row = new QWidget(this);
		auto* rowLayout = new QHBoxLayout(row);
		rowLayout->setContentsMargins(0, 0, 0, 0);
		auto* mode = modeComboAt(name + QStringLiteral("Mode"), modeIndex, shift, bits, modes);
		mode->setEnabled(maniac);
		rowLayout->addWidget(mode);
		rowLayout->addWidget(valueField(name, mode, index, minimum, maximum), 1);
		return row;
	};
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
	if (showManiac) {
		// the mode of the id: p17, its low byte for Move Picture
		pictureForm->addRow(tr("Picture No."), modeValue(QStringLiteral("pictureId"), 17, 0, m_move ? 8 : 4, 0, 1, 9999));
	} else {
		pictureForm->addRow(tr("Picture No."), spin(picture, cmd, QStringLiteral("pictureId"), 0, 1, 9999, 1));
	}
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
	auto* modeButtons = new QButtonGroup(coordinates);
	auto* specific = new QRadioButton(tr("Specific Coordinates"), coordinates);
	auto* variables = new QRadioButton(tr("Variable Reference"), coordinates);
	specific->setObjectName(QStringLiteral("pictureCoordinatesConstant"));
	variables->setObjectName(QStringLiteral("pictureCoordinatesVariable"));
	modeButtons->addButton(specific, 0);
	modeButtons->addButton(variables, 1);
	coordinateLayout->addWidget(specific);
	coordinateLayout->addWidget(variables);
	if (showManiac) {
		// Maniac Patch: the variables hold the ids of the variables of X and Y
		auto* pointers = new QRadioButton(tr("Variable Pointer"), coordinates);
		pointers->setObjectName(QStringLiteral("pictureCoordinatesPointer"));
		pointers->setEnabled(maniac);
		modeButtons->addButton(pointers, 2);
		coordinateLayout->addWidget(pointers);
	}
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
	xVariable->setCurrentIndex(::parameter(cmd, 2, 1) - 1);
	yVariable->setCurrentIndex(::parameter(cmd, 3, 1) - 1);
	variablesForm->addRow(tr("X"), xVariable);
	variablesForm->addRow(tr("Y"), yVariable);
	coordinatePages->addWidget(variablesPage);
	const int positionMode = ::parameter(cmd, 1) & 0xff;
	if (auto* button = modeButtons->button(positionMode)) {
		button->setChecked(true);
	}
	coordinatePages->setCurrentIndex(positionMode == 1 || positionMode == 2 ? 1 : 0);
	connect(modeButtons, &QButtonGroup::idToggled, this, [=](int mode, bool selected) {
		if (!selected) {
			return;
		}
		// the upper bits are the origin of Maniac Patch
		::setParameter(cmd, 1, mode, 0xff);
		coordinatePages->setCurrentIndex(mode == 0 ? 0 : 1);
		if (mode == 0) {
			::setParameter(cmd, 2, x->value());
			::setParameter(cmd, 3, y->value());
		} else {
			if (xVariable->currentIndex() >= 0) {
				::setParameter(cmd, 2, xVariable->currentIndex() + 1);
			}
			if (yVariable->currentIndex() >= 0) {
				::setParameter(cmd, 3, yVariable->currentIndex() + 1);
			}
		}
	});
	connect(xVariable->comboBox(), &QComboBox::currentIndexChanged, this, [=](int index) {
		if (index >= 0 && modeButtons->checkedId() > 0) {
			::setParameter(cmd, 2, index + 1);
		}
	});
	connect(yVariable->comboBox(), &QComboBox::currentIndexChanged, this, [=](int index) {
		if (index >= 0 && modeButtons->checkedId() > 0) {
			::setParameter(cmd, 3, index + 1);
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
	if (showManiac) {
		// p20: width mode, bit 4 for a separate height, then the height mode
		magnificationForm->addRow(modeValue(QStringLiteral("pictureMagnification"), 20, 0, 4, 5, 0, 2000));
		auto* separate = flag(QStringLiteral("pictureSeparateHeight"), tr("Separate height"), 20, 4);
		auto* height = modeValue(QStringLiteral("pictureHeight"), 20, 5, 4, m_move ? 18 : 31, 0, 2000);
		separate->setEnabled(maniac);
		height->setEnabled(maniac && separate->isChecked());
		connect(separate, &QCheckBox::toggled, height, [height, maniac](bool checked) { height->setEnabled(maniac && checked); });
		magnificationForm->addRow(separate);
		magnificationForm->addRow(tr("Height"), height);
	} else {
		auto* scale = spin(magnification, cmd, QStringLiteral("pictureMagnification"), 5, 0, 2000, 100);
		scale->setSuffix(tr("%"));
		magnificationForm->addRow(scale);
	}
	right->addWidget(magnification);
	auto* transparency = new QGroupBox(tr("Transparency"), general);
	auto* transparencyForm = new QFormLayout(transparency);
	auto* bottom = spin(transparency, cmd, QStringLiteral("pictureBottomTransparency"), flags,
		0, 100, ::parameter(cmd, 6), 0xff);
	bottom->setSuffix(tr("%"));
	bottom->setEnabled(rpg2003);
	if (showManiac) {
		transparencyForm->addRow(tr("Top Half"), modeValue(QStringLiteral("pictureTopTransparency"), 21, 0, 32, 6, 0, 100));
	} else {
		auto* top = spin(transparency, cmd, QStringLiteral("pictureTopTransparency"), 6, 0, 100);
		top->setSuffix(tr("%"));
		transparencyForm->addRow(tr("Top Half"), top);
	}
	transparencyForm->addRow(tr("Bottom Half"), bottom);
	right->addWidget(transparency);
	if (m_move) {
		auto* timing = new QGroupBox(tr("Transition Time"), general);
		auto* timingForm = new QFormLayout(timing);
		if (showManiac) {
			// the mode of the duration: slot 2 of p17
			timingForm->addRow(tr("Tenths of a second"), modeValue(QStringLiteral("pictureDuration"), 17, 8, 4, 14, -10000, 10000));
		} else {
			auto* duration = spin(timing, cmd, QStringLiteral("pictureDuration"), 14, 0, 10000);
			duration->setSuffix(tr(" × 0.1 s"));
			timingForm->addRow(duration);
		}
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
	QComboBox* colorMode = nullptr;
	if (showManiac) {
		// one mode for the four values: slot 3 of p17
		colorMode = modeComboAt(QStringLiteral("pictureColorMode"), 17, 12, 4, modes);
		colorMode->setEnabled(maniac);
		colorForm->addRow(tr("Given as"), colorMode);
	}
	for (int index = 0; index < colorNames.size(); ++index) {
		const QString name = QStringLiteral("pictureColor%1").arg(index);
		if (colorMode) {
			colorForm->addRow(colorNames[index], valueField(name, colorMode, 8 + index, 0, 200));
			tint.append(findChild<RpgSpinBox*>(name));
		} else {
			auto* value = spin(color, cmd, name, 8 + index, 0, 200, 100);
			value->setSuffix(tr("%"));
			colorForm->addRow(colorNames[index], value);
			tint.append(value);
		}
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
	if (showManiac) {
		effect->addItem(tr("Fixed Angle"), 3);
	}
	bindCombo(effect, this, cmd, 12);
	// Maniac Patch, fixed angle: angle / divisor, their modes in p16 (p4 for Move Picture)
	const int angleModes = m_move ? 4 : 16;
	QWidget* strength = showManiac ? modeValue(QStringLiteral("pictureEffectPower"), angleModes, 0, 4, 13, -10000, 10000)
		: spin(effects, cmd, QStringLiteral("pictureEffectPower"), 13, 0, 10000);
	auto powerLabel = [](int mode) {
		return mode == 1 ? tr("Speed") : mode == 3 ? tr("Angle") : tr("Magnitude");
	};
	auto* powerName = new QLabel(powerLabel(effect->currentData().toInt()), effects);
	effectsForm->addRow(tr("Effect"), effect);
	effectsForm->addRow(powerName, strength);
	QWidget* divisor = nullptr;
	if (showManiac) {
		divisor = modeValue(QStringLiteral("pictureAngleDivisor"), angleModes, 4, 4, m_move ? 7 : 15, -10000, 10000);
		effectsForm->addRow(tr("Divisor"), divisor);
		effectsForm->setRowVisible(divisor, effect->currentData().toInt() == 3);
	}
	strength->setEnabled(effect->currentData().toInt() != 0);
	connect(effect, &QComboBox::currentIndexChanged, this, [=](int) {
		const int mode = effect->currentData().toInt();
		strength->setEnabled(mode != 0);
		powerName->setText(powerLabel(mode));
		if (divisor) {
			effectsForm->setRowVisible(divisor, mode == 3);
		}
	});
	attributesLayout->addWidget(effects);
	attributesLayout->addStretch();

	if (showManiac) {
		addManiacTab(tabs, maniac);
	}
}

void PictureWidget::addManiacTab(QTabWidget* tabs, bool enabled) {
	const int flags = m_move ? 16 : 14;
	auto* page = new QWidget(tabs);
	page->setEnabled(enabled);
	auto* columns = new QHBoxLayout(page);
	auto* left = new QVBoxLayout();
	auto* right = new QVBoxLayout();
	columns->addLayout(left, 1);
	columns->addLayout(right, 1);
	tabs->addTab(page, tr("Maniac Patch"));

	auto* display = section(left, tr("Display"));
	// the origin is above the coordinate mode (p1 >> 8), as for Show String Picture
	display->addRow(tr("Origin"), choice(QStringLiteral("pictureOrigin"), 1, {{0, tr("Center")}, {1, tr("Top left")},
		{2, tr("Bottom left")}, {3, tr("Top right")}, {4, tr("Bottom right")}, {5, tr("Top")}, {6, tr("Bottom")},
		{7, tr("Left")}, {8, tr("Right")}}, 8, 24));
	display->addRow(tr("Blend mode"), choice(QStringLiteral("pictureBlendMode"), flags, {{0, tr("Normal")},
		{1, tr("Multiply")}, {2, tr("Additive")}, {3, tr("Overlay")}}, 8, 2));
	display->addRow(flag(QStringLiteral("pictureFlipX"), tr("Flip horizontally"), flags, 12));
	display->addRow(flag(QStringLiteral("pictureFlipY"), tr("Flip vertically"), flags, 13));

	if (m_move) {
		// p15: bit 0 is "wait"; EasyRPG Player reads the others but does not use them yet
		auto* options = section(left, tr("Option"));
		const QList<std::pair<int, QString>> bits = {{1, tr("Relative to the current position")},
			{2, tr("Keep the color tint")}, {3, tr("Keep the effect")}, {4, tr("Keep the blend mode")},
			{5, tr("Keep the flips")}, {6, tr("Keep the transition time")}, {8, tr("Position of the magnified picture")}};
		for (const auto& bit : bits) {
			options->addRow(flag(QStringLiteral("pictureOption%1").arg(bit.first), bit.second, 15, bit.first));
		}
	} else {
		// slot 2 of p17: the file name, from this command or from a string (p30)
		auto* file = section(left, tr("Image File"));
		auto* mode = modeComboAt(QStringLiteral("pictureNameMode"), 17, 8, 4, stringModes());
		file->addRow(tr("Name given as"), mode);
		auto* note = new QLabel(tr("The file of the Picture tab"), this);
		file->addRow(tr("Name"), valueField(QStringLiteral("pictureName"), mode, 30, 0, 99999, note));

		auto* sheet = section(right, tr("Spritesheet"));
		sheet->addRow(tr("Columns (0: none)"), number(QStringLiteral("pictureSheetColumns"), 22, 0, 999));
		sheet->addRow(tr("Rows"), number(QStringLiteral("pictureSheetRows"), 23, 0, 999));
		// p24: 2 animates the frames; else the mode of the frame number (from 1)
		auto* frameMode = modeCombo(QStringLiteral("pictureSheetMode"), 24, -1, {{0, tr("Frame"), Kind::Number},
			{1, tr("Frame in variable"), Kind::Variable}, {2, tr("Animation"), Kind::Number}});
		sheet->addRow(tr("Show"), frameMode);
		sheet->addRow(tr("Frame or speed"), valueField(QStringLiteral("pictureSheetFrame"), frameMode, 25, 0, 99999));
		auto* once = boolean(QStringLiteral("pictureSheetOnce"), tr("Play once"), 26);
		sheet->addRow(once);
		showWhen(sheet, once, frameMode, {2});

		auto* layer = section(right, tr("Layer"));
		layer->addRow(tr("Map layer"), number(QStringLiteral("pictureMapLayer"), 27, 0, 10));
		layer->addRow(tr("Battle layer"), number(QStringLiteral("pictureBattleLayer"), 28, 0, 10));
		layer->addRow(flag(QStringLiteral("pictureEraseOnMapChange"), tr("Erase on map change"), 29, 0));
		layer->addRow(flag(QStringLiteral("pictureEraseOnBattleEnd"), tr("Erase when the battle ends"), 29, 1));
		layer->addRow(flag(QStringLiteral("pictureAffectedByTint"), tr("Affected by the screen tint"), 29, 4));
		layer->addRow(flag(QStringLiteral("pictureAffectedByFlash"), tr("Affected by the screen flash"), 29, 5));
		layer->addRow(flag(QStringLiteral("pictureAffectedByShake"), tr("Affected by the screen shake"), 29, 6));
	}
	left->addStretch();
	right->addStretch();
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
	// a legacy command merely by opening its editor (its controls only
	// write when they are changed: Maniac Patch games always show it).
	const bool maniac = EventCommandPalette::usesManiacPatch(m_project.project());
	if (cmd->parameters.size() > 1 || maniac) {
		auto* kind = new QComboBox(group);
		kind->setObjectName(QStringLiteral("erasePictureMode"));
		kind->addItem(tr("Specific Picture"), 0);
		kind->addItem(tr("Variable Reference"), 1);
		kind->addItem(tr("Range"), 2);
		const int currentMode = parameter(cmd, 1);
		// the Maniac Patch selections, as EasyRPG Player reads them
		if (maniac || currentMode == 3) {
			kind->addItem(tr("Range from Variables"), 3);
		}
		if (maniac || currentMode == 4) {
			kind->addItem(tr("Indirect Variable Reference"), 4);
		}
		if (maniac || currentMode == 5) {
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
		const bool rpg2003 = m_project.database().system.ldb_id == 2003 || maniac;
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
