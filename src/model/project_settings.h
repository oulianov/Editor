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

#include <QColor>
#include <QDir>
#include <QImage>
#include <QMap>
#include <QObject>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <functional>
#include <optional>

#include "model/project_soundfont.h"

class DialogService;
class Project;

/**
 * The settings of a game, as Game Project > Project Settings edits them:
 * its folder and title (RPG Maker's Rename Project), the code page of its
 * texts, its engine and the font of its database, what EasyRPG Player
 * reads (the patches of EasyRPG.ini, the size of the screen, the fonts of
 * its Font folder, the game soundfont), and the choices of this game kept
 * on this computer only (its RTP folder, in the Editor's settings).
 *
 * The values are edited here, then write() puts them in the files of the
 * game and in the Editor's settings. Closing the project first when its
 * folder or code page changes, renaming the folder (renameFolder) and
 * reading the game again are the caller's.
 *
 * The texts are in the context ProjectSettingsDialog, as
 * ProjectSettingsWindow.qml's, so their translations stay.
 */
class ProjectSettings : public QObject
{
	Q_OBJECT
	/** The project open in the Editor: its engine is known and can be converted */
	Q_PROPERTY(bool open READ isOpen CONSTANT)
	Q_PROPERTY(QString folderName READ folderName WRITE setFolderName NOTIFY folderNameChanged)
	Q_PROPERTY(QString gameTitle READ gameTitle WRITE setGameTitle NOTIFY gameTitleChanged)
	/** false for EasyRPG projects: their texts are UTF-8 */
	Q_PROPERTY(bool encodingEditable READ encodingEditable CONSTANT)
	/** The value of encodingChoices() chosen: empty to detect it */
	Q_PROPERTY(QString encoding READ encoding WRITE setEncoding NOTIFY encodingChanged)
	/** The code page chosen is not the one written: the texts are read again */
	Q_PROPERTY(bool encodingModified READ encodingModified NOTIFY encodingChanged)
	/** RPG Maker 2003, 2000 or EasyRPG (empty when the project is not open) */
	Q_PROPERTY(QString engineName READ engineName CONSTANT)
	/** The database is RPG Maker 2003's (ldb_id 2003) */
	Q_PROPERTY(bool rpg2k3 READ rpg2k3 CONSTANT)
	/**
	 * [Game] Engine of EasyRPG.ini: the engine EasyRPG Player runs the game
	 * as ("rpg2k3e"...), empty to let it detect it; a value it does not
	 * know as written
	 */
	Q_PROPERTY(QString playerEngine READ playerEngine WRITE setPlayerEngine NOTIFY playerEngineChanged)
	/** The engine the Player runs the game as, chosen or detected, is RPG Maker 2003 */
	Q_PROPERTY(bool playerEngine2k3 READ playerEngine2k3 NOTIFY playerEngineChanged)
	/**
	 * The battles of RPG Maker 2000 in an RPG Maker 2003 game
	 * (system.easyrpg_use_rpg2k_battle_system and its fields, see
	 * PlayerOptions), when the project is open
	 */
	Q_PROPERTY(bool battle2k READ battle2k WRITE setBattle2k NOTIFY featuresChanged)
	/** system.easyrpg_alternative_exp: 0 the engine's curve, 1 RPG Maker 2000's, 2 RPG Maker 2003's */
	Q_PROPERTY(int alternativeExp READ alternativeExp WRITE setAlternativeExp NOTIFY featuresChanged)
	/**
	 * The programs the game can run with, as chosen in the window
	 * (GameSetup::runsWithOf): {key ("rpgrt", "maniac", "easyrpg"), name,
	 * available, why (the choices that keep it from running the game,
	 * one a line)}
	 */
	Q_PROPERTY(QVariantList runtimes READ runtimes NOTIFY runtimesChanged)
	Q_PROPERTY(bool canConvert READ canConvert CONSTANT)
	/** Convert to RPG Maker 2003 after the settings are written */
	Q_PROPERTY(bool convertRequested READ convertRequested WRITE setConvertRequested NOTIFY convertRequestedChanged)
	/** The players in the folder of the game, one a line */
	Q_PROPERTY(QString runtime READ runtime NOTIFY iniChanged)
	/** The program EasyRPG Player reads the engine of (RPG_RT.exe...), empty if none */
	Q_PROPERTY(QString runtimeExe READ runtimeExe NOTIFY iniChanged)
	/** The Windows fonts RPG_RT.exe draws font 1 and font 2 with */
	Q_PROPERTY(QString exeFonts READ exeFonts NOTIFY iniChanged)
	/** The font of the database (lcf::rpg::System::Font), when the project is open */
	Q_PROPERTY(int textFont READ textFont WRITE setTextFont NOTIFY textFontChanged)
	/** Where the symbols of messages ($A...) come from */
	Q_PROPERTY(QString symbolsSource READ symbolsSource CONSTANT)
	Q_PROPERTY(PatchMode patchMode READ patchMode WRITE setPatchMode NOTIFY patchModeChanged)
	/** Detect can be chosen: no other key of [Patch] stops the detection */
	Q_PROPERTY(bool detectAllowed READ detectAllowed NOTIFY iniChanged)
	/** The keys of [Patch] the Editor does not show (AntiLagSwitch...) */
	Q_PROPERTY(QStringList otherPatches READ otherPatches NOTIFY iniChanged)
	/** easyrpg.soundfont: chosen and removed by the dialog, copied on its OK (before write()) */
	Q_PROPERTY(ProjectSoundfont* soundfont READ soundfont CONSTANT)
	/**
	 * The size of the game's screen: WinW and WinH of RPG_RT.ini, read by
	 * EasyRPG Player 0.8 and later and by the Maniac Patch. Empty (0 x 0)
	 * when RPG_RT.ini has none: the player's choice, 320 x 240 or wider
	 * (the Resolution setting of the Player).
	 */
	Q_PROPERTY(QSize screenSize READ screenSize WRITE setScreenSize NOTIFY screenChanged)
	/**
	 * FakeResolution of EasyRPG.ini ([Game]), the Player's Fake Metrics: a
	 * map smaller than the screen in its middle, with black borders
	 */
	Q_PROPERTY(bool centerSmallMaps READ centerSmallMaps WRITE setCenterSmallMaps NOTIFY screenChanged)
	/**
	 * The RTP folder the game looks for the files it lacks in first, one of
	 * the RTP folders of the Editor's settings; empty: the one marked for
	 * its engine. Kept on this computer (EditorSettings::rtpActive), not in
	 * the game.
	 */
	Q_PROPERTY(QString rtpFolder READ rtpFolder WRITE setRtpFolder NOTIFY rtpFolderChanged)

public:
	/** How the patches are decided */
	enum PatchMode {
		/** From the files of the game, as the Player does without [Patch] */
		Detect,
		/** The patches checked, written in EasyRPG.ini */
		Choose,
		/** EasyRPG.ini as it is */
		Refer,
	};
	Q_ENUM(PatchMode)

	/** open: the project open in the Editor */
	ProjectSettings(Project& project, bool open, QObject* parent = nullptr);

	bool isOpen() const { return m_open; }

	QString folderName() const;
	void setFolderName(const QString& name);
	/** The folder is renamed: the project is closed first */
	Q_INVOKABLE bool renamesFolder() const;
	/** Renames the folder of the game (closed) to folderName, its RTP folder
	 *  kept for the new one: its new path, empty when it could not */
	Q_INVOKABLE QString renameFolder() const;

	QString gameTitle() const;
	void setGameTitle(const QString& title);

	bool encodingEditable() const { return m_encodingEditable; }
	QString encoding() const;
	void setEncoding(const QString& encoding);
	bool encodingModified() const;
	/** {value, name}: detect, the code pages of RPG Maker games, and the
	 *  setting written when it is none of them */
	Q_INVOKABLE QVariantList encodingChoices() const;

	QString engineName() const;
	bool rpg2k3() const { return m_rpg2k3; }
	QString playerEngine() const;
	void setPlayerEngine(const QString& engine);
	bool playerEngine2k3() const;
	/** {value, name}: detect (with the engine found), the engines of
	 *  [Game] Engine, and the value written when the Player does not know it */
	Q_INVOKABLE QVariantList playerEngineChoices() const;
	bool battle2k() const;
	void setBattle2k(bool on);
	int alternativeExp() const;
	void setAlternativeExp(int curve);
	QVariantList runtimes() const;
	bool canConvert() const { return m_canConvert; }
	bool convertRequested() const { return m_convert; }
	void setConvertRequested(bool convert);

	QString runtime() const { return m_runtime; }
	QString runtimeExe() const { return m_exe; }
	QString exeFonts() const { return m_exeFonts; }

	int textFont() const { return m_textFont; }
	void setTextFont(int font);
	/** {value, name}: the fonts of the database */
	Q_INVOKABLE QVariantList textFontChoices() const;
	/** A sentence in the text font, as EasyRPG Player draws it, its pixels
	 *  doubled for a device pixel ratio of 2 */
	Q_INVOKABLE QImage fontPreview(const QColor& color) const;

	QString symbolsSource() const;
	/** $A to $Z, then $a to $z, for a device pixel ratio of 2 */
	Q_INVOKABLE QImage symbolsImage(const QColor& color) const;

	PatchMode patchMode() const { return m_patchMode; }
	void setPatchMode(PatchMode mode);
	/** {value, name, enabled} */
	Q_INVOKABLE QVariantList patchModeChoices() const;
	bool detectAllowed() const { return m_others.isEmpty(); }
	QStringList otherPatches() const { return m_others; }
	/** {key, name, explanation}: the patches of the grid, without those
	 *  shown on their own (Maniac, EasyRPG, RPG2k3Commands) */
	Q_INVOKABLE QVariantList patches() const;
	/** On as the patch mode tells (detected, chosen, written) */
	Q_INVOKABLE bool patchOn(const QString& key) const;
	/** Checks a patch when they are chosen; detected, they are chosen from
	 *  now on, from what is detected; as written, nothing changes */
	Q_INVOKABLE void setPatchOn(const QString& key, bool on);
	/** What a patch changes, as rich text; empty for none */
	Q_INVOKABLE QString patchExplanation(const QString& key) const;
	/** What a patch changes, as plain text (a tooltip) */
	Q_INVOKABLE QString patchText(const QString& key) const;

	/** The font file EasyRPG Player uses instead of font 1 (0) or 2 (1), as
	 *  it will be: empty for its own */
	Q_INVOKABLE QString playerFont(int font) const;
	/** As the dialog shows it: the file, or which built-in font */
	Q_INVOKABLE QString playerFontText(int font) const;
	/** Copied in the Font folder of the game by write() */
	Q_INVOKABLE void choosePlayerFont(int font, const QString& file);
	/** The Player's own font: the game's font file goes to the trash */
	Q_INVOKABLE void useBuiltinPlayerFont(int font);
	/** The patterns of the font files the Player reads (*.ttf...) */
	Q_INVOKABLE static QStringList fontPatterns();

	/** The game is in an Asian code page: the Player draws it with other fonts */
	bool cjkGame() const;

	ProjectSoundfont* soundfont() const { return m_soundfont; }

	/** EasyRPG.ini was edited (in the script editor): what it says now */
	Q_INVOKABLE void readIni();
	/** Opens EasyRPG.ini in the script editor (created if missing), for
	 *  what is not shown; failures are told through dialogs */
	bool openIni(DialogService& dialogs);
	Q_INVOKABLE bool openIni();

	QSize screenSize() const;
	void setScreenSize(QSize size);
	bool centerSmallMaps() const;
	void setCenterSmallMaps(bool on);

	QString rtpFolder() const { return m_rtpFolder; }
	void setRtpFolder(const QString& folder);
	/** {value, name}: the engine's default (empty), then the RTP folders of
	 *  the Editor's settings in their order, each with the RTP it holds */
	Q_INVOKABLE QVariantList rtpChoices() const;

	/**
	 * The add-ons the game open runs now (GameSetup) that the choices of
	 * the window turn off, as origins of Ez.Origins: "maniac", "easyrpg",
	 * "rpg2003" (RPG Maker 2003's commands in a 2000 game), "dynrpg",
	 * "keypatch", "powermode"; none for a game not open
	 */
	Q_INVOKABLE QStringList turnedOff() const;
	/** Lists what the game uses from the add-ons turned off (origins);
	 *  dynRpg: DynRPG stays on. true keeps them off, false goes back */
	using TurnOffCheck = std::function<bool(const QStringList& origins, bool dynRpg)>;
	/** What OK asks about the add-ons turned off (confirmTurnOff), by the window that opened this one */
	void setTurnOffCheck(TurnOffCheck check);
	/** On OK, before anything is written: the add-ons turned off checked
	 *  (setTurnOffCheck), once for the same ones; false: the window stays
	 *  open, to change them back */
	Q_INVOKABLE bool confirmTurnOff();

	/** Why the settings cannot be written (the folder name), else empty */
	Q_INVOKABLE QString validate() const;
	/** Writes the title, the code page, the font and the options of the
	 *  database (with their copy in EasyRPG.ini, PlayerOptions), the engine
	 *  and the patches and the fonts of the Player in the files of the
	 *  game, and its RTP folder in the Editor's settings; GameSetup reads
	 *  the game again: the error, else empty */
	QString write(Project& project) const;
	/** write() in the project the settings are of */
	Q_INVOKABLE QString apply() const;

	/** The number of a code page ("1252", "windows-1252", "ibm-943_P15A-2003"), else 0 */
	static int codePageOf(const QString& encoding);
	/** "1252 - Western European...", else the encoding as written */
	static QString describeEncoding(const QString& encoding);

signals:
	void folderNameChanged();
	void gameTitleChanged();
	void encodingChanged();
	void convertRequestedChanged();
	void playerEngineChanged();
	/** The battle system or the experience curve */
	void featuresChanged();
	/** What the game can run with: after any choice of the patches, the
	 *  engine, the battle system or the experience curve */
	void runtimesChanged();
	void textFontChanged();
	void patchModeChanged();
	/** The patches shown changed: their values or the mode */
	void patchesChanged();
	void playerFontsChanged();
	void screenChanged();
	void rtpFolderChanged();
	/** EasyRPG.ini and the players were read again */
	void iniChanged();

private:
	/** The values the patch mode shows */
	const QMap<QString, int>& patchValues() const;
	/** The patches written and detected, the engine as chosen */
	void readPatches();

	Project& m_project;
	bool m_open;
	QDir m_folder;
	QString m_folderName;
	QString m_initialTitle;
	QString m_title;

	bool m_encodingEditable;
	/** As RPG_RT.ini says, empty when detected */
	QString m_encodingSetting;
	QString m_encoding;

	bool m_canConvert = false;
	bool m_convert = false;

	/** The database: RPG Maker 2003's, its version (1: RPG Maker 2000 1.6x's) */
	bool m_rpg2k3 = false;
	int m_databaseVersion = 0;
	/** [Game] Engine as written, as chosen (unset: unchanged), and the
	 *  engine found without it (RpgRtExe::Engine flags) */
	QString m_engineWritten;
	std::optional<QString> m_engineChoice;
	int m_engineDetected = 0;
	/** The options of the database as it says, and as chosen (unset: unchanged) */
	bool m_battleWritten = false;
	std::optional<bool> m_battleChoice;
	int m_expWritten = 0;
	std::optional<int> m_expChoice;

	QString m_runtime;
	QString m_exe;
	QString m_exeFonts;

	int m_textFont = 0;
	QImage m_symbols;
	QString m_symbolsSource;

	PatchMode m_patchMode = Detect;
	QMap<QString, int> m_detected;
	QMap<QString, int> m_chosen;
	/** What EasyRPG.ini says */
	QMap<QString, int> m_written;
	QStringList m_others;

	/** The size of the screen as RPG_RT.ini says, and as chosen (unset: unchanged) */
	QSize m_screenWritten;
	std::optional<QSize> m_screenChoice;
	/** FakeResolution as EasyRPG.ini says, and as chosen (unset: unchanged) */
	bool m_centerWritten = false;
	std::optional<bool> m_centerChoice;

	/** Font files chosen for EasyRPG Player: empty for its own font; unset when unchanged */
	std::optional<QString> m_fontChoice[2];

	/** The RTP folder chosen (empty: the engine's default), and the engine
	 *  of the game (2000, 2003) whose default it is */
	QString m_rtpFolder;
	int m_rtpEngine = 2000;

	ProjectSoundfont* m_soundfont;

	TurnOffCheck m_turnOffCheck;
	/** The add-ons turned off whose check said to keep them off */
	QStringList m_offConfirmed;
};
