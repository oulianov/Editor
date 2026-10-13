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

#include "message_input_widgets.h"
#include "ui/event/event_command_palette.h"
#include "event_branch_helpers.h"
#include "common/dbstring.h"
#include "defines.h"
#include "model/event_command_defaults.h"
#include "model/event_command_list.h"
#include "model/project_data.h"
#include "ui/common/rpg_combobox.h"
#include "ui/common/rpg_spinbox.h"
#include "ui/picker/picker_dialog.h"
#include "ui/picker/picker_faceset_widget.h"
#include "ui/viewer/faceset_graphics_item.h"
#include "ui/viewer/rpg_graphics_view.h"
#include "model/project.h"
#include <QButtonGroup>
#include <QCheckBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>
#include <algorithm>

namespace {
using Cmd = lcf::rpg::EventCommand::Code;

// Choice markers: 0-3 are the choices, 4 is the cancel branch
constexpr int cancel_branch = 4;
constexpr int cancel_handler = 5;

int parameter(const lcf::rpg::EventCommand& cmd, int index, int fallback = 0) {
	return index < static_cast<int>(cmd.parameters.size()) ? cmd.parameters[index] : fallback;
}

void setParameter(lcf::rpg::EventCommand& cmd, int index, int value) {
	if (index >= static_cast<int>(cmd.parameters.size())) {
		std::vector<int32_t> parameters(cmd.parameters.begin(), cmd.parameters.end());
		parameters.resize(index + 1);
		cmd.parameters = lcf::DBArray<int32_t>(parameters.begin(), parameters.end());
	}
	cmd.parameters[index] = value;
}

/** Radio buttons in a section, bound to a parameter. A value without a button
 * (a patch, a damaged file) checks none and is kept until one is chosen. */
QButtonGroup* radios(QWidget* parent, QLayout* layout, const QString& title, const QString& name,
	const QStringList& labels, lcf::rpg::EventCommand* cmd, int index, int fallback = 0) {
	auto* box = new QGroupBox(title, parent);
	auto* row = new QHBoxLayout(box);
	auto* group = new QButtonGroup(box);
	for (int i = 0; i < labels.size(); ++i) {
		auto* radio = new QRadioButton(labels[i], box);
		radio->setObjectName(name + QString::number(i));
		group->addButton(radio, i);
		row->addWidget(radio);
	}
	if (auto* button = group->button(parameter(*cmd, index, fallback))) {
		button->setChecked(true);
	}
	QObject::connect(group, &QButtonGroup::idToggled, parent, [cmd, index](int id, bool checked) {
		if (checked) {
			setParameter(*cmd, index, id);
		}
	});
	layout->addWidget(box);
	return group;
}
}

// Show Choices

ShowChoiceWidget::ShowChoiceWidget(ProjectData& project, QWidget* parent) :
	EventCommandBaseWidget(project, parent)
{
	setWindowTitle(tr("Show Choices"));
}

void ShowChoiceWidget::setData(EventCommandList* commands) {
	// Edits a copy: the page's commands change only in apply()
	m_commands = commands;
	m_edited = commands->command();
	m_cmd = &m_edited;

	const auto& list = commands->commands();
	const auto [start, end] = EventCommandDefaults::range(list, commands->index());
	for (int i = start + 1; i < end; ++i) {
		const int choice = parameter(list[i], 0, -1);
		if (list[i].code == static_cast<int>(Cmd::ShowChoiceOption) && list[i].indent == m_edited.indent
			&& choice >= 0 && choice < 4) {
			m_present[choice] = true;
			m_texts[choice] = ToQString(list[i].string);
		}
	}

	auto* layout = new QVBoxLayout(this);
	auto* choices = new QGroupBox(tr("Choices"), this);
	auto* form = new QFormLayout(choices);
	for (int i = 0; i < 4; ++i) {
		m_choices[i] = new QLineEdit(m_texts[i], choices);
		m_choices[i]->setObjectName(QStringLiteral("choice%1").arg(i + 1));
		form->addRow(tr("Choice %1").arg(i + 1), m_choices[i]);
		connect(m_choices[i], &QLineEdit::textEdited, this, &ShowChoiceWidget::updateCancel);
	}
	layout->addWidget(choices);

	auto* cancel = new QGroupBox(tr("Cancellation Behavior"), this);
	auto* grid = new QGridLayout(cancel);
	m_cancel = new QButtonGroup(cancel);
	const QStringList labels = {tr("Ignore"), tr("Select Choice 1"), tr("Select Choice 2"),
		tr("Select Choice 3"), tr("Select Choice 4"), tr("Execute Handler")};
	for (int i = 0; i < labels.size(); ++i) {
		auto* radio = new QRadioButton(labels[i], cancel);
		radio->setObjectName(QStringLiteral("choiceCancel%1").arg(i));
		m_cancel->addButton(radio, i);
		grid->addWidget(radio, i / 2, i % 2);
	}
	if (auto* button = m_cancel->button(parameter(m_edited, 0))) {
		button->setChecked(true);
	}
	layout->addWidget(cancel);
	layout->addStretch();

	for (int i = 0; i < 4; ++i) {
		m_cancel->button(i + 1)->setEnabled(!m_texts[i].isEmpty());
	}
}

void ShowChoiceWidget::updateCancel() {
	for (int i = 0; i < 4; ++i) {
		auto* button = m_cancel->button(i + 1);
		button->setEnabled(!m_choices[i]->text().isEmpty());
		if (button->isChecked() && !button->isEnabled()) {
			m_cancel->button(0)->setChecked(true);
		}
	}
}

bool ShowChoiceWidget::apply() {
	const int cancel = m_cancel->checkedId() >= 0 ? m_cancel->checkedId() : parameter(m_edited, 0);
	bool texts_changed = false;
	for (int i = 0; i < 4; ++i) {
		texts_changed |= m_choices[i]->text() != m_texts[i];
	}
	if (!texts_changed && cancel == parameter(m_edited, 0)) {
		return true;
	}

	// Empty fields are not choices, like in RPG Maker: the next ones move up with
	// their branches. A kept branch is matched by its old number (new ones get
	// numbers no old branch has), then all are renumbered once applied.
	const int indent = m_edited.indent;
	std::vector<lcf::rpg::EventCommand> markers;
	QStringList texts;
	int new_cancel = cancel;
	for (int i = 0; i < 4; ++i) {
		const QString text = m_choices[i]->text();
		if (text.isEmpty()) {
			if (cancel == i + 1) {
				new_cancel = 0;
			}
			continue;
		}
		if (cancel == i + 1) {
			new_cancel = texts.size() + 1;
		}
		auto marker = EventCommandDefaults::make(Cmd::ShowChoiceOption, indent).front();
		setParameter(marker, 0, m_present[i] ? i : cancel_handler + 1 + i);
		marker.string = ToDBString(text);
		markers.push_back(marker);
		texts << text;
	}
	if (texts.isEmpty()) {
		QMessageBox::warning(this, tr("Show Choices"), tr("Enter at least one choice."));
		return false;
	}
	if (cancel == cancel_handler) {
		auto marker = EventCommandDefaults::make(Cmd::ShowChoiceOption, indent).front();
		setParameter(marker, 0, cancel_branch);
		markers.push_back(marker);
	}

	auto edited = m_edited;
	setParameter(edited, 0, new_cancel);
	if (texts_changed) {
		// The summary line's separator depends on the RPG Maker version: keep the one used
		QStringList old;
		for (const auto& text : m_texts) {
			if (!text.isEmpty()) {
				old << text;
			}
		}
		const bool comma = old.size() > 1 && ToQString(m_edited.string) == old.join(QStringLiteral(", "));
		edited.string = ToDBString(texts.join(comma ? QStringLiteral(", ") : QStringLiteral("/")));
	}
	if (!EventBranchHelpers::apply(*m_commands, edited, Cmd::ShowChoiceEnd, markers, this, false, true)) {
		return false;
	}

	auto& list = m_commands->commands();
	const auto [start, end] = EventCommandDefaults::range(list, m_commands->index());
	int number = 0;
	for (int i = start + 1; i < end; ++i) {
		if (list[i].code == static_cast<int>(Cmd::ShowChoiceOption) && list[i].indent == indent
			&& list[i].parameters[0] != cancel_branch) {
			list[i].parameters[0] = number++;
		}
	}
	return true;
}

// Input Number

InputNumberWidget::InputNumberWidget(ProjectData& project, QWidget* parent) :
	EventCommandBaseWidget(project, parent)
{
	setWindowTitle(tr("Input Number"));
}

void InputNumberWidget::setData(lcf::rpg::EventCommand* cmd) {
	EventCommandBaseWidget::setData(cmd);

	auto* layout = new QFormLayout(this);
	auto* digits = new RpgSpinBox(this);
	digits->setObjectName(QStringLiteral("inputDigits"));
	const int value = parameter(*cmd, 0, 1);
	// 1 to 6 digits in RPG Maker (its help), 9 with the Maniac Patch;
	// unusual values (test games, patches) are shown and kept
	const int max_digits = EventCommandPalette::usesManiacPatch(m_project.project()) ? 9 : 6;
	digits->setRange(std::min(1, value), std::max(max_digits, value));
	digits->setValue(value);
	layout->addRow(tr("Digits Allowed"), digits);

	auto* variable = new VariableRpgComboBox(this);
	variable->comboBox()->setObjectName(QStringLiteral("inputVariable"));
	variable->makeModel(m_project);
	variable->comboBox()->setCurrentIndex(parameter(*cmd, 1, 1) - 1);
	layout->addRow(tr("Store in Variable"), variable);

	connect(digits, &QSpinBox::valueChanged, this, [cmd](int digits) { setParameter(*cmd, 0, digits); });
	connect(variable->comboBox(), &QComboBox::currentIndexChanged, this, [cmd](int index) {
		if (index >= 0) {
			setParameter(*cmd, 1, index + 1);
		}
	});
}

// Change Face Graphic

ChangeFaceGraphicWidget::ChangeFaceGraphicWidget(ProjectData& project, QWidget* parent) :
	EventCommandBaseWidget(project, parent)
{
	setWindowTitle(tr("Face Graphics"));
}

void ChangeFaceGraphicWidget::setData(lcf::rpg::EventCommand* cmd) {
	EventCommandBaseWidget::setData(cmd);

	auto* layout = new QVBoxLayout(this);
	auto* graphic = new QGroupBox(tr("Face Graphic"), this);
	auto* graphic_layout = new QHBoxLayout(graphic);
	auto* view = new FaceSetGraphicsView(graphic);
	view->setObjectName(QStringLiteral("faceView"));
	m_face = new FaceSetGraphicsItem(m_project);
	view->setItem(m_face);
	graphic_layout->addWidget(view);
	auto* buttons = new QVBoxLayout();
	auto* set = new QPushButton(tr("Set..."), graphic);
	set->setObjectName(QStringLiteral("faceSet"));
	auto* erase = new QPushButton(tr("Erase"), graphic);
	erase->setObjectName(QStringLiteral("faceErase"));
	m_file = new QLabel(graphic);
	m_file->setObjectName(QStringLiteral("faceFile"));
	buttons->addWidget(set);
	buttons->addWidget(erase);
	buttons->addWidget(m_file);
	buttons->addStretch();
	graphic_layout->addLayout(buttons, 1);
	layout->addWidget(graphic);

	// Player: 0 left, any other value right
	radios(this, layout, tr("Display Position"), QStringLiteral("facePosition"),
		{tr("Left"), tr("Right")}, cmd, 1);

	auto* option = new QGroupBox(tr("Option"), this);
	auto* option_layout = new QVBoxLayout(option);
	auto* flip = new QCheckBox(tr("Flip Image Horizontally"), option);
	flip->setObjectName(QStringLiteral("faceFlip"));
	flip->setChecked(parameter(*cmd, 2) != 0);
	option_layout->addWidget(flip);
	layout->addWidget(option);
	layout->addStretch();

	auto pick = [this, cmd] {
		auto* picker = new PickerFacesetWidget(parameter(*cmd, 0), this);
		PickerDialog dialog(m_project, FileFinder::FileType::Image, picker, this);
		dialog.setWindowTitle(tr("Select Face Graphic"));
		connect(&dialog, &PickerDialog::fileSelected, this, [=, this](const QString& file) {
			cmd->string = ToDBString(file);
			setParameter(*cmd, 0, picker->index());
			refreshFace();
		});
		dialog.setDirectoryAndFile(FACESET, ToQString(cmd->string));
		dialog.exec();
	};
	connect(set, &QPushButton::clicked, this, pick);
	connect(view, &RpgGraphicsViewBase::clicked, this, pick);
	connect(erase, &QPushButton::clicked, this, [this, cmd] {
		cmd->string = lcf::DBString();
		refreshFace();
	});
	connect(flip, &QCheckBox::toggled, this, [cmd](bool checked) { setParameter(*cmd, 2, checked ? 1 : 0); });
	refreshFace();
}

void ChangeFaceGraphicWidget::refreshFace() {
	const QString file = ToQString(m_cmd->string);
	m_face->refresh(file, parameter(*m_cmd, 0));
	m_file->setText(file.isEmpty() ? tr("(None)") : tr("%1, face %2").arg(file).arg(parameter(*m_cmd, 0)));
}

// Enter Hero Name

EnterHeroNameWidget::EnterHeroNameWidget(ProjectData& project, QWidget* parent) :
	EventCommandBaseWidget(project, parent)
{
	setWindowTitle(tr("Enter Hero Name"));
}

void EnterHeroNameWidget::setData(lcf::rpg::EventCommand* cmd) {
	EventCommandBaseWidget::setData(cmd);

	auto* layout = new QVBoxLayout(this);
	auto* hero = new QGroupBox(tr("Hero"), this);
	auto* hero_layout = new QVBoxLayout(hero);
	auto* actor = new ActorRpgComboBox(hero);
	actor->comboBox()->setObjectName(QStringLiteral("nameHero"));
	actor->makeModel(m_project);
	actor->comboBox()->setCurrentIndex(parameter(*cmd, 0, 1) - 1);
	hero_layout->addWidget(actor);
	layout->addWidget(hero);
	connect(actor->comboBox(), &QComboBox::currentIndexChanged, this, [cmd](int index) {
		if (index >= 0) {
			setParameter(*cmd, 0, index + 1);
		}
	});

	// The keyboard pages depend on the game's encoding: EasyRPG Player shows the
	// language's own pages first, then letters and symbols. RPG Maker offers the first two.
	const QString encoding = m_project.project().encoding().toLower();
	auto is = [&](std::initializer_list<const char*> codes) {
		return std::any_of(codes.begin(), codes.end(), [&](const char* code) { return encoding.contains(QLatin1String(code)); });
	};
	QStringList pages = {tr("Letters"), tr("Symbols")};
	if (is({"932", "943"})) {
		pages = {tr("Hiragana"), tr("Katakana")};
	} else if (is({"949"})) {
		pages = {tr("Hangul 1"), tr("Hangul 2")};
	} else if (is({"936"})) {
		pages = {tr("Simplified Chinese 1"), tr("Simplified Chinese 2")};
	} else if (is({"950", "big5"})) {
		pages = {tr("Traditional Chinese 1"), tr("Traditional Chinese 2")};
	} else if (is({"1251", "5347"})) {
		pages = {tr("Cyrillic"), tr("Letters")};
	}
	radios(this, layout, tr("Initial Character Set"), QStringLiteral("nameCharacterSet"), pages, cmd, 1);

	auto* option = new QGroupBox(tr("Option"), this);
	auto* option_layout = new QVBoxLayout(option);
	auto* default_name = new QCheckBox(tr("Show Default Name in Entry Area"), option);
	default_name->setObjectName(QStringLiteral("nameShowDefault"));
	default_name->setChecked(parameter(*cmd, 2) != 0);
	option_layout->addWidget(default_name);
	layout->addWidget(option);
	layout->addStretch();
	connect(default_name, &QCheckBox::toggled, this, [cmd](bool checked) { setParameter(*cmd, 2, checked ? 1 : 0); });
}
