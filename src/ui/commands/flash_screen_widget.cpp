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

#include "flash_screen_widget.h"
#include "ui_flash_screen_widget.h"

#include "ui/common/color_sample.h"
#include <QGridLayout>
#include <QSlider>
#include <QSpinBox>

FlashScreenWidget::FlashScreenWidget(ProjectData& project, QWidget *parent) :
	EventCommandBaseWidget(project, parent),
	ui(new Ui::FlashScreenWidget) {

	ui->setupUi(this);

	int i = 0;
	for (auto& button : { ui->radioOnce, ui->radioBegin, ui->radioEnd }) {
		ui->groupOptions_arg6->setId(button, i++);
	}

	connect(ui->sliderRed, &QSlider::valueChanged, ui->spinRed_arg0, &QSpinBox::setValue);
	connect(ui->sliderGreen, &QSlider::valueChanged, ui->spinGreen_arg1, &QSpinBox::setValue);
	connect(ui->sliderBlue, &QSlider::valueChanged, ui->spinBlue_arg2, &QSpinBox::setValue);
	connect(ui->sliderSat, &QSlider::valueChanged, ui->spinSat_arg3, &QSpinBox::setValue);

	connect(ui->spinRed_arg0, qOverload<int>(&QSpinBox::valueChanged), ui->sliderRed, &QSlider::setValue);
	connect(ui->spinGreen_arg1, qOverload<int>(&QSpinBox::valueChanged), ui->sliderGreen, &QSlider::setValue);
	connect(ui->spinBlue_arg2, qOverload<int>(&QSpinBox::valueChanged), ui->sliderBlue, &QSlider::setValue);
	connect(ui->spinSat_arg3, qOverload<int>(&QSpinBox::valueChanged), ui->sliderSat, &QSlider::setValue);

	// RPG Maker's sample: the flash color (0-31 per channel) at its strength
	// over a gray screen
	auto* sample = new ColorSample(this);
	if (auto* grid = qobject_cast<QGridLayout*>(ui->groupFlashColor->layout()))
		grid->addWidget(sample, 0, grid->columnCount(), grid->rowCount(), 1);
	auto update_sample = [this, sample] {
		const int strength = ui->spinSat_arg3->value();
		auto channel = [strength](int value) { return 128 + (value * 255 / 31 - 128) * strength / 31; };
		sample->setColors({QColor(channel(ui->spinRed_arg0->value()), channel(ui->spinGreen_arg1->value()), channel(ui->spinBlue_arg2->value()))});
	};
	for (QSpinBox* spin : {ui->spinRed_arg0, ui->spinGreen_arg1, ui->spinBlue_arg2, ui->spinSat_arg3})
		connect(spin, qOverload<int>(&QSpinBox::valueChanged), this, update_sample);
	update_sample();
}

FlashScreenWidget::~FlashScreenWidget() {
	delete ui;
}

void FlashScreenWidget::onParameterChanged(int index, int new_value) {
	// Flash option
	if (index == 6) {
		// Wait for Completion only for "Flash Once"
		ui->checkWait_arg5->setEnabled(new_value == 0);
		// Disable colors and duration for "End Flash"
		ui->groupFlashColor->setEnabled(new_value != 2);
		ui->spinTime_arg4->setEnabled(new_value != 2);
	}
}
