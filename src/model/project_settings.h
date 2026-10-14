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
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <optional>

class DialogService;
class Project;

/**
 * The settings of a game, as Game Project > Project Settings edits them:
 * its folder and title (RPG Maker's Rename Project), the code page of its
 * texts, its engine and the font of its database, and what only EasyRPG
 * Player reads (the patches of EasyRPG.ini, the fonts of its Font folder).
 *
 * The values are edited here, then write() puts them in the files of the
 * game. Closing the project first when its folder or code page changes,
 * renaming the folder and reading the game again are the caller's.
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
	/** Renames the folder of the game (closed) to folderName: its new path,
	 *  empty when it could not */
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
	/** {key, name, explanation}: the patches the Editor shows */
	Q_INVOKABLE QVariantList patches() const;
	/** On as the patch mode tells (detected, chosen, written) */
	Q_INVOKABLE bool patchOn(const QString& key) const;
	/** Checks a patch: only when they are chosen */
	Q_INVOKABLE void setPatchOn(const QString& key, bool on);
	/** What a patch changes, as rich text; empty for none */
	Q_INVOKABLE QString patchExplanation(const QString& key) const;

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

	/** EasyRPG.ini was edited (in the script editor): what it says now */
	Q_INVOKABLE void readIni();
	/** Opens EasyRPG.ini in the script editor (created if missing), for
	 *  what is not shown; failures are told through dialogs */
	bool openIni(DialogService& dialogs);
	Q_INVOKABLE bool openIni();

	/** Why the settings cannot be written (the folder name), else empty */
	Q_INVOKABLE QString validate() const;
	/** Writes the title, the code page, the font, the patches and the fonts
	 *  of the Player in the files of the game: the error, else empty */
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
	void textFontChanged();
	void patchModeChanged();
	/** The patches shown changed: their values or the mode */
	void patchesChanged();
	void playerFontsChanged();
	/** EasyRPG.ini and the players were read again */
	void iniChanged();

private:
	/** The values the patch mode shows */
	const QMap<QString, int>& patchValues() const;

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

	/** Font files chosen for EasyRPG Player: empty for its own font; unset when unchanged */
	std::optional<QString> m_fontChoice[2];
};
