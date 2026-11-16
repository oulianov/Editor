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
#include <QVariantList>
#include <QVariantMap>

class Project;

/**
 * The profile of a game, as the status bar and the list of Open Project
 * show it: small chips, its engine (RPG Maker 2000 or 2003) first, then
 * each add-on it turns on with the tag of its origin (Ez.Origins: M the
 * Maniac Patch, E EasyRPG Player, P a patch): the Maniac Patch, the
 * EasyRPG Extensions, RPG Maker 2003's commands in a 2000 game, the other
 * choices only EasyRPG Player runs (GameSetup::playerOnly), the patches.
 * Their tooltips say what each is and what runs the game
 * (GameSetup::runsWith).
 *
 * The one of the game open is instance(), the singleton Ez.GameProfile in
 * QML: it follows GameSetup. read() reads the setup of any game from its
 * files, cheaply, as GameSetup reads the game open.
 */
class GameProfile : public QObject
{
	Q_OBJECT
	/** The chips of the game open (chipsOf), none without a game */
	Q_PROPERTY(QVariantList chips READ chips NOTIFY chipsChanged)
	/** Their texts, joined by commas ("RPG Maker 2003, Maniac Patch") */
	Q_PROPERTY(QString text READ text NOTIFY chipsChanged)

public:
	static GameProfile& instance();

	QVariantList chips() const;
	QString text() const;

	/**
	 * The setup of a game from its files, with the keys and values of
	 * GameSetup::values(): the format of its database (its System alone is
	 * read), its patches (the [Patch] of its EasyRPG.ini, else detected
	 * from its files), [Game] Engine, the options of its database only
	 * EasyRPG Player reads. Empty when its database cannot be read. Safe in
	 * another thread (the listing of Open Project).
	 */
	static QVariantMap read(const Project& project);
	/**
	 * The chips of a setup (GameSetup::values(), read()): {key ("engine",
	 * "maniac", "easyrpg", "commands2k3", "battle2k", "alternativeExp",
	 * "playerEngine", or the key of a patch: "DynRPG"...), text, tag ("",
	 * "M", "E", "P"), tip, page (the page of Project Settings that changes
	 * it: "engine", "player")}; none for an empty setup or no game
	 */
	static QVariantList chipsOf(const QVariantMap& setup);
	/** The texts of chips, joined by commas */
	static QString textOf(const QVariantList& chips);

signals:
	void chipsChanged();

private:
	GameProfile();
};
