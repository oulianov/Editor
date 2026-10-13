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

#include "change_actor_title_widget.h"
#include "ui_change_actor_title_widget.h"
#include "common/dbstring.h"
#include "model/project_data.h"

#include <lcf/reader_util.h>

ChangeActorTitleWidget::ChangeActorTitleWidget(ProjectData& project, QWidget *parent) :
	EventCommandBaseWidget(project, parent),
	ui(new Ui::ChangeActorTitleWidget) {

	ui->setupUi(this);

	ui->comboActor_arg0->makeModel(m_project);
}

ChangeActorTitleWidget::~ChangeActorTitleWidget() {
	delete ui;
}

void ChangeActorTitleWidget::setData(lcf::rpg::EventCommand* cmd_) {
	EventCommandBaseWidget::setData(cmd_);

	ui->lineTitle_argX->setText(ToQString(cmd_->string));
}

void ChangeActorTitleWidget::onParameterChanged(int index, int new_value) {
	if (index != 0) {
		return;
	}
	// A new hero suggests its title, unless a title was typed: the text is
	// kept when the command is opened, and is never another hero's title
	const auto& actors = m_project.database().actors;
	const auto* actor = lcf::ReaderUtil::GetElement(actors, new_value);
	const auto* previous = lcf::ReaderUtil::GetElement(actors, m_shownActor);
	m_shownActor = new_value;
	const QString text = ui->lineTitle_argX->text();
	if (!actor || !(text.isEmpty() || (previous && text == ToQString(previous->title)))) {
		return;
	}
	ui->lineTitle_argX->setText(ToQString(actor->title));
	m_cmd->string = actor->title;
}
