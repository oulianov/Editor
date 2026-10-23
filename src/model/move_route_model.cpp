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

#include "move_route_model.h"
#include <QBrush>
#include <QGuiApplication>
#include <QPalette>
#include <algorithm>
#include "common/dbstring.h"
#include "model/database_list_data.h"
#include "stringizer.h"

MoveRouteModel::Moves MoveRouteModel::s_clipboard;

MoveRouteModel::MoveRouteModel(QObject* parent) :
	QAbstractListModel(parent)
{
	// Switch ON and Switch OFF name their switch: renamed (in the picker of
	// a move, the database window), the lines show the new name at once
	connect(&DatabaseListNotifier::instance(), &DatabaseListNotifier::listChanged, this, [this] {
		if (moveCount() > 0) {
			emit dataChanged(index(0), index(moveCount() - 1), {Qt::DisplayRole});
		}
	});
}

void MoveRouteModel::setRoute(const lcf::rpg::MoveRoute& route)
{
	beginResetModel();
	m_moves.assign(route.move_commands.begin(), route.move_commands.end());
	endResetModel();
	setRepeat(route.repeat);
	setSkippable(route.skippable);
	emit movesChanged();
}

lcf::rpg::MoveRoute MoveRouteModel::route() const
{
	lcf::rpg::MoveRoute route;
	route.move_commands.assign(m_moves.begin(), m_moves.end());
	route.repeat = m_repeat;
	route.skippable = m_skippable;
	return route;
}

void MoveRouteModel::setMoveEvent(const MoveRouteCodec::MoveEvent& move)
{
	setRoute(move.route);
	setTarget(move.target);
	setFrequency(move.frequency);
}

MoveRouteCodec::MoveEvent MoveRouteModel::moveEvent() const
{
	MoveRouteCodec::MoveEvent move;
	move.target = m_target;
	move.frequency = m_frequency;
	move.route = route();
	return move;
}

void MoveRouteModel::setCommand(const lcf::rpg::EventCommand& cmd, const std::string& encoding)
{
	setMoveEvent(MoveRouteCodec::decode(cmd, encoding));
}

void MoveRouteModel::writeCommand(lcf::rpg::EventCommand& cmd, const std::string& encoding) const
{
	MoveRouteCodec::encode(moveEvent(), cmd, encoding);
}

const MoveRouteModel::Moves& MoveRouteModel::moves() const
{
	return m_moves;
}

int MoveRouteModel::moveCount() const
{
	return static_cast<int>(m_moves.size());
}

bool MoveRouteModel::repeat() const
{
	return m_repeat;
}

void MoveRouteModel::setRepeat(bool repeat)
{
	if (m_repeat != repeat) {
		m_repeat = repeat;
		emit repeatChanged();
	}
}

bool MoveRouteModel::skippable() const
{
	return m_skippable;
}

void MoveRouteModel::setSkippable(bool skippable)
{
	if (m_skippable != skippable) {
		m_skippable = skippable;
		emit skippableChanged();
	}
}

int MoveRouteModel::target() const
{
	return m_target;
}

void MoveRouteModel::setTarget(int target)
{
	if (m_target != target) {
		m_target = target;
		emit targetChanged();
	}
}

int MoveRouteModel::frequency() const
{
	return m_frequency;
}

void MoveRouteModel::setFrequency(int frequency)
{
	if (m_frequency != frequency) {
		m_frequency = frequency;
		emit frequencyChanged();
	}
}

int MoveRouteModel::rowCount(const QModelIndex& parent) const
{
	// the moves, then the end line
	return parent.isValid() ? 0 : moveCount() + 1;
}

QVariant MoveRouteModel::data(const QModelIndex& index, int role) const
{
	if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
		return {};
	}
	const int row = index.row();
	if (row >= moveCount()) {
		switch (role) {
			case Qt::DisplayRole:
				// where moves are inserted, like the empty line of RPG Maker
				return QStringLiteral("<>");
			case Qt::ForegroundRole:
				if (qobject_cast<QGuiApplication*>(QCoreApplication::instance())) {
					return QGuiApplication::palette().brush(QPalette::Disabled, QPalette::Text);
				}
				return {};
			case CodeRole:
				return -1;
			case HasParametersRole:
				return false;
			case EndRole:
				return true;
			case IconNameRole:
			case ParameterStringRole:
				return QString();
			case ParameterARole:
			case ParameterBRole:
			case ParameterCRole:
				return 0;
			default:
				return {};
		}
	}
	const auto& move = m_moves[row];
	switch (role) {
		case Qt::DisplayRole:
			return text(move);
		case CodeRole:
			return move.command_id;
		case HasParametersRole:
			return hasParameters(move.command_id);
		case ParameterStringRole:
			return ToQString(move.parameter_string);
		case ParameterARole:
			return move.parameter_a;
		case ParameterBRole:
			return move.parameter_b;
		case ParameterCRole:
			return move.parameter_c;
		case IconNameRole:
			return iconName(move.command_id);
		case EndRole:
			return false;
		default:
			return {};
	}
}

Qt::ItemFlags MoveRouteModel::flags(const QModelIndex& index) const
{
	if (!index.isValid()) {
		return Qt::NoItemFlags;
	}
	return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemNeverHasChildren;
}

QHash<int, QByteArray> MoveRouteModel::roleNames() const
{
	auto names = QAbstractListModel::roleNames();
	names[CodeRole] = "code";
	names[HasParametersRole] = "hasParameters";
	names[ParameterStringRole] = "parameterString";
	names[ParameterARole] = "parameterA";
	names[ParameterBRole] = "parameterB";
	names[ParameterCRole] = "parameterC";
	names[IconNameRole] = "iconName";
	names[EndRole] = "isEnd";
	return names;
}

bool MoveRouteModel::moveRows(const QModelIndex& source_parent, int source_row, int count,
		const QModelIndex& destination_parent, int destination_child)
{
	// moves only: the end line stays last
	if (source_parent.isValid() || destination_parent.isValid() || count <= 0 || source_row < 0
			|| source_row + count > moveCount() || destination_child < 0 || destination_child > moveCount()
			|| (destination_child >= source_row && destination_child <= source_row + count)) {
		return false;
	}
	if (!beginMoveRows(QModelIndex(), source_row, source_row + count - 1, QModelIndex(), destination_child)) {
		return false;
	}
	auto first = m_moves.begin() + source_row;
	auto last = first + count;
	if (destination_child > source_row) {
		std::rotate(first, last, m_moves.begin() + destination_child);
	} else {
		std::rotate(m_moves.begin() + destination_child, first, last);
	}
	endMoveRows();
	emit movesChanged();
	return true;
}

bool MoveRouteModel::hasParameters(int code)
{
	auto c = static_cast<Code>(code);
	return c == Code::switch_on || c == Code::switch_off || c == Code::change_graphic || c == Code::play_sound_effect;
}

lcf::rpg::MoveCommand MoveRouteModel::newMove(int code)
{
	lcf::rpg::MoveCommand move;
	move.command_id = code;
	if (static_cast<Code>(code) == Code::play_sound_effect) {
		move.parameter_a = 100;  // volume
		move.parameter_b = 100;  // tempo
		move.parameter_c = 50;   // balance
	} else if (static_cast<Code>(code) == Code::switch_on || static_cast<Code>(code) == Code::switch_off) {
		move.parameter_a = 1;
	}
	return move;
}

const std::vector<std::vector<MoveRouteModel::Code>>& MoveRouteModel::commandGroups()
{
	static const std::vector<std::vector<Code>> groups = {
		{Code::move_up, Code::move_right, Code::move_down, Code::move_left,
		 Code::move_upright, Code::move_downright, Code::move_downleft, Code::move_upleft,
		 Code::move_random, Code::move_towards_hero, Code::move_away_from_hero, Code::move_forward},
		{Code::face_up, Code::face_right, Code::face_down, Code::face_left,
		 Code::turn_90_degree_right, Code::turn_90_degree_left, Code::turn_180_degree,
		 Code::turn_90_degree_random, Code::face_random_direction, Code::face_hero, Code::face_away_from_hero},
		{Code::wait, Code::begin_jump, Code::end_jump, Code::lock_facing, Code::unlock_facing,
		 Code::increase_movement_speed, Code::decrease_movement_speed,
		 Code::increase_movement_frequence, Code::decrease_movement_frequence},
		{Code::switch_on, Code::switch_off, Code::change_graphic, Code::play_sound_effect,
		 Code::walk_everywhere_on, Code::walk_everywhere_off, Code::stop_animation, Code::start_animation,
		 Code::increase_transp, Code::decrease_transp},
	};
	return groups;
}

QVariantList MoveRouteModel::commandGroupList() const
{
	QVariantList groups;
	for (const auto& group : commandGroups()) {
		QVariantList codes;
		for (Code code : group) {
			codes.append(static_cast<int>(code));
		}
		groups.append(QVariant(codes));
	}
	return groups;
}

QString MoveRouteModel::commandName(int code)
{
	return Stringizer::moveCommand(code);
}

QString MoveRouteModel::buttonText(int code)
{
	const QString name = commandName(code);
	return hasParameters(code) ? name + "..." : name;
}

QString MoveRouteModel::text(const lcf::rpg::MoveCommand& move)
{
	return Stringizer::moveCommand(move);
}

QString MoveRouteModel::iconName(int code)
{
	switch (static_cast<Code>(code)) {
		case Code::move_up:
		case Code::face_up:
			return QStringLiteral("go-up");
		case Code::move_right:
		case Code::face_right:
			return QStringLiteral("go-next");
		case Code::move_down:
		case Code::face_down:
			return QStringLiteral("go-down");
		case Code::move_left:
		case Code::face_left:
			return QStringLiteral("go-previous");
		case Code::turn_90_degree_right:
			return QStringLiteral("object-rotate-right");
		case Code::turn_90_degree_left:
			return QStringLiteral("object-rotate-left");
		case Code::change_graphic:
			return QStringLiteral("image-x-generic");
		case Code::play_sound_effect:
			return QStringLiteral("audio-x-generic");
		case Code::start_animation:
			return QStringLiteral("media-playback-start");
		case Code::stop_animation:
			return QStringLiteral("media-playback-stop");
		default:
			return QString();
	}
}

int MoveRouteModel::insertMove(int row, const lcf::rpg::MoveCommand& move)
{
	row = std::clamp(row, 0, moveCount());
	beginInsertRows(QModelIndex(), row, row);
	m_moves.insert(m_moves.begin() + row, move);
	endInsertRows();
	emit movesChanged();
	return row + 1;
}

int MoveRouteModel::insertCommand(int row, int code)
{
	return insertMove(row, newMove(code));
}

void MoveRouteModel::setMove(int row, const lcf::rpg::MoveCommand& move)
{
	if (row < 0 || row >= moveCount() || m_moves[row] == move) {
		return;
	}
	m_moves[row] = move;
	emit dataChanged(index(row), index(row));
	emit movesChanged();
}

void MoveRouteModel::setSwitch(int row, int switch_id)
{
	if (row >= 0 && row < moveCount()) {
		auto move = m_moves[row];
		move.parameter_a = switch_id;
		setMove(row, move);
	}
}

void MoveRouteModel::setGraphic(int row, const QString& name, int index)
{
	if (row >= 0 && row < moveCount()) {
		auto move = m_moves[row];
		move.parameter_string = ToDBString(name);
		move.parameter_a = index;
		setMove(row, move);
	}
}

void MoveRouteModel::setSound(int row, const QString& name, int volume, int tempo, int balance)
{
	if (row >= 0 && row < moveCount()) {
		auto move = m_moves[row];
		move.parameter_string = ToDBString(name);
		move.parameter_a = volume;
		move.parameter_b = tempo;
		move.parameter_c = balance;
		setMove(row, move);
	}
}

QList<int> MoveRouteModel::moveRowsOf(const QList<int>& rows) const
{
	QList<int> moves;
	for (int row : rows) {
		if (row >= 0 && row < moveCount()) {
			moves.append(row);
		}
	}
	std::sort(moves.begin(), moves.end());
	moves.erase(std::unique(moves.begin(), moves.end()), moves.end());
	return moves;
}

int MoveRouteModel::removeMoves(const QList<int>& rows)
{
	const auto moves = moveRowsOf(rows);
	if (moves.isEmpty()) {
		return -1;
	}
	// from the last, so that the rows before stay where they are
	for (auto it = moves.rbegin(); it != moves.rend(); ++it) {
		beginRemoveRows(QModelIndex(), *it, *it);
		m_moves.erase(m_moves.begin() + *it);
		endRemoveRows();
	}
	emit movesChanged();
	return moves.front();
}

bool MoveRouteModel::copyMoves(const QList<int>& rows, bool cut)
{
	const auto moves = moveRowsOf(rows);
	if (moves.isEmpty()) {
		return false;
	}
	s_clipboard.clear();
	for (int row : moves) {
		s_clipboard.push_back(m_moves[row]);
	}
	emit clipboardChanged();
	if (cut) {
		removeMoves(moves);
	}
	return true;
}

int MoveRouteModel::paste(int row)
{
	if (s_clipboard.empty()) {
		return -1;
	}
	row = std::clamp(row, 0, moveCount());
	const int count = static_cast<int>(s_clipboard.size());
	beginInsertRows(QModelIndex(), row, row + count - 1);
	m_moves.insert(m_moves.begin() + row, s_clipboard.begin(), s_clipboard.end());
	endInsertRows();
	emit movesChanged();
	return row + count;
}

void MoveRouteModel::clear()
{
	if (m_moves.empty()) {
		return;
	}
	beginRemoveRows(QModelIndex(), 0, moveCount() - 1);
	m_moves.clear();
	endRemoveRows();
	emit movesChanged();
}

bool MoveRouteModel::moveMoves(int first, int count, int destination)
{
	return moveRows(QModelIndex(), first, count, QModelIndex(), destination);
}

bool MoveRouteModel::canPaste()
{
	return !s_clipboard.empty();
}
