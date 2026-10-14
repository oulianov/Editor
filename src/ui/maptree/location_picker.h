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

#include <QAbstractListModel>
#include <QList>
#include <QPointer>
#include <QString>
#include <QtQmlIntegration>
#include <lcf/rpg/treemap.h>
#include "model/map_document.h"

class ProjectData;

/**
 * The maps of a project in the order of the map tree, as the lines of a
 * list indented by their depth: the project first, then the maps with
 * their areas (shown, but no location). Searched by number or name as the
 * lists of maps are; a node stays while one under it matches.
 */
class LocationMapList : public QAbstractListModel
{
	Q_OBJECT
	QML_ELEMENT
	QML_UNCREATABLE("Made by the location picker")
	Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)

public:
	enum Role {
		IdRole = Qt::UserRole + 1,
		DepthRole,
		/** lcf::rpg::TreeMap::MapType: the project, a map or an area */
		TypeRole,
		/** A map: a location can be on it */
		SelectableRole,
	};

	LocationMapList(const lcf::rpg::TreeMap& tree, const QString& root_name, QObject* parent = nullptr);

	QString filter() const;
	void setFilter(const QString& filter);

	/** The line of a map shown, -1 when it is not */
	Q_INVOKABLE int rowOf(int map_id) const;
	Q_INVOKABLE int mapIdAt(int row) const;
	Q_INVOKABLE bool selectableAt(int row) const;
	/** The first map shown, -1 for none */
	Q_INVOKABLE int firstSelectable() const;
	/** A map of the tree a location can be on */
	bool isMap(int map_id) const;

	// QAbstractListModel
	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	QHash<int, QByteArray> roleNames() const override;

signals:
	void filterChanged();

private:
	struct Node {
		int id = 0;
		QString name;
		int depth = 0;
		int type = 0;
		QList<int> children;
	};
	void update();
	bool collect(int index, QList<int>& shown) const;

	// the project at 0
	QList<Node> m_nodes;
	QList<int> m_shown;
	QString m_filter;
};

/**
 * What the map location windows show (MapLocationPicker.qml, Define Area
 * Boundaries): the list of the maps and the map chosen, read from its file,
 * or the map of the open tab as it is edited.
 */
class LocationPicker : public QObject
{
	Q_OBJECT
	QML_ELEMENT
	QML_UNCREATABLE("Opened by Pickers::pickLocation")
	Q_PROPERTY(LocationMapList* maps READ maps CONSTANT)
	Q_PROPERTY(MapDocument* document READ document NOTIFY mapChanged)
	Q_PROPERTY(int mapId READ mapId NOTIFY mapChanged)
	Q_PROPERTY(QString mapName READ mapName NOTIFY mapChanged)
	/** The size of the map shown, in cells (0 without one) */
	Q_PROPERTY(int columns READ columns NOTIFY mapChanged)
	Q_PROPERTY(int rows READ rows NOTIFY mapChanged)

public:
	explicit LocationPicker(ProjectData& project, QObject* parent = nullptr);

	LocationMapList* maps() const;
	MapDocument* document() const;
	int mapId() const;
	QString mapName() const;
	int columns() const;
	int rows() const;

	/** Shows a map: false (none shown) when it cannot be read */
	Q_INVOKABLE bool showMap(int map_id);

	/** The map of the tab shown in the main window, 0 for none */
	static int openMapId();
	/** Its document, as edited (nullptr for none) */
	static MapDocument* openDocument();

signals:
	void mapChanged();

private:
	ProjectData& m_project;
	LocationMapList* m_maps;
	QPointer<MapDocument> m_document;
	// read from its file: ours
	bool m_owned = false;
};
