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

#include "tint_screen_widget.h"
#include "ui_tint_screen_widget.h"

#include "ui/common/color_sample.h"
#include <QSlider>
#include <algorithm>
#include <QSpinBox>

TintScreenWidget::TintScreenWidget(ProjectData& project, QWidget *parent) :
	EventCommandBaseWidget(project, parent),
	ui(new Ui::TintScreenWidget) {

	ui->setupUi(this);

	connect(ui->sliderRed, &QSlider::valueChanged, ui->spinRed_arg0, &QSpinBox::setValue);
	connect(ui->sliderGreen, &QSlider::valueChanged, ui->spinGreen_arg1, &QSpinBox::setValue);
	connect(ui->sliderBlue, &QSlider::valueChanged, ui->spinBlue_arg2, &QSpinBox::setValue);
	connect(ui->sliderSat, &QSlider::valueChanged, ui->spinSat_arg3, &QSpinBox::setValue);

	connect(ui->spinRed_arg0, qOverload<int>(&QSpinBox::valueChanged), ui->sliderRed, &QSlider::setValue);
	connect(ui->spinGreen_arg1, qOverload<int>(&QSpinBox::valueChanged), ui->sliderGreen, &QSlider::setValue);
	connect(ui->spinBlue_arg2, qOverload<int>(&QSpinBox::valueChanged), ui->sliderBlue, &QSlider::setValue);
	connect(ui->spinSat_arg3, qOverload<int>(&QSpinBox::valueChanged), ui->sliderSat, &QSlider::setValue);

	// RPG Maker's sample: light, middle and dark grays with the tint, as the
	// Player applies it (each channel in %, then the saturation)
	auto* sample = new ColorSample(this);
	ui->gridLayout->addWidget(sample, 0, ui->gridLayout->columnCount(), ui->gridLayout->rowCount(), 1);
	auto update_sample = [this, sample] {
		std::vector<QColor> colors;
		for (int gray : {192, 128, 64}) {
			auto channel = [gray](int percent) { return std::clamp(gray * percent / 100, 0, 255); };
			QColor color(channel(ui->spinRed_arg0->value()), channel(ui->spinGreen_arg1->value()), channel(ui->spinBlue_arg2->value()));
			const int luma = qGray(color.rgb());
			auto saturate = [&](int c) { return std::clamp(luma + (c - luma) * ui->spinSat_arg3->value() / 100, 0, 255); };
			colors.emplace_back(saturate(color.red()), saturate(color.green()), saturate(color.blue()));
		}
		sample->setColors(colors);
	};
	for (QSpinBox* spin : {ui->spinRed_arg0, ui->spinGreen_arg1, ui->spinBlue_arg2, ui->spinSat_arg3})
		connect(spin, qOverload<int>(&QSpinBox::valueChanged), this, update_sample);
	update_sample();
}

TintScreenWidget::~TintScreenWidget() {
	delete ui;
}
