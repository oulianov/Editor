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

#include "picker_charset_widget.h"
#include "ui/viewer/rpg_graphics_view.h"
#include <QButtonGroup>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QRadioButton>
#include <QVBoxLayout>

PickerCharsetWidget::PickerCharsetWidget(int index, int direction, int pattern, QWidget* parent) :
	PickerChildWidget(parent), m_index(index), m_direction(std::clamp(direction, 0, 3)),
	m_pattern(std::clamp(pattern, 0, 2)), m_pose(true)
{
	// The groups of RPG Maker's Select Event Graphic window
	auto* layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	auto add_group = [&](const QString& title, const QString& name, const QStringList& options, int checked) {
		auto* box = new QGroupBox(title, this);
		auto* row = new QHBoxLayout(box);
		auto* group = new QButtonGroup(box);
		for (int i = 0; i < options.size(); ++i) {
			auto* radio = new QRadioButton(options[i], box);
			radio->setObjectName(name + QString::number(i));
			radio->setChecked(i == checked);
			group->addButton(radio, i);
			row->addWidget(radio);
		}
		row->addStretch(1);
		layout->addWidget(box);
		return group;
	};
	m_facing = add_group(tr("Facing"), QStringLiteral("radioFacing"), {tr("Up"), tr("Right"), tr("Down"), tr("Left")}, m_direction);
	m_frame = add_group(tr("Frame"), QStringLiteral("radioFrame"), {tr("Left"), tr("Middle"), tr("Right")}, m_pattern);
	layout->addStretch(1);
	connect(m_facing, &QButtonGroup::idClicked, this, [this](int id) {
		m_direction = id;
		updateRect();
	});
	connect(m_frame, &QButtonGroup::idClicked, this, [this](int id) {
		m_pattern = id;
		updateRect();
	});
}

void PickerCharsetWidget::clicked(const QPointF& pos) {
	int x = static_cast<int>(pos.x() / 24);
	int y = static_cast<int>(pos.y() / 32);
	if (x < 0 || y < 0) {
		return;
	}
	if (m_pose) {
		// a cell of the sheet: 4 x 2 characters of 3 frames x 4 facings
		if (x >= 12 || y >= 8) {
			return;
		}
		m_index = (y / 4) * 4 + x / 3;
		m_direction = y % 4;
		m_pattern = x % 3;
		m_facing->button(m_direction)->setChecked(true);
		m_frame->button(m_pattern)->setChecked(true);
	} else {
		m_index = y * 4 + x;
	}
	updateRect();
}

void PickerCharsetWidget::imageChanged(QPixmap image) {
	if (m_pose) {
		// the whole sheet, at its size: every pose can be clicked
		if (!m_pixmap) {
			m_view->setMinimumSize(24 * 12 + 4, 32 * 8 + 4);
			m_view->setMaximumSize(24 * 12 + 4, 32 * 8 + 4);
			m_pixmap = new QGraphicsPixmapItem();
		}
		m_pixmap->setPixmap(image);
		m_view->setItem(m_pixmap);
		updateRect();
		return;
	}
	if (!m_pixmap) {
		m_view->setMinimumSize(196, 132);
		m_view->setMaximumSize(196, 132);
		m_view->scale(2.0, 2.0);
		m_pixmap = new QGraphicsPixmapItem();
	}

	QPixmap new_image(24 * 4, 32 * 2);

	QPainter* paint = new QPainter(&new_image);

	for (int i = 0; i < 8; ++i) {
		int src_x = 24 + ((i % 4) * 24 * 3);
		int src_y = 2 * 32;
		if (i >= 4) {
			src_y = 6 * 32;
		}

		int target_x = (i % 4) * 24;
		int target_y = i >= 4 ? 32 : 0;
		paint->drawPixmap(QRectF(target_x, target_y, 24, 32), image, QRectF(src_x, src_y, 24, 32));
	}
	delete paint;

	m_pixmap->setPixmap(new_image);
	m_view->setItem(m_pixmap);
	updateRect();
}

void PickerCharsetWidget::updateRect() {
	if (!m_rect) {
		m_rect = new QGraphicsRectItem();

		QPen selPen(Qt::white);
		selPen.setWidth(2);
		m_rect->setPen(selPen);

		m_view->scene()->addItem(m_rect);
	}

	QRect rect = {
		(m_index % 4) * 24,
		m_index / 4 * 32,
		24,
		32
	};
	if (m_pose) {
		rect.moveTo(((m_index % 4) * 3 + m_pattern) * 24, ((m_index / 4) * 4 + m_direction) * 32);
	}

	m_rect->setRect(rect);
}
