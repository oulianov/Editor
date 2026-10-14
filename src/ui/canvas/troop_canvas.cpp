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

#include "troop_canvas.h"
#include "common/dbstring.h"
#include "common/image_loader.h"
#include "defines.h"
#include "model/enemy_graphics.h"
#include "model/project.h"
#include "model/project_data.h"
#include "model/troop_layout.h"
#include "qmlbinding/database_session.h"
#include "render/game_graphic.h"
#include <QCursor>
#include <QPainter>
#include <algorithm>

namespace {
constexpr int kWidth = TroopLayout::ScreenWidth;
constexpr int kHeight = TroopLayout::ScreenHeight;
constexpr int kBattlerSize = 48;
/** The dragged thing: the hero */
constexpr int kHero = -2;

/** Still something to see, select and move */
QImage placeholder(int size)
{
	QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
	image.fill(QColor(160, 160, 160, 160));
	return image;
}
}

TroopCanvas::TroopCanvas(QQuickItem* parent) : QQuickPaintedItem(parent)
{
	// the pixels of the game stay square when the item is scaled
	setAntialiasing(false);
	setSmooth(false);
	setImplicitSize(kWidth, kHeight);
	setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
	setAcceptHoverEvents(true);
}

JsonView* TroopCanvas::database() const
{
	return m_database;
}

void TroopCanvas::setDatabase(JsonView* database)
{
	if (m_database == database) {
		return;
	}
	disconnect(m_connection);
	m_database = database;
	if (database && database->json()) {
		m_connection = connect(database->json(), &Json::valueChanged, this, &TroopCanvas::onDataChanged);
	}
	emit databaseChanged();
	refresh();
}

int TroopCanvas::troop() const
{
	return m_troop;
}

void TroopCanvas::setTroop(int troop)
{
	if (m_troop != troop) {
		m_troop = troop;
		m_selected = -1;
		emit troopChanged();
		emit selectedMemberChanged();
		refresh();
	}
}

bool TroopCanvas::readOnly() const
{
	return m_readOnly;
}

void TroopCanvas::setReadOnly(bool readOnly)
{
	if (m_readOnly != readOnly) {
		m_readOnly = readOnly;
		emit readOnlyChanged();
		update();
	}
}

int TroopCanvas::selectedMember() const
{
	return m_selected;
}

void TroopCanvas::setSelectedMember(int member)
{
	if (member < 0 || member >= static_cast<int>(m_members.size())) {
		member = -1;
	}
	if (m_selected != member) {
		m_selected = member;
		emit selectedMemberChanged();
		update();
	}
}

QString TroopCanvas::backdrop() const
{
	return m_backdropName;
}

void TroopCanvas::setBackdrop(const QString& backdrop)
{
	if (m_backdropName != backdrop) {
		m_backdropName = backdrop;
		emit backdropChanged();
		refresh();
	}
}

int TroopCanvas::hero() const
{
	return m_hero;
}

void TroopCanvas::setHero(int hero)
{
	if (m_hero != hero) {
		m_hero = hero;
		emit heroChanged();
		refresh();
	}
}

bool TroopCanvas::heroMovable() const
{
	return m_heroMovable;
}

void TroopCanvas::setHeroMovable(bool movable)
{
	if (m_heroMovable != movable) {
		m_heroMovable = movable;
		emit heroChanged();
		update();
	}
}

bool TroopCanvas::heroFrontRow() const
{
	return m_heroFrontRow;
}

void TroopCanvas::setHeroFrontRow(bool front)
{
	if (m_heroFrontRow != front) {
		m_heroFrontRow = front;
		emit heroChanged();
		update();
	}
}

bool TroopCanvas::heroHovered() const
{
	return m_heroHovered;
}

ProjectData* TroopCanvas::project() const
{
	auto* session = m_database ? DatabaseSession::owning(m_database->json()) : nullptr;
	return session ? &session->copy() : nullptr;
}

lcf::rpg::Troop* TroopCanvas::findTroop() const
{
	ProjectData* data = project();
	if (!data || m_troop < 0 || m_troop >= static_cast<int>(data->database().troops.size())) {
		return nullptr;
	}
	return &data->database().troops[static_cast<size_t>(m_troop)];
}

lcf::rpg::Actor* TroopCanvas::findHero() const
{
	ProjectData* data = project();
	if (!data || m_hero < 0 || m_hero >= static_cast<int>(data->database().actors.size())) {
		return nullptr;
	}
	return &data->database().actors[static_cast<size_t>(m_hero)];
}

void TroopCanvas::onDataChanged(const QString& jsonPtr)
{
	if (m_dragging != -1) {
		// our own move: the pictures stay
		return;
	}
	if (Json::affects(jsonPtr, QStringLiteral("/troops/%1/members").arg(m_troop))
			|| Json::affects(jsonPtr, QStringLiteral("/enemies"))
			|| Json::affects(jsonPtr, QStringLiteral("/actors/%1").arg(m_hero))
			|| Json::affects(jsonPtr, QStringLiteral("/battleranimations"))) {
		refresh();
	}
}

void TroopCanvas::refresh()
{
	m_members.clear();
	m_transparent.clear();
	m_backdrop = QImage();
	m_heroImage = QImage();
	ProjectData* data = project();
	if (data && !m_backdropName.isEmpty() && data->hasProject()) {
		const QString path = data->project().findFile(BACKDROP, m_backdropName, FileFinder::FileType::Image);
		const QImage image = path.isEmpty() ? QImage() : ImageLoader::Load(path).toImage();
		if (!image.isNull()) {
			m_backdrop = image.scaled(kWidth, kHeight);
		}
	}
	if (const auto* troop = findTroop()) {
		const auto& enemies = data->database().enemies;
		for (const auto& member : troop->members) {
			QImage image;
			bool transparent = false;
			if (member.enemy_id >= 1 && member.enemy_id <= static_cast<int>(enemies.size())) {
				const auto& enemy = enemies[static_cast<size_t>(member.enemy_id) - 1];
				image = EnemyGraphics::load(*data, enemy).toImage();
				transparent = enemy.transparent;
			}
			m_members.push_back(image.isNull() ? placeholder(32) : image);
			m_transparent.push_back(transparent);
		}
	}
	if (const auto* actor = findHero()) {
		// the first frame of the idle pose of the hero's battle graphics
		const auto& animations = data->database().battleranimations;
		const int id = actor->battler_animation;
		if (id >= 1 && id <= static_cast<int>(animations.size())) {
			const auto& poses = animations[static_cast<size_t>(id) - 1].poses;
			if (!poses.empty() && poses.front().animation_type == lcf::rpg::BattlerAnimationPose::AnimType_character) {
				const QImage sheet = GameGraphic::load(*data, BATTLECHARSET, ToQString(poses.front().battler_name));
				if (!sheet.isNull()) {
					m_heroImage = sheet.copy(0, poses.front().battler_index * kBattlerSize, kBattlerSize, kBattlerSize);
				}
			}
		}
		if (m_heroImage.isNull()) {
			m_heroImage = placeholder(kBattlerSize);
		}
	}
	if (m_selected >= static_cast<int>(m_members.size())) {
		m_selected = -1;
		emit selectedMemberChanged();
	}
	update();
}

int TroopCanvas::heroRowOffset() const
{
	return m_heroFrontRow ? m_heroImage.width() / 2 : 0;
}

QRectF TroopCanvas::heroRect() const
{
	const auto* actor = findHero();
	if (!actor || m_heroImage.isNull()) {
		return QRectF();
	}
	const QSizeF size = m_heroImage.size();
	const QPointF center(actor->battle_x - heroRowOffset(), actor->battle_y);
	return QRectF(center - QPointF(size.width() / 2, size.height() / 2), size);
}

int TroopCanvas::memberAt(qreal x, qreal y) const
{
	const auto* troop = findTroop();
	if (!troop) {
		return -1;
	}
	// the last one is drawn on top
	for (int i = static_cast<int>(m_members.size()) - 1; i >= 0; --i) {
		const auto& member = troop->members[static_cast<size_t>(i)];
		const QImage& image = m_members[static_cast<size_t>(i)];
		const QPoint pixel(qFloor(x - member.x + image.width() / 2), qFloor(y - member.y + image.height() / 2));
		// the transparent pixels are not the monster
		if (image.rect().contains(pixel) && qAlpha(image.pixel(pixel)) > 0) {
			return i;
		}
	}
	return -1;
}

void TroopCanvas::paint(QPainter* painter)
{
	painter->scale(width() / kWidth, height() / kHeight);
	painter->fillRect(QRect(0, 0, kWidth, kHeight), QColor(32, 32, 48));
	if (!m_backdrop.isNull()) {
		painter->drawImage(0, 0, m_backdrop);
	}
	const auto* troop = findTroop();
	if (troop && troop->members.size() == m_members.size()) {
		for (size_t i = 0; i < m_members.size(); ++i) {
			const auto& member = troop->members[i];
			const QImage& image = m_members[i];
			painter->setOpacity(TroopLayout::opacity(member, m_transparent[i]));
			const QPoint topLeft(member.x - image.width() / 2, member.y - image.height() / 2);
			painter->drawImage(topLeft, image);
			painter->setOpacity(1);
			if (static_cast<int>(i) == m_selected && !m_readOnly) {
				// the frame of a selected monster: white, dashed black over it
				const QRectF frame = QRectF(topLeft, image.size()).adjusted(0.5, 0.5, -0.5, -0.5);
				painter->setPen(QPen(Qt::white, 1));
				painter->drawRect(frame);
				painter->setPen(QPen(Qt::black, 1, Qt::DashLine));
				painter->drawRect(frame);
			}
		}
	}
	const QRectF hero = heroRect();
	if (!hero.isEmpty()) {
		painter->drawImage(hero.topLeft(), m_heroImage);
	}
}

void TroopCanvas::notify(const QString& path) const
{
	if (m_database && m_database->json()) {
		m_database->json()->notifyChanged(path);
	}
}

void TroopCanvas::mousePressEvent(QMouseEvent* event)
{
	const QPointF pos(event->position().x() * kWidth / width(), event->position().y() * kHeight / height());
	if (event->button() == Qt::LeftButton && m_heroMovable && heroRect().contains(pos)) {
		m_dragging = kHero;
		m_grab = pos - heroRect().center();
		setCursor(Qt::ClosedHandCursor);
		event->accept();
		return;
	}
	if (m_readOnly) {
		event->ignore();
		return;
	}
	forceActiveFocus(Qt::MouseFocusReason);
	const int member = memberAt(pos.x(), pos.y());
	setSelectedMember(member);
	if (member >= 0 && event->button() == Qt::LeftButton) {
		const auto& data = findTroop()->members[static_cast<size_t>(member)];
		m_dragging = member;
		m_grab = pos - QPointF(data.x, data.y);
	} else if (member >= 0 && event->button() == Qt::RightButton) {
		emit contextMenuRequested(member, event->position().x(), event->position().y());
	}
	event->accept();
}

void TroopCanvas::mouseMoveEvent(QMouseEvent* event)
{
	const QPointF pos(event->position().x() * kWidth / width(), event->position().y() * kHeight / height());
	if (m_dragging == kHero) {
		auto* actor = findHero();
		if (!actor) {
			return;
		}
		// the Player keeps heroes inside the screen horizontally
		const QPointF center = pos - m_grab;
		const int half = m_heroImage.width() / 2;
		actor->battle_x = std::clamp(qRound(center.x()), half, kWidth - half) + heroRowOffset();
		actor->battle_y = std::clamp(qRound(center.y()), 0, kHeight);
		notify(QStringLiteral("/actors/%1/battle_x").arg(m_hero));
		notify(QStringLiteral("/actors/%1/battle_y").arg(m_hero));
		update();
		return;
	}
	auto* troop = findTroop();
	if (m_dragging >= 0 && troop && m_dragging < static_cast<int>(troop->members.size())) {
		const auto& member = troop->members[static_cast<size_t>(m_dragging)];
		const QPoint before(member.x, member.y);
		TroopLayout::moveMember(*troop, m_dragging, pos - m_grab);
		if (QPoint(member.x, member.y) != before) {
			notify(QStringLiteral("/troops/%1/members/%2").arg(m_troop).arg(m_dragging));
			update();
		}
	}
}

void TroopCanvas::mouseReleaseEvent(QMouseEvent* event)
{
	const int dragged = m_dragging;
	m_dragging = -1;
	if (dragged == kHero) {
		setCursor(Qt::OpenHandCursor);
		emit heroMoved();
	} else if (dragged >= 0) {
		emit memberMoved(dragged);
	}
	event->accept();
}

void TroopCanvas::setHovered(bool hovered)
{
	if (m_heroHovered != hovered) {
		m_heroHovered = hovered;
		emit heroHoveredChanged();
	}
}

void TroopCanvas::hoverMoveEvent(QHoverEvent* event)
{
	const QPointF pos(event->position().x() * kWidth / width(), event->position().y() * kHeight / height());
	const bool over = heroRect().contains(pos);
	setHovered(over);
	if (m_dragging != kHero) {
		setCursor(over && m_heroMovable ? Qt::OpenHandCursor : Qt::ArrowCursor);
	}
}

void TroopCanvas::hoverLeaveEvent(QHoverEvent*)
{
	setHovered(false);
}
