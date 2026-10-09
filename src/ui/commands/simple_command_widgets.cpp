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

#include "simple_command_widgets.h"
#include "core.h"
#include "common/dbstring.h"
#include "model/event_command_list.h"
#include "model/project_data.h"
#include "ui/common/rpg_combobox.h"
#include "ui/common/rpg_spinbox.h"
#include <QButtonGroup>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QRadioButton>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace {
constexpr int this_event = 10005;

void setParameter(lcf::rpg::EventCommand* cmd, int index, int value) {
	if (static_cast<int>(cmd->parameters.size()) <= index) {
		std::vector<int32_t> params(cmd->parameters.begin(), cmd->parameters.end());
		params.resize(index + 1);
		cmd->parameters = lcf::DBArray<int32_t>(params.begin(), params.end());
	}
	cmd->parameters[index] = value;
}

int parameter(const lcf::rpg::EventCommand* cmd, int index) {
	return index < static_cast<int>(cmd->parameters.size()) ? cmd->parameters[index] : 0;
}
}

// Wait

WaitWidget::WaitWidget(ProjectData& project, QWidget* parent) :
	EventCommandBaseWidget(project, parent)
{
	setWindowTitle(tr("Wait"));
}

void WaitWidget::setData(lcf::rpg::EventCommand* cmd) {
	EventCommandBaseWidget::setData(cmd);

	auto* layout = new QVBoxLayout(this);
	auto* group = new QButtonGroup(this);
	auto* time = new QRadioButton(tr("Wait for"), this);
	auto* key = new QRadioButton(tr("Wait until a key is pressed"), this);
	group->addButton(time, 0);
	group->addButton(key, 1);

	auto* tenths = new RpgSpinBox(this);
	tenths->setRange(0, 9999);
	tenths->setSuffix(tr(" × 0.1 s"));
	tenths->setValue(parameter(cmd, 0));

	auto* row = new QHBoxLayout();
	row->addWidget(time);
	row->addWidget(tenths, 1);
	layout->addLayout(row);
	layout->addWidget(key);

	const bool rpg2k3 = m_project.database().system.ldb_id == 2003;
	key->setEnabled(rpg2k3);
	(parameter(cmd, 1) == 1 && rpg2k3 ? key : time)->setChecked(true);
	tenths->setEnabled(time->isChecked());

	connect(tenths, &QSpinBox::valueChanged, this, [cmd](int value) { setParameter(cmd, 0, value); });
	connect(group, &QButtonGroup::idToggled, this, [=](int id, bool checked) {
		if (checked) {
			tenths->setEnabled(id == 0);
			if (id == 1 || cmd->parameters.size() > 1) {
				setParameter(cmd, 1, id);
			}
		}
	});
}

// Comment

CommentWidget::CommentWidget(ProjectData& project, QWidget* parent) :
	EventCommandBaseWidget(project, parent)
{
	setWindowTitle(tr("Comment"));
	auto* layout = new QVBoxLayout(this);
	m_text = new QPlainTextEdit(this);
	m_text->setMinimumSize(420, 160);
	layout->addWidget(m_text);
}

void CommentWidget::setData(EventCommandList* commands) {
	EventCommandBaseWidget::setData(commands);
	using Cmd = lcf::rpg::EventCommand::Code;

	QStringList lines = {ToQString(m_cmd->string)};
	for (size_t i = commands->index() + 1; i < commands->size(); ++i) {
		const auto& cmd = commands->commands()[i];
		if (static_cast<Cmd>(cmd.code) != Cmd::Comment_2) {
			break;
		}
		lines << ToQString(cmd.string);
	}
	m_text->setPlainText(lines.join('\n'));
	m_text->moveCursor(QTextCursor::End);
	m_text->setFocus();
}

void CommentWidget::apply() {
	m_commands->setLines(lcf::rpg::EventCommand::Code::Comment_2, m_text->toPlainText().split('\n'));
}

// Call Event

CallEventWidget::CallEventWidget(ProjectData& project, QWidget* parent) :
	EventCommandBaseWidget(project, parent)
{
	setWindowTitle(tr("Call Event"));
}

void CallEventWidget::setData(lcf::rpg::EventCommand* cmd) {
	EventCommandBaseWidget::setData(cmd);
	m_loading = true;

	auto* layout = new QVBoxLayout(this);
	m_kind = new QButtonGroup(this);
	auto* common = new QRadioButton(tr("Common Event"), this);
	auto* map = new QRadioButton(tr("Map Event"), this);
	auto* variables = new QRadioButton(tr("Map Event chosen by variables"), this);
	m_kind->addButton(common, 0);
	m_kind->addButton(map, 1);
	m_kind->addButton(variables, 2);
	auto* kinds = new QHBoxLayout();
	kinds->addWidget(common);
	kinds->addWidget(map);
	kinds->addWidget(variables);
	layout->addLayout(kinds);

	m_pages = new QStackedWidget(this);
	layout->addWidget(m_pages);

	// common event
	auto* common_page = new QWidget(m_pages);
	auto* common_form = new QFormLayout(common_page);
	auto* common_combo = new CommonEventRpgComboBox(common_page);
	common_combo->makeModel(m_project);
	m_commonEvent = common_combo;
	common_form->addRow(tr("Common Event"), common_combo);
	m_pages->addWidget(common_page);

	// map event and page
	auto* map_page = new QWidget(m_pages);
	auto* map_form = new QFormLayout(map_page);
	m_mapEvent = new QComboBox(map_page);
	m_mapEvent->addItem(tr("This Event"), this_event);
	if (auto* events = core().currentMapEvents()) {
		for (auto it = events->cbegin(); it != events->cend(); ++it) {
			m_mapEvent->addItem(QString("%1: %2").arg(it.key(), 4, 10, QChar('0')).arg(ToQString(it.value()->name)), it.key());
		}
	}
	m_page = new RpgSpinBox(map_page);
	m_page->setRange(1, 100);
	map_form->addRow(tr("Event"), m_mapEvent);
	map_form->addRow(tr("Page"), m_page);
	m_pages->addWidget(map_page);

	// variables holding the event and the page
	auto* var_page = new QWidget(m_pages);
	auto* var_form = new QFormLayout(var_page);
	auto* event_var = new VariableRpgComboBox(var_page);
	auto* page_var = new VariableRpgComboBox(var_page);
	event_var->makeModel(m_project);
	page_var->makeModel(m_project);
	m_eventVariable = event_var;
	m_pageVariable = page_var;
	var_form->addRow(tr("Event ID in"), event_var);
	var_form->addRow(tr("Page in"), page_var);
	m_pages->addWidget(var_page);

	// current values
	const int kind = std::clamp(parameter(cmd, 0), 0, 2);
	m_kind->button(kind)->setChecked(true);
	m_pages->setCurrentIndex(kind);
	const int id = parameter(cmd, 1);
	const int page = parameter(cmd, 2);
	if (kind == 0) {
		m_commonEvent->comboBox()->setCurrentIndex(id - 1);
	} else if (kind == 1) {
		int index = m_mapEvent->findData(id);
		if (index < 0) {
			m_mapEvent->addItem(QString("%1").arg(id, 4, 10, QChar('0')), id);
			index = m_mapEvent->count() - 1;
		}
		m_mapEvent->setCurrentIndex(index);
		m_page->setValue(std::max(1, page));
	} else {
		m_eventVariable->comboBox()->setCurrentIndex(id - 1);
		m_pageVariable->comboBox()->setCurrentIndex(page - 1);
	}

	connect(m_kind, &QButtonGroup::idToggled, this, [this](int id, bool checked) {
		if (checked) {
			m_pages->setCurrentIndex(id);
			store();
		}
	});
	for (auto* combo : {m_commonEvent->comboBox(), m_mapEvent, m_eventVariable->comboBox(), m_pageVariable->comboBox()}) {
		connect(combo, &QComboBox::currentIndexChanged, this, &CallEventWidget::store);
	}
	connect(m_page, &QSpinBox::valueChanged, this, &CallEventWidget::store);

	m_loading = false;
	store();
}

void CallEventWidget::store() {
	if (m_loading) {
		return;
	}
	const int kind = m_kind->checkedId();
	setParameter(m_cmd, 0, kind);
	if (kind == 0) {
		setParameter(m_cmd, 1, m_commonEvent->comboBox()->currentIndex() + 1);
		setParameter(m_cmd, 2, 0);
	} else if (kind == 1) {
		setParameter(m_cmd, 1, m_mapEvent->currentData().toInt());
		setParameter(m_cmd, 2, m_page->value());
	} else {
		setParameter(m_cmd, 1, m_eventVariable->comboBox()->currentIndex() + 1);
		setParameter(m_cmd, 2, m_pageVariable->comboBox()->currentIndex() + 1);
	}
}
