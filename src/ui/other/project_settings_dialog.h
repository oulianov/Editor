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

#include <QDialog>
#include <QDir>
#include <QMap>
#include <optional>

class Project;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;

/**
 * The settings of a game: its folder and title (RPG Maker's Rename
 * Project), the code page of its texts, the engine and the fonts it runs
 * with (converted to RPG Maker 2003 from here), and the patches EasyRPG
 * Player is told about.
 */
class ProjectSettingsDialog : public QDialog
{
	Q_OBJECT

public:
	/** open: the project open in the Editor (its engine is known and can be converted) */
	ProjectSettingsDialog(Project& project, bool open, QWidget* parent = nullptr);

	QString folderName() const;
	QString gameTitle() const;
	/** The code page chosen changed: the texts are read again */
	bool encodingChanged() const;
	/** Convert to RPG Maker 2003 was asked for, after the settings are written */
	bool convertRequested() const { return m_convert; }

	/** Writes the title, the code page, the patches and the fonts in the files of
	 *  the game: the error, else empty. The folder is renamed by the caller. */
	QString writeSettings(Project& project) const;

	void accept() override;

protected:
	void changeEvent(QEvent* event) override;
	bool eventFilter(QObject* watched, QEvent* event) override;

private:
	/** How the patches are decided */
	enum PatchMode { Detect, Choose, Refer };

	QWidget* generalPage(Project& project, bool open);
	QWidget* enginePage(Project& project, bool open);
	/** What only EasyRPG Player reads: the patches, the fonts */
	QWidget* playerPage();

	QString encoding() const;
	PatchMode patchMode() const;
	void showPatches();
	void explainPatch(const QString& key);
	/** EasyRPG.ini was edited (in the script editor): what it says now */
	void readIni();
	void openIni();
	/** The font file EasyRPG Player uses instead of font 1 or 2, as it will be */
	QString playerFont(int font) const;
	void showFonts();
	/** A sentence in the text font of the game */
	void showFontPreview();
	/** The game is in an Asian code page: the Player draws it with other fonts */
	bool cjkGame() const;

	Project& m_project;
	bool m_open;
	QDir m_folder;
	QString m_initialTitle;
	QLineEdit* m_folderName;
	QLineEdit* m_title;
	QComboBox* m_encoding = nullptr;
	QString m_encodingSetting;

	QLabel* m_runtime = nullptr;
	QLabel* m_exeFonts = nullptr;
	QLabel* m_playerFonts[2] = {};
	/** Font files chosen for EasyRPG Player: empty for its own font; unset when unchanged */
	std::optional<QString> m_fontChoice[2];

	QComboBox* m_textFont = nullptr;
	QLabel* m_fontPreview = nullptr;
	QComboBox* m_patchMode;
	QMap<QString, QCheckBox*> m_patchBoxes;
	QLabel* m_otherPatches;
	QLabel* m_patchHelp;
	QWidget* m_patchesPage = nullptr;
	QMap<QString, int> m_detected;
	QMap<QString, int> m_chosen;
	/** What EasyRPG.ini says */
	QMap<QString, int> m_written;
	bool m_patchesWereChosen = false;
	bool m_convert = false;
};
