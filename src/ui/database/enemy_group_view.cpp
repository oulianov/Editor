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

#include "enemy_group_view.h"
#include "enemy_graphics.h"
#include "model/project_data.h"
#include <QContextMenuEvent>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QMenu>
#include <algorithm>

namespace {
	constexpr int kScreenWidth = 320;
	constexpr int kScreenHeight = 240;

	/** A member on the battle screen; its position is the center of the
	 *  monster, as the Player uses it */
	class MemberItem : public QGraphicsPixmapItem {
	public:
		enum { Type = UserType + 1 };

		MemberItem(lcf::rpg::Troop& troop, int index, const QPixmap& pixmap, bool transparent, bool read_only) :
			QGraphicsPixmapItem(pixmap), m_troop(troop), m_index(index), m_transparent(transparent)
		{
			const auto& member = troop.members[index];
			setOffset(-pixmap.width() / 2, -pixmap.height() / 2);
			setPos(member.x, member.y);
			if (!read_only) {
				setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
			}
			updateOpacity();
		}

		int type() const override {
			return Type;
		}

		int index() const {
			return m_index;
		}

		void updateOpacity() {
			// Hidden members are drawn faint, like in RPG Maker
			setOpacity(m_troop.members[m_index].invisible ? 0.35 : (m_transparent ? 0.6 : 1.0));
		}

	protected:
		QVariant itemChange(GraphicsItemChange change, const QVariant& value) override {
			if (change == ItemPositionChange) {
				QPointF p = value.toPointF();
				return QPointF(std::clamp(qRound(p.x()), 0, kScreenWidth), std::clamp(qRound(p.y()), 0, kScreenHeight));
			}
			if (change == ItemPositionHasChanged) {
				auto& member = m_troop.members[m_index];
				member.x = qRound(pos().x());
				member.y = qRound(pos().y());
			}
			return QGraphicsPixmapItem::itemChange(change, value);
		}

	private:
		lcf::rpg::Troop& m_troop;
		int m_index;
		bool m_transparent;
	};
}

EnemyGroupView::EnemyGroupView(QWidget* parent) : QGraphicsView(parent) {
	setScene(new QGraphicsScene(this));
	scene()->setSceneRect(0, 0, kScreenWidth, kScreenHeight);
	scene()->setBackgroundBrush(QColor(32, 32, 48));
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setMinimumSize(kScreenWidth / 2, kScreenHeight / 2);

	m_backdrop = new QGraphicsPixmapItem();
	m_backdrop->setZValue(-1);
	scene()->addItem(m_backdrop);
}

void EnemyGroupView::setData(const ProjectData* project, lcf::rpg::Troop* troop) {
	m_project = project;
	m_troop = troop;
	refresh();
}

void EnemyGroupView::refresh() {
	for (auto* item : scene()->items()) {
		if (item->type() == MemberItem::Type) {
			delete item;
		}
	}
	if (!m_troop || !m_project) {
		return;
	}

	const auto& enemies = m_project->database().enemies;
	for (int i = 0; i < static_cast<int>(m_troop->members.size()); ++i) {
		int enemy_id = m_troop->members[i].enemy_id;
		QPixmap pixmap;
		bool transparent = false;
		if (enemy_id >= 1 && enemy_id <= static_cast<int>(enemies.size())) {
			const auto& enemy = enemies[enemy_id - 1];
			pixmap = EnemyGraphics::load(*m_project, enemy);
			transparent = enemy.transparent;
		}
		if (pixmap.isNull()) {
			// Still something to select and move
			pixmap = QPixmap(32, 32);
			pixmap.fill(QColor(160, 160, 160, 160));
		}
		scene()->addItem(new MemberItem(*m_troop, i, pixmap, transparent, m_readOnly));
	}
}

void EnemyGroupView::setBackdrop(const QPixmap& backdrop) {
	m_backdrop->setPixmap(backdrop.isNull() ? backdrop : backdrop.scaled(kScreenWidth, kScreenHeight));
}

void EnemyGroupView::setReadOnly(bool read_only) {
	m_readOnly = read_only;
	refresh();
}

int EnemyGroupView::selectedMember() const {
	for (auto* item : scene()->selectedItems()) {
		if (item->type() == MemberItem::Type) {
			return static_cast<MemberItem*>(item)->index();
		}
	}
	return -1;
}

void EnemyGroupView::selectMember(int index) {
	scene()->clearSelection();
	for (auto* item : scene()->items()) {
		if (item->type() == MemberItem::Type && static_cast<MemberItem*>(item)->index() == index) {
			item->setSelected(true);
		}
	}
}

void EnemyGroupView::resizeEvent(QResizeEvent* event) {
	QGraphicsView::resizeEvent(event);
	fitInView(sceneRect(), Qt::KeepAspectRatio);
}

void EnemyGroupView::contextMenuEvent(QContextMenuEvent* event) {
	QGraphicsItem* item = itemAt(event->pos());
	if (!item || item->type() != MemberItem::Type || !m_troop || m_readOnly) {
		return;
	}
	auto* member_item = static_cast<MemberItem*>(item);
	auto& member = m_troop->members[member_item->index()];
	selectMember(member_item->index());

	QMenu menu(this);
	auto* invisible = menu.addAction(tr("Invisible"));
	invisible->setCheckable(true);
	invisible->setChecked(member.invisible);
	if (menu.exec(event->globalPos()) == invisible) {
		member.invisible = !member.invisible;
		member_item->updateOpacity();
	}
}
