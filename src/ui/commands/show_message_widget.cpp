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

#include "show_message_widget.h"
#include "ui_show_message_widget.h"
#include "common/dbstring.h"
#include "model/event_command_list.h"
#include "model/project_data.h"
#include "lcf/rpg/eventcommand.h"
#include "message_code_popup.h"

#include <QHBoxLayout>
#include <QToolButton>

ShowMessageWidget::ShowMessageWidget(ProjectData &project, QWidget *parent) :
	EventCommandBaseWidget(project, parent),
	ui(new Ui::ShowMessageWidget) {

	ui->setupUi(this);

	// the codes of messages, explained, typed at the cursor
	auto* codes = new QToolButton(this);
	codes->setObjectName(QStringLiteral("addCode"));
	codes->setText(tr("Add Code"));
	codes->setToolTip(tr("Hero names, variables, colors, pauses, symbols..."));
	codes->setToolButtonStyle(Qt::ToolButtonTextOnly);
	auto* row = new QHBoxLayout();
	row->addWidget(codes);
	row->addStretch();
	ui->verticalLayout->addLayout(row);
	connect(codes, &QToolButton::clicked, this, [this, codes] {
		MessageCodePopup::showCodes(ui->message, m_project, codes);
	});
}

ShowMessageWidget::~ShowMessageWidget() {
	delete ui;
}

void ShowMessageWidget::setData(EventCommandList* commands) {
	EventCommandBaseWidget::setData(commands);

	using Cmd = lcf::rpg::EventCommand::Code;

	ui->message->append(ToQString(m_cmd->string));

	for (size_t i = commands->index() + 1; i < commands->size(); ++i) {
		auto& cur_cmd = commands->commands()[i];

		if (static_cast<Cmd>(cur_cmd.code) != Cmd::ShowMessage_2) {
			break;
		}

		ui->message->append(ToQString(cur_cmd.string));
	}
}

void ShowMessageWidget::apply() {
	QStringList lines = ui->message->toPlainText().split('\n');
	m_commands->setLines(lcf::rpg::EventCommand::Code::ShowMessage_2, lines);
}
