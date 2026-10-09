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

#include "enemy_picker_widget.h"
#include "enemy_graphics.h"
#include "ui/viewer/rpg_graphics_view.h"
#include <QGraphicsPixmapItem>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

EnemyPickerWidget::EnemyPickerWidget(int hue, QWidget* parent) : PickerChildWidget(parent) {
	auto* group = new QGroupBox(tr("Color Alteration"), this);
	m_slider = new QSlider(Qt::Horizontal, group);
	m_slider->setRange(0, 359);
	m_slider->setObjectName(QStringLiteral("sliderHue"));
	m_spin = new QSpinBox(group);
	m_spin->setRange(0, 359);
	m_spin->setObjectName(QStringLiteral("spinHue"));

	auto* row = new QHBoxLayout(group);
	row->addWidget(m_slider, 1);
	row->addWidget(m_spin);

	auto* layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(group);

	m_slider->setValue(hue);
	m_spin->setValue(hue);
	connect(m_slider, &QSlider::valueChanged, m_spin, &QSpinBox::setValue);
	connect(m_spin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
		m_slider->setValue(value);
		redraw();
	});
}

int EnemyPickerWidget::hue() const {
	return m_spin->value();
}

void EnemyPickerWidget::fileChanged(const QString& path) {
	m_path = path;
}

void EnemyPickerWidget::imageChanged(QPixmap) {
	// Drawn from the file: the picker's image has no transparency nor hue
	redraw();
}

void EnemyPickerWidget::redraw() {
	if (!m_pixmap) {
		m_view->setMinimumSize(324, 244);
		m_pixmap = new QGraphicsPixmapItem();
	}
	m_pixmap->setPixmap(EnemyGraphics::load(m_path, hue()));
	m_view->setItem(m_pixmap);
}
