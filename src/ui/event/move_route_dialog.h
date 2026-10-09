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

#include <QDialog>
#include <lcf/rpg/moveroute.h>
#include "model/move_route_codec.h"

class ProjectData;
class QCheckBox;
class QComboBox;
class QListWidget;
class QListWidgetItem;

/**
 * Edits a move route, like the "Move Route" window of RPG Maker: the moves
 * on the left, a button per kind of move on the right. Used for the custom
 * movement of an event page, and with a target and a frequency for the
 * Move Event command.
 */
class MoveRouteDialog : public QDialog
{
	Q_OBJECT

public:
	/** move_event: also choose the target and the frequency (Move Event command) */
	MoveRouteDialog(ProjectData& project, const lcf::rpg::MoveRoute& route, QWidget* parent = nullptr,
		const MoveRouteCodec::MoveEvent* move_event = nullptr);

	lcf::rpg::MoveRoute route() const;
	MoveRouteCodec::MoveEvent moveEvent() const;

	/** One line of the list, as RPG Maker shows it */
	static QString describe(const lcf::rpg::MoveCommand& move);

protected:
	void keyPressEvent(QKeyEvent* event) override;

private:
	void refresh(int select);
	void insert(int code);
	/** Asks for the switch, graphic or sound of a move; false when cancelled */
	bool editParameters(lcf::rpg::MoveCommand& move);
	void editItem(QListWidgetItem* item);
	void removeSelected();

	ProjectData& m_project;
	std::vector<lcf::rpg::MoveCommand> m_moves;
	QListWidget* m_list = nullptr;
	QCheckBox* m_repeat = nullptr;
	QCheckBox* m_skippable = nullptr;
	QComboBox* m_target = nullptr;
	QComboBox* m_frequency = nullptr;
};
