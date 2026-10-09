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

#include "media_widgets.h"
#include "common/dbstring.h"
#include "defines.h"
#include "model/project_data.h"
#include "ui/common/rpg_combobox.h"
#include "ui/common/rpg_spinbox.h"
#include "ui/picker/picker_audio_widget.h"
#include "ui/picker/picker_backdrop_widget.h"
#include "ui/picker/picker_dialog.h"
#include <QButtonGroup>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QRadioButton>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <array>
#include <functional>
#include <lcf/rpg/music.h>
#include <lcf/rpg/sound.h>

namespace {
int parameter(const lcf::rpg::EventCommand* cmd, int index, int fallback = 0) {
	return index < static_cast<int>(cmd->parameters.size()) ? cmd->parameters[index] : fallback;
}

void setParameter(lcf::rpg::EventCommand* cmd, int index, int value) {
	if (index >= static_cast<int>(cmd->parameters.size())) {
		std::vector<int32_t> parameters(cmd->parameters.begin(), cmd->parameters.end());
		parameters.resize(index + 1);
		cmd->parameters = lcf::DBArray<int32_t>(parameters.begin(), parameters.end());
	}
	cmd->parameters[index] = value;
}

/** A spin box bound to a parameter, written only when the user changes it */
RpgSpinBox* spin(QWidget* parent, lcf::rpg::EventCommand* cmd, const QString& name,
	int index, int minimum, int maximum, int fallback = 0) {
	auto* result = new RpgSpinBox(parent);
	result->setObjectName(name);
	const int value = parameter(cmd, index, fallback);
	// Values of patched games are shown as they are, not clamped
	result->setRange(std::min(minimum, value), std::max(maximum, value));
	result->setValue(value);
	QObject::connect(result, &QSpinBox::valueChanged, parent, [cmd, index](int selected) {
		setParameter(cmd, index, selected);
	});
	return result;
}

/** Radio buttons in a section, bound to a parameter. A value without a button
 * checks none and is kept until one is chosen. */
QButtonGroup* radios(QWidget* parent, QLayout* layout, const QString& title, const QString& name,
	const QStringList& labels, lcf::rpg::EventCommand* cmd, int index) {
	auto* box = new QGroupBox(title, parent);
	auto* row = new QHBoxLayout(box);
	auto* group = new QButtonGroup(box);
	for (int i = 0; i < labels.size(); ++i) {
		auto* radio = new QRadioButton(labels[i], box);
		radio->setObjectName(name + QString::number(i));
		group->addButton(radio, i);
		row->addWidget(radio);
	}
	if (auto* button = group->button(parameter(cmd, index))) {
		button->setChecked(true);
	}
	QObject::connect(group, &QButtonGroup::idToggled, parent, [cmd, index](int id, bool checked) {
		if (checked) {
			setParameter(cmd, index, id);
		}
	});
	layout->addWidget(box);
	return group;
}

/** The file name of the command (its string) with a button opening a picker */
struct FileField {
	QHBoxLayout* row;
	QLineEdit* edit;
	QToolButton* browse;
};

FileField fileField(QWidget* parent, lcf::rpg::EventCommand* cmd, const QString& name) {
	FileField field;
	field.row = new QHBoxLayout();
	field.edit = new QLineEdit(ToQString(cmd->string), parent);
	field.edit->setObjectName(name);
	field.browse = new QToolButton(parent);
	field.browse->setObjectName(name + QStringLiteral("Picker"));
	field.browse->setText(QStringLiteral("..."));
	field.row->addWidget(field.edit, 1);
	field.row->addWidget(field.browse);
	QObject::connect(field.edit, &QLineEdit::textEdited, parent, [cmd](const QString& text) {
		cmd->string = ToDBString(text);
	});
	return field;
}

/** Opens the picker on a folder. A chosen file is stored in the command, then
 * `chosen` runs while the picker still exists (to read its other settings). */
void pickFile(QWidget* parent, ProjectData& project, FileFinder::FileType type, PickerChildWidget* child,
	const QString& folder, const FileField& field, lcf::rpg::EventCommand* cmd, std::function<void()> chosen = {}) {
	PickerDialog dialog(project, type, child, parent);
	QObject::connect(&dialog, &PickerDialog::fileSelected, parent, [&](const QString& file) {
		cmd->string = ToDBString(file);
		field.edit->setText(file);
		if (chosen) {
			chosen();
		}
	});
	dialog.setDirectoryAndFile(folder, field.edit->text());
	dialog.exec();
}
}

// Music and sounds

MediaAudioWidget::MediaAudioWidget(ProjectData& project, QWidget* parent, Kind kind) :
	EventCommandBaseWidget(project, parent), m_kind(kind)
{
}

PlayBGMWidget::PlayBGMWidget(ProjectData& project, QWidget* parent) :
	MediaAudioWidget(project, parent, Kind::Music)
{
	setWindowTitle(tr("Play BGM"));
}

PlaySoundWidget::PlaySoundWidget(ProjectData& project, QWidget* parent) :
	MediaAudioWidget(project, parent, Kind::Sound)
{
	setWindowTitle(tr("Play Sound Effect"));
}

ChangeSystemBGMWidget::ChangeSystemBGMWidget(ProjectData& project, QWidget* parent) :
	MediaAudioWidget(project, parent, Kind::SystemMusic)
{
	setWindowTitle(tr("Change System BGM"));
}

ChangeSystemSFXWidget::ChangeSystemSFXWidget(ProjectData& project, QWidget* parent) :
	MediaAudioWidget(project, parent, Kind::SystemSound)
{
	setWindowTitle(tr("Change System Sound Effects"));
}

void MediaAudioWidget::setData(lcf::rpg::EventCommand* cmd) {
	EventCommandBaseWidget::setData(cmd);
	const bool music = m_kind == Kind::Music || m_kind == Kind::SystemMusic;
	const bool system = m_kind == Kind::SystemMusic || m_kind == Kind::SystemSound;
	// The system commands start with the event using the audio
	const int first = system ? 1 : 0;
	const int volume_index = first + (music ? 1 : 0);

	auto* layout = new QVBoxLayout(this);
	if (system) {
		auto* event = new QGroupBox(tr("Event"), this);
		auto* event_layout = new QVBoxLayout(event);
		auto* context = new QComboBox(event);
		context->setObjectName(QStringLiteral("audioEvent"));
		const QStringList events = music
			? QStringList{tr("Battle"), tr("Victory"), tr("Inn"), tr("Skiff"), tr("Ship"), tr("Airship"), tr("Game Over")}
			: QStringList{tr("Cursor"), tr("Decision"), tr("Cancel"), tr("Buzzer"), tr("Begin Battle"), tr("Escape"),
				tr("Enemy Attacks"), tr("Enemy Damage"), tr("Ally Damage"), tr("Evasion"), tr("Enemy Defeated"), tr("Use Item")};
		for (int i = 0; i < events.size(); ++i) {
			context->addItem(events[i], i);
		}
		const int value = parameter(cmd, 0);
		if (value < 0 || value >= events.size()) {
			context->addItem(tr("Unknown (%1)").arg(value), value);
		}
		context->setCurrentIndex(context->findData(value));
		event_layout->addWidget(context);
		layout->addWidget(event);
		connect(context, &QComboBox::currentIndexChanged, this, [cmd, context](int index) {
			setParameter(cmd, 0, context->itemData(index).toInt());
		});
	}

	auto* audio = new QGroupBox(system ? (music ? tr("Associated BGM") : tr("Associated Sound"))
		: (music ? tr("BGM") : tr("Sound Effect")), this);
	auto* form = new QFormLayout(audio);
	auto file = fileField(audio, cmd, QStringLiteral("audioFile"));
	// "(OFF)" stops the music or plays no sound, like the first entry of RPG Maker's list
	auto* off = new QToolButton(audio);
	off->setObjectName(QStringLiteral("audioOff"));
	off->setText(tr("(OFF)"));
	file.row->addWidget(off);
	form->addRow(tr("File"), file.row);
	RpgSpinBox* fade = nullptr;
	if (music) {
		fade = spin(audio, cmd, QStringLiteral("audioFadeIn"), first, 0, 10000);
		fade->setSuffix(tr(" ms"));
		fade->setSingleStep(1000);
		form->addRow(tr("Fade-in Time"), fade);
	}
	auto* volume = spin(audio, cmd, QStringLiteral("audioVolume"), volume_index, 0, 100, 100);
	volume->setSuffix(tr(" %"));
	auto* tempo = spin(audio, cmd, QStringLiteral("audioTempo"), volume_index + 1, 50, 150, 100);
	tempo->setSuffix(tr(" %"));
	auto* balance = spin(audio, cmd, QStringLiteral("audioBalance"), volume_index + 2, 0, 100, 50);
	balance->setToolTip(tr("0: left, 50: center, 100: right"));
	form->addRow(tr("Volume"), volume);
	form->addRow(tr("Tempo"), tempo);
	form->addRow(tr("Balance"), balance);
	layout->addWidget(audio);
	layout->addStretch();

	connect(off, &QToolButton::clicked, this, [cmd, file] {
		file.edit->setText(QStringLiteral("(OFF)"));
		cmd->string = ToDBString(file.edit->text());
	});
	connect(file.browse, &QToolButton::clicked, this, [=, this] {
		PickerAudioWidget* picker;
		if (music) {
			lcf::rpg::Music selection;
			selection.fadein = fade->value();
			selection.volume = volume->value();
			selection.tempo = tempo->value();
			selection.balance = balance->value();
			picker = new PickerAudioWidget(selection);
		} else {
			lcf::rpg::Sound selection;
			selection.volume = volume->value();
			selection.tempo = tempo->value();
			selection.balance = balance->value();
			picker = new PickerAudioWidget(selection);
		}
		// The picker's sliders round and clamp (whole seconds...): only the
		// settings moved in the picker are copied back
		const std::array<int, 4> before = {picker->fadeInTime(), picker->volume(), picker->tempo(), picker->balance()};
		pickFile(this, m_project, music ? FileFinder::FileType::Music : FileFinder::FileType::Sound,
			picker, music ? MUSIC : SOUND, file, cmd, [=] {
			if (fade && picker->fadeInTime() != before[0]) {
				fade->setValue(picker->fadeInTime());
			}
			if (picker->volume() != before[1]) {
				volume->setValue(picker->volume());
			}
			if (picker->tempo() != before[2]) {
				tempo->setValue(picker->tempo());
			}
			if (picker->balance() != before[3]) {
				balance->setValue(picker->balance());
			}
		});
	});
}

// Fade Out BGM

FadeOutBGMWidget::FadeOutBGMWidget(ProjectData& project, QWidget* parent) :
	EventCommandBaseWidget(project, parent)
{
	setWindowTitle(tr("Fade Out BGM"));
}

void FadeOutBGMWidget::setData(lcf::rpg::EventCommand* cmd) {
	EventCommandBaseWidget::setData(cmd);
	auto* layout = new QVBoxLayout(this);
	auto* group = new QGroupBox(tr("Fade Out Time"), this);
	auto* form = new QFormLayout(group);
	auto* time = spin(group, cmd, QStringLiteral("fadeOutTime"), 0, 0, 20000);
	time->setSuffix(tr(" ms"));
	time->setSingleStep(1000);
	form->addRow(tr("Time"), time);
	layout->addWidget(group);
	layout->addStretch();
}

// Play Movie

PlayMovieWidget::PlayMovieWidget(ProjectData& project, QWidget* parent) :
	EventCommandBaseWidget(project, parent)
{
	setWindowTitle(tr("Play Movie"));
}

void PlayMovieWidget::setData(lcf::rpg::EventCommand* cmd) {
	EventCommandBaseWidget::setData(cmd);
	auto* layout = new QVBoxLayout(this);

	auto* movie = new QGroupBox(tr("Movie File"), this);
	auto* movie_layout = new QVBoxLayout(movie);
	auto file = fileField(movie, cmd, QStringLiteral("movieFile"));
	movie_layout->addLayout(file.row);
	layout->addWidget(movie);
	connect(file.browse, &QToolButton::clicked, this, [=, this] {
		pickFile(this, m_project, FileFinder::FileType::Video, new PickerChildWidget(nullptr), MOVIE, file, cmd);
	});

	auto* resolution = new QGroupBox(tr("Resolution"), this);
	auto* resolution_form = new QFormLayout(resolution);
	resolution_form->addRow(tr("Width"), spin(resolution, cmd, QStringLiteral("movieWidth"), 3, 1, 9999, 320));
	resolution_form->addRow(tr("Height"), spin(resolution, cmd, QStringLiteral("movieHeight"), 4, 1, 9999, 240));
	layout->addWidget(resolution);

	// Parameters 1 and 2: the position, or the variables holding it
	auto* position = new QGroupBox(tr("Coordinates of Upper-Left Pixel"), this);
	auto* position_form = new QFormLayout(position);
	auto* modes = new QButtonGroup(position);
	auto* specific = new QRadioButton(tr("Specific Coordinates"), position);
	specific->setObjectName(QStringLiteral("movieSpecific"));
	auto* variables = new QRadioButton(tr("Variable Reference"), position);
	variables->setObjectName(QStringLiteral("movieVariables"));
	modes->addButton(specific, 0);
	modes->addButton(variables, 1);
	const int mode = parameter(cmd, 0);
	const int x_value = parameter(cmd, 1);
	const int y_value = parameter(cmd, 2);
	auto* x = new RpgSpinBox(position);
	auto* y = new RpgSpinBox(position);
	x->setObjectName(QStringLiteral("movieX"));
	y->setObjectName(QStringLiteral("movieY"));
	x->setRange(std::min(-9999, x_value), std::max(9999, x_value));
	y->setRange(std::min(-9999, y_value), std::max(9999, y_value));
	auto* x_variable = new VariableRpgComboBox(position);
	auto* y_variable = new VariableRpgComboBox(position);
	x_variable->comboBox()->setObjectName(QStringLiteral("movieXVariable"));
	y_variable->comboBox()->setObjectName(QStringLiteral("movieYVariable"));
	x_variable->makeModel(m_project);
	y_variable->makeModel(m_project);
	if (mode == 1) {
		x_variable->comboBox()->setCurrentIndex(x_value - 1);
		y_variable->comboBox()->setCurrentIndex(y_value - 1);
	} else {
		x->setValue(x_value);
		y->setValue(y_value);
	}
	position_form->addRow(specific);
	position_form->addRow(tr("X"), x);
	position_form->addRow(tr("Y"), y);
	position_form->addRow(variables);
	position_form->addRow(tr("X"), x_variable);
	position_form->addRow(tr("Y"), y_variable);
	layout->addWidget(position);
	layout->addStretch();

	auto enable = [=] {
		x->setEnabled(specific->isChecked());
		y->setEnabled(specific->isChecked());
		x_variable->setEnabled(variables->isChecked());
		y_variable->setEnabled(variables->isChecked());
	};
	if (auto* button = modes->button(mode)) {
		button->setChecked(true);
	}
	enable();

	// Only the controls of the chosen mode write the position
	auto store = [=] {
		if (specific->isChecked()) {
			setParameter(cmd, 1, x->value());
			setParameter(cmd, 2, y->value());
		} else if (variables->isChecked()) {
			setParameter(cmd, 1, x_variable->comboBox()->currentIndex() + 1);
			setParameter(cmd, 2, y_variable->comboBox()->currentIndex() + 1);
		}
	};
	connect(modes, &QButtonGroup::idToggled, this, [=](int id, bool checked) {
		if (checked) {
			setParameter(cmd, 0, id);
			enable();
			store();
		}
	});
	connect(x, &QSpinBox::valueChanged, this, store);
	connect(y, &QSpinBox::valueChanged, this, store);
	connect(x_variable->comboBox(), &QComboBox::currentIndexChanged, this, store);
	connect(y_variable->comboBox(), &QComboBox::currentIndexChanged, this, store);
}

// Change System Graphics

ChangeSystemGraphicsWidget::ChangeSystemGraphicsWidget(ProjectData& project, QWidget* parent) :
	EventCommandBaseWidget(project, parent)
{
	setWindowTitle(tr("Change System Graphics"));
}

void ChangeSystemGraphicsWidget::setData(lcf::rpg::EventCommand* cmd) {
	EventCommandBaseWidget::setData(cmd);
	auto* layout = new QVBoxLayout(this);
	auto* graphic = new QGroupBox(tr("System Graphic"), this);
	auto* graphic_layout = new QVBoxLayout(graphic);
	auto file = fileField(graphic, cmd, QStringLiteral("systemGraphic"));
	graphic_layout->addLayout(file.row);
	layout->addWidget(graphic);
	connect(file.browse, &QToolButton::clicked, this, [=, this] {
		pickFile(this, m_project, FileFinder::FileType::Image, new PickerBackdropWidget(), SYSTEM, file, cmd);
	});
	radios(this, layout, tr("Graphic Pattern"), QStringLiteral("systemPattern"),
		{tr("Stretch to Fit"), tr("Tiled Display")}, cmd, 0);
	radios(this, layout, tr("Text Font"), QStringLiteral("systemFont"),
		{tr("MS Gothic"), tr("MS Mincho")}, cmd, 1);
	layout->addStretch();
}
