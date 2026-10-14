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

#include "pickers.h"
#include "core.h"
#include "model/database_list_data.h"
#include "model/project.h"
#include "qmlbinding/database_session.h"
#include "qmlbinding/json_view.h"
#include "qmlbinding/project_data_gadget.h"
#include "qmlbinding/qml_host.h"
#include "ui/shell/qml_engine.h"

#include <QGuiApplication>
#include <QMetaEnum>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScopeGuard>
#include <lcf/rpg/music.h>
#include <lcf/rpg/sound.h>

namespace {

void applyChoice(FilePickerModel& model, const Pickers::FileChoice& choice) {
	model.setSpriteIndex(choice.index);
	model.setDirection(choice.direction);
	model.setPattern(choice.pattern);
	model.setHue(choice.hue);
	model.setFadeIn(choice.fadeIn);
	model.setVolume(choice.volume);
	model.setTempo(choice.tempo);
	model.setBalance(choice.balance);
}

Pickers::FileChoice choiceOf(const FilePickerModel& model) {
	Pickers::FileChoice choice;
	choice.name = model.currentName();
	choice.index = model.spriteIndex();
	choice.direction = model.direction();
	choice.pattern = model.pattern();
	choice.hue = model.hue();
	choice.fadeIn = model.fadeIn();
	choice.volume = model.volume();
	choice.tempo = model.tempo();
	choice.balance = model.balance();
	return choice;
}

/** The picker of a kind of file (PickerBase.qml and the pickers made on it) */
QString pickerType(Pickers::Preview preview) {
	using Preview = Pickers::Preview;
	switch (preview) {
	case Preview::CharSet:
	case Preview::CharSetPose:
		return QStringLiteral("CharSetPicker");
	case Preview::FaceSet:
		return QStringLiteral("FaceSetPicker");
	case Preview::Monster:
		return QStringLiteral("MonsterPicker");
	case Preview::Music:
		return QStringLiteral("MusicPicker");
	case Preview::Sound:
		return QStringLiteral("SoundPicker");
	case Preview::None:
		return QStringLiteral("PickerBase");
	case Preview::Image:
		break;
	}
	return QStringLiteral("ImagePicker");
}

/** The entry picker on project; written: once the copy is in it (OK, Apply) */
std::optional<int> pickEntryIn(QObject* from, ProjectData& project, const Pickers::EntryRequest& request,
		const std::function<void()>& written, const std::function<void(int)>& applied) {
	// a copy: Cancel leaves the project as it was. Gone after the window
	// (deleted later), which shows it until then
	auto* session = new DatabaseSession();
	auto* relay = new PickerRelay(session);
	const auto cleanup = qScopeGuard([session] { session->deleteLater(); });
	session->open(&project);
	auto write = [&] {
		if (session->modified()) {
			// the other windows showing these lists follow
			DatabaseListNotifier::assign(project, session->copy(), from);
			if (written) {
				written();
			}
		}
	};
	const QMetaObject::Connection applying = QObject::connect(relay, &PickerRelay::applied, relay, [&](const QVariant& id) {
		write();
		if (applied) {
			applied(id.toInt());
		}
	});
	const auto disconnect = qScopeGuard([applying] { QObject::disconnect(applying); });
	int chosen = 0;
	const int result = QmlHost::exec(QStringLiteral("EntryPicker"), Pickers::windowOf(from), {
		{QStringLiteral("session"), QVariant::fromValue(session)},
		{QStringLiteral("relay"), QVariant::fromValue(relay)},
		{QStringLiteral("listName"), request.list},
		{QStringLiteral("currentId"), request.id},
		{QStringLiteral("none"), request.none},
		{QStringLiteral("customTitle"), request.title},
	}, [&chosen](QQuickWindow* window) {
		chosen = window->property("chosenId").toInt();
	});
	if (result != 1) {
		return std::nullopt;
	}
	write();
	if (applied) {
		applied(chosen);
	}
	return chosen;
}

} // namespace

Pickers::FileChoice Pickers::FileChoice::of(const lcf::rpg::Music& music) {
	FileChoice choice;
	choice.name = QString::fromStdString(music.name);
	choice.fadeIn = music.fadein;
	choice.volume = music.volume;
	choice.tempo = music.tempo;
	choice.balance = music.balance;
	return choice;
}

Pickers::FileChoice Pickers::FileChoice::of(const lcf::rpg::Sound& sound) {
	FileChoice choice;
	choice.name = QString::fromStdString(sound.name);
	choice.volume = sound.volume;
	choice.tempo = sound.tempo;
	choice.balance = sound.balance;
	return choice;
}

void Pickers::FileChoice::to(lcf::rpg::Music& music) const {
	music.name = name.toStdString();
	music.fadein = fadeIn;
	music.volume = volume;
	music.tempo = tempo;
	music.balance = balance;
}

void Pickers::FileChoice::to(lcf::rpg::Sound& sound) const {
	sound.name = name.toStdString();
	sound.volume = volume;
	sound.tempo = tempo;
	sound.balance = balance;
}

QWindow* Pickers::windowOf(QObject* from) {
	for (QObject* object = from; object; object = object->parent()) {
		if (auto* window = qobject_cast<QWindow*>(object)) {
			return window;
		}
		if (auto* item = qobject_cast<QQuickItem*>(object)) {
			if (item->window()) {
				return item->window();
			}
		}
	}
	return QGuiApplication::modalWindow() ? QGuiApplication::modalWindow() : QGuiApplication::focusWindow();
}

std::optional<Pickers::FileChoice> Pickers::pickFile(QObject* from, const FileRequest& request,
		const std::function<void(const FileChoice&)>& applied) {
	const auto& project = core().project();
	// gone after the window (deleted later), which shows it until then
	auto* model = new FilePickerModel(request.preview);
	const auto cleanup = qScopeGuard([model] {
		// the preview stops with the picker
		model->stop();
		model->deleteLater();
	});
	applyChoice(*model, request.current);
	model->open(request.folder, project ? project->projectDir().absolutePath() : QString(),
		core().rtpPath(QString()), request.current.name);
	const QMetaObject::Connection applying = QObject::connect(model, &FilePickerModel::applied, model, [&] {
		if (applied) {
			applied(choiceOf(*model));
		}
	});
	const auto disconnect = qScopeGuard([applying] { QObject::disconnect(applying); });
	const int result = QmlHost::exec(pickerType(request.preview), windowOf(from), {
		{QStringLiteral("picker"), QVariant::fromValue(model)},
		{QStringLiteral("customTitle"), request.title},
	});
	if (result != 1) {
		return std::nullopt;
	}
	const FileChoice choice = choiceOf(*model);
	if (applied) {
		applied(choice);
	}
	return choice;
}

std::optional<int> Pickers::pickEntry(QObject* from, ProjectData& project, const EntryRequest& request,
		const std::function<void(int)>& applied) {
	return pickEntryIn(from, project, request, {}, applied);
}

PickersBridge::PickersBridge(QObject* parent) : QObject(parent) {
}

QVariant PickersBridge::pickFile(const QVariantMap& request, QQuickItem* from) {
	Pickers::FileRequest file;
	file.folder = request.value(QStringLiteral("folder")).toString();
	file.title = request.value(QStringLiteral("title")).toString();
	const QVariant preview = request.value(QStringLiteral("preview"));
	if (preview.typeId() == QMetaType::QString) {
		bool ok = false;
		const int value = QMetaEnum::fromType<Pickers::Preview>().keyToValue(preview.toString().toLatin1(), &ok);
		file.preview = ok ? static_cast<Pickers::Preview>(value) : Pickers::Preview::Image;
	} else if (preview.isValid()) {
		file.preview = static_cast<Pickers::Preview>(preview.toInt());
	}
	Pickers::FileChoice& current = file.current;
	auto number = [&](const char* key, int fallback) {
		const QVariant value = request.value(QLatin1String(key));
		return value.isValid() ? value.toInt() : fallback;
	};
	current.name = request.value(QStringLiteral("name")).toString();
	current.index = number("index", current.index);
	current.direction = number("direction", current.direction);
	current.pattern = number("pattern", current.pattern);
	current.hue = number("hue", current.hue);
	current.fadeIn = number("fadeIn", current.fadeIn);
	current.volume = number("volume", current.volume);
	current.tempo = number("tempo", current.tempo);
	current.balance = number("balance", current.balance);
	const auto choice = Pickers::pickFile(from, file);
	if (!choice) {
		return QVariant();
	}
	return QVariantMap{
		{QStringLiteral("name"), choice->name},
		{QStringLiteral("index"), choice->index},
		{QStringLiteral("direction"), choice->direction},
		{QStringLiteral("pattern"), choice->pattern},
		{QStringLiteral("hue"), choice->hue},
		{QStringLiteral("fadeIn"), choice->fadeIn},
		{QStringLiteral("volume"), choice->volume},
		{QStringLiteral("tempo"), choice->tempo},
		{QStringLiteral("balance"), choice->balance},
	};
}

QVariant PickersBridge::pickEntry(const QVariantMap& request, QQuickItem* from) {
	Pickers::EntryRequest entry;
	entry.list = request.value(QStringLiteral("list")).toString();
	entry.id = request.value(QStringLiteral("id")).toInt();
	entry.none = request.value(QStringLiteral("none")).toBool();
	entry.title = request.value(QStringLiteral("title")).toString();
	std::optional<int> chosen;
	auto* session = qobject_cast<DatabaseSession*>(request.value(QStringLiteral("session")).value<QObject*>());
	// the database of a window (a page's): its session
	if (auto* view = qobject_cast<JsonView*>(request.value(QStringLiteral("database")).value<QObject*>()); !session && view) {
		session = DatabaseSession::owning(view->json());
	}
	if (session) {
		// the copy of the database window: its views read it again
		chosen = pickEntryIn(from, session->copy(), entry, [session] { session->json().notifyChanged(QString()); }, {});
	} else {
		auto* gadget = QmlEngine::instance()->singletonInstance<ProjectDataGadget*>(
			QStringLiteral("org.easyrpg.editor"), QStringLiteral("ProjectData"));
		ProjectData* project = gadget ? gadget->projectData() : nullptr;
		if (!project) {
			return QVariant();
		}
		chosen = pickEntryIn(from, *project, entry, [gadget] { gadget->databaseJson().notifyChanged(QString()); }, {});
	}
	return chosen ? QVariant(*chosen) : QVariant();
}
