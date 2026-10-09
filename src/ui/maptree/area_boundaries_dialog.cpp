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

#include "area_boundaries_dialog.h"
#include "ui/common/map_preview.h"
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

AreaBoundariesDialog::AreaBoundariesDialog(int map_id, const QRect& area, QWidget* parent) :
	QDialog(parent)
{
	setWindowTitle(tr("Define Area Boundaries"));

	auto* hint = new QLabel(tr("Drag a box around the part of the map the area covers."), this);
	hint->setWordWrap(true);
	m_preview = new MapPreview(this);
	m_preview->setObjectName(QStringLiteral("mapPreview"));
	m_preview->setMinimumSize(240, 180);
	m_preview->setRectangleMode(true);
	const bool loaded = m_preview->showMap(map_id);
	m_status = new QLabel(this);
	m_status->setObjectName(QStringLiteral("labelBoundaries"));

	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	m_ok = buttons->button(QDialogButtonBox::Ok);
	connect(buttons, &QDialogButtonBox::accepted, this, &AreaBoundariesDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &AreaBoundariesDialog::reject);
	connect(m_preview, &MapPreview::selectionChanged, this, &AreaBoundariesDialog::updateStatus);

	auto* status_layout = new QHBoxLayout();
	status_layout->addWidget(m_preview->createZoomButtons(this));
	status_layout->addWidget(m_status, 1);
	auto* layout = new QVBoxLayout(this);
	layout->addWidget(hint);
	layout->addWidget(m_preview, 1);
	layout->addLayout(status_layout);
	layout->addWidget(buttons);

	m_preview->setSelection(area);
	if (!area.isEmpty()) {
		m_preview->centerOn(area.center().x(), area.center().y());
	}
	updateStatus();
	if (!loaded) {
		m_status->setText(tr("The map cannot be read."));
	}
	resize(760, 540);
}

QRect AreaBoundariesDialog::area() const {
	return m_preview->selection();
}

void AreaBoundariesDialog::updateStatus() {
	const QRect area = m_preview->selection();
	m_ok->setEnabled(!area.isEmpty());
	m_status->setText(area.isEmpty() ? QString()
		: tr("X: %1  Y: %2  Width: %3  Height: %4").arg(area.x()).arg(area.y()).arg(area.width()).arg(area.height()));
}
