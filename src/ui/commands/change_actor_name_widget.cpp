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

#include "change_actor_name_widget.h"
#include "ui_change_actor_name_widget.h"
#include "common/dbstring.h"

#include <lcf/reader_util.h>

ChangeActorNameWidget::ChangeActorNameWidget(ProjectData& project, QWidget *parent) :
	EventCommandBaseWidget(project, parent),
	ui(new Ui::ChangeActorNameWidget) {

	ui->setupUi(this);

	ui->comboActor_arg0->makeModel(m_project);
}

ChangeActorNameWidget::~ChangeActorNameWidget() {
	delete ui;
}

void ChangeActorNameWidget::setData(lcf::rpg::EventCommand* cmd_) {
	EventCommandBaseWidget::setData(cmd_);

	ui->lineName_argX->setText(ToQString(cmd_->string));
}

void ChangeActorNameWidget::onParameterChanged(int index, int new_value) {
	if (index != 0) {
		return;
	}
	// A new hero suggests its name, unless a name was typed: the text is
	// kept when the command is opened, and is never another hero's name
	const auto& actors = m_project.database().actors;
	const auto* actor = lcf::ReaderUtil::GetElement(actors, new_value);
	const auto* previous = lcf::ReaderUtil::GetElement(actors, m_shownActor);
	m_shownActor = new_value;
	const QString text = ui->lineName_argX->text();
	if (!actor || !(text.isEmpty() || (previous && text == ToQString(previous->name)))) {
		return;
	}
	ui->lineName_argX->setText(ToQString(actor->name));
	m_cmd->string = actor->name;
}
