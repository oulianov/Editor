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

#include "shift_map_dialog.h"
#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

ShiftMapDialog::ShiftMapDialog(QWidget* parent) : QDialog(parent)
{
	setWindowTitle(tr("Shift Map"));

	// The radio buttons are placed like the arrows they stand for
	auto* groupDirection = new QGroupBox(tr("Direction"), this);
	m_direction = new QButtonGroup(this);
	m_direction->setObjectName(QStringLiteral("groupDirection"));
	auto* directionLayout = new QGridLayout(groupDirection);
	auto addRadio = [&](const QString& text, const QString& name, Direction direction, int row, int column) {
		auto* radio = new QRadioButton(text, groupDirection);
		radio->setObjectName(name);
		m_direction->addButton(radio, direction);
		directionLayout->addWidget(radio, row, column);
	};
	addRadio(tr("Up"), QStringLiteral("radioUp"), Up, 0, 1);
	addRadio(tr("Left"), QStringLiteral("radioLeft"), Left, 1, 0);
	addRadio(tr("Right"), QStringLiteral("radioRight"), Right, 1, 2);
	addRadio(tr("Down"), QStringLiteral("radioDown"), Down, 2, 1);
	m_direction->button(Up)->setChecked(true);

	auto* groupUnits = new QGroupBox(tr("Number of Units"), this);
	m_units = new QSpinBox(groupUnits);
	m_units->setObjectName(QStringLiteral("spinUnits"));
	m_units->setRange(1, 100);
	auto* unitsLayout = new QHBoxLayout(groupUnits);
	unitsLayout->addWidget(m_units);

	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	auto* layout = new QVBoxLayout(this);
	layout->addWidget(groupDirection);
	layout->addWidget(groupUnits);
	layout->addWidget(buttons);
}

int ShiftMapDialog::dx() const {
	switch (m_direction->checkedId()) {
	case Left:
		return -m_units->value();
	case Right:
		return m_units->value();
	default:
		return 0;
	}
}

int ShiftMapDialog::dy() const {
	switch (m_direction->checkedId()) {
	case Up:
		return -m_units->value();
	case Down:
		return m_units->value();
	default:
		return 0;
	}
}
