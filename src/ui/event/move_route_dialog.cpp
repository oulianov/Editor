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

#include "move_route_dialog.h"
#include "core.h"
#include "defines.h"
#include "stringizer.h"
#include "ui/common/help.h"
#include "common/dbstring.h"
#include "model/project_data.h"
#include "ui/common/rpg_combobox.h"
#include "ui/picker/picker_audio_widget.h"
#include "ui/picker/picker_charset_widget.h"
#include "ui/picker/picker_dialog.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <lcf/rpg/sound.h>

namespace {
using Code = lcf::rpg::MoveCommand::Code;

// The buttons of RPG Maker's Move Route window, in its order
const std::vector<std::vector<Code>> groups = {
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

bool hasParameters(int code) {
	auto c = static_cast<Code>(code);
	return c == Code::switch_on || c == Code::switch_off || c == Code::change_graphic || c == Code::play_sound_effect;
}

constexpr int this_event = 10005;
}

MoveRouteDialog::MoveRouteDialog(ProjectData& project, const lcf::rpg::MoveRoute& route, QWidget* parent,
		const MoveRouteCodec::MoveEvent* move_event) :
	QDialog(parent), m_project(project), m_moves(route.move_commands.begin(), route.move_commands.end())
{
	setWindowTitle(move_event ? tr("Move Event") : tr("Move Route"));
	auto* layout = new QVBoxLayout(this);

	if (move_event) {
		auto* form = new QFormLayout();
		m_target = new QComboBox(this);
		m_target->addItem(tr("Hero"), 10001);
		m_target->addItem(tr("Boat"), 10002);
		m_target->addItem(tr("Ship"), 10003);
		m_target->addItem(tr("Airship"), 10004);
		m_target->addItem(tr("This Event"), this_event);
		if (auto* events = core().currentMapEvents()) {
			for (auto it = events->cbegin(); it != events->cend(); ++it) {
				m_target->addItem(QString("%1: %2").arg(it.key(), 4, 10, QChar('0')).arg(ToQString(it.value()->name)), it.key());
			}
		}
		int index = m_target->findData(move_event->target);
		if (index < 0) {
			m_target->addItem(QString::number(move_event->target), move_event->target);
			index = m_target->count() - 1;
		}
		m_target->setCurrentIndex(index);

		m_frequency = new QComboBox(this);
		for (int i = 1; i <= 8; ++i) {
			m_frequency->addItem(QString::number(i), i);
		}
		m_frequency->setCurrentIndex(std::clamp(move_event->frequency, 1, 8) - 1);
		form->addRow(tr("Target"), m_target);
		form->addRow(tr("Frequency"), m_frequency);
		layout->addLayout(form);
	}

	auto* columns = new QHBoxLayout();
	layout->addLayout(columns, 1);

	// the route
	auto* left = new QVBoxLayout();
	m_list = new QListWidget(this);
	m_list->setMinimumWidth(240);
	left->addWidget(m_list, 1);
	m_repeat = new QCheckBox(tr("Repeat the moves"), this);
	m_skippable = new QCheckBox(tr("Skip moves that cannot be done"), this);
	m_repeat->setChecked(route.repeat);
	m_skippable->setChecked(route.skippable);
	left->addWidget(m_repeat);
	left->addWidget(m_skippable);
	columns->addLayout(left, 1);

	// a button per move, in groups, two columns
	auto* buttons = new QHBoxLayout();
	buttons->setSpacing(8);
	QVBoxLayout* column = nullptr;
	for (size_t g = 0; g < groups.size(); ++g) {
		if (g % 2 == 0) {
			column = new QVBoxLayout();
			column->setSpacing(8);
			buttons->addLayout(column);
		}
		auto* box = new QVBoxLayout();
		box->setSpacing(2);
		for (Code code : groups[g]) {
			QString name = Stringizer::moveCommand(static_cast<int>(code));
			auto* button = new QPushButton(hasParameters(static_cast<int>(code)) ? name + "..." : name, this);
			button->setAutoDefault(false);
			connect(button, &QPushButton::clicked, this, [this, code] { insert(static_cast<int>(code)); });
			box->addWidget(button);
		}
		column->addLayout(box);
		if (g % 2 == 1 || g + 1 == groups.size()) {
			column->addStretch(1);
		}
	}
	columns->addLayout(buttons);

	auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Help, this);
	connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(box, &QDialogButtonBox::helpRequested, this, [this] { Help::open(this, QStringLiteral("move-route")); });
	layout->addWidget(box);

	connect(m_list, &QListWidget::itemDoubleClicked, this, &MoveRouteDialog::editItem);
	refresh(static_cast<int>(m_moves.size()));
}

QString MoveRouteDialog::describe(const lcf::rpg::MoveCommand& move) {
	QString text = Stringizer::moveCommand(move.command_id);
	switch (static_cast<Code>(move.command_id)) {
		case Code::switch_on:
		case Code::switch_off:
			return text + QString(": [%1: %2]").arg(move.parameter_a, 4, 10, QChar('0')).arg(Stringizer::switchName(move.parameter_a));
		case Code::change_graphic:
			return text + QString(": %1, %2").arg(ToQString(move.parameter_string)).arg(move.parameter_a + 1);
		case Code::play_sound_effect:
			return text + ": " + ToQString(move.parameter_string);
		default:
			return text;
	}
}

void MoveRouteDialog::refresh(int select) {
	m_list->clear();
	for (const auto& move : m_moves) {
		m_list->addItem(describe(move));
	}
	// where moves are inserted, like the empty line of RPG Maker
	auto* end = new QListWidgetItem(QStringLiteral("<>"));
	end->setForeground(palette().brush(QPalette::Disabled, QPalette::Text));
	m_list->addItem(end);
	m_list->setCurrentRow(std::clamp(select, 0, m_list->count() - 1));
}

void MoveRouteDialog::insert(int code) {
	lcf::rpg::MoveCommand move;
	move.command_id = code;
	if (static_cast<Code>(code) == Code::play_sound_effect) {
		move.parameter_a = 100;  // volume
		move.parameter_b = 100;  // tempo
		move.parameter_c = 50;   // balance
	} else if (static_cast<Code>(code) == Code::switch_on || static_cast<Code>(code) == Code::switch_off) {
		move.parameter_a = 1;
	}
	if (hasParameters(code) && !editParameters(move)) {
		return;
	}
	// before the selected line, as RPG Maker does
	int row = std::clamp(m_list->currentRow(), 0, static_cast<int>(m_moves.size()));
	m_moves.insert(m_moves.begin() + row, move);
	refresh(row + 1);
}

bool MoveRouteDialog::editParameters(lcf::rpg::MoveCommand& move) {
	switch (static_cast<Code>(move.command_id)) {
		case Code::switch_on:
		case Code::switch_off: {
			QDialog dialog(this);
			dialog.setWindowTitle(Stringizer::moveCommand(move.command_id));
			auto* layout = new QVBoxLayout(&dialog);
			auto* combo = new SwitchRpgComboBox(&dialog);
			combo->makeModel(m_project);
			combo->comboBox()->setCurrentIndex(std::max(0, move.parameter_a - 1));
			layout->addWidget(combo);
			auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
			connect(box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
			connect(box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
			layout->addWidget(box);
			if (dialog.exec() != QDialog::Accepted) {
				return false;
			}
			move.parameter_a = combo->comboBox()->currentIndex() + 1;
			return true;
		}
		case Code::change_graphic: {
			auto* widget = new PickerCharsetWidget(move.parameter_a, this);
			PickerDialog dialog(m_project, FileFinder::FileType::Image, widget, this);
			bool chosen = false;
			connect(&dialog, &PickerDialog::fileSelected, this, [&](const QString& name) {
				move.parameter_string = ToDBString(name);
				move.parameter_a = widget->index();
				chosen = true;
			});
			dialog.setDirectoryAndFile(CHARSET, ToQString(move.parameter_string));
			dialog.exec();
			return chosen;
		}
		case Code::play_sound_effect: {
			lcf::rpg::Sound sound;
			sound.name = move.parameter_string;
			sound.volume = move.parameter_a;
			sound.tempo = move.parameter_b;
			sound.balance = move.parameter_c;
			auto* widget = new PickerAudioWidget(sound, this);
			PickerDialog dialog(m_project, FileFinder::FileType::Sound, widget, this);
			bool chosen = false;
			connect(&dialog, &PickerDialog::fileSelected, this, [&](const QString& name) {
				move.parameter_string = ToDBString(name);
				move.parameter_a = widget->volume();
				move.parameter_b = widget->tempo();
				move.parameter_c = widget->balance();
				chosen = true;
			});
			dialog.setDirectoryAndFile(SOUND, ToQString(move.parameter_string));
			dialog.exec();
			return chosen;
		}
		default:
			return true;
	}
}

void MoveRouteDialog::editItem(QListWidgetItem* item) {
	int row = m_list->row(item);
	if (row < 0 || row >= static_cast<int>(m_moves.size()) || !hasParameters(m_moves[row].command_id)) {
		return;
	}
	auto move = m_moves[row];
	if (editParameters(move)) {
		m_moves[row] = move;
		refresh(row);
	}
}

void MoveRouteDialog::removeSelected() {
	int row = m_list->currentRow();
	if (row < 0 || row >= static_cast<int>(m_moves.size())) {
		return;
	}
	m_moves.erase(m_moves.begin() + row);
	refresh(row);
}

void MoveRouteDialog::keyPressEvent(QKeyEvent* event) {
	if (m_list->hasFocus() && (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)) {
		removeSelected();
		return;
	}
	if (m_list->hasFocus() && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) {
		editItem(m_list->currentItem());
		return;
	}
	QDialog::keyPressEvent(event);
}

lcf::rpg::MoveRoute MoveRouteDialog::route() const {
	lcf::rpg::MoveRoute route;
	route.move_commands.assign(m_moves.begin(), m_moves.end());
	route.repeat = m_repeat->isChecked();
	route.skippable = m_skippable->isChecked();
	return route;
}

MoveRouteCodec::MoveEvent MoveRouteDialog::moveEvent() const {
	MoveRouteCodec::MoveEvent move;
	move.route = route();
	if (m_target) {
		move.target = m_target->currentData().toInt();
		move.frequency = m_frequency->currentData().toInt();
	}
	return move;
}
