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

#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QVariantList>
#include <string>
#include <vector>
#include <lcf/rpg/eventcommand.h>
#include <lcf/rpg/moveroute.h>
#include "model/move_route_codec.h"

/**
 * The moves of a move route as a list model, like the list of RPG Maker's
 * Move Route window: one row per move, then the empty line ending the route
 * (moves are inserted before the line selected, so there at its end).
 * Also the repeat and skip flags, and for the Move Event command its target
 * and frequency (MoveRouteCodec).
 *
 * The moves with parameters (a switch, a graphic, a sound) are given them
 * by the interface, which asks for them (hasParameters()).
 */
class MoveRouteModel : public QAbstractListModel
{
	Q_OBJECT
	Q_PROPERTY(bool repeat READ repeat WRITE setRepeat NOTIFY repeatChanged)
	Q_PROPERTY(bool skippable READ skippable WRITE setSkippable NOTIFY skippableChanged)
	Q_PROPERTY(int target READ target WRITE setTarget NOTIFY targetChanged)
	Q_PROPERTY(int frequency READ frequency WRITE setFrequency NOTIFY frequencyChanged)
	Q_PROPERTY(int moveCount READ moveCount NOTIFY movesChanged)
	Q_PROPERTY(bool canPaste READ canPaste NOTIFY clipboardChanged)

public:
	using Code = lcf::rpg::MoveCommand::Code;
	using Moves = std::vector<lcf::rpg::MoveCommand>;

	enum Role {
		/** lcf::rpg::MoveCommand::Code, -1 on the end line */
		CodeRole = Qt::UserRole + 1,
		/** The move takes a switch, a graphic or a sound */
		HasParametersRole,
		/** The parameters of the move: the switch (a), the graphic (string,
		 *  a: its index), the sound (string, a b c: volume, tempo, balance) */
		ParameterStringRole,
		ParameterARole,
		ParameterBRole,
		ParameterCRole,
		/** A theme icon (freedesktop name) for the move, empty for none */
		IconNameRole,
		/** The empty line ending the route */
		EndRole,
	};

	explicit MoveRouteModel(QObject* parent = nullptr);

	void setRoute(const lcf::rpg::MoveRoute& route);
	lcf::rpg::MoveRoute route() const;
	/** A Move Event command's route, target and frequency */
	void setMoveEvent(const MoveRouteCodec::MoveEvent& move);
	MoveRouteCodec::MoveEvent moveEvent() const;
	/** Reads and writes the parameters of a Move Event command */
	void setCommand(const lcf::rpg::EventCommand& cmd, const std::string& encoding);
	void writeCommand(lcf::rpg::EventCommand& cmd, const std::string& encoding) const;

	const Moves& moves() const;
	int moveCount() const;
	bool repeat() const;
	void setRepeat(bool repeat);
	bool skippable() const;
	void setSkippable(bool skippable);
	/** The character moved: 10001 the hero, 10002 to 10004 the vehicles,
	 *  10005 this event, else an event of the map */
	int target() const;
	void setTarget(int target);
	/** 1 to 8 */
	int frequency() const;
	void setFrequency(int frequency);

	// QAbstractItemModel
	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	Qt::ItemFlags flags(const QModelIndex& index) const override;
	QHash<int, QByteArray> roleNames() const override;
	bool moveRows(const QModelIndex& source_parent, int source_row, int count,
		const QModelIndex& destination_parent, int destination_child) override;

	/*
	 * The moves
	 */

	/** Asks for a switch, a graphic or a sound */
	Q_INVOKABLE static bool hasParameters(int code);
	/** A new move, with RPG Maker's parameters: switch 1, a sound at its
	 *  full volume and tempo, centered */
	static lcf::rpg::MoveCommand newMove(int code);
	/** The buttons of RPG Maker's Move Route window, in groups, in its order */
	static const std::vector<std::vector<Code>>& commandGroups();
	/** commandGroups() for QML: lists of codes */
	Q_INVOKABLE QVariantList commandGroupList() const;
	/** The name of a kind of move */
	Q_INVOKABLE static QString commandName(int code);
	/** The button of a kind of move: "Switch ON..." when it asks more */
	Q_INVOKABLE static QString buttonText(int code);
	/** One line of the list, as RPG Maker shows it */
	static QString text(const lcf::rpg::MoveCommand& move);
	/** A theme icon for a kind of move, empty for none */
	static QString iconName(int code);

	/*
	 * The changes. row: a line of the list, the end line included
	 */

	/** Inserts a move before row (past the end: at the end); the row after it */
	int insertMove(int row, const lcf::rpg::MoveCommand& move);
	/** A new move of this code (newMove()), before row; the row after it */
	Q_INVOKABLE int insertCommand(int row, int code);
	/** Replaces a move (its parameters edited) */
	void setMove(int row, const lcf::rpg::MoveCommand& move);
	Q_INVOKABLE void setSwitch(int row, int switch_id);
	Q_INVOKABLE void setGraphic(int row, const QString& name, int index);
	Q_INVOKABLE void setSound(int row, const QString& name, int volume, int tempo, int balance);
	/** Removes the moves of rows (the end line stays); the first row
	 *  removed, -1 when none */
	Q_INVOKABLE int removeMoves(const QList<int>& rows);
	/** The moves of rows go to the clipboard of every route (Cut: and are
	 *  removed); false when there is none */
	Q_INVOKABLE bool copyMoves(const QList<int>& rows, bool cut = false);
	/** Pastes the moves copied before row; the row after them, -1 when none */
	Q_INVOKABLE int paste(int row);
	/** Clear List */
	Q_INVOKABLE void clear();
	/** Moves count moves from first to before destination (the rows as
	 *  they are before the move); false when not moved */
	Q_INVOKABLE bool moveMoves(int first, int count, int destination);
	/** The rows of moves among rows, in order, once each (not the end line) */
	QList<int> moveRowsOf(const QList<int>& rows) const;
	static bool canPaste();

signals:
	void repeatChanged();
	void skippableChanged();
	void targetChanged();
	void frequencyChanged();
	/** Any change of the moves */
	void movesChanged();
	void clipboardChanged();

private:
	Moves m_moves;
	bool m_repeat = true;
	bool m_skippable = false;
	int m_target = MoveRouteCodec::MoveEvent().target;
	int m_frequency = MoveRouteCodec::MoveEvent().frequency;
	/** Copied moves, for the routes of every event */
	static Moves s_clipboard;
};
