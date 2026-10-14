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

#pragma once

#include <QHash>
#include <QImage>
#include <QPointer>
#include <QQuickPaintedItem>
#include <QtQmlIntegration>
#include <vector>

#include "qmlbinding/json_view.h"

class ProjectData;
namespace lcf::rpg {
	class Actor;
	class Troop;
}

/**
 * The battle screen of a monster group (320 x 240, the item's own size:
 * scale it to show it larger), drawn from a database window's copy: its
 * members on a backdrop, moved with the mouse (TroopLayout), the selected
 * one framed, the hidden ones faint. The Monster Groups page edits them;
 * the Battle Screen page shows them read-only behind a hero placed with
 * the mouse at their battle position (manual placement).
 */
class TroopCanvas : public QQuickPaintedItem
{
	Q_OBJECT
	QML_ELEMENT
	/** The root of the window's database (its copy is drawn and edited) */
	Q_PROPERTY(JsonView* database READ database WRITE setDatabase NOTIFY databaseChanged)
	/** The index of the monster group, -1 for none */
	Q_PROPERTY(int troop READ troop WRITE setTroop NOTIFY troopChanged)
	/** The members are only shown */
	Q_PROPERTY(bool readOnly READ readOnly WRITE setReadOnly NOTIFY readOnlyChanged)
	/** The member framed, -1 for none */
	Q_PROPERTY(int selectedMember READ selectedMember WRITE setSelectedMember NOTIFY selectedMemberChanged)
	/** A picture of the Backdrop folder behind them, shown here only */
	Q_PROPERTY(QString backdrop READ backdrop WRITE setBackdrop NOTIFY backdropChanged)
	/** The index of the hero shown at their battle position, -1 for none */
	Q_PROPERTY(int hero READ hero WRITE setHero NOTIFY heroChanged)
	/** The hero is moved with the mouse (manual placement) */
	Q_PROPERTY(bool heroMovable READ heroMovable WRITE setHeroMovable NOTIFY heroChanged)
	/** The hero is shown in the front row: half a battler to the left, as the Player */
	Q_PROPERTY(bool heroFrontRow READ heroFrontRow WRITE setHeroFrontRow NOTIFY heroChanged)
	/** The pointer is over the hero */
	Q_PROPERTY(bool heroHovered READ heroHovered NOTIFY heroHoveredChanged)

public:
	explicit TroopCanvas(QQuickItem* parent = nullptr);

	JsonView* database() const;
	void setDatabase(JsonView* database);
	int troop() const;
	void setTroop(int troop);
	bool readOnly() const;
	void setReadOnly(bool readOnly);
	int selectedMember() const;
	void setSelectedMember(int member);
	QString backdrop() const;
	void setBackdrop(const QString& backdrop);
	int hero() const;
	void setHero(int hero);
	bool heroMovable() const;
	void setHeroMovable(bool movable);
	bool heroFrontRow() const;
	void setHeroFrontRow(bool front);
	bool heroHovered() const;

	/** The member drawn at a point of the screen (the one on top), -1 for none */
	Q_INVOKABLE int memberAt(qreal x, qreal y) const;
	/** Reads the pictures again (the monsters or heroes changed) */
	Q_INVOKABLE void refresh();

	void paint(QPainter* painter) override;

signals:
	void databaseChanged();
	void troopChanged();
	void readOnlyChanged();
	void selectedMemberChanged();
	void backdropChanged();
	void heroChanged();
	void heroHoveredChanged();
	/** A member was moved with the mouse (its position is in the copy) */
	void memberMoved(int member);
	/** The hero was moved (their battle position is in the copy) */
	void heroMoved();
	/** The right button on a member (selected first), at a point of the item */
	void contextMenuRequested(int member, qreal x, qreal y);

protected:
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void hoverMoveEvent(QHoverEvent* event) override;
	void hoverLeaveEvent(QHoverEvent* event) override;

private:
	ProjectData* project() const;
	lcf::rpg::Troop* findTroop() const;
	lcf::rpg::Actor* findHero() const;
	void onDataChanged(const QString& jsonPtr);
	/** Where the hero's picture is drawn (empty without a hero) */
	QRectF heroRect() const;
	int heroRowOffset() const;
	void setHovered(bool hovered);
	void notify(const QString& path) const;

	QPointer<JsonView> m_database;
	QMetaObject::Connection m_connection;
	int m_troop = -1;
	bool m_readOnly = false;
	int m_selected = -1;
	QString m_backdropName;
	QImage m_backdrop;
	/** The pictures of the members, in their order */
	std::vector<QImage> m_members;
	std::vector<bool> m_transparent;
	int m_hero = -1;
	bool m_heroMovable = false;
	bool m_heroFrontRow = true;
	bool m_heroHovered = false;
	QImage m_heroImage;
	/** What the left button drags: a member, the hero (-2), nothing (-1) */
	int m_dragging = -1;
	QPointF m_grab;
};
