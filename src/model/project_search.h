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

#include <QHash>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <functional>
#include <memory>
#include <lcf/rpg/map.h>
#include "model/search_results_model.h"
#include "model/search_suggestions.h"

class ProjectData;

/**
 * The search of a project (Search, in the Edit menu): where its events use
 * a variable, a switch or an item, by number or name; the events and
 * common events named so; the texts of the commands. In the maps, the
 * common events and the battle events of the monster groups, as RPG Maker
 * finds them: conditions of pages, commands, routes of moves.
 *
 * The results go in results(). The texts are in the context SearchDialog,
 * as SearchWindow.qml's, so their translations stay.
 */
class ProjectSearch : public QObject
{
	Q_OBJECT
	/** A number (or "12: Name", as suggested) finds that entry; else names
	 *  and texts containing it */
	Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
	Q_PROPERTY(Type type READ type WRITE setType NOTIFY typeChanged)
	Q_PROPERTY(Scope scope READ scope WRITE setScope NOTIFY scopeChanged)
	/** The map of the CurrentMap scope, 0 for none */
	Q_PROPERTY(int currentMapId READ currentMapId WRITE setCurrentMapId NOTIFY currentMapIdChanged)
	/** The maps read stay in memory for the next searches (the Search cache
	 *  setting) */
	Q_PROPERTY(bool cacheEnabled READ cacheEnabled WRITE setCacheEnabled NOTIFY cacheEnabledChanged)
	Q_PROPERTY(SearchResultsModel* results READ results CONSTANT)
	/** The entries suggested while the text is typed, of the type chosen */
	Q_PROPERTY(SearchSuggestions* suggestions READ suggestions CONSTANT)

public:
	/** What is looked for, in the order of the Type list */
	enum Type { AllTypes, Variables, Switches, Items, EventNames, EventText };
	Q_ENUM(Type)
	/** Where, in the order of the Scope list */
	enum Scope { WholeProject, CurrentMap, CommonEvents };
	Q_ENUM(Scope)

	/** The "maps" of the results in common events and in the battle events
	 *  of monster groups (SearchResult::mapId) */
	static constexpr int CommonEventResults = 0;
	static constexpr int TroopResults = -1;

	/** A map as it is in the Editor, edits not saved included; nullptr when
	 *  it is not open (it is read from its file) */
	using MapSource = std::function<std::shared_ptr<const lcf::rpg::Map>(int map_id)>;

	explicit ProjectSearch(ProjectData& project, QObject* parent = nullptr);
	~ProjectSearch() override;

	QString text() const { return m_text; }
	void setText(const QString& text);
	Type type() const { return m_type; }
	void setType(Type type);
	Scope scope() const { return m_scope; }
	void setScope(Scope scope);
	int currentMapId() const { return m_currentMapId; }
	void setCurrentMapId(int id);
	bool cacheEnabled() const { return m_cacheEnabled; }
	/** Empties the cache too */
	void setCacheEnabled(bool enabled);

	/** The maps open in the Editor: searched as they are, never cached */
	void setMapSource(MapSource source);
	/** The map shown in the Editor: the CurrentMap scope of each search */
	void setCurrentMapSource(std::function<int()> source);

	/** Searches the project: the results replace those of the last search;
	 *  their count (none for an empty text) */
	Q_INVOKABLE int search();
	/** The maps are read again from their files on the next search */
	Q_INVOKABLE void clearCache();
	SearchResultsModel* results() const { return m_results; }
	SearchSuggestions* suggestions() const { return m_suggestions; }
	/** The database may have changed: the suggestions are made again, the
	 *  maps read again */
	Q_INVOKABLE void refresh();
	/** Back to a new search: no text, all types, the whole project, no results */
	Q_INVOKABLE void reset();
	/** Shows the place of a result in the Editor (openRequested) */
	Q_INVOKABLE void open(int row);

	/** {value, name} of the Type and the Scope lists */
	Q_INVOKABLE QVariantList typeChoices() const;
	Q_INVOKABLE QVariantList scopeChoices() const;

signals:
	void textChanged();
	void typeChanged();
	void scopeChanged();
	void currentMapIdChanged();
	void cacheEnabledChanged();
	/** A result to show: its map (or CommonEventResults, TroopResults), event, page and line */
	void openRequested(int mapId, int eventId, int pageId, int line);

private:
	std::shared_ptr<const lcf::rpg::Map> loadMap(int map_id);
	/** The maps from the top of the tree to a map: "World > Town" */
	QString mapPath(int map_id) const;

	ProjectData& m_project;
	/** The suggestions of the type chosen */
	void updateSuggestions();

	SearchResultsModel* m_results;
	SearchSuggestions* m_suggestions;
	MapSource m_mapSource;
	std::function<int()> m_currentMapSource;
	QString m_text;
	Type m_type = AllTypes;
	Scope m_scope = WholeProject;
	int m_currentMapId = 0;
	bool m_cacheEnabled = false;
	QHash<int, std::shared_ptr<const lcf::rpg::Map>> m_cache;
};
