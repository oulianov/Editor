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

#include "system_fields.h"
#include <functional>
#include <type_traits>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QScrollArea>
#include <QToolButton>
#include <lcf/rpg/music.h>
#include <lcf/rpg/sound.h>
#include "defines.h"
#include "model/project_data.h"
#include "ui/picker/picker_audio_widget.h"
#include "ui/picker/picker_backdrop_widget.h"
#include "ui/picker/picker_charset_widget.h"
#include "ui/picker/picker_dialog.h"

namespace {
// A read-only line showing text() and a button running pick(), then showing the new text
QWidget* pickerField(QWidget* parent, std::function<QString()> text, std::function<void(QWidget*)> pick) {
	auto* field = new QWidget(parent);
	auto* layout = new QHBoxLayout(field);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(2);
	auto* line = new QLineEdit(text(), field);
	line->setReadOnly(true);
	// a tool button: as tall as the line (a push button has a large bezel on macOS)
	auto* button = new QToolButton(field);
	button->setText(QStringLiteral("..."));
	layout->addWidget(line, 1);
	layout->addWidget(button);
	QObject::connect(button, &QToolButton::clicked, field, [=] {
		pick(field);
		line->setText(text());
	});
	return field;
}

template<class Audio>
QWidget* makeAudioField(ProjectData& project, Audio& audio, FileFinder::FileType type, const QString& dir, QWidget* parent) {
	return pickerField(parent, [&audio] { return ToQString(audio.name); }, [&project, &audio, type, dir](QWidget* field) {
		auto* settings = new PickerAudioWidget(audio);
		PickerDialog dialog(project, type, settings, field);
		QObject::connect(&dialog, &PickerDialog::fileSelected, field, [&](const QString& baseName) {
			audio.name = baseName.toStdString();
			if constexpr (std::is_same_v<Audio, lcf::rpg::Music>) {
				audio.fadein = settings->fadeInTime();
			}
			audio.volume = settings->volume();
			audio.tempo = settings->tempo();
			audio.balance = settings->balance();
		});
		dialog.setDirectoryAndFile(dir, ToQString(audio.name));
		dialog.exec();
	});
}
}

QWidget* SystemFields::imageField(ProjectData& project, const QString& dir, lcf::DBString& file,
		QWidget* parent, int32_t* index) {
	auto text = [&file, index] {
		if (!index || file.empty()) {
			return ToQString(file);
		}
		return QStringLiteral("%1 (%2)").arg(ToQString(file)).arg(*index + 1);
	};
	return pickerField(parent, text, [&project, dir, &file, index](QWidget* field) {
		auto* charset = index ? new PickerCharsetWidget(*index) : nullptr;
		PickerChildWidget* preview = charset ? static_cast<PickerChildWidget*>(charset) : new PickerBackdropWidget();
		PickerDialog dialog(project, FileFinder::FileType::Image, preview, field);
		QObject::connect(&dialog, &PickerDialog::fileSelected, field, [&](const QString& baseName) {
			file = ToDBString(baseName);
			if (charset) {
				*index = charset->index();
			}
		});
		dialog.setDirectoryAndFile(dir, ToQString(file));
		dialog.exec();
	});
}

QWidget* SystemFields::audioField(ProjectData& project, lcf::rpg::Music& music, QWidget* parent) {
	return makeAudioField(project, music, FileFinder::FileType::Music, MUSIC, parent);
}

QWidget* SystemFields::audioField(ProjectData& project, lcf::rpg::Sound& sound, QWidget* parent) {
	return makeAudioField(project, sound, FileFinder::FileType::Sound, SOUND, parent);
}

QScrollArea* SystemFields::scrollable(QWidget* page) {
	auto* area = new QScrollArea();
	area->setFrameShape(QFrame::NoFrame);
	area->setWidgetResizable(true);
	area->viewport()->setAutoFillBackground(false);
	page->setAutoFillBackground(false);
	area->setWidget(page);
	return area;
}

void SystemFields::bindIdList(QWidget* context, const std::vector<QComboBox*>& combos, std::vector<int16_t>& list) {
	for (size_t i = 0; i < combos.size(); ++i) {
		const int id = i < list.size() ? list[i] : 0;
		combos[i]->setCurrentIndex(std::max(0, combos[i]->findData(id)));
	}
	for (auto* combo : combos) {
		QObject::connect(combo, &QComboBox::currentIndexChanged, context, [combos, &list] {
			list.clear();
			for (auto* entry : combos) {
				if (const int id = entry->currentData().toInt()) {
					list.push_back(static_cast<int16_t>(id));
				}
			}
		});
	}
}
