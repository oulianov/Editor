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
#include <QStringList>

/**
 * The options of the Editor (the Settings window), saved with QSettings.
 * Windows that depend on an option connect to changed() and read it again.
 */
class EditorSettings : public QObject
{
	Q_OBJECT

public:
	static EditorSettings& instance();

	/** Language code ("en", "fr"...), empty: the system's; used at the next start */
	QString language() const;
	void setLanguage(const QString& code);

	/** Light or dark windows: "system" (the default, followed when it
	 *  changes), "light" or "dark"; shown at once (applyColorScheme) */
	QString colorScheme() const;
	void setColorScheme(const QString& scheme);
	/**
	 * Shows every window in the color scheme chosen, with the icons drawn
	 * for it (ezbright, ezdark): at start-up, then on its own again when the
	 * option or the system's scheme changes
	 */
	void applyColorScheme();
	/** The windows show dark colors (the scheme chosen, else the system's) */
	bool isDark() const;

	/**
	 * The RTP folders of the Settings, in the order their files are looked
	 * for. The single folder of the Editors before them becomes their first,
	 * the default of the engine it looks like (of both when it looks like neither).
	 */
	QStringList rtpFolders() const;
	/** Also forgets the games' choices of folders no longer in the list */
	void setRtpFolders(const QStringList& folders);
	/** The folder of the games of an engine (2000, 2003) that chose none; empty: none */
	QString rtpDefault(int engine) const;
	void setRtpDefault(int engine, const QString& folder);
	/** The folder a game chose instead of its engine's default (projectKey: its
	 *  folder); empty: the default */
	QString rtpActive(const QString& project) const;
	void setRtpActive(const QString& project, const QString& folder);
	/**
	 * The RTP folders where a game's files are looked for, in order: the
	 * one it uses (its own, else its engine's default, the first of them
	 * there), then the other ones in the order of the list; the missing ones
	 * left out. No project: the list.
	 */
	QStringList rtpSearchPath(const QString& project, int engine) const;
	/** A game's folder as the settings know it (its canonical path) */
	static QString projectKey(const QString& dir);
	/** The keys of the RTP settings, which the scripts take from the user's */
	static QStringList rtpKeys();
	/** The RTP a folder looks like: 2000, 2003, else 0 (another folder, missing) */
	static int rtpEngine(const QString& folder);
	/** The folder holds the EasyRPG RTP (its README says so), made for the
	 *  games of both engines */
	static bool isEasyRpgRtp(const QString& folder);
	/**
	 * Which translation of the RTP a folder holds, by the name of its first
	 * hero (EasyRPG Player's RTP tables, simplified): "ja", "en", "dm" (Don
	 * Miguel's), "rpga" (RPG Advocate's), "ru", "rpgu" (RPG Universe's),
	 * "ko", "zh"; empty: unknown
	 */
	static QString rtpTranslation(const QString& folder, int engine);

	/** The EasyRPG Player for test play; empty: searched next to the Editor and in the PATH */
	QString playerPath() const;
	void setPlayerPath(const QString& path);

	/** How a script opens at a line (rpgsync scripts): a URL or a command with
	 *  {file} and {line}; empty: the program of the system, at its start */
	QString scriptEditor() const;
	void setScriptEditor(const QString& editor);
	static QString defaultScriptEditor();

	/** Read again the files that other programs change in the project (off by default) */
	bool reloadExternalChanges() const;
	void setReloadExternalChanges(bool on);

	/** Maniac mode (off by default): every game is edited as a game of the
	 *  Maniac Patch (GamePatches::usesManiac), its commands and options offered */
	bool maniacMode() const;
	void setManiacMode(bool on);

	/** Water and animated tiles move in the map view */
	bool animateTiles() const;
	void setAnimateTiles(bool on);

	/** Event commands colored by kind, as in RPG Maker 2003 */
	bool commandColors() const;
	void setCommandColors(bool on);

	/** Every other line of the event commands on a tinted background */
	bool commandStripes() const;
	void setCommandStripes(bool on);

	/** Search results kept between searches */
	bool searchCache() const;
	void setSearchCache(bool on);

	/** A newer Editor is looked for at start-up, then every hour (on by default) */
	bool checkForUpdates() const;
	void setCheckForUpdates(bool on);

	/** Reset saved UI state and all open views, keeping preferences and game data. */
	void resetLocalState();

signals:
	void changed();
	void localStateReset();
	/** The windows went from light colors to dark ones or back (isDark) */
	void darkChanged();

private:
	EditorSettings() = default;
	QVariant value(const char* key, const QVariant& fallback) const;
	void setValue(const char* key, const QVariant& value);
	/** The palette and the icons of the scheme chosen, as the platform shows it now */
	void updateColors();

	bool m_watching = false;
	bool m_dark = false;
};
