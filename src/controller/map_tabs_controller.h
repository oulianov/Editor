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

class MapDocument;
class ProjectController;

/**
 * The maps open in the tabs of the main window, in their order: their
 * documents (the ProjectController's), the tab shown, and the temporary tab
 * (italic) of a map only looked at, which the next map opened replaces,
 * like a code editor's, until it is kept (a double-click, an edit). It asks
 * before a tab with unsaved changes closes.
 *
 * A list model, one row per tab (roles named as in QML: mapId, title...).
 * The view of a tab is made when its row is inserted (the document is read
 * then), and lets the document go on mapClosing, before it is deleted.
 * The tab shown changes by setCurrentMap, which the view follows, and the
 * view's own choices come back by setCurrentIndex and moveTab.
 *
 * Its owner (ShellController) calls followTreeMap() when the map tree was
 * read again, once the view of the tree shows the new one.
 */
class MapTabsController : public QAbstractListModel
{
	Q_OBJECT
	Q_PROPERTY(int currentMap READ currentMap NOTIFY currentMapChanged)
	Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentMapChanged)
	Q_PROPERTY(int temporaryMap READ temporaryMap NOTIFY temporaryMapChanged)
	Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
	enum Roles {
		MapIdRole = Qt::UserRole + 1,
		/** The name of the map */
		NameRole,
		/** The text of its tab: the name, "*" when modified, "(deleted)" */
		TitleRole,
		ToolTipRole,
		ModifiedRole,
		/** The temporary tab, in italics */
		TemporaryRole,
		/** Another program deleted the map while it was open */
		DeletedRole,
	};

	explicit MapTabsController(ProjectController* project, QObject* parent = nullptr);

	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	QHash<int, QByteArray> roleNames() const override;

	/** The maps of the tabs, in their order */
	QList<int> mapIds() const;
	int count() const;
	Q_INVOKABLE int indexOf(int id) const;
	/** The map of a tab, 0 for none */
	Q_INVOKABLE int mapAt(int index) const;
	Q_INVOKABLE bool isOpen(int id) const;
	/** The map of the tab shown, 0 for none */
	int currentMap() const;
	int currentIndex() const;
	MapDocument* currentDocument() const;
	/** The map of the temporary tab, 0 for none */
	int temporaryMap() const;
	QString title(int id) const;
	QString toolTip(int id) const;

	/**
	 * Shows a map in its tab, made unless it is open
	 *
	 * @param keep A kept tab, else the temporary tab (which takes the place
	 *        of the one before when it has no changes); an open temporary
	 *        tab is kept
	 * @return false when its file cannot be read (said)
	 */
	Q_INVOKABLE bool openMap(int id, bool keep);
	/** The maps of a workspace in kept tabs after those open: not the maps
	 *  gone, the areas or the maps whose file cannot be read; those opened */
	QList<int> restoreMaps(const QList<int>& ids);
	/** The temporary tab of id becomes a kept one */
	Q_INVOKABLE void keepMap(int id);
	Q_INVOKABLE void setCurrentMap(int id);
	/** The view shows another tab */
	Q_INVOKABLE void setCurrentIndex(int index);
	/** The view moved a tab (dragged) */
	Q_INVOKABLE void moveTab(int from, int to);
	/** Closes a tab, asking first to save its changes; false when it stays
	 *  (Cancel, or not saved) */
	Q_INVOKABLE bool requestClose(int id);
	/** Closes a tab and its document, without asking */
	void closeMap(int id);
	/** Closes every tab, without asking (its changes were asked about) */
	void closeAll();
	/** The map tree was read again: the tabs of the maps removed close (one
	 *  with changes says it was deleted), the others take their new info */
	void followTreeMap();

signals:
	void currentMapChanged(int id);
	void temporaryMapChanged(int id);
	void countChanged();
	/** The tab of a map is gone: its view lets its document go now */
	void mapClosing(int id);

private:
	void connectDocument(int id);
	/** The title and the state of a tab changed */
	void updateTab(int id);
	/** The project closed a document (a map deleted...): its tab goes */
	void onDocumentClosing(int id);
	void removeTab(int id);
	void setTemporaryMap(int id);
	/** After an edit or a save */
	void onEdited(int id);
	void onSavedOrReverted(int id);

	ProjectController* m_project;
	QList<int> m_ids;
	int m_current = 0;
	int m_temporary = 0;
};
