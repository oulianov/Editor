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

#include <QObject>
#include <QString>
#include <QVariant>
#include <QtQmlIntegration>
#include <functional>
#include <optional>
#include "model/file_picker_model.h"

class ProjectData;
class QQuickItem;
class QWindow;

namespace lcf::rpg {
	class Music;
	class Sound;
}

/**
 * The pickers of the Editor (FilePicker.qml, EntryPicker.qml), modal, over
 * the window of the control that opens them (a QML item or a
 * window): they return the choice, nothing when cancelled.
 *
 *   Pickers::FileRequest request{FACESET, Pickers::Preview::FaceSet, {name, index}};
 *   Pickers::pickFile(this, request, [&](const Pickers::FileChoice& choice) { ... });
 */
namespace Pickers {
	using Preview = FilePickerModel::Preview;

	/** A file (its name, without its extension) and what is chosen with it */
	struct FileChoice {
		QString name;
		/** The character of a CharSet, the face of a FaceSet */
		int index = 0;
		/** Of a pose: 0 up, 1 right, 2 down, 3 left */
		int direction = 2;
		/** Of a pose: 0 left, 1 middle, 2 right */
		int pattern = 1;
		/** Of a monster, in degrees */
		int hue = 0;
		/** Of a music, in milliseconds */
		int fadeIn = 0;
		int volume = 100;
		int tempo = 100;
		int balance = 50;

		static FileChoice of(const lcf::rpg::Music& music);
		static FileChoice of(const lcf::rpg::Sound& sound);
		/** Its name and settings into a music or a sound */
		void to(lcf::rpg::Music& music) const;
		void to(lcf::rpg::Sound& sound) const;
	};

	struct FileRequest {
		/** The folder of the game listed (CHARSET...) */
		QString folder;
		Preview preview = Preview::Image;
		/** The file of the data, the picker starts on it (else on the first file) */
		FileChoice current;
		/** The title of the window; empty: "Select from <folder>" */
		QString title;
	};

	/**
	 * Picks a file of a folder of the game or of the RTP
	 *
	 * @param from Over the window of this item or window
	 * @param applied Given the choice on Apply (the picker stays) and on OK
	 * @return The choice on OK, nothing when cancelled
	 */
	std::optional<FileChoice> pickFile(QObject* from, const FileRequest& request,
		const std::function<void(const FileChoice&)>& applied = {});

	struct EntryRequest {
		/** The list of the database, as JSON names it: "switches", "items"... */
		QString list;
		/** The entry shown first (none or unknown: "(None)" when offered, else the first) */
		int id = 0;
		/** "(None)" is offered first (ID 0) */
		bool none = false;
		/** The title of the window; empty: the name of the list */
		QString title;
	};

	/**
	 * Picks an entry of a list of the database. The picker edits a copy of
	 * the project (names, Array Size): it is written into project on OK and
	 * on Apply.
	 *
	 * @param project The project shown (the live one, or the copy of a database window)
	 * @param applied Given the ID chosen on Apply (the picker stays) and on OK
	 * @return The ID chosen on OK (0: "(None)"), nothing when cancelled
	 */
	std::optional<int> pickEntry(QObject* from, ProjectData& project, const EntryRequest& request,
		const std::function<void(int)>& applied = {});

	/** The window a QML item or a window is in (the focused one when none) */
	QWindow* windowOf(QObject* from);

	/** A cell of a map; direction: RPG Maker 2003's Character Facing (0
	 *  retain, 1 up, 2 right, 3 down, 4 left) */
	struct Location {
		int map = 0;
		int x = 0;
		int y = 0;
		int direction = 0;
	};

	struct LocationRequest {
		/** The location shown first (map 0 or not a map: the map open) */
		Location current;
		/** The Character Facing is asked too */
		bool facing = false;
		/** A cell of current.map only, without the list of the maps (the
		 *  location of an event on its map) */
		bool fixedMap = false;
		/** The title of the window; empty: "Select Location" */
		QString title;
	};

	/**
	 * Picks a map and a cell (MapLocationPicker.qml, RPG Maker's Select
	 * Location): the maps of the project, the map chosen to click on, the
	 * current location marked; a double-click on a cell is OK
	 *
	 * @return The location on OK (direction: current.direction unless the
	 *  facing was asked), nothing when cancelled
	 */
	std::optional<Location> pickLocation(QObject* from, const LocationRequest& request);

	/** The map of the tab shown in the main window, 0 for none */
	int openMapId();
}

/**
 * The pickers for QML (the singleton Ez.Pickers):
 *
 *   const choice = Ez.Pickers.pickFile({ folder: "FaceSet", preview: "FaceSet", name: face, index: 2 }, field)
 *   if (choice) { ... choice.name, choice.index ... }
 *   const id = Ez.Pickers.pickEntry({ list: "switches", id: 3, none: true, session: page.session }, field)
 *
 * pickFile: the request has the fields of Pickers::FileRequest and
 * FileChoice (preview: the name of a FilePickerModel::Preview); it returns
 * the choice (the fields of FileChoice), null when cancelled.
 * pickEntry: the request has the fields of Pickers::EntryRequest, and the
 * DatabaseSession of the window (session, or the database it shows:
 * database) whose copy it edits (none: the project); it returns the ID
 * chosen, null when cancelled.
 *
 *   const place = Ez.Pickers.pickLocation({ map: 2, x: 4, y: 7, direction: 0 }, button)
 *   if (place) { ... place.map, place.x, place.y, place.direction ... }
 *
 * pickLocation: the request has the fields of Pickers::Location and
 * LocationRequest (facing: asked when the request has a direction, unless
 * told); it returns the location, null when cancelled. openMapId(): the
 * map of the tab shown (an event's own map), 0 for none.
 */
class PickersBridge : public QObject {
	Q_OBJECT
	QML_NAMED_ELEMENT(Pickers)
	QML_SINGLETON

public:
	explicit PickersBridge(QObject* parent = nullptr);

	Q_INVOKABLE QVariant pickFile(const QVariantMap& request, QQuickItem* from = nullptr);
	Q_INVOKABLE QVariant pickEntry(const QVariantMap& request, QQuickItem* from = nullptr);
	// in ui/maptree/location_picker.cpp
	Q_INVOKABLE QVariant pickLocation(const QVariantMap& request, QQuickItem* from = nullptr);
	Q_INVOKABLE int openMapId() const;
};

/** Apply of a picker window: the C++ that opened it hears it (applied) */
class PickerRelay : public QObject {
	Q_OBJECT
public:
	using QObject::QObject;
	Q_INVOKABLE void apply(const QVariant& value = QVariant()) {
		emit applied(value);
	}
signals:
	void applied(const QVariant& value);
};

/** The model of FilePicker.qml (made by Pickers::pickFile): its enums, Ez.FilePickerModel.CharSet... */
struct FilePickerModelForeign
{
	Q_GADGET
	QML_FOREIGN(FilePickerModel)
	QML_NAMED_ELEMENT(FilePickerModel)
	QML_UNCREATABLE("A picker is opened by Ez.Pickers")
};
